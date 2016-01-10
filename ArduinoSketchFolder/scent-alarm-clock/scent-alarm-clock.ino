
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "ds3231.h"


Adafruit_7segment ledScreen = Adafruit_7segment();

boolean middlecolon = true;

// TODO: Doc is wrong about writeDigitNum(location, number, dot)
// 0x02 - center colon
// 0x04 - left colon - lower dot
// 0x08 - left colon - upper dot
// 0x10 - decimal point

void setup() {
  // put your setup code here, to run once:
  ledScreen.begin(0x70);
  ledScreen.setBrightness(15);     // Fully bright

  ledScreen.print(1234, DEC);
  ledScreen.drawColon(true);
  ledScreen.writeDisplay();
 
}

void loop() {
  // put your main code here, to run repeatedly:

  delay(1000);
  middlecolon = !middlecolon;
  // ledScreen.drawColon( middlecolon );

  // --- Which dots on?
////  ledScreen.writeDigitNum(2, 0, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 1, true ); // upper
////  ledScreen.writeDigitNum(2, 2, true ); // lower + decimal
////  ledScreen.writeDigitNum(2, 3, true ); // lower + upper
//ledScreen.writeDigitNum(2, 4, true ); // upper 
////  ledScreen.writeDigitNum(2, 5, true ); // lower + upper
////  ledScreen.writeDigitNum(2, 6, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 7, true ); // upper
ledScreen.writeDigitNum(2, 0x08, true ); // lower + upper + decimal (all)
  
  ledScreen.writeDisplay();

}
