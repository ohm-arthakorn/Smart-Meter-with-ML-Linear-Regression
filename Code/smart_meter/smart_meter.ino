// ประกาศตัวแปรสำหรับการส่งค่าการใช้งานไปยัง Google Sheet โดยจะประกอบไปด้วยเวลา กำลัง พลังงานสะสม แรงดัน และกระแสที่ได้มีการใช้งาน
#include <WiFi.h>
#include <PubSubClient.h>

//  I have been used "ESP32 C3 Super Mini" so I have to define each pin for purpose. เนื่องจากผมใช้งานบอร์ด ESP32 C3 Super Mini ดังนั้น ผมเลยต้องกำหนดแต่ละ pins เพื่อใช้งาน
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <PZEM004Tv30.h>

// Define pins RX/TX and I2C กำหนด pin สำหรับการใช้งาน RX/TX และ I2C
#define RX_PIN 2
#define TX_PIN 3
#define I2C_SDA 6
#define I2C_SCL 7

#define BAUD 9600
#define Pzem_serial Serial1
#define Subscribe "smart_meter/receive"

PZEM004Tv30 pzem(Pzem_serial, RX_PIN, TX_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Declare variables for PZEM004
float voltage;
float current;
float power;
float energy;
float frequency;
float power_factor;
unsigned long previousMillis = 0;
const long interval = 5000;  // มีการส่งข้อมูลทุก ๆ 5 วินาที
unsigned long currentMillis = millis();


// WiFi & Google Script Link
const char *ssid = "Ohm";
const char *password = "123456789";

// Declare MQTT properties.
WiFiClient espClient;
PubSubClient client(espClient);

const int mqttPort = 1883;
const char *mqttServer = "broker.hivemq.com";
const char *mqttClient = "";
const char *mqttUsername = "";
const char *mqttPassword = "";

// จัดการเรื่องของข้อมูลที่จะมีการส่งไปยัง Node Red
String DataString;
long lastMsg = 0;
int value = 0;
char msg[100];


// -- Function that for reconnect to MQTT Server with Node Red ฟังก์ชันสำหรับการเชื่อมต่ออีกครั้งระหว่าง MQTT และ Node Red --
void reonnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttClient, mqttUsername, mqttPassword)) {
      Serial.println("Connected !");
      client.subscribe(Subscribe);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Try again in 1 seconds !");
      delay(1000);
    }
  }
}

// -- Callback Function ฟังก์ชันเอาไว้สำหรับการแจ้งเตือนเมื่อมีข้อมูลมีการส่งไปยัง Topics ที่เราได้ทำการ Subscribes --
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;

  for (int i = 0; i < length; i++) {
    message = message + char(payload[i]);
  }

  Serial.println(message);
}

// --- Connected WiFi Function ---
void connectWiFiandShowOnLCD() {

  // --- Connecting to WiFi and show in Serial Monitor ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi...");
  lcd.setCursor(0, 0);
  lcd.print("Connecting to ");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi.");
  int i = 6;

  // -- For checking condition that WiFi have been connected ? do others : for loop connecting.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    lcd.setCursor(i, 1);
    lcd.print(".");
    lcd.print(" ");
    i++;
    if (i > 17) {
      i = 5;
    }
  }
  // If I can connected to WiFi show status follow these.
  Serial.print("\nConnected to WiFi! -->");
  Serial.println(ssid);
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected! to");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
}

void ShowStatusValue() {
  // Check if the data is valid
  if (isnan(voltage)) {
    Serial.println("Error reading Voltage please check your wire!");
  } else if (isnan(current)) {
    Serial.println("Error reading current");
  } else if (isnan(power)) {
    Serial.println("Error reading power");
  } else if (isnan(energy)) {
    Serial.println("Error reading energy");
  } else if (isnan(frequency)) {
    Serial.println("Error reading frequency");
  } else if (isnan(power_factor)) {
    Serial.println("Error reading power factor");
  } else {

    // Print the values to the Serial console
    Serial.print("Voltage: ");
    Serial.print(voltage);
    Serial.println("V");

    Serial.print("Current: ");
    Serial.print(current);
    Serial.println("A");

    Serial.print("Power: ");
    Serial.print(power);
    Serial.println("W");

    Serial.print("Energy: ");
    Serial.print(energy, 3);
    Serial.println("kWh");

    Serial.print("Frequency: ");
    Serial.print(frequency, 1);
    Serial.println("Hz");

    Serial.print("PF: ");
    Serial.println(power_factor);
  }
}

void ShowDataOnLCD() {
  if (isnan(voltage) || isnan(current) || isnan(power)) {
    lcd.setCursor(0, 0);
    lcd.print("Voltage is nan!");
    lcd.setCursor(0, 1);
    lcd.print("Check your wire!");
  } else {

    lcd.setCursor(0, 0);  // Go to Col 0, Row 0
    lcd.print("V:");
    lcd.print(voltage, 1);  // Print with 1 decimal place
    lcd.print(" V");


    lcd.setCursor(0, 1);  // Go to Col 0, Row 1
    lcd.print("A:");
    lcd.print(current, 2);  // Print with 2 decimal places
    lcd.print(" A");


    lcd.setCursor(9, 1);  // Go to Col 9, Row 1
    lcd.print("P:");
    lcd.print(power, 0);  // Print with 0 decimal places
    lcd.print(" W");
  }
  delay(1500);
}

void setup() {
  // Debugging Serial port
  Serial.begin(BAUD);
  Pzem_serial.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  Wire.begin(I2C_SDA, I2C_SCL);
  // Section about LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART METER !");
  lcd.setCursor(0, 1);
  lcd.print("Initialized...");
  delay(1000);
  lcd.clear();

  // -- Connection to WiFi การเชื่อมต่อกับ WiFi --
  connectWiFiandShowOnLCD();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.subscribe(Subscribe);
  // Uncomment in order to reset the internal energy counter
  // pzem.resetEnergy()
}


// -- Function for Sending a Voltage to Node-Red Server. ฟังก์ชันสำหรับการส่งค่าข้อมูลที่ได้ขึ้น Server Node-red --
void SendDataFromPZEM() {
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  frequency = pzem.frequency();
  power_factor = pzem.pf();

  if (!(isnan(voltage) || isnan(current) || isnan(power))) {  // <-- เช็คว่าแรงดันมันสามารถอ่านค่าได้ไหม ? ส่งค่า : ไม่ต้องส่งค่า
    lastMsg = currentMillis;
    ++value;
    DataString = "{ \"voltage\" : " + String(voltage) + ", \"current\" :" + String(current) + ", \"power\" :" + String(power) + ", \"energy\" :" + String(energy) + "}";
    DataString.toCharArray(msg, 100);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(Subscribe, msg);
    delay(1);
    client.loop();
  }
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read the data from the sensor
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    energy = pzem.energy();
    frequency = pzem.frequency();
    power_factor = pzem.pf();

    lcd.clear();
    ShowStatusValue();
    ShowDataOnLCD();
    SendDataFromPZEM();
  }
  // มีการเรียกใช้งานฟังก์ชันสำหัรบการส่งข้อมูลไปยัง Node Red.
  reonnectMQTT();
  delay(1000);
}