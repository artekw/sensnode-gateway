
#include <RFM69.h>     //get it here: https://github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h> //get it here: https://github.com/lowpowerlab/RFM69
#include <SPI.h>       //included with Arduino IDE (www.arduino.cc)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "config/rfm69.h"
#include "config/general.h"


//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NODEID 1                      //the ID of this node
#define NETWORKID 100                 //the network ID of all nodes this node listens/talks to
#define FREQUENCY RF69_433MHZ         //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY "test" //identical 16 characters/bytes on all nodes, not more not less!
//#define IS_RFM69HW_HCW                //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
#define ACK_TIME 30                   // # of ms to wait for an ack packet
//*****************************************************************************************************************************
//#define ENABLE_ATC   //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI -75 //target RSSI for RFM69_ATC (recommended > -80)
//*****************************************************************************************************************************
// Serial baud rate must match your Pi/host computer serial port baud rate!
#define SERIAL_BAUD 9600
//*****************************************************************************************************************************
#define MAX_STRING_LEN 20

const String mqttNode = "sensmon-esp";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

#ifdef ENABLE_ATC
RFM69_ATC radio;
#else
RFM69 radio;
#endif

byte buff[61];


void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(10);
    radio = RFM69(RFM69_CS, RFM69_IRQ, false, RFM69_IRQN);

    pinMode(RFM69_RST, OUTPUT);
    digitalWrite(RFM69_RST, LOW);
    delay(100);
    digitalWrite(RFM69_RST, HIGH);
    delay(100);

    radio.initialize(FREQUENCY, NODEID, NETWORKID);
    radio.encrypt(ENCRYPTKEY);

#ifdef ENABLE_ATC
    radio.enableAutoPower(ATC_RSSI);
#endif

    // setup_wifi();
    mqttClient.setServer(mqttServer, 1883);
    mqttClient.setCallback(mqtt_callback);
    ArduinoOTA.begin(); 

    char buff[50];
    sprintf(buff, "\nTransmitting at %d Mhz...", radio.getFrequency() / 1000000);

}

char *subStr(char *str, char *delim, int index)
{
    char *act, *sub, *ptr;
    static char copy[MAX_STRING_LEN];
    int i;

    // Since strtok consumes the first arg, make a copy
    strcpy(copy, str);

    for (i = 1, act = copy; i <= index; i++, act = NULL)
    {
        //Serial.print(".");
        sub = strtok_r(act, delim, &ptr);
        if (sub == NULL)
            break;
    }
    return sub;
}

// Handle incoming commands from MQTT
void mqtt_callback(char *topic, byte *payload, unsigned int payloadLength)
{
}


void radio_loop() {
  if (radio.receiveDone())
  {
    uint8_t senderId;
    int16_t rssi;
    uint8_t data[radio.DATALEN];

    Blink(LED, 3);
    //save packet because it may be overwritten
    senderId = radio.SENDERID;
    rssi = radio.RSSI;
    memcpy(data, (const void *)radio.DATA, radio.DATALEN);
    //check if sender wanted an ACK
    if (radio.ACKRequested())
    {
      radio.sendACK();
    }
    radio.receiveDone(); //put radio in RX mode
    updateClients(senderId, rssi, (const char *)data);
  } else {
    radio.receiveDone(); //put radio in RX mode
  }

}


void updateClients(uint8_t senderId, int32_t rssi, const char *message) {
    char topic[32];
    char payload[192];
    char* nodename;

    switch (senderId) {
        case 15:
            nodename = "testnode";
            break;
        case 2:
            nodename = "artekroom";
            break;
    }

    static const char PROGMEM JSONtemplate[] = R"({"rssi":%d,"message":"%s"})";

    // payload
    size_t len = snprintf_P(payload, sizeof(payload), JSONtemplate, rssi, message);
    if (len >= sizeof(payload)) {
        Serial.println("\n\n*** RFM69 packet truncated ***\n");
    }



    // topic
    len = snprintf(topic, sizeof(topic), "/rfm69gw/%s", nodename);
    if (len >= sizeof(topic)) {
        Serial.println("\n\n*** MQTT topic truncated ***\n");
    }
    if ((strlen(payload)+1+strlen(topic)+1) > MQTT_MAX_PACKET_SIZE) {
        Serial.println("\n\n*** MQTT message too long! ***\n");
    }

    // send 
    if (!mqttClient.publish(topic, payload)) {
        Serial.println("\n\n*** mqtt publish failed ***\n");
    }
}


void setup_wifi() {
    WiFi.hostname(mqttNode.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_KEY);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        if (millis() > 120000)
            ESP.restart();
    }
}

void reconnect()
{
    while (!mqttClient.connected())
    {
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str()))
        {
            mqttClient.publish("/rfm69gw/status", "connected");
        }
        else
        {
            delay(5000);
        }
    }
}

void Blink(byte PIN, int DELAY_MS)
{
    pinMode(PIN, OUTPUT);
    digitalWrite(PIN, LOW);
    delay(DELAY_MS);
    digitalWrite(PIN, HIGH);
}





void loop()
{
    
    if (WiFi.status() != WL_CONNECTED)
    {
        setup_wifi();
        Serial.println("wifi ok");
    }
    
    if (!mqttClient.connected())
    {
        reconnect();
    }

    // MQTT client loop
    if (mqttClient.connected())
    {
        mqttClient.loop();
        Serial.println("mqtt ok");
    }
    radio_loop();
    ArduinoOTA.handle();
}
