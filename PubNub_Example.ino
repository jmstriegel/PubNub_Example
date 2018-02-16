#include <ESP8266WiFi.h>          // Onboard WiFi and HTTP client
#include <Adafruit_Sensor.h>      // Adafruit Unified Sensor Library
#include <DHT.h>                  // DHT22 Temperature Sensor
#include <DHT_U.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_MQTT.h>
#include "config.h"

#define DHTPIN            2       // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT22   // Options: DHT11 DHT22 DHT21

DHT_Unified dht(DHTPIN, DHTTYPE);
bool wifi_connected = false;
uint32_t dhtDelayMS;
uint32_t readTime;


WiFiClient client;
Adafruit_MQTT_Client mqtt(&client,
                          MQTT_BROKER_SERVER,
                          MQTT_BROKER_PORT,
                          MQTT_CLIENT_ID,
                          MQTT_USERNAME,
                          MQTT_PASSWORD);


Adafruit_MQTT_Publish tempPublish = Adafruit_MQTT_Publish(&mqtt, "sensors/temperature");
Adafruit_MQTT_Publish humidityPublish = Adafruit_MQTT_Publish(&mqtt, "sensors/humidity");


void WiFiEvent(WiFiEvent_t event) {
  //Serial.print(String("WiFi:: event: ") + (int)event + "\n");
  switch(event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("WiFi:: connected");
      Serial.println(WiFi.localIP());
      wifi_connected = true;
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("WiFi:: lost connection");
      wifi_connected = false;
      break;
  }
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}


void setup() {
  
  Serial.begin(115200);
  delay(100);
  
  // Init DHT22 Sensor
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dhtDelayMS = sensor.min_delay / 1000;
  readTime = millis();

  // Init Wifi
  wifi_connected = false; 
  Serial.printf("\n\nSetup:: connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  delay(1000);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(!wifi_connected) {
    //wait for wifi to finish connecting
    delay(10);
  }
  
}

void loop() {
  int updateReady = false;
  sensors_event_t event;
  double temp;
  double humidity;

  // Wait until we have a wifi connection
  if (!wifi_connected) {
    Serial.println("Loop:: waiting for WiFi connection...");
    delay(1000);
    return;
  }
  
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition above.
  MQTT_connect();

  
  // Read DHT22 if the right amount of time has passed
  if (millis() - readTime > dhtDelayMS) {
    readTime = millis();
    updateReady = true;
    
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println("Error reading temperature!");
      updateReady = false;
    } else {
      Serial.print("Temperature update: ");
      Serial.println(event.temperature);
      temp = event.temperature;
    }

    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println("Error reading humidity!");
      updateReady = false;
    } else {
      Serial.print("Humidity update: ");
      Serial.println(event.relative_humidity);
      humidity = event.relative_humidity;
    }
  }

  // Send an update to the server if we have one
  if (updateReady) {
    Serial.print(String("Sending temp: ") + temp + " and humidity: " + humidity + "\n");
    if (!tempPublish.publish(temp)) {
      Serial.println("Temp Publish Failed");
    }
    if (!humidityPublish.publish(humidity)){
      Serial.println("Humidity Publish Failed");
    }
    
  }

}
