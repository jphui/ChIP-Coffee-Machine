#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27,16,2);

#define ONE_WIRE_BUS 5
#define FLOWSENSORPIN 2
#define UVpin 7
#define RELAY 6
#define ENTER 4
#define UP 7
#define DOWN 8

boolean doneBoiling;
boolean potDetected;
boolean processComplete;
boolean isDripping;
boolean checkingTemp;
boolean readyToStart;
int enterButtonState = 0;
int cyclesCompleted = 1;

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

//                                      FLOW RATE SENSOR
// count how many pulses!
volatile uint16_t pulses = 0;
// track the state of the pulse pin
volatile uint8_t lastflowpinstate;
// you can try to keep time of how long it is between pulses
volatile uint32_t lastflowratetimer = 0;
// and use that to calculate a flow rate
volatile float flowrate;
// Interrupt is called once a millisecond, looks for any pulses from the sensor!
SIGNAL(TIMER0_COMPA_vect) {
  uint8_t x = digitalRead(FLOWSENSORPIN);
  
  if (x == lastflowpinstate) {
    lastflowratetimer++;
    return; // nothing changed!
  }
  
  if (x == HIGH) {
    //low to high transition!
    pulses++;
  }
  lastflowpinstate = x;
  flowrate = 1000.0;
  flowrate /= lastflowratetimer;  // in hertz
  lastflowratetimer = 0;
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
  }
}

void setup() //                   setup
{

   // SENSOR SETUP
   Serial.begin(9600);
   sensors.begin();
   pinMode(FLOWSENSORPIN, INPUT);
   pinMode(UVpin, INPUT);
   pinMode(RELAY, OUTPUT);
   pinMode(ENTER, INPUT);
   digitalWrite(FLOWSENSORPIN, HIGH);
   lastflowpinstate = digitalRead(FLOWSENSORPIN);
   useInterrupt(true);
   lcd.init();
   lcd.backlight();
   lcd.clear();
   temp();

   // VARIABLE SETUP
   doneBoiling = false;
   potDetected = false;
   processComplete = false;
   isDripping = false;
   checkingTemp = false;
   readyToStart = false;
}

void drip() { //                        "drips" coffee to slow flow
  isDripping = true;
  digitalWrite(RELAY, LOW);
  delay(10000);
  digitalWrite(RELAY, HIGH);
  isDripping = false;
}

void temp() { //                        checks temperature and delays sensor requests
  checkingTemp = true;
  sensors.requestTemperatures();
  delay(500); 
  Serial.print("\nTemperature (C) : ");
  Serial.print(sensors.getTempCByIndex(0));
  if(sensors.getTempCByIndex(0) > 92.0){
    //stop kettle and open valve
    digitalWrite(RELAY, HIGH);
    doneBoiling = true;
  }
  checkingTemp = false;
}
//                                   variables
int approxTime = 0;
int intTemp = 0;
int intFlowRate = 0;
int UVreading = 0;
int UVindex = 0;
boolean finished = false;
//                                   button booleans
int upButtonState = 0;
int downButtonState = 0;
boolean isOnCooldown = false;
boolean buttonWasPressed = false;
boolean enterPressed;
boolean upPressed;
boolean downPressed;
int screen = 1; // starts at main menu
// 1 : Main Menu
// 2 : Hovering Strength
// 3 : Hovering Strong
// 4 : Hovering Weak
// 5 : Hovering Statistics
// 6 : Temperature and Coffees Created
// 7 : Flow Rate and UV Levels
// 8 : End Screen

void buttonCooldown(){ //            ensures button presses aren't read multiple times
  isOnCooldown = true;
  buttonWasPressed = true;
  delay(500);
  isOnCooldown = false;
}

void loop() //                       run over and over again
{ 
  if(!finished){ //only do stuff if coffee is not finished
    //                                 ACTUAL CODE
    UVreading = analogRead(UVpin);
    UVindex = UVreading / 0.1;
    intTemp = sensors.getTempCByIndex(0);
    intFlowRate = flowrate;
    //reset button press variables
    buttonWasPressed = false;
    enterButtonState = 0;
    enterPressed = false;
    upButtonState = 0;
    upPressed = false;
    downButtonState = 0;
    downPressed = false;
    
    //check if enter was pressed
    if(!isOnCooldown){
      enterButtonState = digitalRead(ENTER);
      if(enterButtonState == HIGH){
        buttonCooldown();
        enterPressed = true;
      }
    }
    
    //check if up was pressed
    if(!isOnCooldown){
      upButtonState = digitalRead(UP);
      if(upButtonState == HIGH){
        buttonCooldown();
        upPressed = true;
      }
    }
    
    //check if down was pressed
    if(!isOnCooldown){
      downButtonState = digitalRead(DOWN);
      if(downButtonState == HIGH){
        buttonCooldown();
        downPressed = true;
      }
    }
  
    if(!doneBoiling && screen != 8){
      if(screen == 1){ //on main menu
        if(upPressed || downPressed || enterPressed) screen = 2; //move to hovering strength
      }
      else if(screen == 2){ //hovering strength
        if(upPressed) screen = 1; //move to main menu
        else if(enterPressed) screen = 3; //move to hovering strong
        else if(downPressed) screen = 5; //move to hovering statistics
      }
      else if(screen == 3){ //hovering strong
        if(upPressed) screen = 2; //move to hovering strength
        else if(enterPressed){
          screen = 1; //move to main menu
          //CHANGE ALLOWED FLOW RATE TO LOW
        }
        else if(downPressed) screen = 4; //move to hovering weak
      }
      else if(screen == 4){ //hovering weak
        if(upPressed) screen = 3; //move to hovering strong
        else if(enterPressed){
          screen = 1; // move to main menu
          //CHANGE ALLOWED FLOW RATE TO HIGH
        }
        else if(downPressed) screen = 2; //move to hovering strength
      }
      else if(screen == 5){ //hovering statistics
        if(upPressed) screen = 2; //move to hovering strength
        else if(enterPressed) screen = 6; //move to temp and coffees made
        else if(downPressed) screen = 1; //back to main menu
      }
      else if(screen == 6){ //temp and coffees made
        if(upPressed || enterPressed) screen = 5; //move to hovering statistics
        else if(downPressed) screen = 7; //move to flow rate and uv levels
      }
      else if(screen == 7){
        if(upPressed) screen = 6; //move to temp and coffees made
        else if(enterPressed || downPressed) screen = 5; //move to hovering statistics
      }
    }
    
    if(buttonWasPressed){//only change if a button was pressed
      lcd.clear(); //clear screen
      if(screen == 1){ //main menu
        lcd.print("Coffee's Bruin");
        lcd.setCursor(0,2);
        lcd.print("Time Left: ");
      }
      else if(screen == 2){ //hovering strength
        lcd.print("Strength       <");
        lcd.setCursor(0,2);
        lcd.print("Statistics");
      }
      else if(screen == 3){ //hovering strong
        lcd.print("Strong         <");
        lcd.setCursor(0,2);
        lcd.print("Weak");
      }
      else if(screen == 4){ //hovering weak
        lcd.print("Strong");
        lcd.setCursor(0,2);
        lcd.print("Weak           <");
      }
      else if(screen == 5){ //hovering statistics
        lcd.print("Strength");
        lcd.setCursor(0,2);
        lcd.print("Statistics     <");
      }
      else if(screen == 6){ //temp and coffees completed
        lcd.print("Temperature: ");
        lcd.setCursor(0,2);
        lcd.print("Total Cycles: ");
      }
      else if(screen == 7){ //flow rate and uv levels
        lcd.print("Flow Rate: ");
        lcd.setCursor(0,2);
        lcd.print("UV Levels: ");
      }
    }
    
    else if(screen == 1 && !buttonWasPressed){ //if no button was pressed but time should refresh
      lcd.clear();
      lcd.print("Coffee's Bruin");
      lcd.setCursor(0,2);
      lcd.print("Time Left:" + approxTime);
    }
    
    else if(screen == 8){ //if the coffee is done
      if(!finished){
        lcd.clear();
        lcd.print("Coffee is Ready!");
        lcd.setCursor(0,2);
        lcd.print("Turn Off Kettle");
      }
      //finished = true;
    }
    /*
    // COFFEE MAKING PROCESS
      if(potDetected && !processComplete && readyToStart){
      
      // TEMPERATURE SENSOR
      if(!doneBoiling && !checkingTemp){
        temp();
      }
  
      // DRIP COFFEE EMULATION
      if(doneBoiling && !isDripping){
        drip();
      }
      
      // FLOW RATE SENSOR
      if(doneBoiling){
        if(flowrate < 999){
          //processComplete = true;
        }
      }
      
    }
  
    // COFFEE COMPLETE
    else if(processComplete){
      screen = 8;
      //turn the power to the kettle on and the close the valve
      digitalWrite(RELAY, LOW);
    }
  
  
    
    /*
    digitalWrite(RELAY, HIGH);
    delay(5000);
    digitalWrite(RELAY, LOW);
    delay(5000);
  
    
    Serial.print("\nPulses:");
    Serial.print(pulses, DEC);
    Serial.print("\nHz:");
    Serial.print(flowrate);
    Serial.print("\nFreq: ");
    Serial.println(flowrate);
    
    // if a plastic sensor use the following calculation
    // Sensor Frequency (Hz) = 7.5 * Q (Liters/min)
    // Liters = Q * time elapsed (seconds) / 60 (seconds/minute)
    // Liters = (Frequency (Pulses/second) / 7.5) * time elapsed (seconds) / 60
    // Liters = Pulses / (7.5 * 60)
    
    float liters = pulses;
    liters /= 7.5;
    liters /= 60.0;
    Serial.print(liters);
    Serial.print(" Liters\n"); 
    
  
    int UVreading = analogRead(UVpin);
    int UVindex = UVreading / 0.1;
  
    Serial.print("\nUV Reading: ");
    Serial.print(UVreading);
    Serial.print("\nUV index: ");
    Serial.println(UVindex);
    if(UVreading > 400){
      //startKettle();
    }
    */
    
  }
} // LOOP