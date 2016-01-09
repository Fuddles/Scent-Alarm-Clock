
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

Adafruit_7segment ledScreen = Adafruit_7segment();


void setup() {
  // put your setup code here, to run once:

  ledScreen.setBrightness(15);     // Fully bright

  ledScreen.print(1234);
  ledScreen.drawColon(true);
  ledScreen.writeDisplay();

}

void loop() {
  // put your main code here, to run repeatedly:

}
