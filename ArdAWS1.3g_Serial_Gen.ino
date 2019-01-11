/*  Aduino based automatic weather station.  V1.3g (Nov 2016)

  Manager:    John Kai-wing CHAN, Scientific Officer, Hong Kong Observatory
  Hardware:   , Research Officer, Hong Kong Observatory

  Sensors and Modules:
  Temperature   DHT22
  Humidity      DHT22
  Pressure      BMP280
  Clock         Real time clock DS1307

  Exeternal     Tipping bucket rainguage with reed switch

  Modules:
  Display       20x4 LCD Display
  Interface     SLD01099P Base Shield V2 (SeeedStudio.com)

  Microprocessor:
  Arduino UNO SMD v3

  revision:

*/

// Headers and Libraries
#include <SoftwareSerial.h>
#include <Wire.h>     //for Arduino communications with in/out pins

#include <LiquidCrystal_I2C.h>
#include <RTClib.h>   //for real time clock module
#include <DHT.h>      //for Temperature and Humidity Sensor
#include <BMP280.h>   //for BMP280 pressure sensor module


// Definitions if any
#define DHTPIN A0   //Define the input pin of the DHT22 temperature and humidity sensor
#define DHTTYPE DHT22 //We are using the AM2302 unit int the SEN51035P sensor
#define BACKLIGHT_PIN 3  //Define the backlit pin for the LCD display

#define SAMPLES_PER_MINUTE  12 // 5 second samplign period or 0.2Hz sampling rate
// note sample per minute must be a factor of 60

#define RFPin 2 //Rainfall interrupt pin
#define RFResolution 0.1 //tipping bucket rainguage resolution in mm

#define UID 6998 // this i sthe station ID.  This is different for each module


struct MetData {
  float Temperature;
  float RelHumidity;
  float Pressure;
  float MSLP;
  int Rainfall;
};



DHT dht(DHTPIN, DHTTYPE);  //declare dht as the temperature and humidity object
BMP280 bmp280;//Barometer barometer;  //declare a barometer object

RTC_DS1307 rtc;  //declare a clock object

LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // Set the LCD I2C address
//LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7,3,POSITIVE);

SoftwareSerial mySerial(5, 6); //output port
DateTime now; //current time

//Weather and station elements
MetData minuteData, currentData[SAMPLES_PER_MINUTE];
byte minuteRF[60]; //minute RF - used to estimate the hourly rf
float hourRF = 0.0;

//StnInfo stationInfo;

int nPeriod = (60 / SAMPLES_PER_MINUTE); //sampling period in seconds

// Global Variables
int arrayIndex; // array pointer index
float temperatureOffset = 0.0; //Temperature offset/adjustment
float rhOffset = 0.0;//RH offset/adjustment
float pressureOffset = 0.0;//pressure offset/adjustment
bool displayLit = false;


void setup() {
  //Initialise variables
  byte index;

  for ( index = 0; index < SAMPLES_PER_MINUTE; index ++) //clears the current data array
    currentData[index] = {0.0, 0.0, 0.0, 0.0, 0.0};  //current data record: T, RH, P, MSLP, RF respectively
  minuteData = {0.0, 0.0, 0.0, 0.0, 0.0};  //minute average data record
  for ( index = 0; index < 60; index ++) //clears the minute RF data array
    minuteRF[index] = 0.0;

  //attach interrupt for the raingauge
  pinMode(2, INPUT); //Rain gauge trip interrupt Pin 2 of the Arduino board
  attachInterrupt(digitalPinToInterrupt(2), RFTrigger, RISING);  //Routine to call for RF interrupt

  //initialise the serial ports
  Serial.begin(9600);
  mySerial.begin(2400);  //Data output port
  Serial.println("Initialising");
  //Initialise modules
  initDisplay();//Initialise the display module
  initRTC(); //initialise the Real Time Clock
  initTempRH();//initiaise the DHT22 module
  initPressure();//initialise th pressure modul
  initRF();//Initialise the RF data logging capability

  //  displayClock();
  do {
    delay(200);
    now = rtc.now();
  } while (now.second() % nPeriod != 0);

  Serial.println(rtc.now().second());
}


void loop() {//main loop
  char timeString[20];
  char dateString[20];
  String _dataString;
  String _timeString;
  int _seconds;

  //get the current time and show on the display
  now = rtc.now();
  sprintf(timeString, "%02d:%02d:%02d %02d/%02d/%04d ", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
  lcd.setCursor(0, 0);
  lcd.print(timeString);
  _seconds = now.second();

  //At sampling period, read sensors
  if ((_seconds % nPeriod) == 0) {// sample period up
    arrayIndex = (int) (_seconds / nPeriod);
    Serial.println(arrayIndex);
    //lcd.clear();
    for (byte _row = 1; _row < 4; _row++) {
      lcd.setCursor(0, _row); lcd.print("                    ");
    }
    //lcd.print(timeString);
    readTempRH();
    readPressure();
    logRainfall();
  }

  //At the minute, i.e. zero seconds on the clock, send data
  if (_seconds == 0) {
    Serial.println("Sending Data!");
    sprintf(timeString, "%02d%02d", now.hour(), now.minute()); // reformat the timestring to conform with Co-WIN
    sprintf(dateString, "%04d%02d%02d", now.year(), now.month(), now.day()); // reformat the timestring to conform with Co-WIN

    //now compile the datastring to be sent to the serial port mySerial
    _dataString = "BEGIN%" + String(UID);
    _dataString += "&TS=" + String(timeString)+ String(dateString);
    //_dataString += "&DT=" + String(dateString);
    _dataString += "&BA=" + String(minuteData.Pressure, 1);
    _dataString += "&TP=" + String(minuteData.Temperature, 1);
    _dataString += "&RH=" + String(minuteData.RelHumidity, 1);
    _dataString += "&RR=" + String(hourRF, 1);
    _dataString += "%END";
    _dataString += getXORChecksum(&_dataString);
    Serial.println(_dataString);
    //Serial.print("XOR Checksum byte: "); Serial.println(getXORChecksum(&_dataString),HEX);
    mySerial.println(_dataString);
    minuteData.Rainfall = 0;
    minuteRF[now.minute()] = 0;//reset the minuteRF bin
    //sendData();
  }

  if (mySerial.available()){//data is in the input buffer read the line
    delay(100);
    _dataString = "";
    char _inchar;
    while (mySerial.available()){
         _inchar= mySerial.read();
         _dataString += _inchar;
    }
    Serial.println(_dataString);
    if (_dataString.substring(0,4) == String(UID) && _dataString.endsWith("END")){
      Serial.println("UID Command: Decoding");
      Serial.println("Command: " + _dataString.substring(_dataString.indexOf("$")+1, _dataString.indexOf("+")));
      byte _start = _dataString.indexOf("+")+1;
      byte _end = _dataString.indexOf("$END",_start);
      _timeString = _dataString.substring(_start,_end);
      
      int _year, _month, _day, _hour, _minute, _seconds;

      _year = _timeString.substring(0,4).toInt();
      _month = _timeString.substring(4,6).toInt();
      _day = _timeString.substring(6,8).toInt();
      _hour = _timeString.substring(8,10).toInt();
      _minute = _timeString.substring(10,12).toInt();
      _seconds = _timeString.substring(12,14).toInt();
      rtc.adjust(DateTime(_year, _month, _day, _hour, _minute, _seconds));
     }
  }
  
  //wait till the next second
  delay(1000 - (millis() % 1000));
}


char getXORChecksum(String* _inString){
  byte _checkByte = 0x00;
  char _inChar;

  int stringLength = _inString->length();//strLen(_inString);

  for (byte _index = 0; _index < stringLength ; _index ++){
    _inChar = _inString->charAt(_index);
    _checkByte ^= _inChar;
  }
  Serial.println(_checkByte,HEX);
  return _checkByte;
}

//Display routines
void initDisplay() { //initialise the display used
  lcd.setBacklightPin(BACKLIGHT_PIN, POSITIVE); //Set the backlit pin
  lcd.setBacklight(HIGH);
  lcd.begin(20, 4);              // initialize the lcd
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("INITIALISATION");
  delay(1000);
}


void initRTC() {
  lcd.setCursor(0, 1);
  lcd.print("RTC > ");
  if (!rtc.begin())
    lcd.print("ERROR");
  else
    lcd.print("INITIALISED");
  delay(1000);
}

void getCurrentTime() {
  int _arrayIndex;

  now = rtc.now(); //get the current time
  if ((now.second() % nPeriod) == 0) {
    arrayIndex = (int) (now.second() / nPeriod);

  }

}


//Temperature and RH routines
void initTempRH() {//initialise the temperature and RH module
  lcd.setCursor(0, 2);
  lcd.print("T/RH> ");
  dht.begin();
  lcd.print("INITIALISED");
  for (byte index = 0; index < SAMPLES_PER_MINUTE; index ++) {
    currentData[index].Temperature = 99.9;
    currentData[index].RelHumidity = -1.0;
  }
  delay(1000);//pause for 1 second
}

void readTempRH() {
  float h = dht.readHumidity();  // Read Relative Humidity
  float t = dht.readTemperature();  // Read temperature as Celsius (the default)

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    //Serial.println("Failed to read from DHT sensor!");
    //return;
    currentData[arrayIndex].Temperature = 99.9;
    currentData[arrayIndex].RelHumidity = -1.0;
  }
  else
  {
    currentData[arrayIndex].Temperature = t;
    currentData[arrayIndex].RelHumidity = h;
  }

  lcd.setCursor(0, 1);
  lcd.print(t, 1);
  lcd.print("C");
  lcd.setCursor(6, 1);
  lcd.print(int(h));
  lcd.print("% ");

  byte _count, index;
  minuteData.Temperature = 0.0;
  minuteData.RelHumidity = 0.0;

  _count = 0;
  for (index = 0; index < SAMPLES_PER_MINUTE ; index ++) {
    if (currentData[index].Temperature != 99.9) {
      minuteData.Temperature += currentData[index].Temperature;
      _count ++;
    }
  }
  minuteData.Temperature /= _count;


  _count = 0;
  for (index = 0; index < SAMPLES_PER_MINUTE ; index ++) {
    if (currentData[index].RelHumidity != -1.0) {
      minuteData.RelHumidity += currentData[index].RelHumidity;
      _count++;
    }
  }
  minuteData.RelHumidity /= _count;

  lcd.setCursor(0, 2);
  lcd.print(minuteData.Temperature, 1);
  lcd.print("C");
  lcd.setCursor(6, 2);
  lcd.print(int(minuteData.RelHumidity));
  lcd.print("%");
}



//Air pressure routines
void initPressure() {
  lcd.setCursor(0, 3);
  lcd.print("Baro> ");
  if (!bmp280.init())
    lcd.print("ERROR");
  else
    lcd.print("INITIALISED");
  delay(1000);
}


void readPressure() {
  //  float pressure;

  //get and print temperatures
  float t = bmp280.getTemperature();
  float p =  bmp280.getPressure() / 100.0;
  currentData[arrayIndex].Pressure = p;

  lcd.setCursor(10, 1);
  lcd.print(p, 1);
  lcd.print("hPa");

  minuteData.Pressure = 0.0;
  byte _count = 0;
  for (byte index = 0; index <  SAMPLES_PER_MINUTE ; index ++) {
    if (currentData[index].Pressure > 0) {
      minuteData.Pressure += currentData[index].Pressure;
      _count++;
    }
  }
  minuteData.Pressure /= _count;

  lcd.setCursor(10, 2);
  lcd.print(minuteData.Pressure, 1);
  lcd.print("hPa");
}


void initRF() {

  attachInterrupt(digitalPinToInterrupt(RFPin), RFTrigger, RISING); //rain guage trip interrupt pin 2 of the arduino board
  minuteData.Rainfall = 0;
  hourRF = 0.0;

  for (byte _count = 0; _count < SAMPLES_PER_MINUTE; _count++)
    currentData[_count].Rainfall = 0;
}


void logRainfall() {
  byte _index;

  //calculate the past 60 seconds rf
  minuteData.Rainfall = 0;
  for (_index = 0; _index <  SAMPLES_PER_MINUTE ; _index ++) {
    minuteData.Rainfall += currentData[_index].Rainfall;
    if (arrayIndex == _index) Serial.print(">");
    Serial.print(currentData[_index].Rainfall);
    if (arrayIndex == _index) Serial.print("<");
    Serial.print("\t");
  } // this gets the past minute rainfall
  Serial.println(minuteData.Rainfall * RFResolution, 1);

  for (_index = 0; _index <  60 ; _index ++) {
    if (now.minute() == _index) Serial.print(">");
    Serial.print(minuteRF[_index]);
    if (now.minute() == _index) Serial.print("<");
    if ((_index + 1) % 10 == 0)
      Serial.print("\t");
  }
  Serial.println();
  // this gets the past minute rainfall


  //Serial.print("Past minute RF: ");Serial.println(minuteData.Rainfall);
  lcd.setCursor(0, 3);
  lcd.print("RF: ");
  lcd.print(minuteData.Rainfall * RFResolution, 1);
  lcd.print("mm");

  // calculat ethe past hour RF
  //minuteRF[now.minute()] = minuteData.Rainfall;
  hourRF = 0.0;
  for (_index = 0; _index <  60 ; _index ++)
    hourRF += minuteRF[_index] * RFResolution;
  lcd.print(" / ");
  lcd.print(hourRF, 1);
  lcd.print("mm");
  currentData[arrayIndex].Rainfall = 0;//reset the current array bin
}


// RF Interrupt routines
void RFTrigger() {
  currentData[arrayIndex].Rainfall ++;  //adds 1 tip to the 5 second rainfall tipcount

  minuteRF[now.minute()] ++;// add one tip to the counter bin
  minuteRF[now.minute()] %= 200; //to avoid overflow Max is 200 tips in one minute
  
//  Serial.print("RF ");
//  Serial.println(currentData[arrayIndex].Rainfall);

}



