#include <FS.h>   // SPIFFS Filesystem

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <PubSubClient.h>
#include <Servo.h>
#include <stdlib.h>

#include <Adafruit_NeoPixel.h>

// WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


// this can stay test.mosquitto.org for the global clowder ;)
char mqtt_server[255] = "test.mosquitto.org";
char mqtt_port[6] = "1883";
char cat_name[34] = "33c3katze";

// Servo directly connected to GPIO2
Servo myservo;
const int servoPin = 5;
const int midPosition = 100;

// LEDs connected to GPIO0. They are running with a lower voltage (around 4.3V) so the 3.3V output level is enough to trigger high
const int LEDPin =  14;
const int numLEDs = 2;

const int TRIGGER_PIN = 4;


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(numLEDs, LEDPin, NEO_RGB + NEO_KHZ800);

auto RED = pixels.Color(255,0,0);
auto GREEN = pixels.Color(0, 255,0);
auto BLUE = pixels.Color(0, 0, 255);

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

uint32_t eyecolor = 0;

void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void wink();

void eye_debug(uint32_t color) {
  for ( int i = 0; i < numLEDs; i++ ) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
};

//flag for saving data
bool shouldSaveConfig = false;

void configModeCallback(WiFiManager *myWiFiManager) {
  eye_debug(pixels.Color(255,0,255));
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readConfig() {
  //clean FS, for testing
  //SPIFFS.format();

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(cat_name, json["cat_name"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

}

void setup() {
  // if we didn't use the serial output we could gain 2 GPIOs. 
  Serial.begin(115200);

  Serial.write("jimmkater booting....");

  eyecolor = pixels.Color(255, 0, 255);

  pixels.begin();

  eye_debug(GREEN);


  pinMode(TRIGGER_PIN, INPUT);

  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    SPIFFS.format();
    ESP.eraseConfig();
  };

  readConfig();
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  myservo.attach(servoPin);
  myservo.write(midPosition);
  eye_debug(pixels.Color(0,255,255));
  delay(50);
  myservo.detach();
  eye_debug(pixels.Color(0,0,0));
}

void setup_wifi() {
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_cat_name("cat_name", "the cat's name", cat_name, 32);

  
  WiFiManager wifiManager;

  eye_debug(BLUE);
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_cat_name);

  wifiManager.setConfigPortalTimeout(120);

  if (!wifiManager.autoConnect("Winkekatze")) {
    eye_debug(RED);
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(cat_name, custom_cat_name.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["cat_name"] = cat_name;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if ( String(topic) == String(cat_name) + "/paw/command" ) {
    char* p = (char*)malloc(length + 1);
    p[length] = 0;
  
    // Copy the payload to the new buffer
    memcpy(p, payload, length);
  
    int ircode = atoi(p);

    myservo.attach(servoPin);
    delay(30);
    
    if ( 0 <= ircode && ircode <= 170 ) {
      myservo.write(ircode);
    };
  
    delay(300);
    myservo.detach();
    free(p);
  } else if ( String(topic) == String(cat_name) + "/command" || String(topic) == "winkekatze/allcats" ) {
    wink();
  } else if ( String(topic) == "fux/door/status" ) {
    wink();
  } else if ( String(topic) == String(cat_name)+"/eye/set" ) {
    char* p = (char*)malloc(length + 1);
    p[length] = 0;
  
    // Copy the payload to the new buffer
    memcpy(p, payload, length);

    String colstr(p);
    if ( colstr == "pink" ) {
      eyecolor = pixels.Color(255, 0, 255);
    } else if ( colstr == "red" ) {
      eyecolor = pixels.Color(255, 0, 0);
    } else if ( colstr == "green" ) {
      eyecolor = pixels.Color(0, 255, 0);
    } else if ( colstr == "blue" ) {
      eyecolor = pixels.Color(0, 0, 255);
    } else if ( colstr == "cyan" ) {
      eyecolor = pixels.Color(0, 255, 255);
    } else if ( colstr == "yellow" ) {
      eyecolor = pixels.Color(255, 255, 0);
    }

    free(p);
  }
  
  client.publish((String(cat_name)+"/status").c_str(), "fishing");
}

void wink() {
  const float pi = 3.14;
  const float w = 2*pi/800;

  for ( int i = 0; i < numLEDs; i++) {
    pixels.setPixelColor(i, eyecolor);
  }
  pixels.show();

  myservo.attach(servoPin);

  delay(20);

  myservo.write(midPosition);
  delay(300);

  for( float t = 0.0; t < 2000; t += 15 ) {
     float pos = midPosition + 70.0*sin( w * t ) * pow(2.714, -(w/15.0) * t);
     myservo.write((int) pos);
     delay(15);
  };
  
  myservo.write(midPosition);
  delay(20);

  myservo.detach();

  for ( int i = 0; i < numLEDs; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0 ,0));
  }
  pixels.show();

  // myservo.detach();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(cat_name, (String(cat_name) + "/connected").c_str(), 2, true, "0")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish((String(cat_name) + "/connected").c_str(), "1", true);
      // ... and resubscribe
      client.subscribe( ( String(cat_name) + "/paw/command").c_str() );
      client.subscribe( (String(cat_name) + "/command" ).c_str() );
      client.subscribe("winkekatze/allcats");
      client.subscribe( (String(cat_name) + "/eye/set").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
