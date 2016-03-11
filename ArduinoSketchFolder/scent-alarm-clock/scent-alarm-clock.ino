/**
 *  Alarm clock project that turns a fan on when the alarm triggers
 */

// Comment the line to remove the console debugging (via Serial)
#define DEBUG         true

// Uncomment the line to set the time
// #define SET_RTC_TIME  true

// Screen Dim brightness (1..14)
#define LOW_SCREEN_BRIGHTNESS      5

// Delay and duration when alarm ON
#define DELAY_DOOR_OPEN_BEFORE_MUSIC_SECS     210L
#define MAX_DURATION_DOOR_OPEN_SECS           600L

// Buffer size to print on the console
#define BUFFER_SIZE   256
char    buff[BUFFER_SIZE];

// Loop properties
unsigned int  loopDurationMs  = 200;      // Warning: impact the speed of time increase on setting alarm/time + note length (for tunes)
unsigned long loopStartMs     = 0L;
unsigned long loopEndMs       = 0L;

// Alarm door status
#define ALARM_DOOR_STATUS_CLOSED    0
#define ALARM_DOOR_STATUS_OPENING   1
#define ALARM_DOOR_STATUS_OPEN      2
#define ALARM_DOOR_STATUS_CLOSING   3
int           alarmDoorStatus                 = ALARM_DOOR_STATUS_CLOSED;
int           alarmDoorOpeningPct             = 0;          // Between 0 (closed) and 100 (open)
unsigned long timeTriggeredOpeningClosingMs   = 0L;


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


// ----- Inits for the buttons
#define PIN_BUTTON_SET_CLOCK              2
#define PIN_BUTTON_SET_WAKE_UP_TIME       4
#define PIN_BUTTON_ALARM_ON_OFF           6

// 0 if not pressed, or time in millis
unsigned long timePressedSetClockMs       = 0L;
unsigned long timePressedSetWakeUpTimeMs  = 0L;
unsigned long timePressedAlarmOnOffMs     = 0L;


// ----- Inits for the fan
#define PIN_FAN                           7

// ----- Inits for the DC motor: PINS and trigger time    // TODO: PINS 8,9,12,13
#include <Stepper.h>

#define MOTOR_PIN_IN1                     8
#define MOTOR_PIN_IN2                     9 
#define MOTOR_PIN_IN3                     12 
#define MOTOR_PIN_IN4                     13 

// From https://arduino-info.wikispaces.com/SmallSteppers
//  32 confirmed by http://42bots.com/tutorials/28byj-48-stepper-motor-with-uln2003-driver-and-arduino-uno/
//  64:1 gear ratio
// Number of steps per revolution of INTERNAL motor in 4-step mode
#define STEPS_PER_MOTOR_REVOLUTION        32   
// Steps per OUTPUT SHAFT of gear reduction
#define STEPS_PER_OUTPUT_REVOLUTION       2048            // 32 * 64 = 2048  

// Our total number of steps to open the door
#define TOTAL_STEPS_DOOR_OPENING          400

// Steps is the number of steps in one revolution of the motor. 32 according to
Stepper motor = Stepper( STEPS_PER_MOTOR_REVOLUTION, MOTOR_PIN_IN1, MOTOR_PIN_IN3, MOTOR_PIN_IN2, MOTOR_PIN_IN4 );   // steps, pin1, pin2, pin3, pin4)


// --- Buzzer defs
//      WARNING: Use of the tone() function will interfere with PWM output on pins 3 and 11 
#define BUZZER_PWD_PIN                    5 

#define BUZZER_ALARM                      1
#define BUZZER_CLOSING_DOOR               2
#define BUZZER__LAST_TUNE                 2

#define BUZZER_ONE_NOTE_DURATION_MS       loopDurationMs
#define BUZZER_ONE_NOTE_SUPPL_MS          200

// Tunes are series of letters of equal duration (loopDurationMs), so write the same note several times to play it longer
//  char notes[] = { ' ', 'S', 'A', 'B', 'C', 'K', 'D', 'E', 'F', 'P', 'G', 'a', 'b', 'c', 'k', 'd', 'e', 'f', 'p', 'g', 'h', 'i' };
//  Added by Eric:    S= G3   K= C4#   k= C5#   P= F4#   p= F5#
char* TUNES[]  = { " ",
                   "EEE EEE EEE KK GEEE KK GEEEEE ",    // BUZZER ALARM 
                   "ccaa",                              // BUZZER ALERT CLOSING DOOR
                   " "  }; 
                   
String        currentTunePlayed;
int           currentTunePlayedNoteIdx;
//unsigned long currentTunePlayedNoteStartTimeMs;   // Here we play one note per cycle
boolean       buzzerIsPlaying = false;




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

    // Init RTC 
    initRTCAlarm();

    // Screen
    ledScreen.begin(0x70);
    ledScreen.setBrightness(15);     // Fully bright

    // Buttons
    pinMode( PIN_BUTTON_SET_CLOCK,        INPUT );
    pinMode( PIN_BUTTON_SET_WAKE_UP_TIME, INPUT );
    pinMode( PIN_BUTTON_ALARM_ON_OFF,     INPUT );

    // Fan
    pinMode( PIN_FAN,                     OUTPUT );

    // DC Motor
    motor.setSpeed( 1024 );            // rpms: 2000 forbidden. 1000 gives ~4 seconds for 2048 steps = 360 deg
    // Test: motor.step when >0 clockwise. 512 is 90 deg
    // motor.step( TOTAL_STEPS_DOOR_OPENING );            // WARNING: motor.step is BLOCKING!

    // Buzzer
    pinMode( BUZZER_PWD_PIN,              OUTPUT );
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
    // Display alarm status (upper dot)
    if ( isAlarmSet() ) {
        ledScreen.writeDigitRaw(2, middleColonToggle ? (0x04 | 0x02) : 0x04 );            // Raw, not num!
    }

    // --- Check buttons
    actOnButtons( digitalRead( PIN_BUTTON_SET_CLOCK )        == HIGH, 
                  digitalRead( PIN_BUTTON_SET_WAKE_UP_TIME ) == HIGH, 
                  digitalRead( PIN_BUTTON_ALARM_ON_OFF )     == HIGH );

    // --- Is the alarm triggering?
    if ( isAlarmTriggered() ) {
        // Clear the alarm
        clearAlarm();
        // Set the door status to opening
        if ( alarmDoorStatus != ALARM_DOOR_STATUS_OPENING && alarmDoorStatus != ALARM_DOOR_STATUS_OPEN ) {
            alarmDoorStatus               = ALARM_DOOR_STATUS_OPENING;
            timeTriggeredOpeningClosingMs = 0L;
            // alarmDoorOpeningPct = 0;   // Not for STATUS_CLOSING
        }
    }

    // --- Open the door, turn on the fan, play music
    performDoorFanBuzzerAlarm();

    // Write display when we are sure to have the correct thing displayed 
    ledScreen.writeDisplay();

    // --- Buzzer - currently played tune?
    if (buzzerIsPlaying) {
        playTuneNextNote();
    }


    // --- To loop every loopDurationMs period. 
    //      If took more than loopDurationMs, loop immediately
    //      Otherwise try to loop until the next "tick" precisely (00ms)
    loopEndMs = millis();
    if ( loopEndMs - loopStartMs < loopDurationMs ) {
        int diff = loopDurationMs - (loopEndMs % loopDurationMs);
#ifdef DEBUG
        if ( diff < 100 ) {
            Serial.print( "Waiting for " );
            Serial.print( diff );
            Serial.println( " ms" );
        }
#endif
        delay( diff );
    }
    return;
}







// =============================================================================================
// ++++++ HELPER FUNCTIONS ++++++
// =============================================================================================


// ============================= FAN, MOTOR ==========================================

/**
 * 
 */
void performDoorFanBuzzerAlarm() 
{
    // --- Fan
    boolean isFanOn = (alarmDoorStatus == ALARM_DOOR_STATUS_OPEN);
    digitalWrite( PIN_FAN, isFanOn ? HIGH : LOW ); 

#ifdef DEBUG
        Serial.print( "Fan is " );
        Serial.print( isFanOn ? "ON" : "off" );
        Serial.print( " and Door status is " );
        Serial.print( alarmDoorStatus == ALARM_DOOR_STATUS_CLOSED  ? "CLOSED" 
                    : alarmDoorStatus == ALARM_DOOR_STATUS_OPENING ? "OPENING" 
                    : alarmDoorStatus == ALARM_DOOR_STATUS_OPEN    ? "OPEN" 
                    : alarmDoorStatus == ALARM_DOOR_STATUS_CLOSING ? "CLOSING" 
                    : "BUG!" );
        Serial.print( ", with OpeningPct = " );
        Serial.println( alarmDoorOpeningPct );
#endif


    // --- Door and Buzzer!
    if ( alarmDoorStatus == ALARM_DOOR_STATUS_CLOSED ) {
        // Make sure timeTriggeredOpeningClosingMs reset. Otw, do not do anything else
        timeTriggeredOpeningClosingMs  = 0L;
        return;
    }
    
    if ( alarmDoorStatus == ALARM_DOOR_STATUS_CLOSING ) {
        // First second: do not move and play buzzer alert
        if ( timeTriggeredOpeningClosingMs == 0L ) {
            timeTriggeredOpeningClosingMs = loopStartMs;
            playTune(BUZZER_CLOSING_DOOR);
            // alarmDoorOpeningPct = 100;     // No, as Opening may have been interrupted
        } 
        if ( alarmDoorOpeningPct <= 0 ) {
            // Door is fully closed
            alarmDoorStatus     = ALARM_DOOR_STATUS_CLOSED;
            alarmDoorOpeningPct = 0;
        } else if ( loopStartMs - timeTriggeredOpeningClosingMs >= 1000 ) {
            // Then move the door, alarmDoorOpeningPct going from 100 to 0, to close it over 2 seconds ( / 2000 * 100)
            int targetPct = 100 - (loopStartMs - timeTriggeredOpeningClosingMs - 1000) / 20;
            if ( targetPct < 0 ) {
                targetPct = 0;
            }

            // Move from alarmDoorOpeningPct current position to targetPct
            //    motor.step when >0 counter-clockwise. 512 is 90 deg  // WARNING: motor.step is BLOCKING!
            int numSteps = ((targetPct - alarmDoorOpeningPct) * TOTAL_STEPS_DOOR_OPENING) / 100;
            unsigned long now = millis();
            motor.step( numSteps );     // WARNING: motor.step is BLOCKING!
#ifdef DEBUG
            Serial.print( "Stepper Motor CLOSING by numSteps = " );
            Serial.print( numSteps );
            Serial.print( ", and it took " );
            Serial.print( millis() - now );
            Serial.println( " ms" );
#endif
            alarmDoorOpeningPct = targetPct;            
        }
        return;
    }

    if ( alarmDoorStatus == ALARM_DOOR_STATUS_OPENING ) {
        if ( timeTriggeredOpeningClosingMs == 0L ) {
            timeTriggeredOpeningClosingMs = loopStartMs;
            // playTune(BUZZER_CLOSING_DOOR); // No tune for opening
            // alarmDoorOpeningPct = 100;     // No, as Opening may have been interrupted
        }
        if ( alarmDoorOpeningPct >= 100 ) {
            // Door is fully open
            alarmDoorStatus     = ALARM_DOOR_STATUS_OPEN;
            alarmDoorOpeningPct = 100;
        } else {
            // Then move the door, alarmDoorOpeningPct going from 100 to 0, to close it over 2 seconds ( / 2000 * 100)
            int targetPct = (loopStartMs - timeTriggeredOpeningClosingMs) / 20;
            if ( targetPct > 100 ) {
                targetPct = 100;
            }

            // Move from alarmDoorOpeningPct current position to targetPct
            //    motor.step when >0 counter-clockwise. 512 is 90 deg  // WARNING: motor.step is BLOCKING!
            int numSteps = ((targetPct - alarmDoorOpeningPct) * TOTAL_STEPS_DOOR_OPENING) / 100;
            unsigned long now = millis();
            motor.step( numSteps );     // WARNING: motor.step is BLOCKING!
#ifdef DEBUG
            Serial.print( "Stepper Motor OPENING by numSteps = " );
            Serial.print( numSteps );
            Serial.print( ", and it took " );
            Serial.print( millis() - now );
            Serial.println( " ms" );
#endif
            alarmDoorOpeningPct = targetPct;
        }
        return;
    }
    
    if ( alarmDoorStatus == ALARM_DOOR_STATUS_OPEN ) {
        if ( loopStartMs - timeTriggeredOpeningClosingMs >= 1000L * MAX_DURATION_DOOR_OPEN_SECS ) {
            // After 10 min, stop the alarm anyway
            alarmDoorStatus                = ALARM_DOOR_STATUS_CLOSING;
            timeTriggeredOpeningClosingMs  = 0L;
            clearAlarm();
            return;
        }
        if ( !buzzerIsPlaying && loopStartMs - timeTriggeredOpeningClosingMs >= 1000L * DELAY_DOOR_OPEN_BEFORE_MUSIC_SECS ) {
            // After 15s, play tune !
            playTune( BUZZER_ALARM );
        }
    }
    return;
}






// ============================= DISPLAY ==========================================

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
    printTimeOnLedScreen( rtcTime.hour, rtcTime.min, false );
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

void printTimeOnLedScreen( uint8_t hours, uint8_t minutes, boolean ledFullyBright ) 
{
    ledScreen.print( hours*100 + minutes, DEC);
    if (hours == 0) {
        ledScreen.writeDigitNum( 1, 0 );    // location, number
        if (minutes < 10) {
            ledScreen.writeDigitNum( 3, 0 );
        }
    }
    ledScreen.setBrightness( ledFullyBright || (hours >= 7 && hours <= 20) ? 15 : LOW_SCREEN_BRIGHTNESS );     // Fully bright
    return;
}


// ============================= BUTTONS ==========================================

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
                alarmDoorStatus               = ALARM_DOOR_STATUS_OPENING;
                timeTriggeredOpeningClosingMs = 0L;
                // alarmDoorOpeningPct = 0;   // Not for STATUS_CLOSING!
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
            alarmDoorStatus                = ALARM_DOOR_STATUS_CLOSING;
            timeTriggeredOpeningClosingMs  = 0L;
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
        } else {
            unsigned int secsToAdd = calcSecondsToAdd( loopStartMs - timePressedSetWakeUpTimeMs );
            if ( secsToAdd > 0 ) {
                addToAlarmTime( secsToAdd, false );       // in seconds, reset seconds
            }
            // else just display the time
        }
                
        // Display alarm time. With lower dot to distinguish
        getAlarmTime();       // result in alarmRTCTime
        printTimeOnLedScreen( alarmRTCTime[2], alarmRTCTime[1], true );
        ledScreen.writeDigitRaw(2, 0x08 | 0x02 );            // Lower left + colon.  Raw, not num!
        return;
    } else {
        if ( timePressedSetWakeUpTimeMs != 0L ) {                           // Just released
              timePressedSetWakeUpTimeMs = 0L;
              addToAlarmTime( 0, true );      // in seconds, reset seconds
              // Display alarm time one more cycle
              getAlarmTime();       // result in alarmRTCTime
              printTimeOnLedScreen( alarmRTCTime[2], alarmRTCTime[1], true );
              ledScreen.writeDigitRaw(2, 0x08 | 0x02 );            // Lower left + colon.  Raw, not num!
        }
    }


    // 4.--- Setting the clock
    if ( pressedSetClock ) {
        if ( timePressedSetClockMs == 0L ) {                                // Just pressed
            timePressedSetClockMs  = loopStartMs;
        } else {
            unsigned int secsToAdd = calcSecondsToAdd( loopStartMs - timePressedSetClockMs );
            if ( secsToAdd > 0 ) {
                addToClockTime( secsToAdd, false );       // in seconds, reset seconds
            }
            // else just display the time
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



/** Helper to get the number of seconds to add according to the time pressed. Happen 5 times per seconds */
unsigned int calcSecondsToAdd( long diffFromTimePressedMs )
{
    if ( diffFromTimePressedMs < 700 )                          // Less than 1 sec or Just pressed
        return 0;
  
    if ( diffFromTimePressedMs < 2000 )                          // Less than 2 sec
        return 20;
    if ( diffFromTimePressedMs < 4000 )                          // Less than 4 sec
        return 30;
    if ( diffFromTimePressedMs < 6000 )                          // Less than 6 sec
        return 90;
    if ( diffFromTimePressedMs < 9000 )                          // Less than 9 sec
        return 180;

    // After 9s
    return 800;
}



// ============================= ALARM ==========================================

/**
 *  Init alarm
 */
void initRTCAlarm()
{
    Wire.begin();
    //DS3231_set_creg( DS3231_get_addr(DS3231_CONTROL_ADDR) | DS3231_INTCN );   // NO, otw trigger even when A1 not enabled. Enable INT pin on the DS3231
    DS3231_clear_a1f();               // Reset the alarm signal, but keep previous time and config
#ifdef SET_RTC_TIME
    struct ts rightNowTime = { .sec = 0, .min = 8, .hour = 19, .mday = 9, .mon = 1, .year = 2016 };
    DS3231_set( rightNowTime );
#endif
    return;
}


/**
 *  @return true if the alarm is set  //  triggered
 */
boolean isAlarmSet()
{
    return ( DS3231_get_addr(DS3231_CONTROL_ADDR) & DS3231_A1IE ) ? true : false; 
}

boolean isAlarmTriggered()
{
    boolean isTriggered = ( DS3231_get_addr(DS3231_STATUS_ADDR) & DS3231_A1F ) ? true : false; 
    // To be sure, double check it was enabled
    return ( isTriggered && isAlarmSet() );
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





// ============================= BUZZER ==========================================


/** +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  Play a tune. Start the first note here, it will be continued for the remaining notes in the loop
 *  @param tuneId  corresponds to the index in the TUNES array
 */
void playTune( int tuneId )
{
    // Check we know the tuneId
    if (tuneId < 0 || tuneId > BUZZER__LAST_TUNE)
        tuneId = 0;
  
    // Get the music score
    currentTunePlayed  = String( TUNES[ tuneId ] );

#ifdef DEBUG
        Serial.print( "***** Playing tune #" );
        Serial.print( tuneId );
        Serial.print( " of notes: " );
        Serial.println( currentTunePlayed );
#endif
    
    // Is something already playing? If yes, wait for the note to finish!
    if (buzzerIsPlaying) {
        currentTunePlayedNoteIdx  = -1;    // Trick to start at the first note of this tune
        return;
    }
    
    // Mark the start
    //currentTunePlayedNoteStartTimeMs = loopStartMs;     // millis();
    currentTunePlayedNoteIdx  = 0;
    buzzerIsPlaying           = true;
  
    // Play the first note
    tone( BUZZER_PWD_PIN, frequency( currentTunePlayed[0] ), BUZZER_ONE_NOTE_SUPPL_MS + BUZZER_ONE_NOTE_DURATION_MS );  // 50ms additional to avoid blanks
    return;
}

/**
 * Called by loop() to play the rest of the tune
 */
void playTuneNextNote()
{
  // Should never happen, but we know what it is...
  if ( !buzzerIsPlaying || !currentTunePlayed || currentTunePlayedNoteIdx >= currentTunePlayed.length() ) {
    buzzerIsPlaying = false;
    noTone( BUZZER_PWD_PIN );
#ifdef DEBUG
        Serial.println( "+++++ WARNING - NO TONE!!" );
#endif
    return;
  }

  // We just play one note per cycle
  // Has time elapsed?
  //if ( loopStartMs - currentTunePlayedNoteStartTimeMs < BUZZER_ONE_NOTE_DURATION_MS ) {
  //  return;
  //}
  
  // --- Now play the next note
  currentTunePlayedNoteIdx++;
  // End of the tune?
  if ( currentTunePlayedNoteIdx >= currentTunePlayed.length() ) {
    buzzerIsPlaying = false;
    noTone( BUZZER_PWD_PIN );
#ifdef DEBUG
        Serial.print( "END OF TUNE: currentTunePlayedNoteIdx = " );
        Serial.print( currentTunePlayedNoteIdx );
        Serial.print( " and sizeof(currentTunePlayed) = " );
        Serial.println( currentTunePlayed.length() );
#endif
    return;    
  }
  // Ok now there is a next note to play! We add 100ms additional duration to avoid blanks. The next tone() interrupts anyway
  //currentTunePlayedNoteStartTimeMs = loopStartMs;     // millis();
  tone( BUZZER_PWD_PIN, frequency( currentTunePlayed[currentTunePlayedNoteIdx] ), BUZZER_ONE_NOTE_SUPPL_MS + BUZZER_ONE_NOTE_DURATION_MS );
#ifdef DEBUG
        Serial.print( "NOTE PLAYED: " );
        Serial.println( currentTunePlayed[currentTunePlayedNoteIdx] );
#endif
  return;
}




/**
 *  Return the frequency of a note, to be used in build-in tone() function
 *
 *  WARNING:
 *    - Only one tone can be generated at a time. If a tone is already playing on a different pin, the call to tone() will have no effect. 
 *        If the tone is playing on the same pin, the call will set its frequency.
 *    - Use of the tone() function will interfere with PWM output on pins 3 and 11 (on boards other than the Mega).
 *    - It is not possible to generate tones lower than 31Hz.
 */
unsigned int frequency(char note) 
{
  // This function takes a note character (a-g), and returns the
  // corresponding frequency in Hz for the tone() function.
  
  const int numNotes = 22;  // number of notes we're storing
  
  // For the "char" (character) type, we put single characters in single quotes.

  // Added by Eric:    S= G3   K= C4#   k= C5#   P= F4#   p= F5#
  char         notes[]       = { ' ', 'S', 'A', 'B', 'C', 'K', 'D', 'E', 'F', 'P', 'G', 'a', 'b', 'c', 'k', 'd', 'e', 'f', 'p', 'g', 'h', 'i' };
  unsigned int frequencies[] = {   0, 196, 220, 247, 262, 277, 294, 330, 349, 370, 392, 440, 494, 523, 554, 587, 659, 698, 740, 784, 880, 988 };
  
  // Now we'll search through the letters in the array, and if
  // we find it, we'll return the frequency for that note.
  
  int startIdx = (note < 'a' ? 0 : 10);
  for (int i = startIdx; i < numNotes; i++) {     // Step through the notes
    if (notes[i] == note)  
      return (frequencies[i]);                    // Yes! Return the frequency
  }
  return 0;   // We looked through everything and didn't find it,
              // but we still need to return a value, so return 0.
}
 




/*
// NOTE: Doc is wrong about writeDigitNum(location, number, dot).
// Use writeDigitRaw(location, bitmask)       RAW, not num!
// 0x02 - center colon
// 0x04 - left colon - upper dot
// 0x08 - left colon - lower dot
// 0x10 - decimal point
*/
