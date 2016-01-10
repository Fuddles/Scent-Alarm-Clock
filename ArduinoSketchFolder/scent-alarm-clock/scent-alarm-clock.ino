/**
 *  Alarm clock project that turns a fan on when the alarm triggers
 */

// Comment the line to remove the console debugging (via Serial)
#define DEBUG         true

// Uncomment the line to set the time
// #define SET_RTC_TIME  true

// Buffer size to print on the console
#define BUFFER_SIZE   256
char    buff[BUFFER_SIZE];

// ----- Inits for the RTC clock
#include <Wire.h>
#include "ds3231.h"
struct ts rtcTime;

// ----- Inits for the screen
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

Adafruit_7segment ledScreen         = Adafruit_7segment();
boolean           middleColonToggle = false;

// TODO: Doc is wrong about writeDigitNum(location, number, dot)
// 0x02 - center colon
// 0x04 - left colon - lower dot
// 0x08 - left colon - upper dot
// 0x10 - decimal point


// ----- Inits for the DC motor
// TODO

// ----- Inits for the buttons
// TODO

// Two buttons pressed for 5secs should turn the alarm on to test




// =============================================================================================
// ------ SETUP & LOOP ------
// =============================================================================================

/**
 *  Once, at start
 */
void setup() 
{
    // Console log
    Serial.begin(9600);

    // RTC 
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();               // FIXME: should not reset the alarm but keep previous
#ifdef SET_RTC_TIME
    struct ts rightNowTime = { .sec = 0, .min = 8, .hour = 19, .mday = 9, .mon = 1, .year = 2016 };
    DS3231_set( rightNowTime );
#endif

    // Screen
    ledScreen.begin(0x70);
    ledScreen.setBrightness(15);     // Fully bright

    // TODO: DC Motor
}



/**
 *  Endless loop. Will try to run every second, steadily at '000'ms
 */
void loop() 
{
    unsigned long loopStartMs = millis();

    // --- Get time and display it
    DS3231_get( &rtcTime );
#ifdef DEBUG
    // display the time on the console
    snprintf(buff, BUFFER_SIZE, "%d.%02d.%02d %02d:%02d:%02d", 
            rtcTime.year, rtcTime.mon, rtcTime.mday, rtcTime.hour, rtcTime.min, rtcTime.sec);
    Serial.println(buff);
#endif
    // display the time on the screen, toggling the middle colon every second
    ledScreen.print( rtcTime.hour*100 + rtcTime.min, DEC);
    middleColonToggle = !middleColonToggle;
    ledScreen.drawColon( middleColonToggle );
    ledScreen.writeDisplay();

    // TODO: dots for AM/PM (or 24h format?)
    // TODO: dot when alarm set

    // Which dots on?
    //  ledScreen.writeDigitNum(2, 1); // upper
    //  ledScreen.writeDigitNum(2, 3); // upper + lower
    //  ledScreen.writeDigitNum(2, 0); // upper + lower + decimal



    // --- Check buttons



    // 


  

    // --- To loop every second. 
    //      If took more than 1 sec, loop immediately
    //      Otherwise try to loop until the next second precisely (000ms)
    unsigned long loopEndMs = millis();
    if ( loopEndMs - loopStartMs < 999 ) {
        int diff = 999 - (loopEndMs % 1000);
        delay( diff );
    }
    return;
}



// =============================================================================================
// ++++++ HELPER FUNCTIONS ++++++
// =============================================================================================

/**
 * Set the alarm clock
 */
void set_alarm(uint8_t hour, uint8_t minute, uint8_t second)
{
    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm - see datasheet
    // A1M1 (seconds) (0 to enable, 1 to disable)
    // A1M2 (minutes) (0 to enable, 1 to disable)
    // A1M3 (hour)    (0 to enable, 1 to disable) 
    // A1M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    uint8_t flags[5] = { 0, 0, 0, 1, 1 };

    // set Alarm1
    DS3231_set_a1( second, minute, hour, 0, flags);

    // activate Alarm1
    DS3231_set_creg(DS3231_INTCN | DS3231_A1IE);
}






/*
 ////  ledScreen.writeDigitNum(2, 0, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 1, true ); // upper
////  ledScreen.writeDigitNum(2, 2, true ); // lower + decimal
////  ledScreen.writeDigitNum(2, 3, true ); // lower + upper
//ledScreen.writeDigitNum(2, 4, true ); // upper 
////  ledScreen.writeDigitNum(2, 5, true ); // lower + upper
////  ledScreen.writeDigitNum(2, 6, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 7, true ); // upper
*/
