#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// ระบุสถานะการทำงานของ Sensor
#define SENSOR_DISABLE 0.5
#define SENSOR_PH_DISABLE 0.9
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "5e00e90822444103b8f116bb719b98cb";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "Makjoey69";
char pass[] = "qwertyuipo";
BlynkTimer timer; // Create a Timer object called "timer"!

// ประกาศขา NodeMCU
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13

// ตัวแปรเก็บความเร็วในการ เก็บค่าจาก  Sensor และ ค่าหน่วยเวลาในการแสดงผล Sensor ผ่าน Serial
#define samplingInterval 20
#define printInterval 1000
#define notifyInterval 60000

// ตัวแปรที่ใช้กับ Sensor PH
#define SensorPin A0
#define Offset 0.00
double pHValue;
double voltage;
double minPH = 6.5;
double maxPH = 8.9;
boolean isPHEnable = true;

// ตัวแปรที่ใช้กับ Sensor Temperature
#define ONE_WIRE_BUS D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
double temp;
double avgTemp;
int selectedTemp = 28;
boolean isTempEnable = true;

// ตัวแปรที่ใช้กับ Sensor Ultrasonic
#define trigPin D3
#define echoPin D4
double duration;
double distance;
double avgDistance;

// ตัวแปรที่ใช้ต่อเข้ากับ Relay
#define relay1 D5
#define relay2 D6
#define relay3 D7
#define relay4 D1

// ตัวแปรเก็บค่าเฉลี่ยของแต่ละ Sensor
#define ArrayLenth  40
int pHArray[ArrayLenth];
int pHArrayIndex = 0;
int distanceArray[ArrayLenth];
int distanceArrayIndex = 0;
int tempArray[ArrayLenth];
int tempArrayIndex = 0;

// ตัวแปรที่ใช้กับกระบวนการในการเปลี่ยนนํ้า
int waterChangeCount = 0;
boolean isRelease = false;
boolean isGetPHValue = true;
boolean isPHSensorWorking = false;
boolean isTempSensorWorking = false;
boolean isDistanceSensorWorking = false;
boolean isSend = false;
String state = "";
String fanState = "";
String initialStateText = "Initial State";

// ประกาศตัวแปร LED แสดงสถานะการทำงานของ Sensor ต่างๆ ใน Blynk
WidgetLED ledPH(V8);
WidgetLED ledTemp(V9);
WidgetLED ledDistance(V10);
WidgetLCD lcd(V12);

// ทำการ Sync ข้อมูลเมื่อ Hardware ทำการติดต่อกับ Blynk Server สำเร็จ
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V3, V4, V5, V6, V7);
}

void setup(void)
{
  // ตั้งค่าเริ่มต้นการทำงาน Sensor ต่างๆ
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  sensors.begin();
  Serial.print("Initial state");

  timer.setInterval(1000L, sendUptime); //  Here you set interval (1sec) and which function to call
  // ตั้งค่า Pin แต่ละสถานะการใช้งานแต่ละ Pin
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  // ตั้งค่าเริ่มต้นให้แต่ละ Relay ไม่ทำงาน
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
}
void loop(void)
{
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();


  // สั่งการทำงานให้เก็บข้อมูลจาก Swnsor ต่างๆ ทุก ๆ 20 miliseconds
  if (millis() - samplingTime > samplingInterval)
  {
    // เก็บค่าจาก Sensor PH
    pHArray[pHArrayIndex++] = analogRead(SensorPin);
    if (pHArrayIndex == ArrayLenth)pHArrayIndex = 0;
    voltage = avergearray(pHArray, ArrayLenth) * 3.3 / 1024;
    pHValue = 3.3 * voltage + Offset;

    // เก็บค่าจาก Sensor Ultrasonic
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);

    distance = duration * 0.034 / 2;

    distanceArray[distanceArrayIndex++] = distance;
    if (distanceArrayIndex == ArrayLenth)distanceArrayIndex = 0;
    avgDistance = avergearray(distanceArray, ArrayLenth);

    // เก็บค่าจาก Sensor Temperature
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);

    tempArray[tempArrayIndex++] = temp;
    if (tempArrayIndex == ArrayLenth)tempArrayIndex = 0;
    avgTemp = avergearray(tempArray, ArrayLenth);

    samplingTime = millis();
  }

  // Logic ควบคุมการเปลี่ยนนํ้า โดยให้เริ่มทำงานหลังจากผ่าน Initial State ไป 5 วินาที
  if (millis() >= 5000) {
    // สั่งให้ Blynk ทำงาน
    Blynk.run(); // all the Blynk magic happens here
    timer.run(); // BlynkTimer is working...
    initialStateText = "Initial State";
    if (isPHEnable) {
      if (isPHSensorWorking && isDistanceSensorWorking) {
        if (!(pHValue <= minPH || pHValue >= maxPH) && isGetPHValue) {
          if (avgDistance >= 4) {
            state = "Water: pump in";
            digitalWrite(relay1, HIGH);   // สั่งปิดรีเลย์(หยุดปล่อยน้ำออก)
            digitalWrite(relay2, LOW);   // สั่งเปิดรีเลย์(ปั้มนํ้าเข้า)
            digitalWrite(relay3, HIGH);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
          } else if (avgDistance <= 3) {
            state = "Water: release";
            digitalWrite(relay1, LOW);   // สั่งเปิดรีเลย์(ปล่อยน้ำออก)
            digitalWrite(relay2, HIGH);   // สั่งปิดรีเลย์(หยุดปั้มนํ้าเข้า)
            digitalWrite(relay3, LOW);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
          } else  {
            state = "Water: normal";
            digitalWrite(relay1, HIGH);   // สั่งปิดรีเลย์(หยุดปล่อยน้ำออก)
            digitalWrite(relay2, HIGH);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
            digitalWrite(relay3, HIGH);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
          }
        } else {
          state = "Water: changing";
          isGetPHValue = false; // ปิดการเช็คค่า PH
          if (!(isRelease)) {
            if (!(isSend)) {
              isSend = true;
              Blynk.notify("ดำเนินการเปลี่ยนนํ้า"); // ส่ง Push Notification ไป App
            }
            // สั่งเปิดรีเลย์(ปล่อยน้ำออก)
            digitalWrite(relay1, LOW);
            digitalWrite(relay3, LOW);

            if (avgDistance >= 11) {
              isRelease = true;
              Blynk.notify("ดำเนินการปล่อยนํ้าออกสำเร็จ"); // ส่ง Push Notification ไป App
              digitalWrite(relay1, HIGH);   // สั่งปิดรีเลย์(หยุดปล่อยน้ำออก)
              digitalWrite(relay2, LOW);   // สั่งเปิดรีเลย์(ปั้มนํ้าเข้า)
              digitalWrite(relay3, HIGH);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
            }
          }
          if (isRelease && avgDistance <= 4) {
            isRelease = false; // เช็คการปล่อยนํ้าออกเปลี่ยนกลับไปค่าดั้งเดิมรอการเรียกเปลี่ยนนํ้าใหม่
            isGetPHValue = true; // เปิดการเช็คค่า PH
            isSend = false;
            digitalWrite(relay2, HIGH);   // สั่งเปิดรีเลย์(หยุดปั้มนํ้าเข้า)
            Blynk.notify("ดำเนินการเปลี่ยนนํ้าสำเร็จ");
            Blynk.virtualWrite(V15, 1);
          }
        }
      } else {
        state = "PH,DIST: offline";
      }
    } else {
      state = "PH,DIST: disable";
    }

    // Logic ควบคุมอุณหภูมิ
    if (isTempEnable) {
      if (isTempSensorWorking) {
        if (temp >= selectedTemp) {
          fanState = "Fan: activte";
          digitalWrite(relay4, LOW);   // สั่งเปิดรีเลย์(พัดลม)
        }

        else {
          fanState = "Temp: normal";
          digitalWrite(relay4, HIGH);   // สั่งปิดรีเลย์(พัดลม)
        }
      } else {
        fanState = "Temp : offline";
      }
    } else {
      fanState = "Temp : disable";
    }


    //  เอา Comment ออกเมื่อต้องการให้แดสงผลผ่าน Serial Monitor every 1 seconds
    //    if (millis() - printTime > printInterval)
    //    {
    //    Serial.println();
    //    Serial.print("Voltage:");
    //    Serial.print(voltage, 2);
    //    Serial.print("    pH value: ");
    //    Serial.println(pHValue, 2);
    //
    //    Serial.print("Distance: ");
    //    Serial.println(avgDistance);
    //
    //    Serial.print("Temperature: ");
    //    Serial.println(temp);
    //
    //    Serial.println(state);
    //    Serial.println(fanState);
    //
    //    printTime = millis();
    //    }
  }
  else {
    if (millis() - printTime > printInterval) {
      lcd.clear(); //Use it to clear the LCD Widget
      lcd.print(0, 0, initialStateText); // use: (position X: 0-15, position Y: 0-1, "Message you want to print");
      initialStateText += ".";
      printTime = millis();
    }
  }
}
double avergearray(int* arr, int number) {
  int i;
  int max, min;
  double avg;
  long amount = 0;
  if (number <= 0) {
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if (number < 5) { //less than 5, calculated directly statistics
    for (i = 0; i < number; i++) {
      amount += arr[i];
    }
    avg = amount / number;
    return avg;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0]; max = arr[1];
    }
    else {
      min = arr[1]; max = arr[0];
    }
    for (i = 2; i < number; i++) {
      if (arr[i] < min) {
        amount += min;      //arr<min
        min = arr[i];
      } else {
        if (arr[i] > max) {
          amount += max;  //arr>max
          max = arr[i];
        } else {
          amount += arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount / (number - 2);
  }//if
  return avg;
}

void sendUptime()
{
  // This function sends Arduino up time every 1 second to Virtual Pins
  // In the app, Widget's reading frequency should be set to PUSH
  // You can send anything with any interval using this construction
  // Don't send more that 10 values per second

  Blynk.virtualWrite(V0, minPH);
  Blynk.virtualWrite(V1, maxPH);
  Blynk.virtualWrite(V2, selectedTemp);
  if (isPHSensorWorking) {
    Blynk.virtualWrite(V13, pHValue);
  } else {
    Blynk.virtualWrite(V13, 0);
  }
  if (isTempSensorWorking) {
    Blynk.virtualWrite(V14, avgTemp);
  } else {
    Blynk.virtualWrite(V14, 0);
  }

  lcd.clear(); //Use it to clear the LCD Widget
  lcd.print(0, 0, state); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
  lcd.print(0, 1, fanState);

  setBlynkLEDStatus();
}

void setBlynkLEDStatus() {
  String errorText = "";
  if (pHValue <= SENSOR_PH_DISABLE) {
    isPHSensorWorking = false;
    ledPH.off();
    errorText += "\nSensor PH เกิดเหตุขัดข้อง";
  } else {
    isPHSensorWorking = true;
    ledPH.on();
  }
  if (avgTemp <= SENSOR_DISABLE) {
    isTempSensorWorking = false;
    ledTemp.off();
    errorText += "\nSensor Temprature เกิดเหตุขัดข้อง";
  } else {
    isTempSensorWorking = true;
    ledTemp.on();
  }
  if (avgDistance <= SENSOR_DISABLE) {
    isDistanceSensorWorking = false;
    ledDistance.off();
    //    errorText += "\nSensor Distance เกิดเหตุขัดข้อง";
  } else {
    isDistanceSensorWorking = true;
    ledDistance.on();
  }
  notifyDevice(errorText);
}

// ฟังก์ชันการส่ง Push Notification สำหรับเหตุขัดข้องของ Sensor
void notifyDevice(String errorText) {
  static unsigned long notifyTime = millis();
  if (millis() - notifyTime > notifyInterval) {
    notifyTime = millis();
    Blynk.notify(errorText);
  }
}

// ตั้ง PH ตํ่าสุด
BLYNK_WRITE(V3)
{
  minPH = param.asDouble();
}
// ตั้งค่า PH สูงสุด
BLYNK_WRITE(V4)
{
  maxPH = param.asDouble();
}
// ตั้งค่าอุณหภูมิสูงสุดที่พัดลมจะทำงาน
BLYNK_WRITE(V5)
{
  selectedTemp = param.asInt();
}
// สั่งการทำงาน ON/OFF ค่า PH
BLYNK_WRITE(V6)
{
  isPHEnable = param.asInt();
}
// สั่งการทำงาน ON/OFF ค่า Temp
BLYNK_WRITE(V7)
{
  isTempEnable = param.asInt();
}
BLYNK_WRITE(V15)

{
  minPH = 6.5;
  maxPH = 8.5;
  selectedTemp = 28;
  Blynk.virtualWrite(V3, minPH );
  Blynk.virtualWrite(V4, maxPH);
  Blynk.virtualWrite(V5, selectedTemp);
  sendUptime();

  Blynk.notify("รีเซ็ตเป็นค่าเริ่มต้น");
}
