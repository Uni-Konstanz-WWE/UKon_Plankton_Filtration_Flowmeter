# COCO_flowmeter_V3-20
Measure and display flows using an Arduino Mega and an I2C-LCD

 *********************************************************************
 The fabulous "R3 PLANKTON FILTRATION - FLOW RATE MONITOR" ;-)  (v3-20 DAQ_1p6mm_allLCD, 05apr21)

 ************************************************************************
 coded by David Schleheck for Uni Konstanz
 modified by Alexander Fiedler for AG Schleheck Uni Konstanz
 additional modification by Maximilian Weidele, Wissenschaftliche Werkstätten Elektronik, Universität Konstanz
        
 Spec Ops:
 This program on Arduino_Mega is recording flow rates measured by three flowmeters (Hall-sensors), for example, once per second (adjustable). 
 The data is shown on LCD screen, and can be sent via USB to Excel running on a PC, using a program called PLX-DAQ (optional data recording).

 1) For the Arduion Mega processor that is: counting Hall-signals via the "INTERRUPT"-function from spec. "counting pins" (see below).
    For "interrupts" (ISR functions) see https:www.arduino.cc/en/Reference/AttachInterrupt 
    and Nick Gammon's page, http:gammon.com.au/interrupts, as well as here https:www.sparkfun.com/tutorials/326. 
    The main advantage of interrupts is, there are no countings lost while the programs is busy with doing other things; important for the determination 
    of total counts / total volume since startup of the program. Several interrupts can not run in parallel, but 
    interrupts will work independently: "While interrupts are disabled during an ISR's execution, any other interrupts will be flagged and executed in its turn."
    In fact too complex for my capabilities, but I found an example in the Arduino Forum of two interrupts counting things, coded by Faraday Member (thanks!), 
    "Here is some example code which reads two [now three] interrupts every second."
    https:forum.arduino.cc/index.php?PHPSESSID=2376j0hg3jtm2m9o0udg0c5l87&topic=358557.msg2472071#msg2472071/  
    Three Interrupt-ports are required on the arduino board in use, but the Arduino UNO board has only two of these:
      "Board-Digital Pins Usable For Interrupts:
       Uno, Nano, Mini, other 328-based:  2, 3;
       Mega, Mega2560, MegaADK:           2, 3, 18, 19, 20, 21;
       Micro, Leonardo, other 32u4-based: 0, 1, 2, 3, 7."
    Therefore, we're using ports 2, 3, and 18 on an Arduino MEGA board!
    For Arduino Mega, be aware of Automatic (Software) Reset during the startup of an communication via USB 
    (is this a problem for DAQ or MATLAB connection???),
    "It resets each time a connection is made to it from the computer software (via USB).
    This will intercept the first few bytes of data sent TO the board after a connection 
    is opened. If a sketch running on the board receives one-time configuration or other
    data when it first starts, make sure that the software with which it communicates waits
    a second after opening the connection and before sending this data"
    see https:store.arduino.cc/arduino-mega-2560-rev3
 2) calculate flow rate, in ml/min
 3) record maximal flow rate, Fm ; the flow rate measured right after startup of filtration, that is, after the overpressure has been increased to 2 bar; "empty filters" .
 4) calculate "harvesting flow rate", Fh ; for example "half-maximal" flowrate, at which the biomass on the filters is harvested.
 3) calculate total volume since startup, in ml, and convert to Liter (Note, in all program variables the Liters read as "Litres", e.g. flowMilliLitres_1 ;)
 4) display all data on LCD screen
 5) transmit all data to PC and PLX-DAQ via serial/USB port (serial communication is an interrupt itself, and takes time!)
 5b)[not yet implemented] send data to SD card (problem with interrupts?)= standalone datalogger; instead of sending data to PC 
 6) optical alarm on LCD screen for harvesting (indicated by "<" next to the present flowrate, e.g. 500<   )
 7) this whole thing 3-fold, for three sensors (= one filtration triplicate).
 8) three acustic alarms  / LED flashlights --> acustic alarm for 5 seconds at harvest time, LEDs light up continuosly at harvest time
 9) implemented time needed to reach harvest time, but not yet displayed at LCD display (only present as internal variable)
 opt10) [not yet implemented] three push-buttons for re-setting the total-volume counters
 opt11) [not yet implemented] switchboard for controlling things in dependence on flow rate or total volume


If the device is connected to a PC via the USB it is powered this way. Otherwise it has to be powered by a 9 V battery using the power connector as shown in Flowmeter_Battery.jpg.
