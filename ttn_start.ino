#include <TheThingsNetwork.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/sleep.h>
#include <avr/power.h>

const char *appEui = "70B3D57ED0016999";
const char *appKey = "0966CCBEF5D66224CAC3565C53158799";

// Temperatur
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempMeas;
int tempDPin = 7; // Pin for å slå på spenning til sensoren

// Turbiditet
float turb;
float turbVolt;
int turbPin = A0; // Pin for måling
int turbDPin = 4; // Spenning/bryter
float turbVal;

// Konduktivitet
float condVal = 0.0;
float condVolt, tdsValue, conductivity;
int condPin = A1;
int condDPin = 5;

// Løst oksygen - Dissolved oxygen
float oxVal;
float oxVolt;
int oxPin = A2;
int oxDPin = 6;

#define loraSerial Serial1
#define debugSerial Serial

#define freqPlan TTN_FP_EU868

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);

void setup() {
  loraSerial.begin(57600); // Start serielle
  debugSerial.begin(9600);

  // Initialiser interrupt-pin
  pinMode(3, INPUT);

  // Initialiser digital styring av sensorene
  pinMode(turbDPin, OUTPUT);
  pinMode(oxDPin, OUTPUT);
  pinMode(tempDPin, OUTPUT);
  pinMode(condDPin, OUTPUT);

  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < 10000);

  debugSerial.println("-- STATUS");
  ttn.showStatus();
  debugSerial.println("-- JOIN");
  ttn.join(appEui, appKey);
}

void loop() {
  debugSerial.println("-- LOOP");
  //ttn.wake(); // Vekk opp LORA i tilfelle den sover.

  sensors.begin(); // Start temp. sensor

  // Temperatur
  digitalWrite(tempDPin, HIGH); // Slå på strøm til sensor.
  tempMeas = measureTemp();
  //tempMeas = 20.0;

  // Oksygen
  //oxVal, oxVolt = measureOx(); // Trenger dekoding. Se https://www.dfrobot.com/wiki/index.php/Gravity:_Analog_Dissolved_Oxygen_Sensor_SKU:SEN0237

  // Turbiditet
  digitalWrite(turbDPin, HIGH); // Strøm til sensor
  turb = measureTurb();

  // Konduktivitet
  digitalWrite(condDPin, HIGH); // Strøm til sensor
  conductivity = measureCond(tempMeas);

  // Payload *** Ganger med hundre før omforming til bytes ***
  int temp = tempMeas * 100;
  int cond = conductivity * 100;
  int turb = turbVolt * 100;
  //int ox = oxVolt * 100;

  byte payload[8];
  payload[0] = (byte)((temp & 0xFF00) >> 8); // Temperatur til byte 0 og 1
  payload[1] = (byte)((temp & 0x00FF) );
  payload[2] = (byte)((cond & 0xFF00) >> 8); // Konduktivitet til byte 2 og 3
  payload[3] = (byte)((cond & 0x00FF) );
  payload[4] = (byte)((turb & 0xFF00) >> 8);  // Turbiditet til byte 4 og 5
  payload[5] = (byte)((turb & 0x00FF) );
  //payload[6] = (byte)((ox & 0xFF00) >> 8); // Oksygen til byte 6 og 7
  //payload[7] = (byte)((ox & 0x00FF) );

  //Serial.print(payload[8]);

  // Send it off
  ttn.sendBytes(payload, sizeof(payload));
  //ttn.sleep(300000);
  //delay(50);

  delay(5000);
}

// *** FUNKSJONER FOR Å TA MÅLINGER ***

// TEMPERATUR
float measureTemp() {
  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.print("Temperatur: ");
  float temperature = sensors.getTempCByIndex(0);
  Serial.print(temperature); Serial.println(" *C");
  return temperature;
}

// TURBIDITET (Relativ i %)
float measureTurb() {
  float clearWater = 4.30; // Måleverdien til sensoren for klart vann. Endre her for kalibrering.
  float turbVoltBit = 0.0;
  
  // Ta gjennomsnitt av 100 målinger for litt mer stabile resultater.
  for (int i = 0; i < 100; i++) {
    turbVoltBit += analogRead(turbPin);
  }
  turbVolt = ((turbVoltBit / 100.0) * 5.0) / 1024.0; // Få ut spenningsverdien fra sensoren.
  
  // turbVal = prosentverdi på "klarhet" i vannet. 100% tilsvarer helt rent vann.
  turbVal = (turbVolt / clearWater) * 100.0;
  if (turbVal > 100.0) {
    turbVal = 100.0;  // Kan ikke mer enn 100% klarhet
  }
  debugSerial.print("Turbiditet: "); debugSerial.print(turbVal); debugSerial.println(" %");
  return turbVal;
}

// KONDUKTIVITET
float measureCond(float tempMeas) {
  // Ta gjennomsnitt av 200 målinger.
  for (int i = 0; i < 200; i++) {
    condVal += analogRead(condPin);
  }
  condVal /= 200;
  condVolt = (condVal * 5.0) / 1024.0;

  // Kompensér for temperatur. 
  float compensationCoefficient = 1.0 + 0.02 * (tempMeas - 25.0);
  float compensationVoltage = condVolt / compensationCoefficient;

  // Regn ut TDS (Total Dissolved Solids).
  tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
  
   // Omgjøring fra TDS til konduktivitet. Faktoren 0.67 kan variere litt, men er passende for vårt måleområde.
  conductivity = tdsValue / 0.67;
  debugSerial.print("Konduktivitet: ");
  debugSerial.print(conductivity); Serial.println(" µS/cm");
  return conductivity;
}

// OPPLØST OKSYGEN *** Trenger kalibrering/endring ***
float measureOx() {
  oxVal = analogRead(oxPin);
  oxVolt = (oxVal * 5.0) / 1024.0;
  debugSerial.print("Oksygen (bit): ");
  debugSerial.println(oxVal);
  debugSerial.print("Oxygen (V): ");
  debugSerial.println(oxVolt);
  return oxVal, oxVolt;
}

// *** SOVEFUNKSJONER ***
// The Things UNO - sleep mode
void toSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, wakeupFunction, LOW); //wakeupbutton : pin 3 on leonardo!!!
  Serial1.println("Sleeping");
  sleep_mode();
  detachInterrupt(0);
  sleep_disable(); //fully awake now
}

void wakeupFunction() {
}

// START SOVING - LORA og TT Uno
/*void sleep() {
  debugSerial.println("Sleep mode... ");
  //  *** Intervallet kan endres **
  ttn.sleep(5000);
  debugSerial.println("Lora sleeping...");
  toSleep(); // Leonardo sleep
  ttn.wake(); // Wake LORA
  debugSerial.println("LORA woke up!");
  debugSerial.println("Leonardo woke up!");
  }
*/
