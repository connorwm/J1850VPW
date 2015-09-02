J1850VPW
========

Arduino Code (eventually library) for communicating with J1850 VPW OBD-II equipped cars. 

Code compiles and should work given the correct hardware setup. A couple of optocouplers (or maybe even transistors given Arduino's high tolerances) can be used to correctly send signals to the car's computer. Simply connect the BUS+ line (pin 2) of the OBD-II connector to the desired IOPin on the Arduino and let the software do the work. However, in order to achieve logical highs you will need to switch at least 7 Volts to the bus. The 5 Volts from the Arduino will not be sufficient. Additionally, to protect your Arduino, use an optocoupler to read the signal. These hardware requirements are not built into the code but simple modifications can be made to the sendBit and getBit functions as well as the SOD, EOF, and EOD functions. I plan on making these changes as soon as I can.

Serial communication is necessary to send and receive messages. Code is currently unproven as of original posting due to lack of time to test it out (I also had to sell my car I was using but plan on getting an ECM from another to test it).  If you do try or test this code please let me know: me@connorwmurphy.com

Also, feel free to send me any questions you may have.
