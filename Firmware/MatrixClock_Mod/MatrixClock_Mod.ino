/*
   This example is pulled from the DS1307RTC library with slight
   modifications to demonstrate how to use the alternate I2C pins
   accessible on the SmartMatrix Shield.

   Original DS1307RTC Library:
   https://www.pjrc.com/teensy/td_libs_DS1307RTC.html

   Requires a DS1307, DS1337 or DS3231 RTC chip connected to
   Teensy pins 16 (SCL) and 17 (SDA)

   This SmartMatrix example uses just the indexed color layer
*/

#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <SmartLEDShieldV4.h>  // comment out this line for if you're not using SmartLED Shield V4 hardware (this line needs to be before #include <SmartMatrix3.h>)
#include <SmartMatrix3.h>

#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 32;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN; // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels, or use SMARTMATRIX_HUB75_64ROW_MOD32SCAN for common 64x64 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

const int defaultBrightness = (50 * 255) / 100; // full (100%) brightness
//const int defaultBrightness = (15*255)/100;    // dim: 15% brightness

//const SM_RGB clockColor = {0xff, 0xff, 0xff}; //white
const SM_RGB clockColor = {0x00, 0xff, 0xff}; //cyan

//Mountain Daylight Time == false, i.e. spring forward 1 hour
//Daylight Savings Time (DST) == true, i.e. fall back 1 hour
boolean DST = false; //assuming the clock was set when Daylight Time, spring forward!

boolean prev_buttonDSTState = false;
boolean current_buttonDSTState = false;

boolean AM = true; //AM or PM?

//DST button and LED
const int buttonDSTPin = 15; //DST button (or jumper)
const int DSTLEDPin = 13;    //orange LED
int buttonDSTState = HIGH; //set DST button HIGH, so not pressing





void setup() {
  Serial.begin(9600);
  delay(200);
  Serial.println("DS1307RTC Read Test");
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
  pinMode(18, INPUT);
  pinMode(19, INPUT);
  CORE_PIN16_CONFIG = (PORT_PCR_MUX(2) | PORT_PCR_PE | PORT_PCR_PS);
  CORE_PIN17_CONFIG = (PORT_PCR_MUX(2) | PORT_PCR_PE | PORT_PCR_PS);

  // display a simple message - will stay on the screen if calls to the RTC library fail later
  indexedLayer.fillScreen(0);
  indexedLayer.setFont(gohufont11b);
  indexedLayer.drawString(0, kMatrixHeight / 2 - 6, 1, "CLOCK");
  indexedLayer.swapBuffers(false);

  matrix.setBrightness(defaultBrightness);
}





void loop() {
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

  tmElements_t tm;
  int x = kMatrixWidth / 2 - 15;
  char timeBuffer[9];

  // clear screen before writing new text
  indexedLayer.fillScreen(0);
  indexedLayer.setIndexedColor(1, clockColor); //set color

  if (RTC.read(tm)) {
    Serial.print("Ok, Time = ");
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();

    /* Draw Clock to SmartMatrix */
    uint8_t hour = tm.Hour;
    //uint8_t hour = 24; //use for debugging, uncomment to test boundaries of clock

    if (DST == true && hour != 0) {
      hour = hour - 1;     //adjust for daylight savings time
      Serial.println(hour);
    }
    else if (DST == true && hour == 0) {
      hour = 23;
      Serial.println(hour);
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
    sprintf(timeBuffer, "%d:%02d", hour, tm.Minute);
    if (hour < 10)
      x += 3;
    indexedLayer.setFont(gohufont11b);
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
    }


    indexedLayer.swapBuffers();
  }
  else {
    if (RTC.chipPresent()) {
      Serial.println("The DS1307 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();

      /* Draw Error Message to SmartMatrix */
      indexedLayer.setFont(font3x5);
      sprintf(timeBuffer, "Stopped");
      indexedLayer.drawString(x, kMatrixHeight / 2 - 3, 1, "Stopped");
    } else {
      Serial.println("DS1307 read error!  Please check the circuitry.");
      Serial.println();

      /* Draw Error Message to SmartMatrix */
      indexedLayer.setFont(font3x5);
      indexedLayer.drawString(x, kMatrixHeight / 2 - 3, 1, "No Clock");
    }
    indexedLayer.swapBuffers();
    delay(9000);
  }
  delay(1000);
}//end loop





void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}
