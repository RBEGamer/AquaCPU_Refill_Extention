# WaterGuard
A automatic aquarium refiller with extra fallout detection.
This System fills a reserve Tank of your aquarium from a other watersource like a osmosis setup.
I used the system to fill automaticly the 4.5L tank of my RedSea Reefer Nano. Two water level sensors are installed in the tank so the system can check the water level and refill them from a watersource.


# FEATURES REFILL UNIT
* Automaitc refill of your tank
* Time failsafe
* Pump/Valve fail detection failsafe
* Esay Tank setup
* Sensor state failsafe
* FPGA based sensor state failsafe
* [OPTIONAL] CAN bus interface for interfacing with other AquaCPU Devices

# PARTS
* 2 Water level sensors for the refill tank (optical work best)
* 2 3D printed, water level sensor holders (See  http://www.thingiverse.com/thing:2218807)
* 1 Arduino Nano/Mini
* [OPTIONAL]  1 MCP2515 Can breakout board
* 1 Solid-State-Relais (or normal relais with transistor driver)
* 1 16x4 I2C Display (a 3d printed holder : http://www.thingiverse.com/thing:2218670)
* 1 Magnetic Valve 230V/12V
* 4 pusbuttons (with pull up resistors)
* 8 Luster Terminals
* 1 5V 2A Power Supply
* 1 ADS current sensor breakout

# SOFTWARE
* Download the Arduino-Sketch or the complete VS-Project
* Setup the pinconfig : 66-73
* Setup the invert setting : 74 - 75
* Setup the ACS712 sensor settings : 78
* Power all up and hold the config button to setup the other settings


# IMAGES
## Final simple case
![Gopher image](/documentation/IMAGES/case.jpg)
## Final Hardware setup (glued on wood for testing)
![Gopher image](/documentation/IMAGES/final_hardware.jpeg)

## Water level sensors, mounted on the reserve water tank
![Gopher image](/documentation/IMAGES/water_sensors.jpeg)
