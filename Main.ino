#define BLYNK_TEMPLATE_ID "TMPL63V-8xEXW"
#define BLYNK_TEMPLATE_NAME "Air quality monitoring system"
#define BLYNK_AUTH_TOKEN "6te0klaMI9TriGZgOypyl1f2RDFECo5g"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "SGP30.h"
#include "ADS1X15.h"
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <GP2YDustSensor.h>
#include <MQUnifiedsensor.h>

// Nhập SSID và password của wifi
char ssid[] = "GalaxyM34";
char pass[] = "dangkhoa";
// char ssid[] = "PTIT.HCM_SV";
// char pass[] = "";
BlynkTimer timer;

#define         Board                   ("ESP8266")
#define         TypeMQ2                    ("MQ-2") //MQ2
#define         TypeMQ7                    ("MQ-7") //MQ2

#define         RatioMQ2CleanAir        (9.83) //RS / R0 = 9.83 ppm
#define         RatioMQ7CleanAir        (27.5) //RS / R0 = 27.5 ppm

MQUnifiedsensor MQ2(Board, TypeMQ2);
MQUnifiedsensor MQ7(Board, TypeMQ7);

const uint8_t SHARP_LED_PIN = 13;   // Sharp Dust/particle sensor Led Pin
const uint8_t SHARP_VO_PIN = A0;    // Sharp Dust/particle analog out pin used for reading 
GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1014AU0F, SHARP_LED_PIN, SHARP_VO_PIN);

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27,16,2);  
ADS1115 ADS(0x48);
SGP30 SGP;

#define DHTPIN 14 // Chân D5
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER 12 //D6
#define BUTTON 3 //RX

int16_t val_0;
int16_t val_1;
float TVOCval;
int16_t eCO2val;
int16_t PM25valNow;
int16_t PM25valAvg;
float h;
float t;
// bool warning = false;
int count = 0;

const int CO2Threshold = 5000;
const int COThreshold = 100;
const int LPGThreshold = 1000;

void sendSensor()
{
  // Chúng ta có thể gửi bất kỳ giá trị tại bất kỳ thời điểm nào
  MQ7.externalADCUpdate(ADS.readADC(0)*ADS.toVoltage(1));
  MQ2.externalADCUpdate(ADS.readADC(1)*ADS.toVoltage(1));
  // Không nên gửi nhiều hơn 10 giá trị mỗi giây
  Blynk.virtualWrite(V0, dustSensor.getRunningAverage());
  Blynk.virtualWrite(V1, MQ7.readSensor());
  Blynk.virtualWrite(V2, MQ2.readSensor());
  Blynk.virtualWrite(V3, SGP.getCO2());
  Blynk.virtualWrite(V4, dht.readTemperature());
}

void measureCO_LPG()
{
  ADS.setGain(0);

  val_0 = ADS.readADC(0);  
  val_1 = ADS.readADC(1);  
  float f = ADS.toVoltage(1);  //  voltage factor

  Serial.print("\tAnalog0: "); Serial.print(val_0); Serial.print('\t'); Serial.println(val_0 * f, 3);
  Serial.print("\tAnalog1: "); Serial.print(val_1); Serial.print('\t'); Serial.println(val_1 * f, 3);

  MQ2.externalADCUpdate(val_1*f);
  MQ7.externalADCUpdate(val_0*f);
  delay(20);

  lcd.setCursor(0,0);
  lcd.print("CO: ");
  lcd.print(MQ7.readSensor());
  lcd.print(" ppm");
  lcd.setCursor(0,1);
  lcd.print("LPG: ");
  lcd.print(MQ2.readSensor());
  lcd.print(" ppm");
  Serial.println();
  delay(20);
}
void measureTVOC_CO2()
{
  SGP.measure(true);      //  returns false if no measurement is made
  TVOCval = SGP.getTVOC();
  eCO2val = SGP.getCO2();
  Serial.print(TVOCval);
  Serial.print("\t");
  Serial.print(eCO2val);
  Serial.print("\t");
  delay(20);
  lcd.setCursor(0,0);
  lcd.print("TVOC: ");
  lcd.print(TVOCval);
  lcd.print(" ppb");
  lcd.setCursor(0,1);
  lcd.print("eCO2: ");
  lcd.print(eCO2val);
  lcd.print(" ppm");
  Serial.println();
  delay(20);
}
void measurePM25()
{
  PM25valNow = dustSensor.getDustDensity();
  PM25valAvg = dustSensor.getRunningAverage();
  Serial.print("Dust density: ");
  Serial.print(PM25valNow);
  Serial.print(" ug/m3; Running average: ");
  Serial.print(PM25valAvg);
  Serial.println(" ug/m3");
  lcd.setCursor(0,0);
  lcd.print("PM2.5_Now: ");
  lcd.print(PM25valNow);
  lcd.setCursor(0,1);
  lcd.print("PM2.5_Avg: ");
  lcd.print(PM25valAvg);
  delay(20);
}
void measureTemp_Humi()
{
  Serial.print(F("Humidity: "));
  Serial.print(dht.readHumidity());
  Serial.print(F("%  Temperature: "));
  Serial.print(dht.readTemperature());
  Serial.print(F("°C "));
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(dht.readTemperature());
  lcd.print(" ");
  lcd.print(char(223));
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Humi: ");
  lcd.print(dht.readHumidity());
  lcd.print(" %");
  delay(20);
}
void warning()
{
  ADS.setGain(0);
  MQ7.externalADCUpdate(ADS.readADC(0)*ADS.toVoltage(1));
  MQ2.externalADCUpdate(ADS.readADC(1)*ADS.toVoltage(1));
  SGP.measure(true);
  delay(20);
  
  if(MQ2.readSensor() >= LPGThreshold || MQ7.readSensor() >= COThreshold || SGP.getCO2() >= CO2Threshold)
  {
    digitalWrite(BUZZER, HIGH);
    lcd.clear();
    lcd.print("Nong do khi vuot nguong cho phep!");
  }
  else
  {
    digitalWrite(BUZZER, LOW);
  }
}

void IntCallback()
{
    lcd.clear();
    count += 1;
    if(count == 5)
    {
      count = 0;
    }
}
void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  delay(10);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Wire.begin();
  SGP.begin();
  SGP.GenericReset();
  ADS.begin();
  dustSensor.begin();
  dht.begin();

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), IntCallback, FALLING);
  
  // MQ2 and MQ7 calibration
  MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ2.setA(574.25); MQ2.setB(-2.222); // Configure the equation to to calculate LPG concentration

  MQ7.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ7.setA(99.042); MQ7.setB(-1.518); // Configure the equation to calculate CO concentration value

  MQ2.init();
  MQ7.init(); 

  Serial.print("Calibrating please wait.");
  float calcR0_MQ2 = 0;
  float calcR0_MQ7 = 0;
  for(int i = 1; i<=10; i ++)
  {
    ADS.setGain(0);
    MQ2.externalADCUpdate(ADS.readADC(1)*ADS.toVoltage(1)); // Update data, the arduino will read the voltage from the analog pin
    MQ7.externalADCUpdate(ADS.readADC(0)*ADS.toVoltage(1)); // Update data
    Serial.println(ADS.readADC(0)*ADS.toVoltage(1));
    Serial.println(ADS.readADC(1)*ADS.toVoltage(1));
    calcR0_MQ2 += MQ2.calibrate(RatioMQ2CleanAir);
    calcR0_MQ7 += MQ7.calibrate(RatioMQ7CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0_MQ2/10);
  MQ7.setR0(calcR0_MQ7/10);

  Serial.println("  done!.");

  MQ2.serialDebug(true);
  MQ7.serialDebug(true);

  // initialize the lcd 
  lcd.init();                      
  lcd.backlight();
  lcd.setCursor(2, 0);
  lcd.print("Air Quality");
  lcd.setCursor(5, 1);
  lcd.print("NHOM 4");
  delay(3000);
  lcd.clear();

  timer.setInterval(500L, sendSensor);
}

void loop() {
  
  // put your main code here, to run repeatedly:
  Blynk.run();
  timer.run();
  delay(10);

  switch(count)
  {
    case 1:
    {
      // Đo và hiển thị giá trị MQ2 và MQ7
      measureCO_LPG();
      warning();
      break;
    }
    case 2:
    {
      // Đo và hiển thị giá trị TVOC và eCO2
      measureTVOC_CO2();
      warning();
      break;
    }
    case 3:
    {
       // Đo và hiển thị giá trị bụi mịn PM2.5
      measurePM25();
      warning();
      break;
    }
    case 4:
    {
      // Đo và hiển thị giá trị nhiệt độ, độ ẩm
      measureTemp_Humi();
      warning();
      break;
    }
    default:
    {
      warning();
      String message = "Air Quality Monitor      ";
      lcd.setCursor(5, 1);
      lcd.print("NHOM 4");

      // Lặp qua từng ký tự của chuỗi
      for (int i = 0; i <= message.length() - 16; i++) {
        // Hiển thị phần của chuỗi bắt đầu từ vị trí i trên màn hình LCD
        lcd.setCursor(0, 0);
        lcd.print(message.substring(i, i + 16));
        delay(200);
      }
      break;
    }
  }  
}



