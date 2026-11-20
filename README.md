# Samarine
<img width="1920" height="632" alt="chassis v16" src="https://github.com/user-attachments/assets/e3fc9cd8-694c-4530-a8b0-acddf55f3e6a" /> 
Samarine is a remotely operated underwater vehicle (ROV), meant to make underwater robotics more accessible to students and researchers who want to leverage its advantages to increase our understanding of marine environments.
 
It is made with a 3D printed chassis and 2 custom PCB boards (files on this repo), and an IP camera plus 2 LEDs. It features an ESP32 as the main "brain" of the robot. The ESP32 will receive control signals from a laptop and the camera will stream video to the laptop directly in H.264 video. 
 
## Why do this project?
Well, I live in a city where marine biology is very diverse and people from all over the country come to study it. But for local students, this kind of research is not always accessible, since you would need to acquire expensive diving equipment and courses in order to be able to explore the marine environment. Samarine is a way to make research more accessible. V1 includes just a camera, but it also leaves many ESP32 pins free to use for module attachment. Being completely open source, people can make their own modules and perform different kinds of research for a fraction of the cost of diving (sometimes even with more precision).

 ## propeler 
 you can use any. but im planning to use this https://www.printables.com/model/405235 for toroidal propelers
## Motor distribution
 
It has 3 motors: one to go up and down, and two for going forward and turning left or right.
 
## 3D assembly 
Well, I think it's pretty intuitive. There are 6 parts made to be printed in a 3D printer with an area of at least 32×33 cm. First you permanently glue with superglue the parts that make the top and bottom part.
<img width="857" height="508" alt="image" src="https://github.com/user-attachments/assets/7d75ab7d-f524-45fa-baf2-2c3b33719ae6" />
<img width="565" height="295" alt="image" src="https://github.com/user-attachments/assets/22462e6f-66fc-44bf-90e5-b13bd5a2d257" />
<img width="880" height="721" alt="image" src="https://github.com/user-attachments/assets/ec89767f-89fa-405f-b72e-6b40c420c45b" />
Finally, we just screw the top and bottom parts together with M3 screws, attach the motors to the motor pieces and screw everything together.
<img width="746" height="551" alt="image" src="https://github.com/user-attachments/assets/324fce4d-ef8e-4c88-81bf-17999ab5969e" />
 
## Quick guide on its electronics
 
 
The hull has a **12-pin waterproof connector** (M16, IP68) in the end cap. Everything that leaves the hull goes through it. The PCB's tether header J1 uses 6 of those pins; the camera's Ethernet pigtail goes straight to the connector and does not pass through the PCB.
 
**Tether connector (12 pins)**
 
| Pin | Signal | To | Notes |
| --- | --- | --- | --- |
| 1 | RS+ (A) | J1-2 | Twisted pair with pin 2 |
| 2 | RS− (B) | J1-1 | |
| 3 | CAM1 | Camera pigtail | Straight to the camera, not through the PCB. Twisted pair with pin 4 |
| 4 | CAM2 | Camera pigtail | |
| 5 | CAM3 | Camera pigtail | Twisted pair with pin 6 |
| 6 | CAM4 | Camera pigtail | |
| 7 | 1S | J1-4 | Balance tap |
| 8 | 2S | J1-5 | Battery + |
| 9 | GND | J1-6 | Battery − (BAT−) |
| 10 | ON | J1-3 | Turns the ROV on |
| 11 | Spare | — |  |
| 12 | Spare | — |  |
 
100 Mb Ethernet only uses two pairs (RJ45 pins 1, 2, 3, 6), so one cable pair stays free for RS-485. Keep each signal pair on one twisted pair of the cable.
 
**Inside the hull**
 
- **Battery → J7:** BAT−, 1S mid-tap, 2S+. Use thick wire (≥ 18 AWG) on BAT− and 2S+, since all three ESCs draw from it.
- **ESCs:** soldered to the board on their power and signal pads. Each ESC's 3 motor wires go to its thruster; swap any two to reverse spin (or flip it in BLHeliSuite).
- **Camera:** power from J2 (CAM+, GND); Ethernet pigtail to connector pins 3–6.
- **Lights:** J8.
- **Sensors / modules:** J3 (I²C) and J4–J6 (GPIO), routed to the front module connectors.
## Where electronics go
 (will do more detailed assembly tutorial with images once i got the hardware)
<img width="1387" height="1134" alt="image" src="https://github.com/user-attachments/assets/4b3a687a-39aa-4bf8-9ef9-f2d5588cf13a" />
 
Note that the LED path is for both the LED star and the LED controller.
 
