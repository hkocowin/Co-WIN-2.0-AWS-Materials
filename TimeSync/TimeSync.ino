#include "Wire.h"
#include <stdio.h>
#include <string.h>

#define DS3231_I2C_ADDRESS 0x68

byte year, month, day, hour, minute;
byte second;
int state = LOW;
int const sentenceSize = 20;
int sentenceIndex = 1;
char sentence[sentenceSize];

byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ( (val / 16 * 10) + (val % 16) );
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  writeOnAddress(2, 3);// set day of week manually (1=Sunday, 7=Saturday)
}

void loop() {
  synchTime();
}

void synchTime(void) {
  if (Serial.available()) {
    char ch = Serial.read();
    if (sentenceIndex <= sentenceSize && ch != '\n' && ch != '\r' && ch != ',') {
      sentence [sentenceIndex] = ch;
      sentenceIndex++;
    } else {
      sentence[sentenceIndex] = '\0';
      applySentence(sentence, sentenceIndex - 1);
      sentenceIndex = 1;
    }
  }
}

void applySentence (char* sent, int leng) {
  switch (sent[1]) {
    case 83:    //S = second
      {
        char secondString[3];
        strcpy(secondString, &sent[2]);
        second = atoi(secondString);
        writeOnAddress(second , 0x00);
        break;
      }

    case 68:    //D = Minute  (Daghigheh in Persian)
      {
        char minuteString[3];
        strcpy(minuteString, &sent[2]);
        minute = atoi(minuteString);
        writeOnAddress(minute , 0x01);
        break;
      }
    case 72:    //H = Hour
      {
        char hourString[3];
        strcpy(hourString, &sent[2]);
        hour = atoi(hourString);
        writeOnAddress(hour , 0x02);
        break;
      }


    case 84:   //T = Day Of Month (Tag in German)
      {
        char dayString[3];
        strcpy(dayString, &sent[2]);
        day = atoi(dayString);
        writeOnAddress(day , 0x04);
        break;
      }

    case 77:  /// M = Month
      {
        char monthString[3];
        strcpy(monthString, &sent[2]);
        month = atoi(monthString);
        writeOnAddress(month , 0x05);
        break;
      }

    case 74:   /// J = Year (Jahr in German)
      {
        char yearString[3];
        strcpy(yearString, &sent[4]);
        year = atoi(yearString);
        writeOnAddress(year , 0x06);
        toggleLED();
        break;
      }

    case 66:  ///B Write Time on Serial: You should Write "B," on serial to get time
      displayTime();
      break;
  }
}

void writeOnAddress(byte value, int address)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(address);
  Wire.write(decToBcd(value));
  Wire.endTransmission();
}

void readDS3231time(byte *second,
                    byte *minute,
                    byte *hour,
                    byte *dayOfWeek,
                    byte *dayOfMonth,
                    byte *month,
                    byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}


void displayTime()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  // send it to the serial monitor
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute < 10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second < 10)
  {
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch (dayOfWeek) {
    case 1:
      Serial.println("Sunday");
      break;
    case 2:
      Serial.println("Monday");
      break;
    case 3:
      Serial.println("Tuesday");
      break;
    case 4:
      Serial.println("Wednesday");
      break;
    case 5:
      Serial.println("Thursday");
      break;
    case 6:
      Serial.println("Friday");
      break;
    case 7:
      Serial.println("Saturday");
      break;
  }
}

void toggleLED () {
  if (state == LOW) {
    state = HIGH;
  } else {
    state = LOW;
  }
  digitalWrite(13, state);
}
