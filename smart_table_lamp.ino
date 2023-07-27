#include <ESP8266WiFi.h>
#include "PubSubClient.h"

#define MQTT_VERSION MQTT_VERSION_3_1_1

// Wifi: SSID and password
const char* WIFI_SSID = "404 Not Found";  // 你的wifi名字
const char* WIFI_PASSWORD = "xxxxxxx";    // 你的wifi密码

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = "mqttx_tablelamp001";   // mqtt的用户id，可以不改
const PROGMEM char* MQTT_SERVER_IP = "192.168.2.167";        // mqtt服务器地址
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;              // mqtt服务器端口，默认1883
const PROGMEM char* MQTT_USER = "admin";                     // mqtt账号
const PROGMEM char* MQTT_PASSWORD = "xxxxxxx";               // mqtt密码

// homeassistant的light类，开关、亮度、色温要用不同的主题，分别要一个state主题和一个command主题，用来上报信息和接收信息
// 可参考 https://home-assistant-china.github.io/components/light.mqtt/
// MQTT: topics
// state
const PROGMEM char* MQTT_LIGHT_STATE_TOPIC = "homeassistant/tablelamp/light/status";
const PROGMEM char* MQTT_LIGHT_COMMAND_TOPIC = "homeassistant/tablelamp/light/switch";

// brightness
const PROGMEM char* MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC = "homeassistant/tablelamp/brightness/status";
const PROGMEM char* MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC = "homeassistant/tablelamp/brightness/set";

// colors 
const PROGMEM char* MQTT_LIGHT_COLORTEMP_STATE_TOPIC = "homeassistant/tablelamp/colortemp/status";
const PROGMEM char* MQTT_LIGHT_COLORTEMP_COMMAND_TOPIC = "homeassistant/tablelamp/colortemp/set";

// payloads by default (on/off)
const PROGMEM char* LIGHT_ON = "ON";
const PROGMEM char* LIGHT_OFF = "OFF";

// variables used to store the state, the brightness and the color of the light
boolean m_light_state = false;
uint8_t m_light_brightness = 255;
uint32_t m_light_colortemp = 300;

// pins used for the rgb led (PWM)
const PROGMEM uint8_t RGB_LIGHT_PIN_0 = D0;
const PROGMEM uint8_t RGB_LIGHT_PIN_1 = D1;

// buffer used to send/receive data with MQTT
const uint8_t MSG_BUFFER_SIZE = 20;
char m_msg_buffer[MSG_BUFFER_SIZE]; 

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// function called to adapt the brightness and the color of the led
void setColorTemp(uint32_t p_colortemp, uint8_t p_brightness) 
{
  analogWrite(RGB_LIGHT_PIN_0, (255 - map(p_colortemp, 153, 500, 0, 255)) * p_brightness / 255);
  analogWrite(RGB_LIGHT_PIN_1, map(p_colortemp, 153, 500, 0, 255) * p_brightness / 255);
}

// function called to publish the state of the led (on/off)
void publishLightState() 
{
  if (m_light_state) 
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
  } else 
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
  }
}

// function called to publish the brightness of the led (0-100)
void publishLightBrightness() 
{
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_light_brightness);
  client.publish(MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishLightColorTemp() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_light_colortemp);
  client.publish(MQTT_LIGHT_COLORTEMP_STATE_TOPIC, m_msg_buffer, true);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) 
{
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) 
  {
    payload.concat((char)p_payload[i]);
  }
  // handle message topic
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic)) 
  {
    Serial.println(payload);
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) 
    {
      if (m_light_state != true) 
      {
        m_light_state = true;
        setColorTemp(m_light_colortemp, m_light_brightness);
        publishLightState();
      }
    } else if (payload.equals(String(LIGHT_OFF))) 
    {
      if (m_light_state != false) 
      {
        m_light_state = false;
        setColorTemp(m_light_colortemp,0);
        publishLightState();
      }
    }
  } else if (String(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) 
  {
    Serial.println(payload);
    uint8_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 255) 
    {
      // do nothing...
      return;
    } else 
    {
      m_light_brightness = brightness;
      setColorTemp(m_light_colortemp, m_light_brightness);
      publishLightBrightness();
    }
  } else if (String(MQTT_LIGHT_COLORTEMP_COMMAND_TOPIC).equals(p_topic)) 
  {
    // get the position of the first and second commas
    Serial.println(payload);
    uint32_t colortemp = payload.toInt();
    if (colortemp < 153 || colortemp > 500) 
    {
      return;
    } else 
    {
      m_light_colortemp = colortemp;
    }
    
    setColorTemp(m_light_colortemp, m_light_brightness);
    publishLightColorTemp();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("INFO: connected");
      
      // Once connected, publish an announcement...
      // publish the initial values
      publishLightState();
      publishLightBrightness();
      publishLightColorTemp();

      // ... and resubscribe
      client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_COLORTEMP_COMMAND_TOPIC);
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // init the serial
  Serial.begin(115200);

  // init the RGB led
  pinMode(RGB_LIGHT_PIN_0, OUTPUT);
  pinMode(RGB_LIGHT_PIN_1, OUTPUT);
  analogWriteRange(255);
  analogWriteFreq(40000);
  setColorTemp(153, 0);

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
