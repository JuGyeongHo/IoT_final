#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>

//1st wifi
const char* ssid = "NTH413";
const char* password = "cseenth413";
//2nd wifi
const char* ssid = "U+NetED24"
const char* password = "@KDF7BFBDB"

const char* mqtt_server = "192.168.219.113"; 
const int mqtt_port = 1883;
// const char* mqtt_user = "iot";
// const char* mqtt_pass = "12345";


// OLED Configuration
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Light configuration 
#define VALVE_PIN    21  // RIGHT

void showMessage(const String& line1, const String& line2 = "") {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(line1);
  if (line2 != "") display.println(line2);
  display.display();
}

//MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print(topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "control/light") {
    if(message == "on"){
      digitalWrite(VALVE_PIN, HIGH);
      Serial.println("[Light] ON");
      showMessage("[Light] ON", String(topic));
    }
    else if(message == "off"){
      digitalWrite(VALVE_PIN, LOW);
      Serial.println("[VALVE] OFF");
      showMessage("[VALVE] OFF", String(topic));
    }
    else{
      showMessage("Invalid Message", String(topic));
    }
  } 
}

//MQTT connect
void connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");

      client.subscribe("control/light");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

// WiFi Connection 
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    showMessage("WiFi connected!", WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed.");
    showMessage("WiFi failed");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);

  // OLED Initialization 
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 OLED init failed");
    while (true);
  }

  showMessage("Connecting to", ssid);
  connectWiFi();
  //MQTT connect
  connectMQTT();  
}

void loop() {
  server.handleClient();
  
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
}
