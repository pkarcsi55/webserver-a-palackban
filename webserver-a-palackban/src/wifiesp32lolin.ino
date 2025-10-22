// WEMOS Lolin32 Lite + BMP280 (HW-611) – 1 Hz mérés (állítható) + akku szűrés + reset + dinamikus Wi-Fi beállítás
// 2025 – Piláth verzió (kibővített Wi-Fi kezeléssel)

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "index_html.h"
#include "esp_bt.h"
#include <Preferences.h>   // Wi-Fi adatok tárolásához NVS-ben

// --- Wi-Fi ---
Preferences prefs;
char STA_SSID[32] = "xBagiNet";
char STA_PASS[64] = "x19920724";

// --- Globálisok ---
WebServer server(80);
Adafruit_BMP280 bmp;
bool bmp_ok = false;

// --- Akkumulátor-szűrés paraméterek ---
const float VMIN = 3.0f;          // 0%
const float VMAX = 3.8f;          // 100%
const float UBAT_ALPHA = 0.20f;   // EMA simítás (~5 mintás simítás 1 Hz-nél)
const int   BATTERY_PERCENT_STEP = 2; // százalék lépésköz (2%)

// --- Adattárolás ---
struct DataPoint {
  float T_C;
  float T_K;
  float P_hPa;
  float Ubat;
  int   Upercent;
  unsigned long timestamp;
};

const int MAX_DATA = 100;
DataPoint history[MAX_DATA];
int histIndex = 0;
int histCount = 0;
DataPoint lastData;

// --- Mérési időzítő ---
unsigned long lastMeasure = 0;
unsigned long MEASURE_INTERVAL = 1000; // 1 s alapértelmezetten

// --- Akkumulátor EMA állapot ---
bool  ubatFiltInit = false;
float ubatFilt = 0.0f;

// --- Akkumulátor mérés ---
float readBatteryVoltage() {
  int raw = analogRead(35);
  float Vadc = (raw / 4095.0f) * 3.3f;
  float Vbat = Vadc * 2.0f;
  return Vbat;
}

int filterAndPercent(float Vbat) {
  if (!ubatFiltInit) { ubatFilt = Vbat; ubatFiltInit = true; }
  else ubatFilt = UBAT_ALPHA * Vbat + (1.0f - UBAT_ALPHA) * ubatFilt;

  float p = (ubatFilt - VMIN) / (VMAX - VMIN) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;

  int rounded = (int)((p + BATTERY_PERCENT_STEP / 2.0f) / BATTERY_PERCENT_STEP) * BATTERY_PERCENT_STEP;
  if (rounded < 0) rounded = 0;
  if (rounded > 100) rounded = 100;
  return rounded;
}

// --- Wi-Fi kapcsolatkezelés ---
bool connectSTA(uint32_t timeout_ms = 10000) {
  WiFi.mode(WIFI_STA);

  // Mentett Wi-Fi adatok betöltése
  prefs.begin("wifi", true);
  String savedSSID = prefs.getString("ssid", STA_SSID);
  String savedPASS = prefs.getString("pass", STA_PASS);
  prefs.end();

  strncpy(STA_SSID, savedSSID.c_str(), sizeof(STA_SSID));
  strncpy(STA_PASS, savedPASS.c_str(), sizeof(STA_PASS));

  Serial.printf("[WiFi] Mentett AP: %s\n", STA_SSID);
  Serial.printf("[WiFi] Csatlakozás: %s\n", STA_SSID);

  WiFi.begin(STA_SSID, STA_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Sikeres csatlakozás!");
    Serial.print("[WiFi] IP cím: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  // Ha nem sikerült, új adatokat kér
  Serial.println("\n[WiFi] Sikertelen csatlakozás (timeout).");
  Serial.println("Adj meg új hálózati adatokat!");

  Serial.print("SSID: ");
  while (Serial.available() == 0) { delay(10); }
  String newSSID = Serial.readStringUntil('\n');
  newSSID.trim();

  Serial.print("Jelszó: ");
  while (Serial.available() == 0) { delay(10); }
  String newPASS = Serial.readStringUntil('\n');
  newPASS.trim();

  if (newSSID.length() == 0) {
    Serial.println("[WiFi] Üres SSID – megszakítva.");
    return false;
  }

  strncpy(STA_SSID, newSSID.c_str(), sizeof(STA_SSID));
  strncpy(STA_PASS, newPASS.c_str(), sizeof(STA_PASS));

  // Mentés NVS-be
  prefs.begin("wifi", false);
  prefs.putString("ssid", newSSID);
  prefs.putString("pass", newPASS);
  prefs.end();
  Serial.println("[WiFi] Új adatok elmentve, újrapróbálkozás...");

  // Új csatlakozási próbálkozás
  WiFi.begin(STA_SSID, STA_PASS);
  t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Sikeres csatlakozás az új adatokkal!");
    Serial.print("[WiFi] IP cím: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("[WiFi] Sikertelen a csatlakozás az új adatokkal is.");
    return false;
  }
}

// --- History frissítés ---
void addDataPoint(float T_C, float P_hPa, float Ubat, int Upercent) {
  DataPoint dp;
  dp.T_C = T_C;
  dp.T_K = T_C + 273.15f;
  dp.P_hPa = P_hPa;
  dp.Ubat = Ubat;
  dp.Upercent = Upercent;
  dp.timestamp = millis();

  history[histIndex] = dp;
  histIndex = (histIndex + 1) % MAX_DATA;
  if (histCount < MAX_DATA) histCount++;
  lastData = dp;
}

// --- HTTP handlerek ---
void handleRoot() {
  // Dinamikusan behelyettesíti az IP-címet a HTML-be
  String html = FPSTR(INDEX_HTML);
  html.replace("{{IP}}", WiFi.localIP().toString());
  server.send(200, "text/html; charset=utf-8", html);
}


void handleApiSensor() {
  if (!bmp_ok || histCount == 0) {
    server.send(503, "application/json", "{\"ok\":false,\"err\":\"Nincs mérési adat\"}");
    return;
  }
  char buf[220];
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"T_C\":%.2f,\"T_K\":%.2f,\"P\":%.1f,\"U\":%.2f,\"Upercent\":%d,\"t\":%lu}",
           lastData.T_C, lastData.T_K, lastData.P_hPa, lastData.Ubat, lastData.Upercent, lastData.timestamp);
  server.send(200, "application/json", buf);
}

void handleApiHistory() {
  String json = "[";
  int idx = histIndex;
  for (int i = 0; i < histCount; i++) {
    idx = (idx - 1 + MAX_DATA) % MAX_DATA;
    DataPoint dp = history[idx];
    char buf[220];
    snprintf(buf, sizeof(buf),
             "{\"T_C\":%.2f,\"T_K\":%.2f,\"P\":%.1f,\"U\":%.2f,\"Upercent\":%d,\"t\":%lu}",
             dp.T_C, dp.T_K, dp.P_hPa, dp.Ubat, dp.Upercent, dp.timestamp);
    json += buf;
    if (i < histCount - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiReset() {
  histIndex = 0;
  histCount = 0;
  ubatFiltInit = false;
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"History törölve\"}");
  Serial.println("[RESET] History törölve kliens kérésére.");
}

void handleApiSetRate() {
  if (!server.hasArg("interval")) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Hiányzó paraméter: interval\"}");
    return;
  }
  int newInt = server.arg("interval").toInt();
  if (newInt < 200) newInt = 200;
  if (newInt > 10000) newInt = 10000;
  MEASURE_INTERVAL = newInt;
  server.send(200, "application/json", "{\"ok\":true}");
  Serial.printf("[RATE] Mintavételi idő beállítva: %d ms\n", MEASURE_INTERVAL);
}

void handleNotFound() {
  server.send(404, "text/plain; charset=utf-8", "404 - Nincs ilyen oldal");
}

// --- Setup ---
void setup() {
  btStop();
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  while (!Serial); // vár a soros port csatlakozásra (pl. USB-n)
  delay(300);

  Wire.begin(19, 23);//Wire.begin(23, 19); // SDA=23, SCL=19 

  connectSTA(10000);

  if (!bmp.begin(0x77)) {
    Serial.println("[BMP280] Nem található a szenzor!");
    bmp_ok = false;
  } else {
    Serial.println("[BMP280] Sikeres inicializálás.");
    bmp_ok = true;
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_63
    );
  }

  server.on("/", handleRoot);
  server.on("/api/sensor", handleApiSensor);
  server.on("/api/history", handleApiHistory);
  server.on("/api/reset", handleApiReset);
  server.on("/api/setrate", handleApiSetRate);
  server.onNotFound(handleNotFound);
  server.begin();

  WiFi.setSleep(true);
  Serial.println("[HTTP] Webszerver elindítva (port 80).");
}

// --- Loop ---
void loop() {
  server.handleClient();

  if (bmp_ok && millis() - lastMeasure >= MEASURE_INTERVAL) {
    lastMeasure = millis();

    float T = bmp.readTemperature();
    float P = bmp.readPressure() / 100.0f;
    float U = readBatteryVoltage();
    int Upct = filterAndPercent(U);

    addDataPoint(T, P, U, Upct);
    Serial.printf("[Mérés] T=%.2f °C  P=%.1f hPa  U=%.2f V (%d%%)\n", T, P, U, Upct);
  }
}
