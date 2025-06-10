#include <WiFi.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C

struct APConfig {
    String ssid;
    String password;
    int RSSI;
};

std::vector<APConfig> wifiAPs = { // wifi ssid, password, min dBm
    {"YunseongChoe", "handong20", -90},
    {"Universe2022", "asdf1234", -90}
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

volatile bool wifi_connected = false;
std::vector<TaskHandle_t> wifiTaskHandles;
SemaphoreHandle_t connectMutex;
//show message
void showMessage(const String& line1, const String& line2 = "") {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(line1);
  if (line2 != "") display.println(line2);
  display.display();
}

// FreeRTOS WiFi 연결 Task
void wifiConnectTask(void *parameter) {
  int idx = (int)parameter;
  // -90이면 그냥 죽어 
  if (wifiAPs[idx].RSSI <= -90) {
    vTaskDelete(NULL);
    wifiTaskHandles[idx] = NULL;
  }
  //waiting 
  xSemaphoreTake(connectMutex, portMAX_DELAY); // Blocking
  if (wifi_connected) { // wifi connect 
    xSemaphoreGive(connectMutex);
    vTaskDelete(NULL);
    wifiTaskHandles[idx] = NULL;
  }

  WiFi.begin(wifiAPs[idx].ssid, wifiAPs[idx].password);
  int retry = 0;

  while (!wifi_connected && WiFi.status() != WL_CONNECTED && retry < 30) {//retry 
    delay(150);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED && !wifi_connected) {
    wifi_connected = true;
    Serial.printf("Connected to %s! IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    showMessage("WiFi connected!",(String)WiFi.SSID()+" "+WiFi.localIP().toString());
  } else {
    Serial.printf("Task %d: WiFi failed. ", idx);
    Serial.print(WiFi.status());
    Serial.println(wifi_connected);
  }
  
  {
    xSemaphoreGive(connectMutex);
    vTaskDelete(NULL);
    wifiTaskHandles[idx] = NULL;
  }
}

// WiFi Connection (여러 AP 병렬 연결)
void connectWiFi() {
  for (int i = 0; i < wifiAPs.size(); i++) {
    xTaskCreatePinnedToCore(
      wifiConnectTask,
      "WiFiTask",
      4096,
      (void*)i,
      1,
      &wifiTaskHandles[i],
      0
    );
  }
  int wait = 0;
  while (!wifi_connected && wait < 10) {
    delay(200);
    wait++;
  }
  if (!wifi_connected) {
    showMessage("WiFi ALL failed");
  }
}

/* RSSI 초기화 */
void resetAllRSSI() {
    for (auto& ap : wifiAPs) {
        ap.RSSI = -90;
    }
}

void reorderPriority() {
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_STA);
    int found = WiFi.scanNetworks(); // 네트워크 스캔
    
    int retry = 0;
    while (found <= 0 && retry < 10) {//retry 
      found = WiFi.scanNetworks(); // 네트워크 스캔
      delay(100);
      retry++;
    } 

    Serial.printf("\n네트워크 %d개 스캔됨\n", found);

    // 발견된 AP의 RSSI 값 갱신
    for (int i = 0; i < found; i++) {
        String ssid = WiFi.SSID(i);
        for (size_t j = 0; j < wifiAPs.size(); j++) {
            if (wifiAPs[j].ssid == ssid) {
                wifiAPs[j].RSSI = WiFi.RSSI(i);
                break;
            }
        }
    }

    // RSSI 기준 정렬 (높은 RSSI 우선)
    std::sort(wifiAPs.begin(), wifiAPs.end(),
        [](const APConfig& a, const APConfig& b) {
            return a.RSSI > b.RSSI;
        });

    // 출력
    Serial.println("\n[정렬된 AP 목록]");
    for (const auto& ap : wifiAPs) {
        Serial.printf("- %s (%ddBm)\n", ap.ssid.c_str(), ap.RSSI);
    }
}

void setup() {
  Serial.begin(115200);

  // OLED Initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 OLED init failed");
    while (true);
  }

  wifiTaskHandles.resize(wifiAPs.size(), NULL);
  connectMutex = xSemaphoreCreateMutex();

  showMessage("Connecting to WiFi");
  connectWiFi();
}

void loop() {
  static unsigned long wifiRetryTime = 0;
  static bool tryingReconnect = false;

  if (WiFi.status() != WL_CONNECTED) {
    if (!tryingReconnect && millis() - wifiRetryTime > 3000) {
      xSemaphoreTake(connectMutex, portMAX_DELAY); // Blocking
      resetAllRSSI();
      reorderPriority();
      xSemaphoreGive(connectMutex);

      showMessage("WiFi reconnecting...");
      tryingReconnect = true;
      
      wifi_connected = false;
      // 2. WiFi 완전 종료 및 STA 모드 전환
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      delay(500);
      WiFi.mode(WIFI_STA);
      delay(500);

      // 3. 다시 여러 AP 연결 시도
      connectWiFi();

      wifiRetryTime = millis();
      tryingReconnect = false;
    }
  }
  // 이미 연결되어 있으면, 아무런 추가 연결 시도 X
}