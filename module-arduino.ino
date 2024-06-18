#include "RtcAlarmManager.h"
#include <AccelStepper.h>
#include <RtcDS1302.h>
#include <Wire.h>

#define MAX_ALARMS 10

int PIN_TIME_CLK = 11;
int PIN_TIME_DAT = 12;
int PIN_TIME_RST = 13;

int PIN_STEP_DIR = 8;
int PIN_STEP_STEP = 9;
int PIN_STEP_EN = 10;

int PIN_PIR_SIG = 7;

int alarm_last_idx = 0;

ThreeWire ds1302(PIN_TIME_DAT, PIN_TIME_CLK, PIN_TIME_RST); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(ds1302);
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP_STEP, PIN_STEP_DIR);

// foreward declare our alarm manager callback
void alarmCallback(uint8_t id, const RtcDateTime &alarm);
// global instance of the manager with three possible alarms
RtcAlarmManager<alarmCallback> Alarms(MAX_ALARMS);

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

  Rtc.SetIsWriteProtected(false);
  Rtc.SetIsRunning(true);

  Serial.println("Running...");
}

void clearAlarm() {
  Serial.println("Resetting alarms");

  for (int i = 0; i < MAX_ALARMS; i++) {
    Alarms.RemoveAlarm(i);
  }

  RtcDateTime now = Rtc.GetDateTime();

  // Sync the Alarms to current time
  Alarms.Sync(now);

  int result = Alarms.AddAlarm(now, 120 * c_MinuteAsSeconds); // every 2 minutes
  if (result < 0) {
    // an error happened
    Serial.print("AddAlarm Sync failed : ");
    Serial.print(result);
  }

  Serial.println("Resetting alarm done");
}

void updateNow(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour,
               uint8_t minute, uint8_t second) {
  RtcDateTime target(year, month, dayOfMonth, hour, minute, second);
  Rtc.SetDateTime(target);

  Serial.print("Time configured @ (");
  printDateTime(target);
  Serial.println(")");
}

void setAlarm(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour,
              uint8_t minute, uint8_t second) {
  if (alarm_last_idx + 1 >= MAX_ALARMS) {
    Serial.println("Max Alarm size exceeded!");
    return;
  }
  RtcDateTime target(year, month, dayOfMonth, hour, minute, second);

  int result = Alarms.AddAlarm(target, AlarmPeriod_Daily);
  if (result < 0) {
    // an error happened
    Serial.print("AddAlarm Daily failed : ");
    Serial.print(result);
    return;
  }

  alarm_last_idx = result;
  Serial.print("New alarm set (");
  printDateTime(target);
  Serial.print(") = ");
  Serial.print(alarm_last_idx);
  Serial.println();
}

void loop() {
  delay(5000);

  updateNow(2024, 6, 18, 21, 40, 0);

  clearAlarm();

  // InitWithDateTimeFormatString<RtcLocaleEnUs>(F("MMM DD YYYY"), date);
  // InitWithDateTimeFormatString<RtcLocaleEnUs>(F("hh:mm:ss"), time);

  setAlarm(2024, 6, 18, 21, 40, 5);

  while (1) {
    delay(1000); // 1000 ms

    RtcDateTime now = Rtc.GetDateTime();
    Alarms.Sync(now);
    Serial.print("Now = ");
    printDateTime(now);
    Serial.print(" ");
    Serial.print(now.Ntp32Time());
    Serial.println();

    Alarms.ProcessAlarms();

    Serial.println(Alarms._seconds);
    for (int i = 0; i < MAX_ALARMS; i++) {
      Serial.println(Alarms._alarms[i].When);
    }
  }
  /**
  TODO

  - Serial read form tx/rx
  - manage alarms
  - manage pir
  - manage time

  - define use cases
  - create actuator functions
   */

  int pir_input = digitalRead(PIN_PIR_SIG);

  // Serial.print("PIR = ");
  // Serial.println(pir_input);

  static int delayMs = 800;

  for (int l = 0; l < 5; l++) {
    for (int i = 0; i < 200; i++) {
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
  //     //    1) the battery on the device is low or even missing and the power
  //     line was disconnected Serial.println("RTC lost confidence in the
  //     DateTime!");
  // }

  delay(100); // 100 ms
  Alarms.ProcessAlarms();

  Alarms.RemoveAlarm(4);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void alarmCallback(uint8_t id, [[maybe_unused]] const RtcDateTime &alarm) {
  Serial.print("Alarm triggered! =");
  Serial.println(id);
  // NOTE:  Due to this sketch not deleting alarms, the returned ids from
  // AddAlarm can be assumed to start at zero and increment from there.
  // Otherwise the ids would need to be captured and used here
  //
  switch (id) {
  case 0: {
    // periodic sync from trusted source to minimize
    // drift due to inaccurate CPU timing
    RtcDateTime now = Rtc.GetDateTime();
    Alarms.Sync(now);
    printDateTime(now);
  }
    Serial.println(" @ INTERVAL ALARM: Alaram time synced!");
    break;

  default:
    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
    Serial.println(" @ Alarm triggered!, Give food!!");
  }
}

void printDateTime(const RtcDateTime &dt) {
  char datestring[26];

  snprintf_P(datestring, countof(datestring),
             PSTR("%04u-%02u-%02u %02u:%02u:%02u"), dt.Year(), dt.Month(),
             dt.Day(), dt.Hour(), dt.Minute(), dt.Second());
  Serial.print(datestring);
}
