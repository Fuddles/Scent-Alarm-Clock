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
unsigned int  loopDurationMs  = 200;      // Warning: impact the speed of time increase on setting alarm/time + note length (for tunes)
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

// ----- Inits for the DC motor: PINS and trigger time    // TODO: PINS 8,9,12,13
#define MOTOR_PIN_IN1                     10
#define MOTOR_PIN_IN2                     11 
#define MOTOR_PIN_IN3                     12 
#define MOTOR_PIN_IN4                     13 

unsigned long timeTriggeredAlarm          = 0L;

// --- Buzzer defs
//      WARNING: Use of the tone() function will interfere with PWM output on pins 3 and 11    // FIXME!!
#define BUZZER_PWD_PIN                    5 

#define BUZZER_ALARM                      1
#define BUZZER_CLOSING_DOOR               2
#define BUZZER__LAST_TUNE                 2

#define BUZZER_ONE_NOTE_DURATION_MS       loopDurationMs
#define BUZZER_ONE_NOTE_SUPPL_MS          200

// Tunes are series of letters of equal duration (loopDurationMs), so write the same note several times to play it longer
//  char notes[] = { ' ', 'S', 'A', 'B', 'C', 'K', 'D', 'E', 'F', 'P', 'G', 'a', 'b', 'c', 'k', 'd', 'e', 'f', 'p', 'g', 'h', 'i' };
//  Added by Eric:    S= G3   K= C4#   k= C5#   P= F4#   p= F5#
char* TUNES[] = {  " ",
                   "CCCCGGcccc ",        // BUZZER ALARM 
                   "iiiCC",              // BUZZER ALERT CLOSING DOOR
                   " "  }; 
                   
char*         currentTunePlayed;
int           currentTunePlayedNoteIdx;
unsigned long currentTunePlayedNoteStartTimeMs;
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

    // TODO: DC Motor

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

    // --- Check buttons
    actOnButtons( digitalRead( PIN_BUTTON_SET_CLOCK )        == HIGH, 
                  digitalRead( PIN_BUTTON_SET_WAKE_UP_TIME ) == HIGH, 
                  digitalRead( PIN_BUTTON_ALARM_ON_OFF )     == HIGH );
    // Display alarm status (upper dot)
    if ( isAlarmSet() ) {
        ledScreen.writeDigitNum(2, 1);
    }

    // --- Is the alarm triggering?
    if ( isAlarmTriggered() ) {
        // Clear the alarm
        clearAlarm();
        // Set the door status to opening
        if ( alarmDoorStatus != ALARM_DOOR_STATUS_OPENING && alarmDoorStatus != ALARM_DOOR_STATUS_OPEN ) {
            alarmDoorStatus = ALARM_DOOR_STATUS_OPENING;
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


// ============================= FAN, MOTOR ==========================================

/**
 * 
 */
void performDoorFanBuzzerAlarm() 
{
    // --- Fan
    boolean isFanOn = (alarmDoorStatus == ALARM_DOOR_STATUS_OPEN);
    digitalWrite( PIN_FAN, isFanOn ? HIGH : LOW ); 


               
//#define ALARM_DOOR_STATUS_CLOSED    0
//#define ALARM_DOOR_STATUS_OPENING   1
//#define ALARM_DOOR_STATUS_OPEN      2
//#define ALARM_DOOR_STATUS_CLOSING   3
//int     alarmDoorStatus     = ALARM_DOOR_STATUS_CLOSED;
//int     alarmDoorOpeningPct = 0;

//unsigned long timeTriggeredAlarm          = 0L;


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
 *  Init alarm
 */
void initRTCAlarm()
{
    Wire.begin();
    DS3231_set_creg( DS3231_get_addr(DS3231_CONTROL_ADDR) | DS3231_INTCN );   // Enable INT pin on the DS3231
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
    currentTunePlayed  = TUNES[ tuneId ];
  
    // Is something already playing? If yes, wait for the note to finish!
    if (buzzerIsPlaying) {
        currentTunePlayedNoteIdx  = -1;    // Trick to start at the first note of this tune
        return;
    }
    
    // Mark the start
    currentTunePlayedNoteStartTimeMs = loopStartMs;     // millis();
    currentTunePlayedNoteIdx         = 0;
    buzzerIsPlaying = true;
  
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
  if ( !buzzerIsPlaying || !currentTunePlayed || currentTunePlayedNoteIdx >= sizeof(currentTunePlayed) ) {
    buzzerIsPlaying = false;
    noTone( BUZZER_PWD_PIN );
    return;
  }
  
  // Has time elapsed?
  if ( loopStartMs - currentTunePlayedNoteStartTimeMs < BUZZER_ONE_NOTE_DURATION_MS ) {
    return;
  }
  
  // --- Now play the next note
  currentTunePlayedNoteIdx++;
  // End of the tune?
  if ( currentTunePlayedNoteIdx >= sizeof(currentTunePlayed) ) {
    buzzerIsPlaying = false;
    noTone( BUZZER_PWD_PIN );
    return;    
  }
  // Ok now there is a next note to play! We add 50ms additional duration to avoid blanks. The next tone() interrupts anyway
  currentTunePlayedNoteStartTimeMs = loopStartMs;     // millis();
  tone( BUZZER_PWD_PIN, frequency( currentTunePlayed[currentTunePlayedNoteIdx] ), BUZZER_ONE_NOTE_SUPPL_MS + BUZZER_ONE_NOTE_DURATION_MS );

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
// TODO: Doc is wrong about writeDigitNum(location, number, dot). Or mess up with drawColon() ?
// 0x02 - center colon
// 0x04 - left colon - lower dot
// 0x08 - left colon - upper dot
// 0x10 - decimal point

////  ledScreen.writeDigitNum(2, 0, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 1, true ); // upper
////  ledScreen.writeDigitNum(2, 2, true ); // lower + decimal
////  ledScreen.writeDigitNum(2, 3, true ); // lower + upper
//ledScreen.writeDigitNum(2, 4, true ); // upper 
////  ledScreen.writeDigitNum(2, 5, true ); // lower + upper
////  ledScreen.writeDigitNum(2, 6, true ); // lower + upper + decimal (all)
////  ledScreen.writeDigitNum(2, 7, true ); // upper
*/
