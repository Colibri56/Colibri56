#include <Arduino.h>
#include <Wire.h>
// #include <DS3232RTC.h>
// DS3232RTC myRTC;    // NANO configuration pins - A4-SDA, A5-SCL

// #include "FS.h"
#include "SD.h"
#include "SPI.h"
// Define CS pin for the SD card module
#define SD_CS 5   // chip select - GPIO 5
File myFile;

#include <NTPClient.h>    // NTP server
#include <WiFiUdp.h>
#include <WiFi.h>

const char *ssid = "eldar";
const char *password = "Banjuk56!";

// Define NTP settings
const char *ntpServerName = "pool.ntp.org";
const int timeZone = 2; // Change this based on your location

// Create an instance of the WiFiUDP class
WiFiUDP ntpUDP;

// Create an instance of the NTPClient class
NTPClient timeClient(ntpUDP, ntpServerName, timeZone);

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
// NANO A4 => SDA (green)
//      A5 => SCL (yellow)
// ESP32 21 => SDA (green)
//       22 => SCL (yellow)

//Input & Button Logic
#define RELAY_NO true     // define relays are NORMALLY OPEN
#define RELAY_NUM  3      // the number of active relays - water lines
int relayGPIOs[RELAY_NUM] = {14, 26, 32}; // 26-Green, 32-White, 14-Orange 
#define INTERRUPT_PIN 2        // Interrup button
const int buttons = 3;                    // number of buttons
//const int inputPins[buttons] = {3,9,10};   // Arduino NANO config  3- Master/Select button, 9- scroll back, 10- scroll Next 
const int inputPins[buttons] = {2,4,5};   // ESP32 config  2- Master/Select button, 9- scroll back, 10- scroll Next
bool lastButtonState[buttons] = {LOW,LOW,LOW}; // contains the last buuton state

const int arraysizeMainMenue = 5; // values of number of MENUE elements in the relevant array 
const int arraysizeWaterLine = 5;   
const int arraysizeWaterPlans = 10;   
const int arraysizePlanParameters = 6; 
const int arrayDayOfTheWeek = 7;  
const int arraysizeMinutesInteravl = 9;  
const int arraysizeStopResume = 3;  
const int arraysizeManualMode = 4;
const int arrayManualindex = arraysizeWaterPlans-1;
const int maxWaterValves = 3;   // number of actual relays/water valves     
const int arraysizeSetupMode = 4;

// bool valveStatus [maxWaterValves] = {LOW,LOW,LOW};    // when valve HIGH is turned ON    ***** del ****
// Array holding the watering schedule
// a  Water Water Line number 
// b  Line activity status   1 = Activ, 0 = Inactive 
// c  start Hour
// d  start Minutes
// e  Watering duration - calculated in minutes
// f Calculated start time in minutes (c * 60 + d)
// irrigation plan array 
// array of actual irrigation plans. Followed by all menues values
long irrigationPlan [arraysizeWaterPlans][arraysizePlanParameters]= {{1,1,9,10,5,0}, {2,1,12,15,55,0}, {3,1,12,15,56,0}, {2,1,19,05,59,0},{2,1,16,42,15,0},{3,1,14,26,20,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{3,1,6,55,41,0},{0,0,0,0,0,0}}; 
long irrigationDaysOfTheWeekPlan [arraysizeWaterPlans][arrayDayOfTheWeek] = {{0,0,0,0,0,0,0},{0,0,0,0,0,1,0},{0,0,0,0,0,0,1},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0}}; //Monday is the first entry
String menueMainTitel [arraysizeMainMenue] = {{"Main            "},{"Standby         "},{"Manual          "},{"Stop/Resume     "},{"SetUp           "}};         // main menue
String menueMain [arraysizeMainMenue] = {{"0-Main        "},{"1-Standby       "},{"2-Manual        "},{"3-Stop/Resume   "},{"4-SetUp         "}};   // main menue
String menuePlanParameter [arraysizePlanParameters] = {{"0 - ESC plan    "},{"Water Line: "},{"Active/Not:     "},{"Hours:          "},{"Minutes:        "},{"Duration:       "}};   // water plan parameters
String menueWaterLine [arraysizeWaterLine] = {{"0 - ESC"},{"1"},{"2"},{"3"},{"4"}};                                                  // water line names
String menueMinutesInteravls [arraysizeMinutesInteravl] = {{"0 - ESC"},{"5 min"},{"10 min"},{"15 min"},{"20 min"},{"30 min"},{"40 min"},{"45 min"},{"50 min"}}; //  nutes interval
int menueMinutesInteravlValues [arraysizeMinutesInteravl] = {0,5,10,15,20,30,40,45,50};               // minutes interval  
String menueStopResume [arraysizeStopResume] = {{"0 - ESC         "},{"1 - STOP        "},{"2 - Resume      "}};                                                                  // STOP all watering
String menueManualMode[arraysizeManualMode] = {{"0-Esc           "},{"1 START manual"}, {"2 STOP Manual "}, {"3 DISPLAY Manual"}};
String menueSetupMode[arraysizeSetupMode] = {{"0-Esc           "},{"1-Add           "}, {"2-Update        "}, {"3-Erase         "}};
String dayOfTheWeekList[7] = {{"Sun"},{"Mon"},{"Tue"},{"Wed"},{"Thr"},{"Fri"},{"Sat"}};

bool valvePlanStatus[arraysizeWaterPlans] = {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,}; // Quick indication of cuurent watering plans
int openValvesCounter = 0;    // display on LCD the number of active plans
int doNotWater = LOW;         // HIGH signals to halt watering - shut off all water valves 
int doNotWaterFirstTimeSwitch;
String firstLine =  "Colibri         ";       // LCD line 1 16 char
String secondLine = "Irrigation v.10 ";       // LCD line 2 16 char
String ipus = "                ";             // a blank phrase to clear lcd variable
int remainedTime;
int tempWaterPlanLine;                        // all following temp fields are used as on going containers till the final saving
int tempWaterPlanHours;
int tempWaterPlanMinutes;
int tempWaterPlanDuration;
int tempWaterPlanCalculation;
int tempUpdatedParamertersIndex;
bool abortFlag;
                // timing parameters
int pointer;                              // menue pointer
int pointerMax; 
int pointerMin;
bool standbyFlag;    // indicates that LCD goes into "dark and scilence" mode

unsigned long lastDebounceTime[buttons] = {0,0,0};
unsigned long  debounceDelay = 180;
int stepMode = 1;   // {"0 - Main"},{"1 - Standby"},{"2 - Manual"},{"3 - Stop/Resume"},{"4 - SetUp"}

// user Standby period
long backlightDuration = 20000;       // 60 seconds full backlight
long noBacklightDuration = 15000;     // 60 seconds display without backlight
volatile int interruptFlag = LOW;     // indication for required action due to active interrupt
//bool firstTimeModuleStep = LOW;       // HIGH - ignore interrupt first time gets into a new module. LOW - normal
unsigned long lastInterrupt = 0;      // the time of last interrupt
long currentTime;
long lastStandbyTime = millis();
int  dakot;
int  shaot;
int  hodesh;
int  yom;
int  dayOfWeek; //Monday is the first entry
int  i;

void setup() {                                       // *************** S E T U P ******************
   Serial.begin(115200);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      Serial.print(".");
   }
  Serial.println(" ");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize RTC with the current epoch time
  configTime(timeZone * 3600, 0, ntpServerName);
  time_t now;
  time(&now);
  struct timeval tv = {now, 0};
  settimeofday(&tv, nullptr);

  // Update RTC using NTP time
  timeClient.begin();
  timeClient.update();

  time_t rawtime = timeClient.getEpochTime();
  tv = {rawtime, 0};
  settimeofday(&tv, nullptr);

  // Print current time obtained from NTP
//  Serial.println(" ");
  Serial.println("Connected to Wi-Fi");
//  Serial.println(" ");
  Serial.print("Current time: ");
  getTimeFromRTC();
  Serial.print(shaot);
  Serial.print(":");
  Serial.println(dakot);
  
  for(int i = 0; i < buttons; i++) {
    pinMode(inputPins[i], INPUT);
  }
  pinMode(INTERRUPT_PIN, INPUT);  
//  myRTC.begin();
//  setSyncProvider(myRTC.get);   // the function to get the time from the RTC
   
  lcd.init();
  lcd.backlight ();

   // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/netunim.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/netunim.txt", "Creating watering parameters file \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
  
//  attachInterrupt(digitalPinToInterrupt(3),interruptDecision,RISING);    // Arduino NANO
  attachInterrupt(INTERRUPT_PIN,interruptDecision,RISING);    // ESP32
 
  for (int i=0;i<arraysizeWaterPlans;i++){            // Calculate duration in minutes
   irrigationPlan [i][5] =  irrigationPlan [i][2] * 60 + irrigationPlan [i][3]; //calculate start time in minutes
  }
  for (int i=0; i< maxWaterValves; i++){             // initiate water valves
     digitalWrite(relayGPIOs[i],HIGH);               // HIGH actually turns the Relay OFF
     delay(500);
     pinMode(relayGPIOs[i], OUTPUT);  
  }
 
  for(i=0; i < arraysizePlanParameters; i++){        // ZERO Manual array 
    irrigationPlan [arrayManualindex][i] = 0;
  }   

   
//  logo(); 

} // ****END SETUP

void loop() {                                        // *************** L O O P ******************

  switch (stepMode) {         
    case 0: {                // Main Menu  {"0 - Main"},{"1 - Standby"},{"2 - Manual"},{"3 - Stop/Resume"},{"4 - SetUp"}
      mainMenue();
    break;
    }  
    case 1: {                // Standby Mode - display currnet status
      defaultDisplay();
    break;
    } 
    case 2: {                // manual Mode - instant starts one time irrigation need 
      manualMode();
      break;
  }
      case 3: {              // Stop Resume Mode - Halt all irrigation plans
      stopResumeMode();
      break;
  }
   case 4: {                 // water plan setup
//      interruptFlag = LOW; 
//      waterParameterSelection();
    break;      
    }
 }    
} // END of loop

void defaultDisplay() {                              // *********   D E F A U L T   D I S P L A Y   ****************** stepMode = 1 >> standby mode
int gpioNumber;
int i;
int switcher;
ipusim(0);
lcd.display(); 
interruptFlag = LOW;
standbyFlag = LOW; 

while(stepMode == 1){ 
    getTimeFromRTC();
    displayLoop();
    satndbyMode();
    checkInterrupt(); 
    if(interruptFlag == HIGH){
      interruptFlag = LOW;
      if (standbyFlag == HIGH){              
          standbyFlag = LOW;
          stepMode = 1;
      }
      else
      {              
          stepMode = 0;
          return;
      }   
    } 
    if(irrigationPlan [arrayManualindex][1] == HIGH){     // shut down manual operation when finish
      if(currentTime > (irrigationPlan [i][5] + irrigationPlan [i][4])){ 
         irrigationPlan [arrayManualindex][1] = LOW; 
      }     
    }   
    for ( i = 0; i < arraysizeWaterPlans; i++){     // scan water plans to display the ones currently ON
      gpioNumber = irrigationPlan [i][0] - 1;       // prepare the exact number from the GPIO array

      if (irrigationPlan [i][1] == HIGH){           // water plane Action indicator is active      
        if((irrigationDaysOfTheWeekPlan [i][dayOfWeek] == HIGH && currentTime >= irrigationPlan [i][5])&& (currentTime <= irrigationPlan [i][5] + irrigationPlan [i][4])){  // Active plans
          ipusim(2);
          remainedTime = (irrigationPlan [i][5] + irrigationPlan [i][4] - currentTime);    // remained time to shut off the water valve
          lcd.setCursor(0,1);
          lcd.print(String(irrigationPlan[i][0]) + " " + String(irrigationPlan[i][2]) + ":" +  String(irrigationPlan[i][3]) + String(" ") +  String(irrigationPlan[i][4]));
          switcher = !switcher;
          lcd.setCursor(10,1);             
          if (switcher == LOW) {
            lcd.print("> ");
          }
          else{
            lcd.print(" >");
          }        
          lcd.setCursor(13,1);
          lcd.print(remainedTime);                    
          if(valvePlanStatus [i] == LOW){                  // first time that opens the valve per water line
             digitalWrite(relayGPIOs[gpioNumber]  ,LOW);   // When relay is LOW it means it is working and open the appropriate valve (NC mode)          
             valvePlanStatus [i] = HIGH;
             openValvesCounter ++ ;                        // number of working lines at the moment
          }
          lcd.setCursor(15,0);
          lcd.print(openValvesCounter);
          delay(1500);
           if((currentTime >= (irrigationPlan [arrayManualindex][5])) && (currentTime <= (irrigationPlan [arrayManualindex][5] + irrigationPlan [arrayManualindex][4]))){
             lcd.setCursor(1,1);
             lcd.print("m"); 
          } 
          else
          {
             lcd.setCursor(6,0);
             lcd.print(" ");         
          }
      }
      else{
           if(valvePlanStatus  [i] == HIGH){
              digitalWrite(relayGPIOs[gpioNumber],HIGH); 
              valvePlanStatus [i] = LOW;
              openValvesCounter -- ;                              // number of working lines at the moment
              lcd.setCursor(15,0);
              lcd.print(openValvesCounter);
              ipusim(2);             
           }        
      }    // END of Non active plans                    
   }                     // END of Water Active plans    
  }                      // END of Water plans loop         
 }                       // END of while loop 

} // END of operationMode

void manualMode()  {                                 // *************** M A N U A L  mode  ******************    stepMode = 2  

pointerMax = arraysizeManualMode - 1;      
lastButtonState[0] = LOW;
ipusim(1);
ipusim(2);  
firstLine = String("Manual watering "); 
pointer = 0; 
while (lastButtonState[0] == LOW){          //  {{"Esc"},{"setup Water Plan"}, {"S T O P"}}                    
        displayMenue(firstLine,menueManualMode[pointer]);    
        scrollMenue();      
}
 if(pointer == 0){
   stepMode = 1;  
   return;
 }
 if(pointer == 1){                                         // IMEDIATE S T A R T
   selectWaterPlanLine(arrayManualindex); 
   selectWaterPlanStartHH(arrayManualindex);
   if (tempWaterPlanHours == 0){
    tempWaterPlanHours = shaot;
   }
   selectWaterPlanStartMM(arrayManualindex);
   if (tempWaterPlanMinutes == 0){
    tempWaterPlanMinutes = dakot;
   }
   tempWaterPlanCalculation = tempWaterPlanMinutes + tempWaterPlanHours * 60;  // the total start time in minutes 
   selectWaterPlanDuration(arrayManualindex);
   tempUpdatedParamertersIndex = 0;  // Zero counter 
   if (tempWaterPlanLine > 0){
      tempUpdatedParamertersIndex++;
   }   
   if (tempWaterPlanMinutes > 0){
      tempUpdatedParamertersIndex++;
   }   
   if (tempWaterPlanDuration > 0){
      tempUpdatedParamertersIndex++; 
   }    
   if (tempUpdatedParamertersIndex > 0){
       irrigationPlan [arrayManualindex][0] = tempWaterPlanLine;
       irrigationPlan [arrayManualindex][1] = HIGH;
       irrigationPlan [arrayManualindex][2] = tempWaterPlanHours;
       irrigationPlan [arrayManualindex][3] = tempWaterPlanMinutes;
       irrigationPlan [arrayManualindex][4] = tempWaterPlanDuration;
       irrigationPlan [arrayManualindex][5] = tempWaterPlanCalculation;
//       valvePlanStatus  [arrayManualindex] = LOW; // indication of active valve
       irrigationDaysOfTheWeekPlan [arrayManualindex][dayOfWeek] = HIGH;
   }
   Serial.print("Line: ");
   Serial.println( irrigationPlan [arrayManualindex][0]);
   Serial.print("Status: ");
   Serial.println( irrigationPlan [arrayManualindex][1]);
   Serial.print("HH: ");
   Serial.println( irrigationPlan [arrayManualindex][2]);
   Serial.print("MM: ");
   Serial.println( irrigationPlan [arrayManualindex][3]);
   Serial.print("Duration: ");
   Serial.println( irrigationPlan [arrayManualindex][4]);
   Serial.print("Calculated: ");
   Serial.println( irrigationPlan [arrayManualindex][5]);
   Serial.print("Pointer: ");
   Serial.println(pointer);
   stepMode = 1;
   lastStandbyTime = millis();
   return;
 }   
if(pointer == 2){                                                  // S T O P and CLEAR - ABORT
  int i;
  for(i=0; i < arraysizePlanParameters; i++){            // ZERO array due to cancelation
    irrigationPlan [arrayManualindex][i] = 0;
  }  
    digitalWrite(relayGPIOs[tempWaterPlanLine-1]  ,HIGH);  // turn OFF relays
    openValvesCounter -- ; 
    tempWaterPlanLine = 0;                    
    tempWaterPlanHours = 0;
    tempWaterPlanMinutes = 0;
    tempWaterPlanDuration = 0;
    tempWaterPlanCalculation = 0;
    tempUpdatedParamertersIndex = 0;
    irrigationDaysOfTheWeekPlan [arrayManualindex][dayOfWeek] = LOW;
    stepMode = 1;
    lastStandbyTime = millis();
    return;
}
 if(pointer == 3){                                                // DISPLAY manual watering values
    lcd.setCursor(0,0);
    lcd.print("Line: "); 
    lcd.print(tempWaterPlanLine);
    lcd.print(" ");
    lcd.print(irrigationPlan [arrayManualindex][1]); 
    lcd.print(" "); 
    lcd.print(tempWaterPlanHours);
    lcd.print(":"); 
    lcd.print(tempWaterPlanMinutes);
    lcd.setCursor(0,1);
    lcd.print("For: "); 
    lcd.print(tempWaterPlanDuration);
    lcd.print(" minutes ");
    stepMode = 1;
    lastStandbyTime = millis(); 
    lastButtonState[0] = LOW;
    while (lastButtonState[0] == LOW){  // waiting fo a complete view
      scrollMenue();
    }
 }      
}   // END of operationStandbyMode

void selectWaterPlanLine(int j) {                    // ********* W A T E R  L I N E   U P D A T E  ****************** 
    lastButtonState[0] = LOW;
    pointerMax = maxWaterValves;
    pointerMin = 1;
    ipusim(1); 
    ipusim(2); 
    pointer = 1;
    firstLine = String("Water line: ") + String(" : ") + String(menueWaterLine[pointer]);             
    while (lastButtonState[0] == LOW){                     
        scrollMenue();
        secondLine = String("New: ") + String(menueWaterLine[pointer]);
        displayMenue(firstLine,secondLine); 
    }  
    tempWaterPlanLine = pointer;
  
} // END waterlineUpdate

void selectWaterPlanStartHH(int j) {                 // ********* W A T E R  P L A N  S T A R T  H O U R    ****************** 
  
    pointerMin = 0;         // limit for 24 hours
    pointerMax = 23;        // limit for 24 hours
    lastButtonState[0] = LOW;
    ipusim(1);
    ipusim(2);    
    pointer = 0;
    firstLine = String("Current HH: ") + String(pointer); 
    while (lastButtonState[0] == LOW){                      
        secondLine = String("new HH: ") + String(pointer);
        displayMenue(firstLine,secondLine); 
        scrollMenue();
   }
    tempWaterPlanHours = pointer;  //update the selected start hour into a temporary field     
  
} // END selectWaterPlanStartHH 

void selectWaterPlanStartMM(int j) {                 // ********* W A T E R  P L A N  M I N U T E S   U P D A T E  ****************** 
    pointerMin = 0;        
    pointerMax = 59;       
    lastButtonState[0] = LOW;
    ipusim(1);
    ipusim(2);
    pointer = 0;
    firstLine = String("Minutes: ") + String(pointer); 
    pointer = 0;
    while (lastButtonState[0] == LOW){                     
        secondLine = String("new MM: ") + String(menueMinutesInteravlValues[pointer]);
        displayMenue(firstLine,secondLine); 
        scrollMenue();
   }
    tempWaterPlanMinutes = menueMinutesInteravlValues[pointer];

} // END selectWaterPlanStartMM 

void selectWaterPlanDuration(int j) {                // ********* W A T E R  P L A N  D U R A T I O N   U P D A T E  ****************** 

    pointerMin = 0;         // first array entry
    pointerMax = arraysizeMinutesInteravl;        // Duration intervals
    lastButtonState[0] = LOW;
    ipusim(1);
    ipusim(2); 
    firstLine = "Select duration";             // Stop/Resume menue
    pointer = 1;
    while (lastButtonState[0] == LOW){                     //   water start time intervals in 10 minutes 
        
        secondLine = String("Duration: ") + String(menueMinutesInteravlValues[pointer]);
        displayMenue(firstLine,secondLine); 
        scrollMenue();
   }
    tempWaterPlanDuration = menueMinutesInteravlValues[pointer];
  
} // END selectWaterPlanDuration 
  
void stopResumeMode(){                               // ******** S T O P and R E S U M E  mode *******           stepMode = 3 
//  pointerMax = arraysizeStopResume - 1;   
int i;  
int gpioNumber; 
  lastButtonState[0] = LOW;
  ipusim(1);
  ipusim(2);  
  firstLine = menueMainTitel [3];             // Stop/Resume menue header
  pointer = 0; 
  while (lastButtonState[0] == LOW){         //  {{"0 - ESC"},{"1 - STOP"},{"2 - Resume"}};          //  display local menue          
    displayMenue(firstLine,menueStopResume[pointer]);
    scrollMenue(); 
  }  
    interruptFlag = LOW;  
    doNotWaterFirstTimeSwitch = HIGH;    
    switch (pointer) {
    case 0: {                // ESC
      stepMode = 0; 
      break;
    }  // END case 0
    case 1: {                // Stop watering    
      while (interruptFlag == LOW){
        displayLoop();
        satndbyMode();
        checkInterrupt(); 
        if(interruptFlag == HIGH){
             interruptFlag = LOW;
             if (standbyFlag == HIGH){ 
                standbyFlag = LOW;
                stepMode = 3;
             } 
             else {  
              stepMode = 1;
              return;
             }
         }    
         if(doNotWaterFirstTimeSwitch == HIGH){    // All water plans are shut off on first iteration
          doNotWaterFirstTimeSwitch == LOW;
          lastStandbyTime = millis();
          lcd.setCursor(0,1);
          lcd.print("System is OFF");
          for ( i = 0; i < arraysizeWaterPlans; i++){      // scan for active valves and turn them off
            gpioNumber = irrigationPlan [i][0] - 1;    
            digitalWrite(relayGPIOs[gpioNumber]  ,HIGH);   // turn OFF relays
          } 
          // clear also array 9 - manual watering       
        }     
        delay(500);    
    }
    break;                 
    } // END case 1
    case 2: {                // Resume watering
      int i;
      int gpioNumber;
      for ( i = 0; i < arraysizeWaterPlans; i++){      // scan water plans to display the ones currently ON
         if((currentTime >= irrigationPlan [i][5])&& (currentTime <= irrigationPlan [i][5] + irrigationPlan [i][4])){  // Active plans
            gpioNumber = irrigationPlan [i][0] - 1;    
            digitalWrite(relayGPIOs[gpioNumber]  ,LOW);   // turn ON relays
         }
      }   
      stepMode = 1;
    break;
    }   // END case 2
  }     // END SWITCH
}   // END of operationStandbyMode

void setupMode() {                                   // ********* S E T U P  MODE ******************     stepMode = 4
/*
int setupMode = 0; 
  pointerMax = arraysizeSetupMode;      
  lastButtonState[0] = LOW;
  ipusim(1);
  ipusim(2); 
  firstLine = menueMainTitel [4];             // Stop/Resume menue header
  pointer = 0; 

  while (lastButtonState[0] == LOW){         //  {"Esc"},{"Add"}, {"Update"}, {"Erase"}                 
        secondLine = menueSetupMode[pointer];     
        displayMenue(firstLine,secondLine);
        scrollMenue();   
  }
    setupMode = pointer;
    switch (setupMode) {          
    case 0: {                                     // ESC 
      stepMode = 0;
    break;
    }  
    case 1: {                                     // Add
      for (int i = 0; i > (arraysizeWaterPlans - 1); i++){
        if(irrigationPlan [i][0] == 0){
          pointer = i;
          waterParameterSelection();
        }
        else {
          if(i == arraysizeWaterPlans){
            firstLine  = "Add a water plan"; 
            secondLine = "Plans list FULL";
            displayMenue(firstLine,secondLine);
            delay(2000); 
             stepMode = 4;
             break;
          }
        }
   
      }
      stepMode = 0;
    break;
    } 
    case 2: {                                     // Updae
      manualMode();
      stepMode = 0;
      break;
  }
      case 3: {                                  // Erase
        lastButtonState[0] = LOW;
        firstLine = menueSetupMode [3];             // Erase menue
        pointer = 0;
        while (lastButtonState[0] == LOW){                     //   {{"Esc"},{"Add"}, {"Update"}, {"Erase"}};    
          scrollMenue();
          displayMenue(firstLine,menueStopResume[pointer]); 
    }
    if(lastButtonState[0] == HIGH){
      lastButtonState[0] = LOW;
    }
      }
      firstLine = "U sure? ",irrigationPlan [pointer][0];
//                                                          menueSetupMode[????]
      stepMode = 0;
      break;
  }*/
 }  // END of setupMode
  
void areUsure() {                                    // *********A R E  U  S U R E ****************** 
    
  lastButtonState[0] = LOW;
  firstLine = "Are You SURE";  
  secondLine = "No     yes";         
  pointer = 0;
  int counter = 0;
  int maxCounter = 2;
  bool upperLowerCase = LOW;  // switch Yes No from normal to capital leters and back   
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print(firstLine);
  lcd.setCursor(2,1);
  lcd.print(secondLine); 

  while (lastButtonState[0] == LOW){
    readButtons();  
    if (lastButtonState [1] == HIGH) {   // back button - NO - regreat abortion
         lastButtonState [1] = LOW;
         abortFlag = LOW;
    }
    if (lastButtonState [2] == HIGH) {   // next button - YES - confirm abortion
         lastButtonState [2] = LOW;
         abortFlag = HIGH;
    }       
    counter = counter + 1;
    if (abortFlag == HIGH) {   // HIGH means abort
           lcd.setCursor(2,1);
           lcd.print("no");      // lower case to set focus on the other option
        if (counter > maxCounter){
          if(upperLowerCase == LOW){
            lcd.setCursor(7,1);
            lcd.print("YES");
            upperLowerCase = HIGH;
            counter = 0;
          }
          else {
            lcd.setCursor(7,1);
            lcd.print("yes"); 
            upperLowerCase = LOW;
            counter = 0;
          }    
        }
     }    
     if (abortFlag == LOW){    // LOW means regreat, retun to the last point
       lcd.setCursor(13,1);
       lcd.print("yes");  // lower case to set focus on the other option
       if (counter > maxCounter){
         if(upperLowerCase == LOW){
           lcd.setCursor(2,1);
           lcd.print("NO");
           upperLowerCase = HIGH;
           counter = 0;
         } 
         else {
          lcd.setCursor(2,1);
          lcd.print("no");
          upperLowerCase = LOW;
          counter = 0; 
         }
      } 
    }
 }
 lastButtonState[0] = LOW;
 interruptFlag = LOW;

} // END areUsure 

void satndbyMode() {                                 // ********* S T A N D B Y MODE ***************  

       if ((millis() - lastStandbyTime) < backlightDuration){
          standbyFlag = LOW;
          lcd.backlight ();
          return;
       }   
       standbyFlag = HIGH; 
       if ((millis() - lastStandbyTime) < backlightDuration + noBacklightDuration){
             lcd.noBacklight();                       // display but no backlight
             return; 
       }       
       if ((millis() - lastStandbyTime) >= backlightDuration + noBacklightDuration){
           lcd.noBacklight();
           lcd.noDisplay();                           // no display - dark 
           return; 
       }         
} // END of satndbyMode

void interruptDecision() {                          // ********* I N T E R R U P T DECISION ******************                  Arduino NANO
// void IRAM_ATTR interruptDecision() {                // ********* I N T E R R U P T DECISION ******************           ESP32
if((millis() - lastInterrupt) > 500){     // Checks if previous interrupt occured at leas 0.5 sec before
   interruptFlag = HIGH;
   lastInterrupt = millis();
}    
}  // END of interruptDecision

void logo() {                                       // *************** L O G O ******************

/*  int i;
  signed int j = -1;
  firstLine = "Colibri";
  secondLine = String ("Irrigation v 1.0");
  int arraySize = 16;
  String letter;
//  String textLine[arraySize]= {"C","o","l","i","b","r","i"," "," "," "," "," "," "," "," "," "};
// String textLine[arraySize]= firstLine;
*/
lcd.clear();
lcd.setCursor(0,0);
lcd.print(firstLine);
lcd.setCursor(0,1);
lcd.print(secondLine);

/*  
  for (i=0;i<7;i= i+1){
      letter = textLine [i];
      j = j + 2;
      lcd.setCursor(j,0);
      lcd.print(letter);
      delay(30);
  } 
textLine[arraySize]= secondLine;
  j = 0;
  for (i=0;i<16;i= i+1){
      letter = textLine [i];
      lcd.setCursor(j,1);
      lcd.print(letter);
      delay(4);
      j = j + 1;
  }  */ 
delay(4000);  
stepMode = 0;
} // END of logo  

void readButtons() {                                // *************** R E A D BUTTONS ******************
  
 for(int i = 0; i < buttons; i++) {
//      Serial.print(" readButtons >> "); Serial.print(lastButtonState[0]);Serial.print(" <<");
      if ((millis() - lastDebounceTime[i])> debounceDelay){    // beyond the debaouncing window
        byte reading = digitalRead(inputPins[i]);
//        Serial.print("reading >> "); Serial.print(reading);Serial.print(" <<");Serial.print("  index >> "); Serial.print(i);Serial.println(" <<");Serial.print(" >> "); Serial.print(millis()-lastDebounceTime[i]);Serial.print(" <<");Serial.print(" >>deb:  "); Serial.print(debounceDelay);Serial.print(" <<");
         if (reading != lastButtonState[i]){      // Cycle first time switched
             lastDebounceTime[i] = millis();
             lastButtonState[i] = reading;
         }
    }
 }
 delay(200);  
}  // END of readButtons

void scrollMenue() {                                // *************** S C R O L L MENUE ******************
      readButtons();
      if(lastButtonState[2] == HIGH){    // UP
        lastButtonState[2] = LOW;
        pointer ++;
        if(pointer > pointerMax){
          pointer = pointerMin;
        }  
      }
      if(lastButtonState[1] == HIGH){    // Back
          lastButtonState[1] = LOW;
          pointer --;
          if(pointer < pointerMin){
            pointer = pointerMax;
          }
      }    
}  // END of scrollMenue

void ipusim(int lineNum){                           // *************** I P U S E I M     ****************** 
     if(lineNum == 0){         //   0 - ipus line 1 + line 2
        lcd.setCursor(0,0);
        lcd.print(ipus);
        lcd.setCursor(0,1);
        lcd.print(ipus);  
     }
     if(lineNum == 1){
        lcd.setCursor(0,0);
        lcd.print(ipus); 
     }
    if(lineNum == 2){
       lcd.setCursor(0,1);
       lcd.print(ipus); 
    }
}  // END ipus

void mainMenue()  {                                 // *************** M A I N   MENUE  ******************       
             
lastButtonState[0] = LOW;    // {"Main  "},{"Standby  "},{"Manual   "},{"Stop/Resume  "},{"SetUp   "}

ipusim(1);
ipusim(2); 
pointerMin = 0;
pointerMax =  arraysizeMainMenue;
firstLine = menueMainTitel [0];             // main menue header
lastStandbyTime = millis();
pointer = 0;
  while (lastButtonState[0] == LOW){ 
     displayMenue(firstLine,menueMain[pointer]); 
     scrollMenue();
     satndbyMode();
//     Serial.println("Pointer: ");
//     Serial.print(pointer);
  }
  stepMode = pointer;
  if ((millis() - lastStandbyTime) > backlightDuration) {
    stepMode = 1;
  }
}  // END of mainMenue

void displayMenue(String firstLine, String secondLine)  { // *************** D I S P L A Y   MENUE  ******************    
lcd.setCursor(0,0);
lcd.print(firstLine);
lcd.setCursor(0,1);
lcd.print(secondLine);
      
}  // END of mainMen

void checkInterrupt()    {                          // *************** C H E C K   I N T E R R U P T  ******************    
      if(interruptFlag == HIGH){
            lastStandbyTime = millis();
            lcd.backlight();
            lcd.display();      
      }  
}  // END of checkInterrupt

void displayLoop() {                                // *********   D I S P L A Y    L O O P *****************
    bool switcher = LOW;
    
     lcd.setCursor(6,0);
     lcd.print(dayOfTheWeekList[dayOfWeek]);
     lcd.setCursor(9,0);
     lcd.print(yom);
     lcd.print("/");
     lcd.print(hodesh);
//    time_t t = myRTC.get();
//    float celsius = myRTC.temperature() / 4.0 -2; // -2C is to calibrate
      currentTime = shaot *60 + dakot;
    if (shaot < 10){
      lcd.setCursor(0,0);
      lcd.print("0");
      lcd.print(shaot);
    }
    else{
      lcd.setCursor(0,0);
      lcd.print(shaot);
    }
      lcd.setCursor(2,0);
      lcd.print(":");
      lcd.setCursor(3,0);
    if (dakot < 10){
      lcd.print("0");
      lcd.print(dakot);
    }
    else{
      lcd.print(dakot);
    }
/*    lcd.setCursor(5,0);
    lcd.print("   ");
    lcd.setCursor(8,0);
    lcd.print(celsius);
    lcd.setCursor(13,0);
    lcd.print("c");
*/    
}  // END of displayLoop  

void getTimeFromRTC() {                           // *********   G E T  T I M E from R T C  *****************
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  dakot = timeinfo.tm_min;
  shaot = timeinfo.tm_hour;
  hodesh = timeinfo.tm_mon + 1;
  yom = timeinfo.tm_mday;
  dayOfWeek = timeinfo.tm_wday;     // Get the day of the week - starting from Monday : 0 to 6
}  // END of getTimeFromRTC    
//                                                    **** ****     F U N C T I O N S   E N D   **** ****
