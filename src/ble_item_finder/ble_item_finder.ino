/*
 * BW16 BLE Item Finder ("Горячо-холодно")
 * ----------------------------------------
 * Ищет конкретное BLE-устройство (по MAC-адресу или по имени) и подсказывает
 * насколько ты близко к нему: чем ближе — тем чаще мигает светодиод и выше
 * тон зуммера. Основано на силе принимаемого сигнала (RSSI).
 *
 * ПРИМЕНЕНИЕ:
 *   - Ищешь наушники/трекер/телефон, который транслирует BLE (у многих
 *     наушников и фитнес-браслетов BLE включён почти постоянно)
 *   - Хочешь понять, в какой стороне комнаты/дома находится устройство
 *
 * КАК НАСТРОИТЬ ПОД СВОЁ УСТРОЙСТВО:
 *   1. Сначала прошей examples/anti_tracker_alert/anti_tracker_alert.ino
 *      ИЛИ просто временно включи вывод всех адресов ниже (см. комментарий
 *      "РЕЖИМ ОТЛАДКИ"), чтобы увидеть MAC-адрес твоего устройства в
 *      Serial Monitor, пока оно рядом с платой.
 *   2. Впиши найденный MAC в TARGET_MAC ниже (или имя в TARGET_NAME,
 *      если MAC не хочешь искать вручную и устройство транслирует имя).
 *
 * ВАЖНО:
 *   RSSI — шумная метрика, зависит от препятствий, ориентации антенны и
 *   мощности передатчика конкретного устройства. Это не GPS и не точный
 *   дальномер, а именно "горячо-холодно". Также не все устройства
 *   рекламируют себя (advertise) постоянно — у некоторых BLE "спит"
 *   большую часть времени, especially если уже подключены к телефону.
 *
 * ЖЕЛЕЗО:
 *   - BW16 (RTL8720DN)
 *   - Опционально: активный зуммер на BUZZER_PIN, светодиод на LED_PIN
 *     (по умолчанию используется встроенный)
 */

#include "BLEDevice.h"

// ---------- Настройки поиска ----------
// Впиши MAC-адрес искомого устройства (формат "AA:BB:CC:DD:EE:FF"),
// либо оставь пустым "" и укажи TARGET_NAME вместо этого.
#define TARGET_MAC   ""
#define TARGET_NAME  "MyEarbuds"   // используется только если TARGET_MAC == ""

// РЕЖИМ ОТЛАДКИ: если true — в Serial Monitor печатается КАЖДОЕ найденное
// BLE-устройство (адрес, имя, RSSI), чтобы можно было подсмотреть MAC своего
// устройства. Включи на первый запуск, потом верни обратно в false.
#define DEBUG_PRINT_ALL_DEVICES  true

// ---------- Настройки индикации ----------
#define LED_PIN            LED_BUILTIN
#define BUZZER_PIN         4          // подключи активный/пассивный зуммер сюда, либо не подключай вообще
#define LOST_TIMEOUT_MS    4000       // если сигнал не появлялся дольше этого - считаем "не найдено"

// ---------- Состояние ----------
BLEAdvertData foundDevice;
volatile int8_t  lastRssi    = -127;
volatile uint32_t lastSeenMs = 0;
volatile bool     everFound  = false;

bool isTargetDevice() {
  if (strlen(TARGET_MAC) > 0) {
return String(foundDevice.getAddr().str()).equalsIgnoreCase(TARGET_MAC);
  }
  if (foundDevice.hasName()) {
    return foundDevice.getName() == String(TARGET_NAME);
  }
  return false;
}

// ---------- Callback сканирования ----------
void scanFunction(T_LE_CB_DATA *p_data) {
  foundDevice.parseScanInfo(p_data);

  if (DEBUG_PRINT_ALL_DEVICES) {
    Serial.print("[видно] addr=");
    Serial.print(foundDevice.getAddr().str());
    Serial.print("  rssi=");
    Serial.print(foundDevice.getRSSI());
    if (foundDevice.hasName()) {
      Serial.print("  name=");
      Serial.print(foundDevice.getName());
    }
    Serial.println();
  }

  if (isTargetDevice()) {
    lastRssi   = foundDevice.getRSSI();
    lastSeenMs = millis();
    everFound  = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println();
  Serial.println("=== BW16 BLE Item Finder ===");
  if (strlen(TARGET_MAC) > 0) {
    Serial.print("Ищу устройство по MAC: ");
    Serial.println(TARGET_MAC);
  } else {
    Serial.print("Ищу устройство по имени: ");
    Serial.println(TARGET_NAME);
  }
  if (DEBUG_PRINT_ALL_DEVICES) {
    Serial.println("(режим отладки включён - видно все окружающие устройства)");
  }

  BLE.init();
  BLE.configScan()->setScanMode(GAP_SCAN_MODE_ACTIVE);
  BLE.configScan()->setScanInterval(100);   // сканировать часто...
  BLE.configScan()->setScanWindow(90);      // ...и почти всё время слушать эфир
  BLE.configScan()->updateScanParams();
  BLE.setScanCallback(scanFunction);
  BLE.beginCentral(0);
  BLE.configScan()->startScan();  // без аргумента = сканировать непрерывно

  Serial.println("Сканирование запущено.");
}

void loop() {
  uint32_t now = millis();
  bool signalFresh = everFound && (now - lastSeenMs < LOST_TIMEOUT_MS);

  if (!signalFresh) {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    static uint32_t lastMsg = 0;
    if (now - lastMsg > 2000) {
      Serial.println("... сигнал не найден, ищу ...");
      lastMsg = now;
    }
    delay(100);
    return;
  }

  // RSSI обычно в диапазоне примерно -30 (вплотную) .. -100 (на грани приёма).
  // Чем ближе к -30, тем "горячее".
  int8_t rssi = lastRssi;
  int32_t rssiClamped = constrain((int32_t)rssi, -100, -30);

  uint32_t blinkIntervalMs = map(rssiClamped, -100, -30, 800, 60);   // дальше - реже мигаем
  int toneFreqHz           = map(rssiClamped, -100, -30, 300, 1800); // дальше - ниже тон

  digitalWrite(LED_PIN, (now / blinkIntervalMs) % 2);
  tone(BUZZER_PIN, toneFreqHz);

  static uint32_t lastReport = 0;
  if (now - lastReport > 500) {
    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm  (чем ближе к 0 - тем ближе устройство)");
    lastReport = now;
  }
}
