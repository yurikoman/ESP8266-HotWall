/*
  Basic ESP8266 MQTT example

  This sketch demonstrates the capabilities of the pubsub library in combination
  with the ESP8266 board/library.

  It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

  To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Update these with values suitable for your network.

#define RELAY_PIN 0
#define ONE_WIRE_BUS 2
#define BUILTIN_LEDs 1
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* ssid = "yuriko";
const char* password = "012345789";
const char* mqtt_server = "192.168.31.184";
float temp_prev;
unsigned long temp_last_send;
unsigned long time_last;
int wall_start, wall_stop;
bool publish_startstop;


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

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

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  char var[32];
  bool flag = false;
  int i;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    var[i] = (char)payload[i];
  }
  var[i] = (char)0;
  Serial.println();

  if (strcmp(topic, "wall/start") == 0) {
    wall_start = atoi(var);
    Serial.print("wall start = ");
    Serial.println(wall_start);
    flag = true;
  }
  if (strcmp(topic, "wall/stop") == 0) {
    wall_stop = atoi(var);
    Serial.print("wall stop = ");
    Serial.println(wall_stop);
    flag = true;
  }
  if (flag) {
    if (wall_start >= wall_stop) {
      wall_start = wall_stop - 5;
      snprintf (msg, 50, "%d", wall_start);
      client.publish("wall/start", msg);
    }
  }

  if (strcmp(topic, "wall/power") == 0) {
    if (atoi(var) > 0) {
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUILTIN_LEDs, HIGH);
      Serial.println("Button ON");
    } else {
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUILTIN_LEDs, LOW);
      Serial.println("Button OFF");
    }
  }
}

bool reconnect() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "mqtt:topic:bf7e3080:walltemp";
    // Attempt to connect
    if (client.connect(clientId.c_str(), "walltemp", "again+21")) {
      Serial.println("connected");
      client.subscribe("wall/start");
      client.subscribe("wall/stop");
      client.subscribe("wall/power");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      return false;
    }
  }
}

void setup() {
  wall_start = 11;
  wall_stop = 15;
  pinMode(RELAY_PIN, OUTPUT);
  //  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(BUILTIN_LEDs, OUTPUT);
  digitalWrite(BUILTIN_LEDs, LOW);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  sensors.begin();
  temp_last_send = 0;
  time_last = 0;
  publish_startstop = false;
}
void loop() {
  float temp;
  unsigned long now = millis();
  bool is_connected;

  if (now - time_last < 1000) {
    delay(100);
    return;
  }
  sensors.requestTemperatures(); // Send the command to get temperatures
  temp = sensors.getTempCByIndex(0);
  Serial.print("temp = ");
  Serial.println(temp);
  if (!client.connected()) {
    is_connected = reconnect();
  }
  if (!publish_startstop && client.connected()) {
    snprintf (msg, 50, "%d", wall_start);
    client.publish("wall/start", msg);
    snprintf (msg, 50, "%d", wall_stop);
    client.publish("wall/stop", msg);
    publish_startstop = true;
  }
  client.loop();
  if (temp < (float)wall_start) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BUILTIN_LEDs, HIGH);
    if (client.connected()) {
      client.publish("wall/power", "1");
    }
    Serial.println("Heating ON");
  } else if (temp > (float)wall_stop) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUILTIN_LEDs, LOW);
    if (client.connected()) {
      client.publish("wall/power", "0");
    }
    Serial.println("Heating OFF");
  }

  if (temp_prev != temp || now - temp_last_send > 300000) {
    if (client.connected()) {
      snprintf (msg, 50, "%.1f", temp);
      client.publish("wall/temp", msg);
      temp_last_send = millis();
      temp_prev = temp;
      Serial.print("send = ");
      Serial.println(msg);
    }
  }
  time_last = millis();
}
