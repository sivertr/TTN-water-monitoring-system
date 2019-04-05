// Timerpins
int onSignal = 1; // KAN IKKE VÆRE 0 eller 1 NÅR SERIALS KJØRER
int doneSignal = 2;

void setup() {
  // Timing-greier
  pinMode(onSignal, OUTPUT);
  digitalWrite(onSignal, HIGH); // Send høyt "ON" slik at TTUno forblir påslått.
  pinMode(doneSignal, OUTPUT);
  digitalWrite(doneSignal, LOW); // Sett "DONE" til lav og gjør klar til å sette det høyt når målinger er gjort.
}

// Initialiser TTN-bibliotek og applikasjons-IDer
#include <TheThingsNetwork.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char *appEui = "70B3D57ED0016999";
const char *appKey = "0966CCBEF5D66224CAC3565C53158799";

#define loraSerial Serial1
#define debugSerial Serial
#define ONE_WIRE_BUS 6  // Måling på D6 / IO6
#define freqPlan TTN_FP_EU868

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void loop() {
  loraSerial.begin(57600); // Start serielle
  debugSerial.begin(9600);

  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < 10000);

  debugSerial.println("-- STATUS");
  ttn.showStatus();
  debugSerial.println("-- JOIN");

  // Slå på lys for å indikere at TTUno prøver å koble til nettverket
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  ttn.join(appEui, appKey, 4, 15000);

  digitalWrite(LED_BUILTIN, LOW);
  delay(50);

  debugSerial.println("-- LOOP");

  // Ta målinger
  float tempMeas = measureTemp();
  float oxVolt = measureOx(); // Trenger endring/kalibrering. Se https://www.dfrobot.com/wiki/index.php/Gravity:_Analog_Dissolved_Oxygen_Sensor_SKU:SEN0237
  float turbidity = measureTurb();
  float conductivity = measureCond(tempMeas);

  // Payload *** Ganger med hundre før omforming til bytes ***
  int temp = tempMeas * 100;
  int cond = conductivity * 100;
  int turb = turbidity * 100;
  int ox = oxVolt * 100;

  byte payload[8];
  payload[0] = (byte)((temp & 0xFF00) >> 8); // Temperatur til byte 0 og 1
  payload[1] = (byte)((temp & 0x00FF) );
  payload[2] = (byte)((cond & 0xFF00) >> 8); // Konduktivitet til byte 2 og 3
  payload[3] = (byte)((cond & 0x00FF) );
  payload[4] = (byte)((turb & 0xFF00) >> 8);  // Turbiditet til byte 4 og 5
  payload[5] = (byte)((turb & 0x00FF) );
  payload[6] = (byte)((ox & 0xFF00) >> 8); // Oksygen til byte 6 og 7
  payload[7] = (byte)((ox & 0x00FF) );

  // Send bytes
  ttn.sendBytes(payload, sizeof(payload));
  
  debugSerial.end();
  loraSerial.end();
  delay(100);
  
  pinMode(onSignal,OUTPUT);
  pinMode(doneSignal, OUTPUT);
  digitalWrite(doneSignal, HIGH); // Send høyt "DONE" for å starte timerkrets.
  delay(200);
  digitalWrite(onSignal, LOW); // Slå av TTUno
  delay(2000);
}

// *** FUNKSJONER FOR Å TA MÅLINGER ***

// TEMPERATUR
float measureTemp() {
  sensors.begin(); // Start digital temp. sensor
  delay(500);

  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.print("Temperatur: ");
  float temperature = sensors.getTempCByIndex(0);
  Serial.print(temperature); Serial.println(" *C");
  return temperature;
}

// TURBIDITET (Relativ "sikt" i %)
float measureTurb() {
  int turbPin = A4; // Pin for måling
  float clearWater = 4.30; // Spenningsverdien sensoren målte for klart vann. Endre her for kalibrering.
  float turbVoltBit = 0.0;

  // Ta gjennomsnitt av 100 målinger for litt mer stabile resultater.
  for (int i = 0; i < 100; i++) {
    turbVoltBit += analogRead(turbPin);
  }
  float turbVolt = ((turbVoltBit / 100.0) * 5.0) / 1024.0; // Få ut spenningsverdien fra sensoren.

  // turbVal = prosentverdi på "klarhet" i vannet. 100% tilsvarer helt rent vann.
  float turbVal = (turbVolt / clearWater) * 100.0;
  if (turbVal > 100.0) {
    turbVal = 100.0;  // Kan ikke mer enn 100% klarhet
  }
  debugSerial.print("Turbiditet: "); debugSerial.print(turbVal); debugSerial.println(" %");
  return turbVal;
}

// KONDUKTIVITET
float measureCond(float tempMeas) {

  float condVal = 0.0;
  int condPin = A0;

  // Ta gjennomsnitt av 100 målinger.
  for (int i = 0; i < 100; i++) {
    condVal += analogRead(condPin);
  }
  condVal /= 100;
  float condVolt = (condVal * 5.0) / 1024.0;

  // Kompensér for temperatur.
  float compensationCoefficient = 1.0 + 0.02 * (tempMeas - 25.0);
  float compensationVoltage = condVolt / compensationCoefficient;

  // Regn ut TDS (Total Dissolved Solids).
  float tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

  // Omgjøring fra TDS til konduktivitet. Faktoren 0.67 kan variere litt, men er passende for vårt måleområde.
  float conductivity = tdsValue / 0.67;
  debugSerial.print("Konduktivitet: ");
  debugSerial.print(conductivity); Serial.println(" µS/cm");
  return conductivity;
}

// OPPLØST OKSYGEN *** Trenger kalibrering/endring ***
float measureOx() {
  float oxVal = 0.0;
  int oxPin = A1;

  // Ta gjennomsnitt av 100 målinger
  for (int i = 0; i < 100; i++) {
    oxVal += analogRead(oxPin);
  }
  float oxVolt = ((oxVal / 100.0) * 5.0) / 1024.0;
  //debugSerial.print("Oksygen (bit): ");
  //debugSerial.println(oxVal);
  debugSerial.print("Oxygen (V): ");
  debugSerial.println(oxVolt);
  return oxVal, oxVolt;
}
