#include <RtcDS1302.h>

int PIN_TIME_RST = 13;
int PIN_TIME_DAT = 12;
int PIN_TIME_CLK = 11;

int PIN_STEP_EN = 10;
int PIN_STEP_DIR = 8;
int PIN_STEP_STEP = 9;

ThreeWire ds1302(PIN_TIME_DAT, PIN_TIME_CLK, PIN_TIME_RST); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(ds1302);

void setup() {
  Serial.begin(115200);

  Rtc.Begin();
   
  pinMode(PIN_STEP_EN, OUTPUT);
  pinMode(PIN_STEP_DIR, OUTPUT);
  pinMode(PIN_STEP_STEP, OUTPUT);

  digitalWrite(PIN_STEP_EN, LOW);
  digitalWrite(PIN_STEP_DIR, LOW);
  digitalWrite(PIN_STEP_STEP, LOW);

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
}

void loop() {
  static int delayMs = 800;

  Serial.println(delayMs);

  for(int l = 0; l < 5; l++) {
    for(int i = 0; i < 200; i++) {
      digitalWrite(PIN_STEP_STEP, LOW);
      delayMicroseconds(delayMs);
      digitalWrite(PIN_STEP_STEP, HIGH);
      delayMicroseconds(delayMs);
    }
  }

  RtcDateTime now = Rtc.GetDateTime();

  printDateTime(now);
  Serial.println();

  if (!now.IsValid())
  {
      // Common Causes:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }

  delay(10000); // ten seconds
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
