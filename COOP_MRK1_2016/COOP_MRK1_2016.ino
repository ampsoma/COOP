/*
 * Sam Dougherty
 * ampsoma@gmail.com
 * Displays sunset sunrise on lcd
 * i2c addresses:
 * lcd 0x20
 * rtc 0x68
*/

//Included Libraries. Don't touch. no semi colon terminator
#include <Wire.h>	//For talking to RTC and LCD, I2C communication
#include <LiquidCrystal.h>	//LCD library
#include <DS1307RTC.h> //RTC (Real Time Clock) library
#include <TimeLord.h>	//For Astronomical clock functions
#include <TimeLib.h>    //time stuff

//super global variables, cant be changed after program compiles
#define DS1307_ADDRESS 0x68 //adress fir de RTC
#define DEBUG 1  //set to 1 to enable debug messages through serial port monitor
#define debounceDelay 50 //debounce delay in mills when checking switches.
//Pinout on Arduino Board, what connects to where
#define BTN_1 2 //button one
#define BTN_2 3 //button two
#define LMTSW_U 4 //upper limit reed switch
#define LMTSW_L 5 //lower limit reed switch
#define RLY_1 6 //relay one trigger
#define RLY_2 7 //relsay two trigger
#define ledPin 13 //effin onboard led pin

//LOCATION (springtailfarm via google earth estimation)
float const LAT = 44.0677528, LONG = -072.4842056; //lat(S=neg),long(W=neg) 
//TIME ZONE
int const TMZN = -5; // tmzn IS EST (w/o DST) is UTC -5
int const ofset_am = 30;//open the door how much later after "sunrise" as calculated. in min, -earlier +later
int const ofset_pm = -30;//close door how much later after "sunset" as calculated. in min, -earlier + later

TimeLord PoorFarm; // Initilizes Timelord instance PoorFarm
byte sunTime[]  = {0, 0, 0, 20, 1, 16}; // 20 jan 16 to start, rtc fixes this 
byte srss[] = {0,0,0,0}; // sunrise,sunset array [srhr,srmn,sshr,ssmn]
int minNow, minLast = -1, hourNow, hourLast = -1, minOfDay; //time parts to trigger various actions.
//-1 init so hour/min last inequality is triggered the first time around 
int mSunrise, mSunset; //sunrise and sunset expressed as minute of day (0-1439)

//Global program variables
int BKLT_State; //blacklight state
int DISP_State; //display state counter
int lastBTN_1State = HIGH; //for button debounce
int lastBTN_2State= HIGH;
int LCDState = HIGH; //lcd backlight controll trigger
int door;

//Button debounce intervals (in miliseconds)
long lastDebounceTime = 0;


//connect to lcd
LiquidCrystal lcd(0);

///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////setup///////////////////////////////
//////////////////////////////////////////////////////////////////////////
void setup() {
	
  	setSyncProvider(RTC.get);   // the function to get the time from the RTC 
  	//timelord start
  	PoorFarm.TimeZone(TMZN * 60); //Tells TimeZone what time zone we are in
  	PoorFarm.Position(LAT, LONG);  //Tells TimeLord where we 
  	PoorFarm.DstRules(3,2,11,1,60); // DST Rules for USA
  	//Set up the LCD
  	lcd.begin(16, 2);//(rows,columns),16x2 display over I2C port expander
	lcd.setCursor(0, 0);
  	lcd.print("LCD_RTC_MRK5b"); //Start up screen 
    lcd.setCursor(0, 1);
    lcd.print("by:Sam Dougherty");
  	delay(4000); //Start up delay
  	lcd.clear(); //Clear LCD before beginning program loop 
  	// check for RTC, display warning if not workin
  	#if DEBUG == 1
 		Serial.begin(9600); //open a serial connection
 		while (!Serial) ; // wait until Arduino Serial Monitor opens
 		Serial.println("Debug activated");
 		if(timeStatus()!= timeSet) {
 	  	  Serial.println("Unable to sync with the RTC");
		}
 		else{
 	  	  Serial.println("RTC has set the system time"); 
		}     
 	#endif 
    // Define on pins on board
  	pinMode(BTN_1, INPUT);
  	pinMode(BTN_2, INPUT);
  	pinMode(LMTSW_U, INPUT);
  	pinMode(LMTSW_L, INPUT);
  	pinMode(RLY_1, INPUT);
  	pinMode(RLY_2, INPUT);  
  	pinMode(ledPin, OUTPUT);
  	digitalWrite(ledPin, LOW);
	
	}
///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////loop/////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void loop() {
  lcdBkLt();
  lcdDisplay();
  if (timeStatus()!= timeNotSet) {
    minNow = minute();
    if (minNow != minLast) {
        minLast = minNow;
        hourNow = hour();
        minOfDay = hourNow * 60 + minNow; //minute of day will be in the range 0-1439
        #if DEBUG == 1
          Serial.print(" hourNow");
          Serial.print(hourNow);
          Serial.print(" minNow");
          Serial.print(minNow);
          Serial.print("  minOfDay:");
          Serial.print(minOfDay);
          Serial.print("  minLast:");
          Serial.print(minLast);
          Serial.print("  hourLast:");
          Serial.print(hourLast);
          Serial.println();
        #endif
		  doorControll(); //door controller function, uses values calculated in main loop.
        if (hourNow != hourLast) { // Noting that the Sunrise/Sunset is only calculated every hour => less power. 
      /* Sunrise: */
      sunTime[3] = day(); // Uses the Time library to give Timelord the current date
      sunTime[4] = month();
      sunTime[5] = year();
      PoorFarm.SunRise(sunTime); // Computes Sun Rise. Prints:
	  srss[1] = sunTime[tl_hour];
	  srss[2] = sunTime[tl_minute];
      mSunrise = sunTime[2] * 60 + sunTime[1];
       #if DEBUG == 1
              Serial.print(" Sunrise:");
              Serial.print((int) sunTime[tl_hour]);
              Serial.print(":");
              Serial.print((int) sunTime[tl_minute]);
       #endif
      /* Sunset: */
      sunTime[3] = day(); // Uses the Time library to give Timelord the current date
      sunTime[4] = month();
      sunTime[5] = year();
      PoorFarm.SunSet(sunTime); // Computes Sun Set. Prints:
    srss[3] = sunTime[tl_hour];
    srss[4] = sunTime[tl_minute];
      mSunset = sunTime[2] * 60 + sunTime[1];
            #if DEBUG == 1
              Serial.print(" Sunset:");
              Serial.print((int) sunTime[tl_hour]);
              Serial.print(":");
              Serial.println((int) sunTime[tl_minute]);
            #endif
            
            hourLast = hourNow;
       }
        #if DEBUG == 1
          Serial.print("  mSunrise:");
          Serial.print(mSunrise);
          Serial.print("  mSunset:");
          Serial.print(mSunset);
          Serial.print("  LED:");
          Serial.print(minOfDay < mSunrise || minOfDay >= mSunset);
          Serial.println();
        #endif
    }
  }			 
}// loop

////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////other functions///////////////////////
//////////////////////////////////////////////////////////////////////////
void lcdBkLt() {
  
	int BKLT_State = sw_Debounce(BTN_1,lastBTN_1State))
      if (BTNState == HIGH) {
        LCDState = !LCDState;
	}
  lcd.setBacklight(LCDState);
}

void lcdDisplay(){
	t = String("T");
	r = String("Rise");
	s = String(" Set");
	int pState = HIGH; // previous display state
	if LCDState==HIGH { //display backlight on
		if dispscrn==1{ //doorstate w/ time, sr, st
			//door state 
			lcd.setCursor(0, 0);
			if doorstate=1{ //closed
				lcd.print("|CLOSED|");
			}
			if doorstate=2{ //closing
				lcd.print("|--VV--|");
			}
			if doorstate=3{ //opening
				lcd.print("|--^^--|");
			}
			if doorstate=4{ //open
				lcd.print("|OPEN|");
			}
			//Time
			lcd.setCursor(10,0);
		    if(hour() < 10){
		    	t += "0";
			}
			t += hour();	
		    if(minute() < 10){
		    	t += "0";
			}
		    t += minute();
			lcd.print(t);
			//sunrise sunset disp
			lcd.setCursor(0,2);
			if srss[1] <10){
				r += "0";}
			r += srss[1];
			if srss[2] <10){
				r += "0";}
			r += srss[2];
			if srss[3] <10){
				s += "0";}
			s += srss[3];
			if srss[4] <10){
				s += "0";}
			s += srss[4];
			lcd.print(r + s );
		}
		if dispscrn==2{ //moon % full phase and waxing and waining
		
		}
		
		if dispscrn==3{ //day of week (sat-sun) and date iso
		
		}
		if dispscrn==4{//uptime
		
		}
		if pState != HIGH{
			pstate = HIGH;
		}
	}
	
	if LCDState == LOW{ //display backlight off
		if pState = HIGH{
			lcd.clear();
			pstate = LOW;
		}
	}	
}
	
	//time and date
	//alternate sun rise sun set time and ofsetts
	//door open/door closed/door moving
	//door overide active
	
	
	
    lcd.setCursor(0, 0);
    lcd.print(year());
    //lcd.print(".");
    if(month() < 10){
    lcd.print("0");}
    lcd.print(month());
    //lcd.print(".");
    if(day() < 10){
    lcd.print("0");}
    lcd.print(day());
    lcd.print("T");
    if(hour() < 10){
    lcd.print("0");}
    lcd.print(hour());
    lcd.print(":");
    if(minute() < 10){
    lcd.print("0");}
    lcd.print(minute());
    
	 
    lcd.print(srss[4]);
	lcd.print(" ");
	lcd.print(door);
}

void doorControll(){
	doorOpen = (minOfDay < (mSunrise + ofset_am) or minOfDay >= (mSunset + ofset_pm));	
	
	if overide == 0 {
		//open door if 0 close door if 1 
		if doorOpen = 0 {
			//check if door is open
		}
		if doorOpen = 1 {
			
		}
	}
}

int sw_Debounce (int inpt, int btnState){
	
    int reading = digitalRead(inpt);
	
    if (reading != btnState) {
      lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != btnState) {
        btnState = reading;
      }
  	}
	return (btnState)
	
}


















