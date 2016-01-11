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
unsigned int  loopDurationMs  = 200;      // Warning: impact the speed of time increase on setting alarm/time
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
uint8_t   alarmRTCTime[4];                // second,minute,hour,day

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
    // Display alarm status (upper dot)
    if ( isAlarmSet() ) {
        ledScreen.writeDigitNum(2, 1);
    }
                  
/*    
    if ( digitalRead( PIN_BUTTON_ALARM_ON_OFF ) == HIGH ) {
      ledScreen.print( 3333 );
      analogWrite( PIN_FAN, 255 );
    } else {
      analogWrite( PIN_FAN, 0 );    
    }
*/


//#define ALARM_DOOR_STATUS_CLOSED    0
//#define ALARM_DOOR_STATUS_OPENING   1
//#define ALARM_DOOR_STATUS_OPEN      2
//#define ALARM_DOOR_STATUS_CLOSING   3
//int     alarmDoorStatus     = ALARM_DOOR_STATUS_CLOSED;
//int     alarmDoorOpeningPct = 0;


//timePressedSetClockMs       = 0L;
//unsigned long timePressedSetWakeUpTimeMs  = 0L;
//unsigned long timePressedAlarmOnOffMs 




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
            if ( isAlarmSet() ) {
                clearAlarm();
            } else {
                enableAlarm();
            }            
        }
        // else if (alarmDoorStatus == ALARM_DOOR_STATUS_CLOSING) // Ignore the press
        return;
    }

    // 3.--- Setting the alarm time
    if ( pressedSetWakeUpTime ) {
        if ( timePressedSetWakeUpTimeMs == 0L ) {                           // Just pressed
            timePressedSetWakeUpTimeMs  = loopStartMs;
        } else if ( loopStartMs - timePressedSetWakeUpTimeMs < 1000 ) {     // Less than 1 sec
            // Just display the time
        } else if ( loopStartMs - timePressedSetWakeUpTimeMs < 2000 ) {     // Less than 2 sec
            addToAlarmTime( 25, false );       // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetWakeUpTimeMs < 3000 ) {     // Less than 3 sec
            addToAlarmTime( 60, false );       // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetWakeUpTimeMs < 4000 ) {     // Less than 4 sec
            addToAlarmTime( 150, false );      // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetWakeUpTimeMs < 5000 ) {     // Less than 5 sec
            addToAlarmTime( 300, false );      // in seconds, reset seconds
        } else {                                                            // After 5 sec
            addToAlarmTime( 1500, false );     // in seconds, reset seconds
        }
        
        // Display alarm time
        getAlarmTime();       // result in alarmRTCTime
        ledScreen.print( alarmRTCTime[2]*100 + alarmRTCTime[1], DEC);
        ledScreen.drawColon( true );
        return;
    } else {
        if ( timePressedSetWakeUpTimeMs != 0L ) {                           // Just released
              timePressedSetWakeUpTimeMs = 0L;
              addToAlarmTime( 0, true );      // in seconds, reset seconds
        }
    }


    // 4.--- Setting the clock
    if ( pressedSetClock ) {
        if ( timePressedSetClockMs == 0L ) {                                // Just pressed
            timePressedSetClockMs  = loopStartMs;
        } else if ( loopStartMs - timePressedSetClockMs < 1000 ) {          // Less than 1 sec
            // Just display the time
        } else if ( loopStartMs - timePressedSetClockMs < 2000 ) {          // Less than 2 sec
            addToClockTime( 25, false );       // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetClockMs < 3000 ) {          // Less than 3 sec
            addToClockTime( 60, false );       // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetClockMs < 4000 ) {          // Less than 4 sec
            addToClockTime( 150, false );      // in seconds, reset seconds
        } else if ( loopStartMs - timePressedSetClockMs < 5000 ) {          // Less than 5 sec
            addToClockTime( 300, false );      // in seconds, reset seconds
        } else {                                                            // After 5 sec
            addToClockTime( 1500, false );     // in seconds, reset seconds
        }

        // Display the updated clock
        displayTimeClock();
        return;
    } else {
        if ( timePressedSetClockMs != 0L ) {                           // Just released
              timePressedSetClockMs = 0L;
              addToClockTime( 0, true );      // in seconds, reset seconds
        }
    }

    // Nothing pressed
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
    // Copied from DS3231_get_a1 to get the uint8_t[4] directly into alarmRTCTime[]
    uint8_t n[4];
    //uint8_t t[4];             // alarmRTCTime second,minute,hour,day
    uint8_t f[5];               // flags
    uint8_t i;

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(DS3231_ALARM1_ADDR);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDR, 4);

    for (i = 0; i <= 3; i++) {
        n[i] = Wire.read();
        f[i] = (n[i] & 0x80) >> 7;
        alarmRTCTime[i] = bcdtodec(n[i] & 0x7F);
    }
    f[4] = (n[3] & 0x40) >> 6;
    alarmRTCTime[3] = bcdtodec(n[3] & 0x3F);
    return alarmRTCTime;
}


/**
 * Set the alarm time
 */
void setAlarmTime(uint8_t second, uint8_t minute, uint8_t hour )
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


/**
 *  Add a number of seconds to alarm time, and set the alarm on
 *  @param resetSeconds set the alarm seconds do 0
 */
void addToAlarmTime( unsigned int addSeconds, boolean resetSeconds ) 
{
    getAlarmTime();           // result in alarmRTCTime second,minute,hour,day

    if (addSeconds > 86400) {
        addSeconds %= 86400;
    }
    uint8_t hours   = alarmRTCTime[2] + (unsigned int)(addSeconds / 3600);
    uint8_t minutes = alarmRTCTime[1] + (unsigned int)(addSeconds / 60) % 60;
    uint8_t seconds = resetSeconds ? 0 : alarmRTCTime[0] + (unsigned int)(addSeconds % 60);
    if (seconds >= 60) {
        minutes++;
        seconds -= 60;
    }
    if (minutes >= 60) {
        hours++;
        minutes -= 60;
    }
    if (hours >= 24) {
      hours -= 24;
    }

    setAlarmTime( seconds, minutes, hours );
    return;  
}



// ============================= SET CLOCK ==========================================

/**
 *  Add a number of seconds to clock time
 *  @param resetSeconds set the clock seconds do 0
 */
void addToClockTime( unsigned int addSeconds, boolean resetSeconds ) 
{
    DS3231_get( &rtcTime );

    if (addSeconds > 86400) {
        addSeconds %= 86400;
    }
    uint8_t hours   = rtcTime.hour + (unsigned int)(addSeconds / 3600);
    uint8_t minutes = rtcTime.min  + (unsigned int)(addSeconds / 60) % 60;
    uint8_t seconds = resetSeconds ? 0 : rtcTime.sec + (unsigned int)(addSeconds % 60);
    if (seconds >= 60) {
        minutes++;
        seconds -= 60;
    }
    if (minutes >= 60) {
        hours++;
        minutes -= 60;
    }
    if (hours >= 24) {
      hours -= 24;
    }

    // Set time, keeping days, months, and year
    rtcTime.hour = hours;
    rtcTime.min  = minutes;
    rtcTime.sec  = seconds;
    DS3231_set( rtcTime );
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
