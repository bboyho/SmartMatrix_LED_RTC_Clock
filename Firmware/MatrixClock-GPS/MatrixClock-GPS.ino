/*
   This example is pulled from the u-blox GNSS library with slight
   modifications to demonstrate how to use the alternate I2C pins
   accessible on the SmartMatrix Shield.

   Requires a u-blox module connected to
   Teensy pins 16 (SCL) and 17 (SDA)

   This SmartMatrix example uses just the indexed color layer
*/

#include <Wire.h>
//#define Wire Wire1 //if using secondary I2C port with Teensy (i.e. Teensy 3.6's SDA1 & SCL1)
//#include <Time.h>  //time libray that was used with the SmartLED Shield with RTC example
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS
#include <SmartLEDShieldV4.h>  // uncomment this line for SmartLED Shield V4 (needs to be before #include <SmartMatrix3.h>)
#include <SmartMatrix3.h>

SFE_UBLOX_GNSS myGNSS;

long lastTime = 0; //Simple local timer. Limits amount if I2C traffic to Ublox module.
long latitude = 0;
long longitude = 0;
long altitude = 0;
byte SIV = 0;

//Mountain Daylight Time == false, i.e. spring forward 1 hour, LED ON
//Daylight Savings Time (DST) == true, i.e. fall back 1 hour, LED OFF
boolean DST = false; //assuming the clock was set to the region without applying Daylight Savings (e.g. 12/31/2021 & UTC - 7)
boolean enableDST = true; //option to disable DST if your country does not observe DST
int zoneOffsetHour = -7; //adjust according to your standard time zone
byte DoW = 0; //needed to adjust hour for DST, or if you want to know the Day of the Week
boolean military = false; //adjust for miltary or AM/PM
boolean AM = false; //AM or PM?

// Use these variables to set the initial time: 3:03:00
int hours = 3;
int minutes = 3;
int seconds = 0;

//Tid Bit: https://www.sparkfun.com/news/2571#yearOrigin
int years = 2020; //year that SparkFun was founded!
int months = 9;  //month that SparkFun was founded!
int days = 1;    //day that SparkFun was founded!

// How fast do you want the clock to update? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;



#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

const int defaultBrightness = 100 * (255 / 100); // full brightness
//const int defaultBrightness = 15*(255/100);    // dim: 15% brightness

//const SM_RGB clockColor = {0xff, 0xff, 0xff}; //white
const SM_RGB clockColor = {0x00, 0xff, 0xff}; //cyan



//DST button and LED
const int buttonDSTPin = 15; //DST button (or jumper)
const int DSTLEDPin = 13;    //orange LED
int buttonDSTState = HIGH; //set DST button HIGH, so not pressing

boolean prev_buttonDSTState = false;
boolean current_buttonDSTState = false;



void setup() {
  Serial.begin(115200);
  //while (!Serial);             // Leonardo: wait for serial monitor

  delay(200);
  Serial.println("Initializing SmartLED Matrix and u-Blox GPS Clock");
  Serial.println("-------------------");

  // setup DST button and LED
  pinMode(buttonDSTPin, INPUT_PULLUP); //use internal pullup resistor to DST button
  pinMode(DSTLEDPin, OUTPUT);//set DST LED
  digitalWrite(DSTLEDPin, HIGH);//turn LED OFF, assuming the clock was set when Daylight Time, spring forward!

  // setup matrix
  matrix.addLayer(&indexedLayer);
  matrix.begin();

  /* I2C Changes Needed for SmartMatrix Shield */
  // switch pins to use 16/17 for I2C instead of 18/19, after calling matrix.begin()//
  //pinMode(18, INPUT);//SDA
  //pinMode(19, INPUT);//SCL
  //CORE_PIN16_CONFIG = (PORT_PCR_MUX(2) | PORT_PCR_PE | PORT_PCR_PS);
  //CORE_PIN17_CONFIG = (PORT_PCR_MUX(2) | PORT_PCR_PE | PORT_PCR_PS);

  Wire.begin();
  //Alternative way to set up alternative I2C pins on Teensy 3.2, this seems to work better for the GNSS module
  Wire.setSDA(17);
  Wire.setSCL(16);

  Wire.setClock(400000);   // Set clock speed to be the fastest for better communication (fast mode)

  if (myGNSS.begin() == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("Ublox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1)
      ;
  }

  myGNSS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGNSS.saveConfiguration();        //Save the current settings to flash and BBR


  // display a simple message - will stay on the screen if calls to the GNSS library fail later
  indexedLayer.fillScreen(0);
  indexedLayer.setFont(gohufont11b);
  indexedLayer.drawString(0, kMatrixHeight / 2 - 6, 1, "CLOCK");
  indexedLayer.swapBuffers(false);

  matrix.setBrightness(defaultBrightness);
}



void loop() {


  checkbutton_DST();//check if jumper connected for DST

  update_Time();   //adjust UTC date/time based on time zone and DST

  displayDigital_Date_Time();  //after calculating, display the date and time

  delay(500);// small delay so it doesn't update matrix too fast
}



void checkbutton_DST() {
  buttonDSTState = digitalRead(buttonDSTPin);//check state of MDT button

  //----------DST  Mode----------
  //if DST button pressed, change DST

  if (buttonDSTState == LOW) {
    current_buttonDSTState = true; //button has been pressed
    if (prev_buttonDSTState != current_buttonDSTState) {

      if (DST == false) {//change to Mountain Saving Time
        digitalWrite(DSTLEDPin, LOW);//turn LED OFF
        DST = true;
      }
      else {//DST == true, change back to Mount Daylight Time
        digitalWrite(DSTLEDPin, HIGH);//turn LED ON
        DST = false;
      }
    }
    prev_buttonDSTState = current_buttonDSTState;//update DST button's state
  }
  else if (buttonDSTState == HIGH) {
    current_buttonDSTState = false;//button has not been pressed
    if (prev_buttonDSTState != current_buttonDSTState) {
      //do nothing here when button is not pressed

    }
    prev_buttonDSTState = current_buttonDSTState;//update DST button's state
  }

}


/*===============================================================*/
// Simple function to increment seconds and then increment minutes
// and hours if necessary.
void update_Time() {

  //Query module only every second. Doing it more often will just cause I2C traffic.
  //The module only responds when a new position is available
  if (millis() - lastTime > 1000) {
    lastTime = millis(); //Update the timer

    latitude = myGNSS.getLatitude();
    longitude = myGNSS.getLongitude();
    altitude = myGNSS.getAltitude();
    SIV = myGNSS.getSIV();

    years = myGNSS.getYear();
    months = myGNSS.getMonth();
    days = myGNSS.getDay();
    hours = myGNSS.getHour();
    minutes = myGNSS.getMinute();
    seconds = myGNSS.getSecond();

    calcZone_DST(); //adjust zone and used to check if it is Daylight Savings Time

  }
  //Serial.print(F("Lat: "));
  //Serial.print(latitude);


  //Serial.print(F(" Long: "));
  //Serial.print(longitude);
  //Serial.print(F(" (degrees * 10^-7)"));


  //Serial.print(F(" Alt: "));
  //Serial.print(altitude);
  //Serial.print(F(" (mm)"));


  //Serial.print(F(" SIV: "));
  //Serial.print(SIV);

  //Serial.println();

}





//Nate's snazzy code!
//Given a year/month/day/current UTC/local offset give me local time
void calcZone_DST() {
  //Since 2007 DST starts on the second Sunday in March and ends the first Sunday of November
  //Let's just assume it's going to be this way for awhile (silly US government!)
  //Example from: http://stackoverflow.com/questions/5590429/calculating-daylight-savings-time-from-only-date

  DoW = day_of_week(); //Get the day of the week. 0 = Sunday, 6 = Saturday
  int previousSunday = days - DoW;

  //DST = false; //Assume we're not in DST
  if (enableDST == true) {
    if (months > 3 && months < 11) DST = true; //DST is happening!

    //In March, we are DST if our previous Sunday was on or after the 8th.
    if (months == 3)
    {
      if (previousSunday >= 8) DST = true;
    }
    //In November we must be before the first Sunday to be DST.
    //That means the previous Sunday must be before the 1st.
    if (months == 11)
    {
      if (previousSunday <= 0) DST = true;
    }
  }




  //adjust time for DST here if it applies to your region
  if (DST == true) {//adjust time Daylight Savings Time
    hours = hours + 1;
  }
  else { //leave time as is for Daylight Time
  }




  //adjust time based on Time Zone
  hours = hours + zoneOffsetHour;

  //adjust for offset zones when hour is negative value
  if (hours < 0) {
    days = days - 1;

    hours = hours + 24;
  }
  else if ( hours > 23)    {
    days = days + 1;

    hours = hours - 24;
  }



  //adjust for AM/PM mode
  if (military == false) {
    if (hours >= 0 && hours <= 11) {// we are in AM
      if (hours == 0) {
        hours = 12;
      }
      AM = true;
    }
    else { // hours >= 12 && hours <= 23, therefore we are in PM!!!
      if (hours > 12  && hours <= 23) {
        hours = hours - 12;
      }
      AM = false;
    }

  }



  /*
    Serial.print("Hour: ");
    Serial.println(hour);
    Serial.print("Day of week: ");
    if(DoW == 0) Serial.println("Sunday");
    if(DoW == 1) Serial.println("Monday");
    if(DoW == 2) Serial.println("Tuesday");
    if(DoW == 3) Serial.println("Wednesday");
    if(DoW == 4) Serial.println("Thursday");
    if(DoW == 5) Serial.println("Friday!");
    if(DoW == 6) Serial.println("Saturday");
  */

}




//Given the current year/month/day
//Returns 0 (Sunday) through 6 (Saturday) for the day of the week
//From: http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
//This function assumes the month from the caller is 1-12
char day_of_week() {
  //Devised by Tomohiko Sakamoto in 1993, it is accurate for any Gregorian date:
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4  };
  years -= months < 3;
  return (years + years / 4 - years / 100 + years / 400 + t[months - 1] + days) % 7;
}



void displayDigital_Date_Time() {

  //tmElements_t tm;
  int x = kMatrixWidth / 2 - 15;
  char timeBuffer[9];

  // clear screen before writing new text
  indexedLayer.fillScreen(0);
  indexedLayer.setIndexedColor(1, clockColor); //set color

  Serial.print("Ok, Time = ");
  print2digits(hours);
  Serial.write(':');
  print2digits(minutes);
  Serial.write(':');
  print2digits(seconds);
  Serial.print(", Date (M/D/Y) = ");
  Serial.print(months);
  Serial.write('/');
  Serial.print(days);
  Serial.write('/');
  Serial.print(years);
  Serial.println();

  /* Draw Clock to SmartMatrix */
  uint8_t hour = hours;
  //uint8_t hour = 24; //use for debugging, uncomment to test boundaries of clock

  if (DST == true && hour != 0) {
    hour = hour - 1;     //adjust for daylight savings time
    //Serial.println(hour);
  }
  else if (DST == true && hour == 0) {
    hour = 23;
    //Serial.println(hour);
  }

  //setup hour before displaying on LED Matrix
  if (hour > 12) {
    hour -= 12;
    AM = false;
  }
  if (hour == 0) {
    hour = 12;
    AM = true;
  }



  //display on LED Matrix
  indexedLayer.setFont(gohufont11b);
  
  //display date
  /*
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, months);
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, "/");
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, days);
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, "/");
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, years);
*/

  
  sprintf(timeBuffer, "%d:%02d", hour, minutes);
  if (hour < 10)
    x += 3;
  // ...drawstring(x, y, 1, timeBuffer)
  //center time and AM/PM if less than 9
  if (hour < 10) {
  indexedLayer.drawString(x + 1, kMatrixHeight / 2 - 6, 1, timeBuffer); //draw clock in the middle of 32x32

    if (AM == true) {
      indexedLayer.drawString(x + 7, kMatrixHeight / 2 + 4, 1, "AM"); //draw AM
    }

    else if (AM == false) {
      indexedLayer.drawString(x + 7, kMatrixHeight / 2 + 4, 1, "PM"); //draw PM
    }


    indexedLayer.swapBuffers();

  }



  //center time and AM/PM if less than 10-12
  else if (hour >= 10) {
  indexedLayer.drawString(x, kMatrixHeight / 2 - 6, 1, timeBuffer); //draw clock in the middle of 32x32

    if (AM == true) {
      indexedLayer.drawString(x + 10, kMatrixHeight / 2 + 4, 1, "AM"); //draw AM
    }

    else if (AM == false) {
      indexedLayer.drawString(x + 10, kMatrixHeight / 2 + 4, 1, "PM"); //draw PM

    }


    indexedLayer.swapBuffers();
  }

  else {

    Wire.beginTransmission(0x42);

    if (Wire.endTransmission() == 1) {
      Serial.println("GNSS read error!  Please check the circuitry.");
      Serial.println();

      //Draw Error Message to SmartMatrix
      indexedLayer.setFont(font3x5);
      indexedLayer.drawString(x, kMatrixHeight / 2 - 3, 1, "No Clock");
    }
    indexedLayer.swapBuffers();
    delay(9000);

  }

}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}
/*===============================================================*/
