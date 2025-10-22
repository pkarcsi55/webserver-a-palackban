# 💧 Webserver a palackban
ESP32-alapú hőmérséklet- és nyomásmérő rendszer GY-BM280 szenzorral, beépített webes felülettel.

## 🔧 Hardver
- **Mikrokontroller:** Wemos Lolin 32 (ESP32)
- **Szenzor:** GY-BM280 (Bosch BMP280 kompatibilis)
- **Tápfeszültség:** 3,3 V (USB-ről vagy akkumulátorról)
- **Kommunikáció:** Wi-Fi – beépített webserver
- **Bekötés:**
  | BMP280 láb | ESP32 láb | Megjegyzés |
  |-------------|-----------|------------|
  | VCC         | 3,3 V     |            |
  | GND         | GND       |            |
  | SCL         | 19        | I²C óra    |
  | SDA         | 23        | I²C adat   |
  | CSB         | 3,3 V     | I²C módhoz |
  | SD0         | 3,3 V     | I²C cím fixálása |

## 📦 Könyvtárak
A program az alábbi Arduino-kiegészítéseket igényli:
- **Adafruit BMP280 Library**
- **Adafruit Unified Sensor**
- **WiFi.h** és **WebServer.h** (beépített ESP32-könyvtárak)

Telepítés az Arduino Library Managerből ajánlott.

## 💻 Fő funkciók
- Valós idejű hőmérséklet- és nyomásadatok megjelenítése.
- Akkumulátorfeszültség kijelzés.
- Webes felület, amely megjeleníti az utolsó 100 mérési adatot.
- Mintavételi idő állítható (200–10000 ms).
- Adatok másolása Excel-kompatibilis formátumban.
- QR-kód az eszköz IP-címéhez.

## 🧠 Kód felépítése
- **`wifiesp32lolin.ino`**  
  Wi-Fi-kapcsolat létrehozása, BMP280-szenzor inicializálása és REST API-végpontok (`/api/sensor`, `/api/history`, `/api/reset`, `/api/setrate`) kiszolgálása.
- **`index_html.h`**  
  A beágyazott webes felület HTML + CSS + JavaScript kódja (PROGMEM-ben tárolva).

## 🌐 Használat
1. Töltsd fel a kódot az ESP32-re.  
2. A soros monitoron megjelenik az IP-cím.  
3. A böngészőben írd be: `http://<IP_cím>`  
4. A felület megjeleníti a méréseket és a történetet.

## 📷 Példaképek
(lásd a `docs/` és `images/` mappákat)

## ⚙️ További ötletek
- Adatok mentése SD-kártyára.
- Grafikon rajzolása a webes felületen.
- MQTT támogatás integrálása.

## 📄 Licenc
MIT License – szabadon felhasználható oktatási célokra.
