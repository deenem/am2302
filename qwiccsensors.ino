// Arduino code I wrote for a Adafruit ESP32 Thing plus and some sensors
// Reads sensors and publishes to Home Assistant via MQTT

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <SPI.h>

#define ALT_FT 5400.00 // Altitude in feet for HPA adjustment

//#define SSD1306 

#include <Adafruit_LPS2X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HTS221.h>
#include <Adafruit_GFX.h>


// Include SSD1306 code
#ifdef SSD1306 
#include <Adafruit_SSD1306.h>
#endif

#include <Adafruit_VCNL4040.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#ifdef SSD1306 
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

Adafruit_LPS22 lps;
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_HTS221 hts;

const char* ssid = "...";
const char* password = "...";

#define DISP_LINES 8
#define LINE_LEN 30

int iLineCnt = 0;
char buffer[DISP_LINES][LINE_LEN+1];

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "10.0.1.45";
const char* mqtt_user = "inverter";
const char* mqtt_pwd = "...";
const char* mqtt_client_name = "adafruit_esp32_thing_1";
const String mqtt_topic_prefix = "adafruit/thing/";
const String device_name = "Qwicc Box";
const String device_manf = "Adafruit";
const String device_model = "ESP32 Thing Plus";
const String sub_topic = mqtt_topic_prefix + String("/output"); 

WiFiClient espClient;
PubSubClient client(espClient);

#define NUM_SENSORS 5
#define TEMP_0  0
#define HUM_1   1
#define PRES_2  2
#define LUX_3   3
#define LIGHT_4 4

typedef struct {
  char *name;
  char *unique_id;
  char *state_topic;
  char *device_class;
  char *u_o_m;
  float value;
} MQTTSensor;

MQTTSensor sensor[NUM_SENSORS] = {
    {"Qwicc HTS221 Temperature",   "hts221_temp_1",     "hts221/temp",           "temperature", "C",   0},
    {"Qwicc HTS221 Humidity",      "hts221_humidity_1", "hts221/humidity",       "humidity",    "%",   0},
    {"Qwicc LPS22 Pressure",       "lps22_pressure_1",  "lps22/presure",         "pressure",    "hPa", 0},
    {"Qwicc VCNL4040 Lux",         "vcnl4040_lux_1",    "vcnl4040/illuminance",  "illuminance", "lx",  0},
    {"Qwicc VCNL4040 White Light", "vcnl4040_light_1",    "vcnl4040/whitelight", "illuminance", "lx",  0}
    };

void setup_OTA()
{
    
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("Qwicc_ESP");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_VCNL4404(){

  Serial.println("Adafruit VCNL4040 Config demo");

  if (!vcnl4040.begin()) {
    Serial.println("Couldn't find VCNL4040 chip");
    while (1);
  }
  Serial.println("Found VCNL4040 chip");

  //vcnl4040.setProximityLEDCurrent(VCNL4040_LED_CURRENT_200MA);
  Serial.print("Proximity LED current set to: ");
  switch(vcnl4040.getProximityLEDCurrent()) {
    case VCNL4040_LED_CURRENT_50MA: Serial.println("50 mA"); break;
    case VCNL4040_LED_CURRENT_75MA: Serial.println("75 mA"); break;
    case VCNL4040_LED_CURRENT_100MA: Serial.println("100 mA"); break;
    case VCNL4040_LED_CURRENT_120MA: Serial.println("120 mA"); break;
    case VCNL4040_LED_CURRENT_140MA: Serial.println("140 mA"); break;
    case VCNL4040_LED_CURRENT_160MA: Serial.println("160 mA"); break;
    case VCNL4040_LED_CURRENT_180MA: Serial.println("180 mA"); break;
    case VCNL4040_LED_CURRENT_200MA: Serial.println("200 mA"); break;
  }
  
  //vcnl4040.setProximityLEDDutyCycle(VCNL4040_LED_DUTY_1_40);
  Serial.print("Proximity LED duty cycle set to: ");
  switch(vcnl4040.getProximityLEDDutyCycle()) {
    case VCNL4040_LED_DUTY_1_40: Serial.println("1/40"); break;
    case VCNL4040_LED_DUTY_1_80: Serial.println("1/80"); break;
    case VCNL4040_LED_DUTY_1_160: Serial.println("1/160"); break;
    case VCNL4040_LED_DUTY_1_320: Serial.println("1/320"); break;
  }

  //vcnl4040.setAmbientIntegrationTime(VCNL4040_AMBIENT_INTEGRATION_TIME_80MS);
  Serial.print("Ambient light integration time set to: ");
  switch(vcnl4040.getAmbientIntegrationTime()) {
    case VCNL4040_AMBIENT_INTEGRATION_TIME_80MS: Serial.println("80 ms"); break;
    case VCNL4040_AMBIENT_INTEGRATION_TIME_160MS: Serial.println("160 ms"); break;
    case VCNL4040_AMBIENT_INTEGRATION_TIME_320MS: Serial.println("320 ms"); break;
    case VCNL4040_AMBIENT_INTEGRATION_TIME_640MS: Serial.println("640 ms"); break;
  }


  //vcnl4040.setProximityIntegrationTime(VCNL4040_PROXIMITY_INTEGRATION_TIME_8T);
  Serial.print("Proximity integration time set to: ");
  switch(vcnl4040.getProximityIntegrationTime()) {
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_1T: Serial.println("1T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_1_5T: Serial.println("1.5T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_2T: Serial.println("2T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_2_5T: Serial.println("2.5T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_3T: Serial.println("3T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_3_5T: Serial.println("3.5T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_4T: Serial.println("4T"); break;
    case VCNL4040_PROXIMITY_INTEGRATION_TIME_8T: Serial.println("8T"); break;
  }

  //vcnl4040.setProximityHighResolution(false);
  Serial.print("Proximity measurement high resolution? ");
  Serial.println(vcnl4040.getProximityHighResolution() ? "True" : "False");

  Serial.println("");
  
}

void setup_LPS22(void) {
  Serial.println("Adafruit LPS22 test!");

  // Try to initialize!
  if (!lps.begin_I2C()) {
    Serial.println("Failed to find LPS22 chip");
    while (1) { delay(10); }
  }
  Serial.println("LPS22 Found!");

  lps.setDataRate(LPS22_RATE_10_HZ);
  Serial.print("Data rate set to: ");
  switch (lps.getDataRate()) {
    case LPS22_RATE_ONE_SHOT: Serial.println("One Shot / Power Down"); break;
    case LPS22_RATE_1_HZ: Serial.println("1 Hz"); break;
    case LPS22_RATE_10_HZ: Serial.println("10 Hz"); break;
    case LPS22_RATE_25_HZ: Serial.println("25 Hz"); break;
    case LPS22_RATE_50_HZ: Serial.println("50 Hz"); break;

  }
}


void setup_HTS221(void) {
  Serial.println("Adafruit HTS221 test!");

  // Try to initialize!
  if (!hts.begin_I2C()) {
    Serial.println("Failed to find HTS221 chip");
    while (1) { delay(10); }
  }
  Serial.println("HTS221 Found!");

  Serial.print("Data rate set to: ");
  switch (hts.getDataRate()) {
   case HTS221_RATE_ONE_SHOT: Serial.println("One Shot"); break;
   case HTS221_RATE_1_HZ: Serial.println("1 Hz"); break;
   case HTS221_RATE_7_HZ: Serial.println("7 Hz"); break;
   case HTS221_RATE_12_5_HZ: Serial.println("12.5 Hz"); break;
  }

}

#ifdef SSD1306
void setup_SSD1306() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

  // Clear the buffer
  display.clearDisplay();

  snprintf (buffer[iLineCnt++], LINE_LEN, "Hello");
  snprintf (buffer[iLineCnt++], LINE_LEN, "World");
}
#endif


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  // Feel free to add more if statements to control more GPIOs with MQTT
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  //if (String(topic) == sub_topic) {
  //}
}

void setup_MQTT() {
   client.setServer(mqtt_server, 1883);
   client.setCallback(callback);
   client.setBufferSize(512);
}
void setup() {

  Serial.begin(115200);
  // Wait until serial port is opened
  while (!Serial) { delay(1); }
  Serial.println("Booting");

  setup_OTA();
  setup_MQTT();
  setup_VCNL4404();
  setup_LPS22();
  setup_HTS221();
#ifdef SSD1306
  setup_SSD1306();
#endif
}


// Loops

void loop_OTA()
{
  ArduinoOTA.handle();
}

void loop_VCNL4040() 
{
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %d", "Prox", vcnl4040.getProximity());
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %d", "Lux", vcnl4040.getLux());
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %d", "Light", vcnl4040.getWhiteLight());
  
  sensor[LUX_3].value = vcnl4040.getLux();
  sensor[LIGHT_4].value = vcnl4040.getWhiteLight();
  delay (100);
}

void loop_LPS22() {
  sensors_event_t temp;
  sensors_event_t pressure;
  lps.getEvent(&pressure, &temp);// get pressure
  
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %.2f C", "Temp", temp.temperature);
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %.2f nPa", "Pres", pressure.pressure);

  sensor[PRES_2].value = pressure.pressure + (ALT_FT / 30.0);

  delay(100);
}

void loop_HTS221() {

  sensors_event_t temp;
  sensors_event_t humidity;
  hts.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %.2f C", "Temp", temp.temperature);
  snprintf (buffer[iLineCnt++], LINE_LEN, "%5s : %.2f rH","Hum" , humidity.relative_humidity);
  sensor[HUM_1].value = humidity.relative_humidity;
  sensor[TEMP_0].value = temp.temperature;
  delay(500);
}


#ifdef SSD1306
void loop_SSD1306(){
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  
  for (int i = 0; i < DISP_LINES; ++i )
  {
    display.write(buffer[i]);
    display.write('\n');
  }
  display.display();
}
#endif

void publish_discovery() {


  for (int i = 0; i < NUM_SENSORS; ++i) {

      DynamicJsonDocument doc(1024);

      String stateTopic = mqtt_topic_prefix + String(sensor[i].state_topic);
      String discoveryTopic = String("homeassistant/sensor/") + String(sensor[i].unique_id) + String("/config");
  
      doc["uniq_id"] = sensor[i].unique_id;
      doc["name"] = sensor[i].name;
      if (sensor[i].device_class != 0) doc["dev_cla"] = sensor[i].device_class;
      doc["unit_of_meas"] = sensor[i].u_o_m;
      doc["state_class"] = "measurement";
      
      doc["stat_t"] = stateTopic.c_str();

      JsonObject device  = doc.createNestedObject("dev");
      device["name"] = device_name;
      device["ids"][0] = WiFi.macAddress();
      
      device["mf"] = device_manf;
      device["mdl"] = device_model;
      
      String output;
      serializeJson(doc, output);

      Serial.print("Discovery...\n");
      Serial.print(discoveryTopic.c_str());
      Serial.print("\n");
      Serial.print(output.c_str());
      Serial.print("\n");
      
      client.publish(discoveryTopic.c_str(),output.c_str());
  }
}

void reconnect() {
  // Loop until we're reconnected
  delay(5000);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_name, mqtt_user, mqtt_pwd)) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(sub_topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

   publish_discovery();

}

void loop_MQTT() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  for (int i = 0; i < NUM_SENSORS; ++i) {
      if (!isnan(sensor[i].value)) {
        String stateTopic = mqtt_topic_prefix + String(sensor[i].state_topic);
        char tempString[8];
        dtostrf(sensor[i].value, 1, 2, tempString);
        client.publish(stateTopic.c_str(), tempString);
      }
  }
}

void loop() {
  iLineCnt = 0;
  loop_OTA();
  loop_VCNL4040();
  loop_LPS22();
  loop_HTS221();
  
#ifdef SSD1306
  loop_SSD1306();
#endif

  loop_MQTT();
  delay(5000);
  
}
