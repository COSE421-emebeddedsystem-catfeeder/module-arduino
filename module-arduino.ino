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

struct Config {
  int USE_PIR;
  int FOOD_AMT;
};

Config config  = {false, 5} ;

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

void updateNow(char *time) {
  RtcDateTime target;
  target.InitWithDateTimeFormatString(F("YYYY-MM-DDThh:mm:ssz"), time);

  Rtc.SetDateTime(target);
  Alarms.Sync(target);

  Serial.print("Time configured @ (");
  printDateTime(target);
  Serial.println(")");
}

void setAlarm(char *time) {
  if (alarm_last_idx + 1 >= MAX_ALARMS) {
    Serial.println("Max Alarm size exceeded!");
    return;
  }

  RtcDateTime target;
  target.InitWithDateTimeFormatString(F("YYYY-MM-DDThh:mm:ssz"), time);

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

void giveFood() {
  Serial.println("Motor started");
  static int delayMs = 800;

  for (int l = 0; l < config.FOOD_AMT; l++) {
    for (int i = 0; i < 200; i++) {
      digitalWrite(PIN_STEP_STEP, LOW);
      delayMicroseconds(delayMs);
      digitalWrite(PIN_STEP_STEP, HIGH);
      delayMicroseconds(delayMs);
    }
  }
  Serial.println("Motor stopped");
}

String CMD_STARTS(">>>");

void processCmd() {
  String cmd;
  String operand;
  char *cmd_buf;

  do {
    cmd = Serial.readStringUntil('\n');

    if (cmd.startsWith(CMD_STARTS)) {
      cmd_buf = cmd.c_str();

      char cmd_code = cmd_buf[3];
      operand = cmd.substring(5);

      Serial.print("CMD = '");
      Serial.print(cmd_code);
      Serial.print("', '");
      Serial.print(operand);
      Serial.println("'");

      switch (cmd_code) {
      case '?':
        Serial.println("Cmd Help\n"
                       "?                        > Print help message\n"
                       "0                        > Clear config / alarms\n"
                       "1:{0|1}                  > Configure PIR enable\n"
                       "2:{YYYY-MM-DDThh:mm:ssz} > Set time\n"
                       "3:{YYYY-MM-DDThh:mm:ssz} > Set alarm\n"
                       "4                        > Give food\n"
                       "5                        > Show stat\n"
                       "6:{int}                  > Set food amount \n");
        break;
      case '0':
        config.USE_PIR = false;
        clearAlarm();
        break;
      case '1':
        config.USE_PIR = operand.equals("1");
        break;
      case '2':
        updateNow(operand.c_str());
        break;
      case '3':
        setAlarm(operand.c_str());
        break;
      case '4':
        giveFood();
        break;
      case '5':
        static char buffer[256];

        RtcDateTime dt = Rtc.GetDateTime();
        snprintf_P(buffer, countof(buffer),
                   PSTR("| Clock    = %04u-%02u-%02u %02u:%02u:%02u\n"
                        "| PIR      = %d\n"
                        "| Alarms   = %d\n"
                        "| FOOD_AMT = %d\n"
                        ),
                   dt.Year(), dt.Month(), dt.Day(), dt.Hour(), dt.Minute(),
                   dt.Second(), config.USE_PIR, alarm_last_idx, config.FOOD_AMT);

        Serial.println(buffer);
        break;
        case '6':
        config.FOOD_AMT = operand.toInt();
        break;
      }
    }
  } while (cmd.length() != 0);
}

void loop() {
  Alarms.ProcessAlarms();

  delay(1000);

  if (digitalRead(PIN_PIR_SIG)) {
    if (config.USE_PIR) {
      Serial.println("PIR Detected. Give food!");
      giveFood();
    } else {
      Serial.println("PIR Detected. configured not to give food..");
    }

    delay(5000);
  }

  processCmd();
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
    giveFood();
  }
}

void printDateTime(const RtcDateTime &dt) {
  char datestring[26];

  snprintf_P(datestring, countof(datestring),
             PSTR("%04u-%02u-%02u %02u:%02u:%02u"), dt.Year(), dt.Month(),
             dt.Day(), dt.Hour(), dt.Minute(), dt.Second());
  Serial.print(datestring);
}
