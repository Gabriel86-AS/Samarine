/*
  ============================================================
   SAMARINE V1 - ESP32 firmware 
  ============================================================
  What it does:
    1. Listens for commands from the laptop over RS-485 (tether).
    2. Drives 3 thrusters (left, right, vertical) through their ESCs.
    3. Dims/brightens the front lights.
    4. Measures the battery (whole pack + each cell) and reports it.
    5. Stops the motors by itself if the tether goes silent (failsafe).

  Board: "ESP32 Dev Module"  |  Arduino-ESP32 core 3.x (uses ledcAttach)
  No extra libraries needed.

  ---------- COMMAND PROTOCOL (plain text, one line each) ----------
  Laptop -> ROV :  M,<left>,<right>,<vertical>,<light>\n
                   left/right/vertical = -100..100 (% power, negative = reverse)
                   light               =    0..100 (% brightness)
                   Example: "M,50,50,-20,80"  -> forward at half power, sink a bit, lights 80%
  ROV -> Laptop :  B,<pack V>,<cell1 V>,<cell2 V>,<OK|LOW>\n   (max 2 times per second)

  You can type the same commands in the Arduino Serial Monitor (USB, 115200,
  "Newline") to test on the bench without the RS-485 adapter.

  IMPORTANT: for the thrusters to go in reverse, each BLHeli_S ESC must be set
  to "3D mode" (bidirectional) with BLHeliSuite. In 3D mode 1500 us = stopped.
*/

#include <Arduino.h>

// ---------------- PINS (taken from Samarine Motherboard.kicad_sch) ----------------
const int PIN_ESC_LEFT   = 23;  // label ESC_L -> ESC_2
const int PIN_ESC_RIGHT  = 22;  // label ESC_R -> ESC_1
const int PIN_ESC_VERT   = 25;  // label ESC_C -> ESC_3 (up/down thruster)
const int PIN_LIGHTS     = 27;  // label LED   -> J8 (lights)
const int PIN_RS485_RX   = 16;  // RX2 <- MAX3485 RXD
const int PIN_RS485_TX   = 17;  // TX2 -> MAX3485 TXD
const int PIN_RS485_EN   = 4;   // DVR_EN: HIGH = ROV talks, LOW = ROV listens
const int PIN_BAT_PACK   = 34;  // whole 2S pack through 100k / 33k divider (R6/R5)
const int PIN_BAT_CELL1  = 35;  // cell 1 (1S) through 100k / 100k divider (R4/R3)

// ---------------- SETTINGS ----------------
const long BAUD_RATE        = 115200; // must match the laptop
const int  ESC_FREQ_HZ      = 50;     // classic servo signal: one pulse every 20 ms
const int  ESC_RES_BITS     = 16;     // PWM resolution (0..65535)
const int  PULSE_STOP_US    = 1500;   // 3D mode: stopped
const int  PULSE_RANGE_US   = 400;    // 1500 +/- 400 -> 1100..1900 us (full reverse..full forward)
const int  LIGHT_FREQ_HZ    = 1000;   // PWM frequency for the lights
const unsigned long FAILSAFE_MS  = 1000; // no command for 1 s -> motors stop
const unsigned long TELEMETRY_MS = 500;  // battery report at most every 0.5 s
const float CELL_LOW_V      = 3.3;    // below this a cell is "LOW" -> time to surface

// Divider ratios: Vreal = Vpin * (Rtop + Rbottom) / Rbottom
const float PACK_DIVIDER  = (100.0 + 33.0) / 33.0;   // R6=100k, R5=33k
const float CELL1_DIVIDER = (100.0 + 100.0) / 100.0; // R4=100k, R3=100k

// ---------------- STATE ----------------
unsigned long lastCommandMs   = 0;  // when the last valid command arrived
unsigned long lastTelemetryMs = 0;  // when we last sent the battery report
String rs485Buffer = "";            // characters received from the tether
String usbBuffer   = "";            // characters received from USB (bench tests)


// Convert a power value (-100..100) into an ESC pulse and send it
void setThruster(int pin, int power) {
  power = constrain(power, -100, 100);                          // never trust input
  int pulseUs = PULSE_STOP_US + power * PULSE_RANGE_US / 100;   // e.g. 50% -> 1700 us
  uint32_t duty = (uint32_t)pulseUs * 65535UL / 20000UL;        // us -> PWM duty (20 ms period)
  ledcWrite(pin, duty);
}

// Set the light brightness (0..100 %)
void setLights(int percent) {
  percent = constrain(percent, 0, 100);
  ledcWrite(PIN_LIGHTS, map(percent, 0, 100, 0, 255));
}

// Stop all three thrusters
void stopThrusters() {
  setThruster(PIN_ESC_LEFT, 0);
  setThruster(PIN_ESC_RIGHT, 0);
  setThruster(PIN_ESC_VERT, 0);
}

// Read a pin several times and return the averaged voltage in volts
float readVolts(int pin, float divider) {
  long totalMv = 0;
  for (int i = 0; i < 8; i++) totalMv += analogReadMilliVolts(pin); // factory-calibrated reading
  return (totalMv / 8.0) / 1000.0 * divider;
}

// Send the battery report to the laptop (RS-485) and to USB
void sendTelemetry() {
  float pack  = readVolts(PIN_BAT_PACK, PACK_DIVIDER);
  float cell1 = readVolts(PIN_BAT_CELL1, CELL1_DIVIDER);
  float cell2 = pack - cell1;                       // cell 2 = pack minus cell 1
  bool low = (cell1 < CELL_LOW_V) || (cell2 < CELL_LOW_V);

  char msg[48];
  snprintf(msg, sizeof(msg), "B,%.2f,%.2f,%.2f,%s\n", pack, cell1, cell2, low ? "LOW" : "OK");

  digitalWrite(PIN_RS485_EN, HIGH);  // take the line to talk
  Serial2.print(msg);
  Serial2.flush();                   // wait until every byte has left
  digitalWrite(PIN_RS485_EN, LOW);   // give the line back to the laptop

  Serial.print(msg);                 // copy to USB for debugging
}

// Understand one full line of text
void handleLine(const String &line) {
  int left, right, vert, light;
  // sscanf returns how many numbers it found; we need all 4
  if (sscanf(line.c_str(), "M,%d,%d,%d,%d", &left, &right, &vert, &light) == 4) {
    setThruster(PIN_ESC_LEFT, left);
    setThruster(PIN_ESC_RIGHT, right);
    setThruster(PIN_ESC_VERT, vert);
    setLights(light);
    lastCommandMs = millis();        // feed the failsafe

    // Reply right after a command: the laptop just finished talking,
    // so the shared RS-485 line is free (no collisions).
    if (millis() - lastTelemetryMs >= TELEMETRY_MS) {
      lastTelemetryMs = millis();
      sendTelemetry();
    }
  }
}

// Collect characters until a newline, then process the line
void readLines(Stream &port, String &buffer) {
  while (port.available()) {
    char c = port.read();
    if (c == '\n') {
      handleLine(buffer);
      buffer = "";
    } else if (c != '\r' && buffer.length() < 64) {  // ignore \r, cap length against garbage
      buffer += c;
    }
  }
}


void setup() {
  Serial.begin(BAUD_RATE);                                         // USB
  Serial2.begin(BAUD_RATE, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX); // tether

  pinMode(PIN_RS485_EN, OUTPUT);
  digitalWrite(PIN_RS485_EN, LOW);   // start listening

  // Attach PWM to the ESC pins and the lights
  ledcAttach(PIN_ESC_LEFT,  ESC_FREQ_HZ, ESC_RES_BITS);
  ledcAttach(PIN_ESC_RIGHT, ESC_FREQ_HZ, ESC_RES_BITS);
  ledcAttach(PIN_ESC_VERT,  ESC_FREQ_HZ, ESC_RES_BITS);
  ledcAttach(PIN_LIGHTS,    LIGHT_FREQ_HZ, 8);

  analogSetAttenuation(ADC_11db);    // lets the ADC read up to ~3.1 V

  // Arm the ESCs: they must see "stopped" for a few seconds after power-up
  stopThrusters();
  setLights(0);
  delay(3000);

  Serial.println("Samarine ready");
}

void loop() {
  readLines(Serial2, rs485Buffer);   // commands from the laptop (tether)
  readLines(Serial,  usbBuffer);     // commands from USB (bench tests)

  // FAILSAFE: if the tether goes quiet, stop the thrusters (lights stay as they were)
  if (millis() - lastCommandMs > FAILSAFE_MS) {
    stopThrusters();
  }
}
