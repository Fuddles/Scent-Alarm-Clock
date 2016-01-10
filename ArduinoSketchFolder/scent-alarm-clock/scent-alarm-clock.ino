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

// Loop properties
unsigned int  loopDurationMs  = 200;
unsigned long loopStartMs     = 0L;
unsigned long loopEndMs       = 0L;

// Alarm door status
#define ALARM_DOOR_STATUS_CLOSED    0
#define ALARM_DOOR_STATUS_OPENING   1
#define ALARM_DOOR_STATUS_OPEN      2
#define ALARM_DOOR_STATUS_CLOSING   3
int     alarmDoorStatus     = ALARM_DOOR_STATUS_CLOSED;
int     alarmDoorOpeningPct = 0;          // Between 0 (closed) and 100 (open)

// Testing
#define DELAY_2BUTTONS_TEST_MS      3000


// ----- Inits for the RTC clock
#include <Wire.h>
#include "ds3231.h"
struct ts rtcTime;

// ----- Inits for the screen
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

Adafruit_7segment ledScreen                 = Adafruit_7segment();
boolean           middleColonToggle         = false;
unsigned long     timeToggledMiddleColonMs  = 0L;

// TODO: Doc is wrong about writeDigitNum(location, number, dot)
// 0x02 - center colon
// 0x04 - left colon - lower dot
// 0x08 - left colon - upper dot
// 0x10 - decimal point


// ----- Inits for the buttons
#define PIN_BUTTON_SET_CLOCK              2
#define PIN_BUTTON_SET_WAKE_UP_TIME       4
#define PIN_BUTTON_ALARM_ON_OFF           3

// 0 if not pressed, or time in millis
unsigned long timePressedSetClockMs       = 0L;
unsigned long timePressedSetWakeUpTimeMs  = 0L;
unsigned long timePressedAlarmOnOffMs     = 0L;


// ----- Inits for the fan
#define PIN_FAN                           6


// ----- Inits for the DC motor
// TODO
// PINS 10 to 13




// TODO





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

    // Buttons
    pinMode( PIN_BUTTON_SET_CLOCK,        INPUT );
    pinMode( PIN_BUTTON_SET_WAKE_UP_TIME, INPUT );
    pinMode( PIN_BUTTON_ALARM_ON_OFF,     INPUT );

    // Fan
    pinMode( PIN_FAN,                     OUTPUT );

    // TODO: DC Motor

    return;
}



/**
 *  Endless loop. Will try to run every second, steadily at '000'ms
 */
void loop() 
{
    loopStartMs = millis();

    // --- Get time and display it
    DS3231_get( &rtcTime );
    displayTimeClock();

    // --- Check buttons
    actOnButtons( digitalRead( PIN_BUTTON_SET_CLOCK )        == HIGH, 
                  digitalRead( PIN_BUTTON_SET_WAKE_UP_TIME ) == HIGH, 
                  digitalRead( PIN_BUTTON_ALARM_ON_OFF )     == HIGH );
    // TODO: display alarm status (upper dot)
    
                  
/*
    if ( digitalRead( PIN_BUTTON_SET_CLOCK ) == HIGH ) {
      ledScreen.print( 1111 );
    }
    else {
        timePressedSetClockMs = 0L;
    } 
    
    
    if ( digitalRead( PIN_BUTTON_SET_WAKE_UP_TIME ) == HIGH ) {
      ledScreen.print( 2222 );
    }
*/
    
    if ( digitalRead( PIN_BUTTON_ALARM_ON_OFF ) == HIGH ) {
      ledScreen.print( 3333 );
      analogWrite( PIN_FAN, 255 );
    } else {
      analogWrite( PIN_FAN, 0 );    
    }

    // 



    // Write display when we are sure to have the correct thing displayed 
    ledScreen.writeDisplay();

    // --- To loop every loopDurationMs period. 
    //      If took more than loopDurationMs, loop immediately
    //      Otherwise try to loop until the next "tick" precisely (00ms)
    loopEndMs = millis();
    if ( loopEndMs - loopStartMs < loopDurationMs ) {
        int diff = loopDurationMs - (loopEndMs % loopDurationMs);
#ifdef DEBUG
        Serial.print( "Waiting for " );
        Serial.print( diff );
        Serial.println( " ms" );
#endif
        delay( diff );
    }
    return;
}



// =============================================================================================
// ++++++ HELPER FUNCTIONS ++++++
// =============================================================================================


/**
 * Display the time from the RTC 
 */
void displayTimeClock()
{
#ifdef DEBUG
    // display the time on the console
    snprintf(buff, BUFFER_SIZE, "%d.%02d.%02d %02d:%02d:%02d", 
        rtcTime.year, rtcTime.mon, rtcTime.mday, rtcTime.hour, rtcTime.min, rtcTime.sec);
    Serial.println(buff);
#endif
    // display the time on the screen, toggling the middle colon every second
    ledScreen.print( rtcTime.hour*100 + rtcTime.min, DEC);
    if ( loopStartMs - timeToggledMiddleColonMs > 950 ) {
        middleColonToggle = !middleColonToggle;
        timeToggledMiddleColonMs = loopStartMs;
    }
    ledScreen.drawColon( middleColonToggle );

    // TODO: dots for AM/PM (or 24h format?)
    // TODO: dot when alarm set

    // Which dots on?
    //  ledScreen.writeDigitNum(2, 1); // upper
    //  ledScreen.writeDigitNum(2, 3); // upper + lower
    //  ledScreen.writeDigitNum(2, 0); // upper + lower + decimal

    return;
}



/**
 * Act according to the button(s) pressed 
 */
void actOnButtons( boolean pressedSetClock, boolean pressedSetWakeUpTime, boolean pressedAlarmOnOff )
{
    // 1.--- Two buttons pressed for 3 secs should turn the alarm on, to test
    if ( pressedSetClock && pressedSetWakeUpTime ) {
        if ( alarmDoorStatus == ALARM_DOOR_STATUS_OPENING || alarmDoorStatus == ALARM_DOOR_STATUS_OPEN ) {
            return;     // Nothing to do
        }
        if ( timePressedSetClockMs == 0L || timePressedSetWakeUpTimeMs == 0L ) {
            // It's the moment when both are pressed that count the start
            timePressedSetClockMs       = loopStartMs;
            timePressedSetWakeUpTimeMs  = loopStartMs;
        }
        else {
            // Find out if we need to change some status
            if (  (alarmDoorStatus == ALARM_DOOR_STATUS_CLOSING || alarmDoorStatus == ALARM_DOOR_STATUS_CLOSED) 
               && (loopStartMs - timePressedSetClockMs > DELAY_2BUTTONS_TEST_MS) ) {
                alarmDoorStatus     = ALARM_DOOR_STATUS_OPENING;
                // alarmDoorOpeningPct = 0;   // Not for STATUS_CLOSING
            }
        }
        return;
    }

    // 2.--- Setting the alarm on/off
    if ( timePressedAlarmOnOffMs != 0L ) {
        if ( pressedAlarmOnOff ) {    // Button still pressed, we ignore
            return;
        }
        // Alarm on/off button has just been released. Reset timePressed and go on.
        timePressedAlarmOnOffMs = 0L;
    }
    else if ( pressedAlarmOnOff ) {   // First time pressed
        timePressedAlarmOnOffMs = loopStartMs;
        // If alarm is on, turn it off
        if ( alarmDoorStatus == ALARM_DOOR_STATUS_OPENING || alarmDoorStatus == ALARM_DOOR_STATUS_OPEN ) {
            alarmDoorStatus = ALARM_DOOR_STATUS_CLOSING;
        } 
        else if (alarmDoorStatus == ALARM_DOOR_STATUS_CLOSED) {
            // Toggle between on and off the alarm

            // TODO !!!!
            
        }
        // else if (alarmDoorStatus == ALARM_DOOR_STATUS_CLOSING) // Ignore the press
        return;
    }

    // 3.--- Setting the alarm time
    if ( pressedSetWakeUpTime ) {


        // Display alarm time

        return;
    }


    // 4.--- Setting the clock
    if ( pressedSetClock ) {


        return;
    }


//#define ALARM_DOOR_STATUS_CLOSED    0
//#define ALARM_DOOR_STATUS_OPENING   1
//#define ALARM_DOOR_STATUS_OPEN      2
//#define ALARM_DOOR_STATUS_CLOSING   3
//int     alarmDoorStatus     = ALARM_DOOR_STATUS_CLOSED;
//int     alarmDoorOpeningPct = 0;


//timePressedSetClockMs       = 0L;
//unsigned long timePressedSetWakeUpTimeMs  = 0L;
//unsigned long timePressedAlarmOnOffMs 

    return;
}


// ============================= ALARM ==========================================

/**
 *  @return true if the alarm is set  //  triggered
 */
boolean isAlarmSet()
{
    return ( DS3231_get_addr(DS3231_CONTROL_ADDR) & DS3231_A1IE ) ? true : false; 
}

boolean isAlarmTriggered()
{
    return ( DS3231_get_addr(DS3231_STATUS_ADDR) & DS3231_A1F ) ? true : false; 
}


/**
 *  Enable or clear Alarm
 *  If alarm time is passed when we reenable the alarm, it will naturally be active the next day
 */
void clearAlarm()
{ 
    // Disable + Clear the status
    DS3231_set_creg( DS3231_get_addr(DS3231_CONTROL_ADDR) & ~DS3231_A1IE );
    DS3231_clear_a1f();
}

void enableAlarm() 
{
    DS3231_set_creg( DS3231_get_addr(DS3231_CONTROL_ADDR) | DS3231_A1IE );
}


/**
 *  Get alarm time as uint8_t[4] second, minute, hour, day
 */
uint8_t* getAlarmTime()
{
    // Copied from DS3231_get_a1 to get the uint8_t[4] directly
    uint8_t n[4];
    uint8_t t[4];               // second,minute,hour,day
    uint8_t f[5];               // flags
    uint8_t i;

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(DS3231_ALARM1_ADDR);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDR, 4);

    for (i = 0; i <= 3; i++) {
        n[i] = Wire.read();
        f[i] = (n[i] & 0x80) >> 7;
        t[i] = bcdtodec(n[i] & 0x7F);
    }
    f[4] = (n[3] & 0x40) >> 6;
    t[3] = bcdtodec(n[3] & 0x3F);

    return t;
}


/**
 * Set the alarm time
 */
void setAlarmTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm - see datasheet
    // A1M1 (seconds) (0 to enable, 1 to disable)
    // A1M2 (minutes) (0 to enable, 1 to disable)
    // A1M3 (hour)    (0 to enable, 1 to disable) 
    // A1M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    uint8_t flags[5] = { 0, 0, 0, 1, 1 };           // Every day, when hours match

    // set Alarm1 and enable it
    DS3231_set_a1( second, minute, hour, 0, flags);
    enableAlarm();
    return;
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
