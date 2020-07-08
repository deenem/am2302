
/********
Read Temp and Humidity from DHT22 sensor and publish to an MQTT broker
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <DHT.h>


#define DHTPIN 4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#define ledPin 2

DHT dht(DHTPIN, DHTTYPE);

// Replace the next variables with your SSID/Password combination
const char* ssid = "47 Ivy Road C";
const char* password = "7UjmnhY6";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "10.0.1.45";
const char* mqtt_user = "inverter";
const char* mqtt_pwd = "inverter";

const String client_id = "esp32_client";
const String sub_topic = client_id + String("/output"); 
const String temp_topic = client_id + String("/temperature/state");
const String hum_topic = client_id + String("/humidity/state");


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

float temperature = 0;
float humidity = 0;


void setup() {
  Serial.begin(115200);

  dht.begin();

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(ledPin, OUTPUT);
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
  if (String(topic) == sub_topic) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void publish_discovery() {
    String configTopicTemp = String("homeassistant/sensor/") + client_id + String("_temp/config");
    String configTopicHum = String("homeassistant/sensor/") + client_id + String("_humidity/config");
    String tempMessage = String("{\"name\":\"ESP32 Temperature\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"°C\",");
    tempMessage += String("\"state_topic\": \"") + temp_topic + String("\" }");

    String humMessage = String("{\"name\":\"ESP32 Humidity\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\",");
    humMessage += String("\"state_topic\": \"") + hum_topic + String("\" }");
    
    client.publish(configTopicTemp.c_str(),tempMessage.c_str());
    client.publish(configTopicHum.c_str(),humMessage.c_str());
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id.c_str(), mqtt_user, mqtt_pwd)) {
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
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    temperature = dht.readTemperature();
    
    // Convert the value to a char array
    if (!isnan(temperature)) {
      char tempString[8];
      dtostrf(temperature, 1, 2, tempString);
      Serial.print("Temperature: ");
      Serial.println(tempString);
      client.publish(temp_topic.c_str(), tempString);
    }

    humidity = dht.readHumidity();

    if (!isnan(humidity)) {
      // Convert the value to a char array
      char humString[8];
      dtostrf(humidity, 1, 2, humString);
      Serial.print("Humidity: ");
      Serial.println(humString);
      client.publish(hum_topic.c_str(), humString);
    }
  }
}
