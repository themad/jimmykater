#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <stdlib.h>
#include <Adafruit_NeoPixel.h>

// WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// this can stay test.mosquitto.org for the global clowder ;)
const char* mqtt_server = "test.mosquitto.org";

// Servo directly connected to GPIO2
Servo myservo;
const int servoPin = 2;
const int midPosition = 100;

// LEDs connected to GPIO0. They are running with a lower voltage (around 4.3V) so the 3.3V output level is enough to trigger high
const int LEDPin =  4;
const int numLEDs = 2;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(numLEDs, LEDPin, NEO_RGB + NEO_KHZ800);

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

uint32_t eyecolor = 0;

void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void wink();

void setup() {
  // if we didn't use the serial output we could gain 2 GPIOs. 
  Serial.begin(115200);

  Serial.write("jimmkater booting....");

  eyecolor = pixels.Color(255, 0, 255);

  pixels.begin();
  for ( int i = 0; i < numLEDs; i++ ) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  }

  pixels.show();

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  myservo.attach(servoPin);
  myservo.write(midPosition);
  delay(50);
  myservo.detach();
  for ( int i = 0; i < numLEDs; i++ ) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
}

void setup_wifi() {
  WiFiManager wifiManager;
  
  wifiManager.autoConnect("Winkekatze");
}

void callback(char* topic, byte* payload, unsigned int length) {
  if ( String(topic) == "jimmykater/paw/command" ) {
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
  } else if ( String(topic) == "jimmykater/command" || String(topic) == "winkekatze/allcats" ) {
    wink();
  } else if ( String(topic) == "fux/door/status" ) {
    wink();
  } else if ( String(topic) == "jimmykater/eye/set" ) {
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
  
  client.publish("jimmykater/status", "fishing");
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
    if (client.connect("jimmykater", "jimmykater/connected", 2, true, "0")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("jimmykater/connected", "1", true);
      // ... and resubscribe
      client.subscribe("jimmykater/paw/command");
      client.subscribe("jimmykater/command");
      client.subscribe("winkekatze/allcats");
      client.subscribe("jimmykater/eye/set");
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
