// To Run the RS485
#include <SoftwareSerial.h>  

// To Run Lora 
#include <SPI.h> 
#include <LoRa.h>

// RGB LED Control
#include <Adafruit_NeoPixel.h>

// To Package sensor data
#include <ArduinoJson.h>

// RS485 Configuration for NPK Sensor
#define sensorFrameSize 11
#define sensorWaitingTime 1000
#define sensorByteResponse 0x0E

//Lora SPI Pin configuration
#define SS 3
#define RST 2
#define DIO 6

//Data direction pin for RS485
const int dtr = 5;

// RS485 Byte Address Request to Sensor
unsigned char nReq[8] = { 0x01, 0x03, 0x00, 0x1E, 0x00, 0x01, 0xE4, 0x0C };  //Nitrogen
unsigned char pReq[8] = { 0x01, 0x03, 0x00, 0x1F, 0x00, 0x01, 0xB5, 0xCC };  //Phosporous
unsigned char kReq[8] = { 0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xC0 };  //Potassium

unsigned char byteResponse[11] = {};

unsigned long lastSensorRead = 0;

uint8_t R = 0;
uint8_t G = 0;
uint8_t B = 0;

SoftwareSerial sensor(4, 7);
Adafruit_NeoPixel pixels(1, 19, NEO_GRB + NEO_KHZ800);

StaticJsonDocument<96> doc;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  sensor.begin(9600);

  pinMode(dtr, OUTPUT);
  digitalWrite(dtr, LOW);

  if (!sensor) {  // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid pin configuration, check config");
    while (1) {  // Don't continue with invalid configuration
      delay(1000);
    }
  }

  pixels.begin();
  pixels.clear();

  LoRa.setPins(SS, RST, DIO);  // set CS, reset, IRQ pin

  if (!LoRa.begin(868E6)) {  // initialize ratio at 868 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }
}

void loop() {

  //Get the sensor value every 15secs  and send it through lora
  if (millis() - lastSensorRead >= 15000) {
    
    String packagedData = "";
    
    R = random(256);
    G = random(256);
    B = random(256);

    Serial.println("Setting the color of RGB LED");
    Serial.printf("The Value of R: %d G: %d B: %d",R,G,B);
    Serial.println();

    pixels.setPixelColor(0, pixels.Color(R, G, B));
    pixels.show();  // Send the updated pixel colors to the hardware.


    float N = getSensorValue(nReq);
    float P = getSensorValue(pReq);
    float K = getSensorValue(kReq);

    Serial.printf("Sensor Value, N: %f P : %f K : %f",N,P,K);
    
    //Package all the data 

    doc["N"] = N;
    doc["P"] = P;
    doc["K"] = K;
    doc["R"] = R;
    doc["G"] = G;
    doc["B"] = B;

    serializeJson(doc, packagedData);
    
    Serial.println("Sending data through Lora!");
    Serial.println(packagedData);

    LoRa.beginPacket();
    LoRa.print(packagedData);
    LoRa.endPacket();

    lastSensorRead = millis();
  }
}

float getSensorValue(unsigned char req[]) {
  //request reading from the sensor
  sensor.flush();
  digitalWrite(dtr, HIGH);
  sensor.write(req, 8);
  digitalWrite(dtr, LOW);

  // Wait for sensor to response
  unsigned long resptime = millis();
  while ((sensor.available() < sensorFrameSize) && ((millis() - resptime) < sensorWaitingTime)) {
    delay(1);
  }

  while (sensor.available()) {
    for (int n = 0; n < sensorFrameSize; n++) {
      byteResponse[n] = sensor.read();
      delay(1);
    }
  }

  //Print the byte response!
  for (int i = 0; i < 7; i++) {
    Serial.print(byteResponse[i], HEX);
    Serial.print(" ");
  }

  float combined_value = 0.0;
  //Convert Hex value to Decimal
  if (byteResponse[2] == 0x02) {
    combined_value = (byteResponse[3] << 8) | byteResponse[4];
    Serial.println("calculating 2 byte response");
  } else if (byteResponse[2] == 0x04) {
    combined_value = (byteResponse[5] << 8) | byteResponse[6];
    Serial.println("calculating 4 byte response");
  }

  for (int i = 0; i < 11; i++) {
    byteResponse[i] = 0;
  }

  Serial.println("Value : " + String(combined_value));
  return combined_value;
}