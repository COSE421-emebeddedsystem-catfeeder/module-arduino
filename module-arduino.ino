#include <RtcDS1302.h>
#include <AccelStepper.h>
#include "RtcAlarmManager.h"
#include <Wire.h>

int PIN_TIME_CLK = 11;
int PIN_TIME_DAT = 12;
int PIN_TIME_RST = 13;

int PIN_STEP_DIR = 8;
int PIN_STEP_STEP = 9;
int PIN_STEP_EN = 10;

int PIN_PIR_SIG = 7;

ThreeWire ds1302(PIN_TIME_DAT, PIN_TIME_CLK, PIN_TIME_RST); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(ds1302);
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP_STEP, PIN_STEP_DIR);   

// foreward declare our alarm manager callback
void alarmCallback(uint8_t id, const RtcDateTime& alarm);
// global instance of the manager with three possible alarms
RtcAlarmManager<alarmCallback> Alarms(3);


void setup() {
  Serial.begin(115200);

  Rtc.Begin();
   
  pinMode(PIN_STEP_EN, OUTPUT);
  pinMode(PIN_STEP_DIR, OUTPUT);
  pinMode(PIN_STEP_STEP, OUTPUT);
  
  pinMode(PIN_PIR_SIG, INPUT);

  digitalWrite(PIN_STEP_EN, LOW);
  digitalWrite(PIN_STEP_DIR, LOW);
  digitalWrite(PIN_STEP_STEP, LOW);

  stepper.setMaxSpeed(2000);
  stepper.setAcceleration(1500);
  stepper.setCurrentPosition(0);

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) 
  {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected())
  {
      Serial.println("RTC was write protected, enabling writing now");
      Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  // Sync the Alarms to current time
  Alarms.Sync(now);

  // NOTE:  Due to this sketch not deleting alarms, the returned ids from
    // AddAlarm can be assumed to start at zero and increment from there.
    // Otherwise the ids would need to be captured and used in the callback
    //
    int8_t result;
    // add an alarm to sync time from rtc at a regular interval,
    // due to CPU timing variance, the Alarms time can get off over
    // time, so this alarm will trigger a resync every 20 minutes 
    result = Alarms.AddAlarm(now, 2 * c_MinuteAsSeconds); // every 2 minutes
    if (result < 0) 
    {
        // an error happened
        Serial.print("AddAlarm Sync failed : ");
        Serial.print(result);
    }

    // add a daily alarm at 5:30am
    RtcDateTime working(now.Year(), now.Month(), now.Day(), 5, 30, 0);
    result = Alarms.AddAlarm(working, AlarmPeriod_Daily);
    if (result < 0)
    {
        // an error happened
        Serial.print("AddAlarm Daily failed : ");
        Serial.print(result);
    }

    // add a weekly alarm for Saturday at 7:30am
    working = RtcDateTime(now.Year(), now.Month(), now.Day(), 7, 30, 0);
    working = working.NextDayOfWeek(DayOfWeek_Saturday);
    result = Alarms.AddAlarm(working, AlarmPeriod_Weekly);
    if (result < 0)
    {
        // an error happened
        Serial.print("AddAlarm Weekly failed : ");
        Serial.print(result);
    }

    Serial.println("Running...");
}

void loop() {
  int pir_input = digitalRead(PIN_PIR_SIG);

  // Serial.print("PIR = ");
  // Serial.println(pir_input);

  static int delayMs = 800;

  for(int l = 0; l < 5; l++) {
    for(int i = 0; i < 200; i++) {
      digitalWrite(PIN_STEP_STEP, LOW);
      delayMicroseconds(delayMs);
      digitalWrite(PIN_STEP_STEP, HIGH);
      delayMicroseconds(delayMs);
    }
  }

  // RtcDateTime now = Rtc.GetDateTime();

  // printDateTime(now);
  // Serial.println();

  // if (!now.IsValid())
  // {
  //     // Common Causes:
  //     //    1) the battery on the device is low or even missing and the power line was disconnected
  //     Serial.println("RTC lost confidence in the DateTime!");
  // }

  delay(100); // 100 ms
  Alarms.ProcessAlarms();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[26];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

void alarmCallback(uint8_t id, [[maybe_unused]] const RtcDateTime& alarm)
{
    Serial.println("Alarm triggered!");
    // NOTE:  Due to this sketch not deleting alarms, the returned ids from
    // AddAlarm can be assumed to start at zero and increment from there.
    // Otherwise the ids would need to be captured and used here 
    //
    switch (id)
    {
    case 0:
        {   
            // periodic sync from trusted source to minimize
            // drift due to inaccurate CPU timing
            RtcDateTime now = Rtc.GetDateTime();
            Alarms.Sync(now); 
        }
        Serial.println("INTERVAL ALARM: Alaram time synced!");
        break;

    case 1:
        Serial.println("DAILY ALARM: Its 5:30am!");
        break;

    case 2:
        Serial.println("WEEKLY ALARM: Its Saturday at 7:30am!");
        break;
    }
}
