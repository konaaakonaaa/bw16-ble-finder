/*
 * BW16 Anti-Tracker Alert
 * ------------------------
 * Слушает BLE-эфир и запоминает АНОНИМНЫЕ (без рекламируемого имени)
 * устройства вокруг. Если какое-то анонимное устройство продолжает
 * "видеться" стабильно долгое время (по умолчанию 15 минут) — это
 * подозрительно похоже на трекер (типа AirTag/аналогов), который
 * кто-то мог подложить в сумку, машину или одежду, чтобы следить за
 * перемещениями.
 *
 * КАК ЭТО РАБОТАЕТ (и в чём ограничения):
 *   - Ведётся таблица увиденных анонимных MAC-адресов с моментом первого
 *     и последнего появления.
 *   - Если адрес не появлялся дольше STALE_TIMEOUT_MS - запись удаляется
 *     (считаем, что устройство просто проехало мимо/осталось в другом месте).
 *   - Если адрес продолжает появляться дольше PERSISTENCE_THRESHOLD_MS
 *     без перерывов больше STALE_TIMEOUT_MS - подаём тревогу.
 *
 *   ЭТО ЭВРИСТИКА, А НЕ ДОКАЗАТЕЛЬСТВО. Ложные срабатывания возможны:
 *   - твои же устройства (наушники, часы, весы) тоже анонимны по BLE-адресу
 *     и будут постоянно рядом - стоит занести их в белый список (см. ниже)
 *   - соседские стационарные BLE-устройства (умные лампочки, датчики)
 *     будут "висеть" рядом долго, но неподвижно - это не трекер, а просто
 *     соседский гаджет за стеной
 *
 * БЕЛЫЙ СПИСОК:
 *   Впиши MAC-адреса СВОИХ устройств в WHITELIST[], чтобы не получать
 *   ложные тревоги на собственные наушники/часы.
 *
 * ЖЕЛЕЗО:
 *   - BW16 (RTL8720DN)
 *   - Опционально: зуммер/светодиод для тревоги
 */

#include "BLEDevice.h"

// ---------- Настройки ----------
#define MAX_TRACKED                24
#define PERSISTENCE_THRESHOLD_MS   (15UL * 60UL * 1000UL)  // 15 минут непрерывного присутствия -> тревога
#define STALE_TIMEOUT_MS           (5UL  * 60UL * 1000UL)  // 5 минут без сигнала -> забыть устройство
#define LED_ALERT_PIN              LED_BUILTIN
#define BUZZER_PIN                 4

// Впиши сюда MAC-адреса своих собственных BLE-устройств (наушники, часы,
// весы и т.п.), чтобы не тревожиться на них. Формат "AA:BB:CC:DD:EE:FF".
const char *WHITELIST[] = {
  // "AA:BB:CC:DD:EE:FF",
  // "11:22:33:44:55:66",
};
const uint8_t WHITELIST_COUNT = sizeof(WHITELIST) / sizeof(WHITELIST[0]);

// ---------- Структура для отслеживаемого устройства ----------
struct TrackedDevice {
  char     addr[18];   // "AA:BB:CC:DD:EE:FF" + '\0'
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint16_t sightings;
  int8_t   lastRssi;
  bool     alerted;
  bool     used;
};

TrackedDevice tracked[MAX_TRACKED];
BLEAdvertData foundDevice;

bool isWhitelisted(const String &addr) {
  for (uint8_t i = 0; i < WHITELIST_COUNT; i++) {
    if (addr.equalsIgnoreCase(WHITELIST[i])) return true;
  }
  return false;
}

int findTrackedSlot(const String &addr) {
  for (uint8_t i = 0; i < MAX_TRACKED; i++) {
    if (tracked[i].used && addr.equalsIgnoreCase(tracked[i].addr)) return i;
  }
  return -1;
}

int findFreeSlot() {
  for (uint8_t i = 0; i < MAX_TRACKED; i++) {
    if (!tracked[i].used) return i;
  }
  return -1; // таблица заполнена - придётся подождать, пока что-то устареет
}

// ---------- Callback сканирования ----------
void scanFunction(T_LE_CB_DATA *p_data) {
  foundDevice.parseScanInfo(p_data);

  // Интересуют только анонимные устройства (без имени) - у большинства
  // трекеров-маячков имя либо не транслируется, либо это generic-имя.
  if (foundDevice.hasName()) return;

  String addr = foundDevice.getAddr().str();
  if (isWhitelisted(addr)) return;

  uint32_t now = millis();
  int slot = findTrackedSlot(addr);

  if (slot < 0) {
    slot = findFreeSlot();
    if (slot < 0) return; // таблица переполнена, пропускаем это устройство в этот раз
    addr.toCharArray(tracked[slot].addr, sizeof(tracked[slot].addr));
    tracked[slot].firstSeenMs = now;
    tracked[slot].sightings   = 0;
    tracked[slot].alerted     = false;
    tracked[slot].used        = true;
  }

  tracked[slot].lastSeenMs = now;
  tracked[slot].lastRssi   = foundDevice.getRSSI();
  tracked[slot].sightings++;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_ALERT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  for (uint8_t i = 0; i < MAX_TRACKED; i++) tracked[i].used = false;

  Serial.println();
  Serial.println("=== BW16 Anti-Tracker Alert ===");
  Serial.printf("Порог тревоги: %lu мин непрерывного присутствия\n",
                (unsigned long)(PERSISTENCE_THRESHOLD_MS / 60000UL));
  Serial.printf("В белом списке: %u устройств\n", WHITELIST_COUNT);

  BLE.init();
  BLE.configScan()->setScanMode(GAP_SCAN_MODE_PASSIVE); // пассивный режим экономит трафик эфира, нам не нужен scan response
  BLE.configScan()->setScanInterval(1000);
  BLE.configScan()->setScanWindow(300);
  BLE.configScan()->updateScanParams();
  BLE.setScanCallback(scanFunction);
  BLE.beginCentral(0);
  BLE.configScan()->startScan(); // непрерывное сканирование

  Serial.println("Мониторинг запущен.");
}

void loop() {
  uint32_t now = millis();

  bool anyAlert = false;

  for (uint8_t i = 0; i < MAX_TRACKED; i++) {
    if (!tracked[i].used) continue;

    // Устарело - забываем
    if (now - tracked[i].lastSeenMs > STALE_TIMEOUT_MS) {
      tracked[i].used = false;
      continue;
    }

    uint32_t presenceDuration = now - tracked[i].firstSeenMs;

    if (!tracked[i].alerted && presenceDuration > PERSISTENCE_THRESHOLD_MS) {
      tracked[i].alerted = true;
      anyAlert = true;
      Serial.println();
      Serial.println(">>> ПОДОЗРИТЕЛЬНОЕ УСТРОЙСТВО <<<");
      Serial.printf("addr=%s  рядом уже %lu мин  сигналов=%u  последний rssi=%d\n",
                    tracked[i].addr,
                    (unsigned long)(presenceDuration / 60000UL),
                    tracked[i].sightings,
                    tracked[i].lastRssi);
      Serial.println("Если это не твой гаджет - возможно, кто-то подложил трекер.");
    }
  }

  // Индикация: тревога держится, пока хотя бы одно устройство помечено alerted
  bool alertActive = false;
  for (uint8_t i = 0; i < MAX_TRACKED; i++) {
    if (tracked[i].used && tracked[i].alerted) { alertActive = true; break; }
  }

  if (alertActive) {
    digitalWrite(LED_ALERT_PIN, (now / 300) % 2);
    if ((now / 300) % 2) tone(BUZZER_PIN, 1500); else noTone(BUZZER_PIN);
  } else {
    digitalWrite(LED_ALERT_PIN, (now / 1000) % 10 == 0); // редкий heartbeat
    noTone(BUZZER_PIN);
  }

  // Периодическая сводка
  static uint32_t lastReport = 0;
  if (now - lastReport > 15000) {
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_TRACKED; i++) if (tracked[i].used) activeCount++;
    Serial.printf("[stats] сейчас отслеживается анонимных устройств: %u/%u\n",
                  activeCount, MAX_TRACKED);
    lastReport = now;
  }

  delay(200);
}
