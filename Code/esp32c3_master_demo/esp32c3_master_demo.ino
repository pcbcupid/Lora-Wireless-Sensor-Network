#include <WiFi.h>
#include <PubSubClient.h>
// To Run the RS485
#include <SoftwareSerial.h>
// Energy Meter uses Modbus protocol
#include <ModbusMaster.h>
// To Run Lora
#include <SPI.h>
#include <LoRa.h>

// RGB LED Control
#include <Adafruit_NeoPixel.h>

// To Package sensor data
#include <ArduinoJson.h>

//Lora SPI Pin configuration
#define SS 3
#define RST 2
#define DIO 6

#define DTR 5

// Replace the next variables with your SSID/Password combination
const char* ssid = "Airtel_Internet";
const char* password = "@werty2398";

const char* mqtt_server = "165.232.185.212";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

Adafruit_NeoPixel pixels(1, 19, NEO_GRB + NEO_KHZ800);
SoftwareSerial emSerial(4, 7);
ModbusMaster emNode;

StaticJsonDocument<128> doc;

void setup() {
  //Serial setup
  Serial.begin(115200);

  // Modbus for energy meter setup

  pinMode(DTR, OUTPUT);
  digitalWrite(DTR, LOW);

  emSerial.begin(9600, EspSoftwareSerial::SWSERIAL_8E1);
  emNode.begin(1, emSerial);

  //  callbacks allow us to configure the RS485 transceiver correctly
  emNode.preTransmission(modbusPreTransmission);
  emNode.postTransmission(modbusPostTransmission);

  pixels.begin();
  pixels.clear();

  LoRa.setPins(SS, RST, DIO);  // set CS, reset, IRQ pin

  if (!LoRa.begin(868E6)) {  // initialize ratio at 868 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  // put your main code here, to run repeatedly:
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  String input = "";

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (packetSize) {
    // received a packet
    Serial.print("Received packet'");

    // read packet
    while (LoRa.available()) {
      input = LoRa.readString();
      Serial.println(input);
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    int rssi = LoRa.packetRssi();
    int RGBbrigtness = map(abs(rssi),100,20,10,100);
    Serial.println(rssi);

    

    DeserializationError error = deserializeJson(doc, input);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    float nitro = doc["nitro"];
    float phos = doc["phos"];
    float pot = doc["pot"];
    int R = doc["R"];
    int G = doc["G"];
    int B = doc["B"];

    float voltage = emRead(109);
    float current = emRead(121);
    float frequency = emRead(171);

    Serial.printf("EM reading -> Voltage : %f Current : %f Frequency %f ", voltage, current, frequency);
    Serial.println();

    pixels.setPixelColor(0, pixels.Color(R, G, B));
    pixels.setBrightness(RGBbrigtness);
    pixels.show();  // Send the updated pixel colors to the hardware.
    
    char tempString[8];//temprory String
    //publish the data to mqtt
    dtostrf(nitro, 1, 2, tempString);
    client.publish("n", tempString);
    
    dtostrf(phos, 1, 2, tempString);
    client.publish("p", tempString);
    
    dtostrf(pot, 1, 2, tempString);
    client.publish("k", tempString);

    dtostrf(voltage, 1, 2, tempString);
    client.publish("v",tempString);

    dtostrf(current, 1, 2, tempString);
    client.publish("c", tempString);
    
    dtostrf(frequency, 1, 2, tempString);
    client.publish("f", tempString);
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("MasterNode","sara","digital2Ocean")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Pin 5 made high for Modbus transmision mode
void modbusPreTransmission() {
  delay(500);
  digitalWrite(DTR, HIGH);
}
// Pin 5 made low for Modbus receive mode
void modbusPostTransmission() {
  digitalWrite(DTR, LOW);
  delay(500);
}

float emRead(uint16_t ra) {
  uint16_t data[2];
  float reading = 0.0;

  uint8_t result = emNode.readHoldingRegisters(ra, 2);
  if (result == emNode.ku8MBSuccess) {
    data[0] = emNode.getResponseBuffer(0x00);
    data[1] = emNode.getResponseBuffer(0x01);
    reading = *((float*)data);
    emNode.clearResponseBuffer();
  } else {
    Serial.printf("Failed Node-%d, Response Code: ", index);
    Serial.print(result, HEX);
    Serial.println("");
    delay(5000);
  }

  return reading;
}
