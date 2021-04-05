// *********************************************************************
// The fabulous "R3 PLANKTON FILTRATION - FLOW RATE MONITOR" ;-)  (v3-20 DAQ_1p6mm_allLCD, 05apr21)

// ************************************************************************
// coded by David Schleheck for Uni Konstanz
// modified by Alexander Fiedler for AG Schleheck Uni Konstanz
        
// Spec Ops:
// This program on Arduino_Mega is recording flow rates measured by three flowmeters (Hall-sensors), for example, once per second (adjustable). 
// The data is shown on LCD screen, and can be send via USB to Excel running on a PC, using a program called PLX-DAQ (optional data recording).

// 1) For the Arduion Mega processor that is: counting Hall-signals via the "INTERRUPT"-function from spec. "counting pins" (see below).
//    For "interrupts" (ISR functions) see https://www.arduino.cc/en/Reference/AttachInterrupt 
//    and Nick Gammon's page, http://gammon.com.au/interrupts, as well as here https://www.sparkfun.com/tutorials/326. 
//    The main advantage of interrupts is, there are no countings lost while the programs is busy with doing other things; important for the determination 
//    of total counts / total volume since startup of the program. Several interrupts can not run in parallel, but 
//    interrupts will work independently: "While interrupts are disabled during an ISR's execution, any other interrupts will be flagged and executed in its turn."
//    In fact too complex for my capabilities, but I found an example in the Arduino Forum of two interrupts counting things, coded by Faraday Member (thanks!), 
//    "Here is some example code which reads two [now three] interrupts every second."
//    https://forum.arduino.cc/index.php?PHPSESSID=2376j0hg3jtm2m9o0udg0c5l87&topic=358557.msg2472071#msg2472071/  
//    Three Interrupt-ports are required on the arduino board in use, but the Arduino UNO board has only two of these:
//      "Board-Digital Pins Usable For Interrupts:
//       Uno, Nano, Mini, other 328-based:  2, 3;
//       Mega, Mega2560, MegaADK:           2, 3, 18, 19, 20, 21;
//       Micro, Leonardo, other 32u4-based: 0, 1, 2, 3, 7."
//    Therefore, we're using ports 2, 3, and 18 on an Arduino MEGA board!
//    For Arduino Mega, be aware of Automatic (Software) Reset during the startup of an communication via USB 
//    (is this a problem for DAQ or MATLAB connection???),
//    "It resets each time a connection is made to it from the computer software (via USB).
//    This will intercept the first few bytes of data sent TO the board after a connection 
//    is opened. If a sketch running on the board receives one-time configuration or other
//    data when it first starts, make sure that the software with which it communicates waits
//    a second after opening the connection and before sending this data"
//    see https://store.arduino.cc/arduino-mega-2560-rev3
// 2) calculate flow rate, in ml/min
// 3) record maximal flow rate, Fm ; the flow rate measured right after startup of filtration, that is, after the overpressure has been increased to 2 bar; "empty filters" .
// 4) calculate "harvesting flow rate", Fh ; for example "half-maximal" flowrate, at which the biomass on the filters is harvested.
// 3) calculate total volume since startup, in ml, and convert to Liter (Note, in all program variables the Liters read as "Litres", e.g. flowMilliLitres_1 ;)
// 4) display all data on LCD screen
// 5) // switched off // transmit all data to PC and PLX-DAQ via serial/USB port (serial communication is an interrupt itself, and takes time!)
// 5b)[not yet implemented] send data to SD card (problem with interrupts?)= standalone datalogger; instead of sending data to PC 
// 6) optical alarm on LCD screen for harvesting (indicated by "<" next to the present flowrate, e.g. 500<   )
// 7) this whole thing 3-fold, for three sensors (= one filtration triplicate).
// 8) three acustic alarms  / LED flashlights --> acustic alarm for 5 seconds at harvest time, LEDs light up continuosly at harvest time
// 9) implemented time needed to reach harvest time, but not yet displayed at LCD display (only present as internal variable)
// opt10) [not yet implemented] three push-buttons for re-setting the total-volume counters
// opt11) [not yet implemented] switchboard for controlling things in dependence on flow rate or total volume


// DEFINES

#define BAUD_RATE     128000  //115200  //9600  //128000

#define SENSOR1_PIN   2
#define SENSOR2_PIN   3
#define SENSOR3_PIN  18

#define SPEAKER_PIN   9

#define LED1_PIN     10
#define LED2_PIN     11
#define LED3_PIN     12

// Harvest Flow Percentages (1...100)
#define HARVEST_FLOW_1  50
#define HARVEST_FLOW_2  50
#define HARVEST_FLOW_3  50

// Calibration Factors (for flowmeters: FCH-m-PP with a 1 mm nozzle)
#define CALIBRATION_FACTOR_1  0.3530
#define CALIBRATION_FACTOR_2  0.3330
#define CALIBRATION_FACTOR_3  0.3530

//*********************************************
//Include libraries for
//communication via I2C (e.g. with LCD screen)
#include <Wire.h> 
//Controller for LCD screen ( 20x4, Manufacturer JOY-IT, Model# SBC-LCD20X4)
#include <LiquidCrystal_I2C.h> 
//see http://www.jhrweb.de/arduino-sainsmart-iici2ctwi-serial-2004-20x4-lcd-module-shield.html  
//or try this library: https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads/
//Hardware adressing:
LiquidCrystal_I2C lcd(0x27,20,4);
//if LCD does not work, comment-out (//) the line above 
//and de-comment the line below with another I2C address (0x3F)
//LiquidCrystal_I2C lcd(0x3F,20,4); 
//Pin connection between LCD and arduino-MEGA, VCC:+5V, GND:GND, SDA:pin20, SCL:pin21.
//******************************************

// VARIABLES
//______________________________________________________________________________
// FLOWMETER CALIBRATION
// flow sensor used: Manufacturer, B.I.O-TECH e.K., Zeitlarnerstrasse 32, D-94474 Vilshofen; 
// model, FCH-m-PP Mini-Flowmeter., Art.#: 92202916
// http://www.btflowmeter.com/en/home.html
// Note that the flowmeter range is dependent on the diameter of the nozzle used, from 0.2mm up to 2.0mm !

// For example, FCH-m-POM with a 1.6-mm nozzle:
// 
// CALCULATED CALIBRATION (datasheet)
// 0.03 - 1,6 L/min, 8500 pulses/L
// 1000 ml/min = 8500 pulses/min = 141.67 pulses/second
// 1ml/second = 0.14167 pulses

// CORRECTED CALIBRATION !! 
// as determined by experimental testing (and still not accurate enough!): 0.0855 (why?)


// ATTENTION!!! New flowmeters: FCH-m-PP with a 1 mm nozzle:
// CALCULATED CALIBRATION (datasheet)
// 0.015 - 0.9 L/min, 20500 pulses/L
// 1000 ml/min = 20500 pulses/min = 341.66 pulses/second
// 1 ml/second = 0.34166 pulses
// Calibration corrected by experimental testing (see calibrationFactor below)

// calculation = counts DIVIDED by calibration factor

//****************************************************************************************************************************
float calibrationFactor_1 = CALIBRATION_FACTOR_1;    //"float", datatype for floating-point numbers; numbers that have decimal points
float calibrationFactor_2 = CALIBRATION_FACTOR_2;
float calibrationFactor_3 = CALIBRATION_FACTOR_3;
//****************************************************************************************************************************

//parameters for determining the harvesting flow rate (in % of max flow rate) e.g. 50 = half-maximal flow rate
unsigned int flowparam1 = HARVEST_FLOW_1;        
unsigned int flowparam2 = HARVEST_FLOW_2;        
unsigned int flowparam3 = HARVEST_FLOW_3;        
//****************************************************************************************************************************

// Status LED pin (on the Arduino Board)
int statusLed = LED_BUILTIN;

// three LEDs as optical alarm for harvest
int LED1 = LED1_PIN;
int LED2 = LED2_PIN;
int LED3 = LED3_PIN;

// speaker is pin 9
int speaker = SPEAKER_PIN;

// three Interrupt Inputs for the Flow Sensors
int sensor1 = SENSOR1_PIN;
int sensor2 = SENSOR2_PIN;
int sensor3 = SENSOR3_PIN;

unsigned long Millistillharvest1 = 0;
unsigned long Millistillharvest2 = 0;
unsigned long Millistillharvest3 = 0;

//time stamp in microseconds, since starting the program, for serial recording 
//unsigned long time = 0;

//pulse counts changed during one counting interval / count_ISR
volatile unsigned int count_1 = 0;
volatile unsigned int count_2 = 0;
volatile unsigned int count_3 = 0;
//these counts go back to zero in each round, by the void "getcount" below (during "nointerrupts")

// variables for the "protected copies" of counts,
// important for displaying the data while counting on.
// All calculations resp. ml/min are done with the values of "copyCounts".
unsigned int copyCount_1 = 0;
unsigned int copyCount_2 = 0;
unsigned int copyCount_3 = 0;

//variable for timing the interrupt-counters in "micros" = 1,000,000 microseconds in a second.
static unsigned long lastSecond = 0;

unsigned int flowRate_1 = 0;        //calculated flowrate in this 1 second interval; was changed to "unsigned int", from previously "float".
unsigned int flowRate_2 = 0;
unsigned int flowRate_3 = 0;

unsigned long flowRate_max_1 = 0;    // maximal flowrate; updated in every loop against actual flowrate
unsigned long flowRate_max_2 = 0;
unsigned long flowRate_max_3 = 0;

unsigned long flowRate_half_max_1 = 0;  // "half-maximal flowrate", harvesting flow rate
unsigned long flowRate_half_max_2 = 0;
unsigned long flowRate_half_max_3 = 0;

float flowMilliLitres_1 = 0.0;        // was changed to "float" because of decimal point, it can be 0.99 ml or less per second (1/60 of minute), from previously "unsigned int".
float totalMilliLitres_1 = 0.0;         // was changed to "(unsigned) int", the decimal fraction is discarded (?)
float flowMilliLitres_2 = 0.0;
float totalMilliLitres_2 = 0.0;
float flowMilliLitres_3 = 0.0;
float totalMilliLitres_3 = 0.0;

float totalLitres_1 = 0.0;    // "float" because of decimal point (1/1000)
float totalLitres_2 = 0.0;
float totalLitres_3 = 0.0;


//SUBROUTINES
//dont touch
//*******************
//subroutine for reading the counters and resetting prior to next round of counting, while interrupts are switched off
void getCount()
{
  noInterrupts();
  copyCount_1 = count_1;
  count_1 = 0;
  copyCount_2 = count_2;
  count_2 = 0;
  copyCount_3 = count_3;
  count_3 = 0;
  interrupts();

}
//subroutines for (almost) continuous counting (ISR)
void count_1_ISR()
{count_1++;}
void count_2_ISR()
{count_2++;}
void count_3_ISR()
{count_3++;}


// SETUP
//*******************************************************************
void setup() //this part runs only once
{
 // Start counting with the following parameters:
 // Define addresses# of the interrupts, which variables are to be used, 
 // and which part of the sensor signal triggers the counting (RISING),
 // "attachInterrupt([INTERRUPT#], count_1_ISR, RISING);" //pin[NUMBER] on Arduion MEGA.   
 // Pins usable for Interrupts on Arduino Mega, Mega2560, MegaADK boards are [2,3, and 18,19,20,21], 
 // which are addressed as interrupts# [0,1, and 5,4,3,2!] respectively!
 // However, pins 20 and 21 can NOT be used as interupts in this setup, since 
 // they are busy as SDA (20) and SCL (21) for I2C-to-LCD communication 
 // (i.e., on Arduino MEGA the I2C is NOT on pins A4 and A5, as with Ardunio UNO). 
 // see https://arduino-info.wikispaces.com/MegaQuickRef 
 // Therefore, we use interrupt# 0 (= pin  2 on Arduion MEGA connected to sensor 1)
 //                   interrupt# 1 (= pin  3, sensor 2)
 //                   interrupt# 5 (= pin 18, sensor 3)
  attachInterrupt(digitalPinToInterrupt(sensor1), count_1_ISR, RISING); //pin  2 (Arduion MEGA)
  attachInterrupt(digitalPinToInterrupt(sensor2), count_2_ISR, RISING); //pin  3 (Arduion MEGA)
  attachInterrupt(digitalPinToInterrupt(sensor3), count_3_ISR, RISING); //pin 18 (Arduion MEGA)

  // Set up the sensor lines as inputs with pullups
  pinMode(sensor1, INPUT_PULLUP);
  pinMode(sensor2, INPUT_PULLUP);
  pinMode(sensor3, INPUT_PULLUP);

  // Set up the LED lines as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  digitalWrite(LED1, LOW);  // initially switched off
  digitalWrite(LED2, LOW);  // initially switched off
  digitalWrite(LED3, LOW);  // initially switched off

  // Set up the speaker line as an output
  pinMode(speaker, OUTPUT); // this is the beeper
  digitalWrite(speaker, LOW);  // initially switched off
   
  // Set up the status LED line as an output
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed, HIGH);  // We have an active-low LED attached

  //set up LCD screen
  lcd.init();
  lcd.begin(20,4);
  delay(20);         //some time for initializing 
  lcd.backlight();
  delay(500);
  lcd.cursor();
  
  lcd.setCursor (1,0);
  lcd.print("Hello scientist :)");
  delay(1000);
  lcd.noCursor();
  lcd.setCursor (1,2);
  lcd.print("Initializing...");
  delay(1000);
  lcd.setCursor (1,3);
  lcd.print("Let's filtrate!");
  delay(1000);
  //lcd.clear();
                               //write labels onto LCD screen
    lcd.begin(20,4);               // initialize the LCD for 20x4; addressed as (row0-19, line0-4)
    //lcd.clear();                 // empty the LCD //switched off, "blanking out" is used instead, see below
    lcd.setCursor (0,0);
    lcd.print("Fm");  
    lcd.setCursor (0,1);
    lcd.print(">");
    lcd.setCursor (0,2);
    lcd.print("Fh");
    lcd.setCursor (0,3);   
    lcd.print("V");
    delay (2000);  
     
  // Initialize a serial connection for reporting measurements via USB to host PC
  // with appropriate Baud-rate, e.g. 9600 Baud (the faster the better)
  Serial.begin(BAUD_RATE);

  // Initialize data transfer to Excel via PLX-DAQ:
  // The program PLX-DAQ on a PC analyzes incoming serial-data strings from the BASIC Stamp for action.
  // Strings begin with a directive informing PLX-DAQ of what action to take.  
  // The data sent must be formatted properly to be understood by PLX-DAQ, see here https://www.parallax.com/downloads/plx-daq 
  // and http://www.instructables.com/id/Sending-data-from-Arduino-to-Excel-and-plotting-it/
  // All directives are in CAPITAL letters, and some are followed by comma-separated data. 
  // Each string MUST end in a carriage return (CR).
  // Strings not beginning with directives will be ignored. 
  // Strings containing ASCII characters < 10 or > 200 will not be processed and indicated as an error.  
  
  Serial.println("CLEARDATA");                //clears up any data left from previous projects
  Serial.println("LABEL, time, total_sec, f1,f2,f3");         //"LABEL", so that Excel knows the next things will be the names of the columns
  Serial.println("RESETTIMER");             //resets timer to 0

}

// LOOP
//************************************************************
void loop() //this part runs over and over again
{
// "micros" returns the number of microseconds since the Arduino board began running the current program. 
// This number will overflow (go back to zero) after approximately 70 minutes. On 16 MHz Arduino 
// boards (e.g. Mega, Duemilanove and Nano), this function has a resolution of four microseconds (i.e. the value 
// returned is always a multiple of four).
// Note: there are 1,000 microseconds in a millisecond, and 1,000,000 microseconds in a second.

  if (micros() - lastSecond >= 1000000L) //"1000000L" = by default, an integer constant is treated as an int with 
  //the attendant limitations in values. To specify an integer constant with another data type, follow it with
  //an 'U' to force the constant into an unsigned data format, or 'L' to force it into a long data format.
  {
    lastSecond += 1000000;
    //add another 1,000,000 microseconds to the total counter
    
    getCount(); //go to subroutine for creating the "copyCounts", i.e., switch off interrupts, 
    //read the counters, set them to zero, and switch interrupts on again; this takes time.
     
    // Then go on, calculating from "copyCount" to flow rate in ml/min
    // IMPORTANT: Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of microseconds that have passed since the last execution of copyCount, and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure coming from the sensor
    flowRate_1 = ((micros() / lastSecond) * copyCount_1) / calibrationFactor_1;
    flowRate_2 = ((micros() / lastSecond) * copyCount_2) / calibrationFactor_2;
    flowRate_3 = ((micros() / lastSecond) * copyCount_3) / calibrationFactor_3;
     
    // compare actual flowrate with stored maximal flowrate; 
    // if larger, then store actual flowrate as new max.flowrate, and calculate new half-max.flowrate / harvesting flowrate for a given "H_setting" 
    if (flowRate_1 >= flowRate_max_1)
    { flowRate_max_1 = flowRate_1;
      flowRate_half_max_1 = (flowRate_max_1 * flowparam1 / 100); }
    if (flowRate_2 >= flowRate_max_2)
    { flowRate_max_2 = flowRate_2;
      flowRate_half_max_2 = (flowRate_max_2 * flowparam2 / 100); }
    if (flowRate_3 >= flowRate_max_3)
    { flowRate_max_3 = flowRate_3;
      flowRate_half_max_3 = (flowRate_max_3 * flowparam3 / 100); }

    //then calculate total volume flow since startup:
    // Divide the flow rate in ml/min by 60 to determine how many ml have passed through the sensor in this 1 second interval
    flowMilliLitres_1 = flowRate_1 / 60;
    flowMilliLitres_2 = flowRate_2 / 60;
    flowMilliLitres_3 = flowRate_3 / 60;
    // Add the millilitres passed in this 1 second to the cumulative total:
    totalMilliLitres_1 += flowMilliLitres_1;
    totalMilliLitres_2 += flowMilliLitres_2;
    totalMilliLitres_3 += flowMilliLitres_3;
    //convert into Liters; for display on LCD (99.9 Liters)
    totalLitres_1 = (totalMilliLitres_1 / 1000);
    totalLitres_2 = (totalMilliLitres_2 / 1000);
    totalLitres_3 = (totalMilliLitres_3 / 1000);
  
// serial-printing to PLX-DAQ and data transfer to Excel
  // see here https://www.parallax.com/downloads/plx-daq
  // All directives are in CAPITAL letters, and some are followed by comma-separated data. 
  // Each string MUST end in a carriage return (CR).
  // Strings not beginning with directives will be ignored. 
  // Strings containing ASCII characters < 10 or > 200 will not be processed and indicated as an error. 
    Serial.print("DATA,TIME,TIMER,");
    Serial.print(int(flowRate_1));  // Print the integer part of the variable
    Serial.print(",");
    Serial.print(int(flowRate_2));
    Serial.print(",");
    Serial.print(int(flowRate_3));
    Serial.print(",");
    //Serial.print(int(totalMilliLitres_1));
    //Serial.print(",");
    //Serial.print(int(totalMilliLitres_2));
    //Serial.print(",");
    //Serial.print(int(totalMilliLitres_3));
    Serial.println (); //carriage return
     
    
    
    // print all date once per second onto the LCD screen
    // and indicate (=optical alarm) if flow rate has dropped below harvesting-flowrate (with "<" next to flowrate)
    // We use a 20x4 display, thus lcd.setCursor positions is (column 0-19, row 0-4)
    
  
    lcd.setCursor (3,0); // instead of lcd.clear, which needs a lot of time, use 
    lcd.print ("    ");  // over-writing by an appropriate amount of blanks first (blanking out), then re-position the cursor and write new data
    // with "standing zero"; adjust cursor position, max. 99999
    if (flowRate_max_1 < 10000) lcd.setCursor (3,0); 
    if (flowRate_max_1 < 1000) lcd.setCursor (4,0);
    if (flowRate_max_1 < 100) lcd.setCursor (5,0);
    if (flowRate_max_1 < 10) lcd.setCursor (6,0);  
    lcd.print (flowRate_max_1);
    lcd.setCursor (3,1);
    lcd.print ("    ");
    if (flowRate_1 < 10000) lcd.setCursor (3,1);
    if (flowRate_1 < 1000) lcd.setCursor (4,1);
    if (flowRate_1 < 100) lcd.setCursor (5,1);
    if (flowRate_1 < 10) lcd.setCursor (6,1);        
    lcd.print (flowRate_1);
    
      //visual alarm ("<") for harvesting:
      if (flowRate_1 > flowRate_half_max_1)            //if actual flowrate is (again) above half-max-flowrate, blank-out  "<" by " "; 
      {                                                //for avoiding "false alarm" during filling of the filters.
        lcd.setCursor (7,1); 
        lcd.print(" ");   
        digitalWrite(LED1, LOW);                      // deactivates LED in Pin 10   
        noTone(speaker);
        Millistillharvest1 = millis();    
      
      }
      
      else if (flowRate_1 < flowRate_half_max_1)       //optical alarm; if actual flowrate falls below half-max-flowrate, indicate this with "<"
      {
        lcd.setCursor (7,1);
        lcd.print ("<");
        digitalWrite(LED1, HIGH);                    // activates LED in pin 10 

        //now comes activation of the speaker in a second if condition within the first if condition
         // check to see if it's time to change the state of the speaker
         
         if (Millistillharvest1 + 5000 < millis())  //checks whether 5000 milli seconds have passed since flowrate was under half-max flowrate
        {
          noTone(speaker);    // turns off speaker 
        }
         else if ((Millistillharvest1 + 5000 >= millis()) && (millis()>60000) ) //if not more than 5 seconds have passed since above condition --> speaker gets activated
        {
          tone(speaker, 1000);   //sends 1000 Hz sound signal from pin speaker
        }
        else
        {
          noTone(speaker);
        }
        
      }
      else
      {  
        lcd.setCursor (7,1); 
        lcd.print(" ");
        digitalWrite(LED1, LOW);  
        noTone(speaker);
      }
      
    lcd.setCursor (3,2);
    lcd.print ("    ");
    if (flowRate_half_max_1 < 10000) lcd.setCursor (3,2);
    if (flowRate_half_max_1 < 1000) lcd.setCursor (4,2);
    if (flowRate_half_max_1 < 100) lcd.setCursor (5,2);
    if (flowRate_half_max_1 < 10) lcd.setCursor (6,2);
    lcd.print (flowRate_half_max_1);
    lcd.setCursor (2,3);
    lcd.print ("     ");
    if (totalLitres_1 < 100.0) lcd.setCursor (1,3);
    if (totalLitres_1 < 10.0) lcd.setCursor (2,3);
    lcd.print (totalLitres_1, 3); //print with three decimal points

    lcd.setCursor (9,0);
    lcd.print ("    ");
    if (flowRate_max_2 < 10000) lcd.setCursor (9,0);
    if (flowRate_max_2 < 1000) lcd.setCursor (10,0);
    if (flowRate_max_2 < 100) lcd.setCursor (11,0);
    if (flowRate_max_2 < 10) lcd.setCursor (12,0);  
    lcd.print (flowRate_max_2);
    lcd.setCursor (9,1);
    lcd.print ("    ");
    if (flowRate_2 < 10000) lcd.setCursor (9,1);
    if (flowRate_2 < 1000) lcd.setCursor (10,1);
    if (flowRate_2 < 100) lcd.setCursor (11,1);
    if (flowRate_2 < 10) lcd.setCursor (12,1);        
    lcd.print (flowRate_2);
      //visual alarm ("<") for harvesting:
      if (flowRate_2 > flowRate_half_max_2)            
      {                                                
        lcd.setCursor (13,1); 
        lcd.print(" "); 
        digitalWrite(LED2, LOW);
        noTone(speaker);
        Millistillharvest2 = millis();                                 
      }
      else if (flowRate_2 < flowRate_half_max_2)       
      {
        lcd.setCursor (13,1); 
        lcd.print("<");
        digitalWrite(LED2, HIGH);
        
         if (Millistillharvest2 + 5000 < millis())  //checks whether 5000 milli seconds have passed since flowrate was under half-max flowrate
        {
          noTone(speaker);    // turns off speaker 
        }
         else if ((Millistillharvest2 + 5000 >= millis()) && (millis()>60000) ) //if not more than 5 seconds have passed since above condition --> speaker gets activated
        {
          tone(speaker, 1000);   //sends 1000 kHz sound signal from pin speaker
        }
        else
        {
          noTone(speaker);
        }
        
      }
      else
      {  
        lcd.setCursor (13,1); 
        lcd.print(" ");
        digitalWrite(LED2, LOW); 
        noTone(speaker); 
      }
      
    lcd.setCursor (9,2);
    lcd.print ("    ");
    if (flowRate_half_max_2 < 10000) lcd.setCursor (9,2);
    if (flowRate_half_max_2 < 1000) lcd.setCursor (10,2);
    if (flowRate_half_max_2 < 100) lcd.setCursor (11,2);
    if (flowRate_half_max_2 < 10) lcd.setCursor (12,2);
    lcd.print (flowRate_half_max_2);
    lcd.setCursor (8,3);
    lcd.print ("     ");
    if (totalLitres_2 < 100.0) lcd.setCursor (7,3);
    if (totalLitres_2 < 10.0) lcd.setCursor (8,3);
    lcd.print (totalLitres_2, 3); //print with three decimal points
    
    lcd.setCursor (15,0);
    lcd.print ("    ");
    if (flowRate_max_3 < 10000) lcd.setCursor (15,0);
    if (flowRate_max_3 < 1000) lcd.setCursor (16,0);
    if (flowRate_max_3 < 100) lcd.setCursor (17,0);
    if (flowRate_max_3 < 10) lcd.setCursor (18,0);  
    lcd.print (flowRate_max_3);
    lcd.setCursor (15,1);
    lcd.print ("    ");
    if (flowRate_3 < 10000) lcd.setCursor (15,1);
    if (flowRate_3 < 1000) lcd.setCursor (16,1);
    if (flowRate_3 < 100) lcd.setCursor (17,1);
    if (flowRate_3 < 10) lcd.setCursor (18,1);        
    lcd.print (flowRate_3);
      //visual alarm ("<") for harvesting:
      if (flowRate_3 > flowRate_half_max_3)            //if actual flowrate is (again) above half-max-flowrate, blank-out the "<" with a "."; 
      {                                                //important for avoiding "false alarm" during filling of the filters...
        lcd.setCursor (19,1); 
        lcd.print(" ");                                // "." means "all good, wait"
        digitalWrite(LED3, LOW);  
        noTone(speaker);
        Millistillharvest3 = millis(); 
      }
      else if (flowRate_3 < flowRate_half_max_3)       //optical alarm; if actual flowrate falls below half-max-flowrate, indicate this with "<"
      {
        lcd.setCursor (19,1); 
        lcd.print("<");
        digitalWrite(LED3, HIGH);
         
         if (Millistillharvest3 + 5000 < millis())  //checks whether 5000 milli seconds have passed since flowrate was under half-max flowrate
        {
          noTone(speaker);    // turns off speaker 
        }
          else if ((Millistillharvest3 + 5000 >= millis()) && (millis()>60000) ) //if not more than 5 seconds have passed since above condition --> speaker gets activated
        {
          tone(speaker, 1000);   //sends 1000 kHz sound signal from pin speaker
        }
        else
        {
          noTone(speaker);
        }
      }
      else
      {  
        lcd.setCursor (19,1); 
        lcd.print(" ");
        digitalWrite(LED3, LOW);  
        noTone(speaker);
      }
      
    lcd.setCursor (15,2);
    lcd.print ("    ");
    if (flowRate_half_max_3 < 10000) lcd.setCursor (15,2);
    if (flowRate_half_max_3 < 1000) lcd.setCursor (16,2);
    if (flowRate_half_max_3 < 100) lcd.setCursor (17,2);
    if (flowRate_half_max_3 < 10) lcd.setCursor (18,2);
    lcd.print (flowRate_half_max_3);
    //lcd.setCursor (15,3);
    //lcd.print ("    ");
    //if (totalMilliLitres_3 < 10000) lcd.setCursor (15,3);
    //if (totalMilliLitres_3 < 1000) lcd.setCursor (16,3);
    //if (totalMilliLitres_3 < 100) lcd.setCursor (17,3);
    //if (totalMilliLitres_3 < 10) lcd.setCursor (18,3);
    //lcd.print (totalMilliLitres_3);
    lcd.setCursor (14,3);
    lcd.print ("     ");
    if (totalLitres_3 < 100.0) lcd.setCursor (13,3);
    if (totalLitres_3 < 10.0) lcd.setCursor (14,3);
    lcd.print (totalLitres_3, 3); //print with three decimal points 

    //end-of-loop
    digitalWrite(statusLed, LOW);
    delay(20);
    digitalWrite(statusLed, HIGH);  
  }
}
