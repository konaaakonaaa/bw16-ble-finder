# Железо и настройка среды

## Плата

- BW16 (модуль на чипе RTL8720DN, 2.4/5 ГГц WiFi + BLE 5.0)
- USB-кабель
- Опционально: активный/пассивный зуммер и/или отдельный светодиод

## Установка / компиляция

Раз у тебя уже стоит Arduino IDE с board package Realtek AmebaD (например,
после прошивки других BW16-проектов) — ничего дополнительно ставить не
нужно. Библиотека `AmebaBLE` (файл `BLEDevice.h`) идёт в комплекте с board
package, отдельно её ставить не требуется.

Если package ещё не стоит — инструкция:
https://www.amebaiot.com/en/amebad-bw16-arduino-getting-started/

## Загрузка прошивки

1. Tools → Board → выбери BW16 (Realtek AmebaD)
2. Подключи плату по USB, Tools → Port → выбери появившийся COM-порт
3. Если порт не появляется или загрузка не начинается — зажми
   кнопку BOOT/burn на плате при подключении/во время загрузки
   (так же, как при прошивке других скетчей на этой плате)
4. Sketch → Upload
5. После загрузки открой Serial Monitor на 115200 baud

## Если BLE не находит вообще никаких устройств

- Проверь, что искомое устройство действительно транслирует BLE-рекламу
  прямо сейчас (для наушников часто нужно включить режим "pairing"/сопряжения,
  иначе они молчат по BLE)
- Попробуй `GAP_SCAN_MODE_ACTIVE` вместо `PASSIVE` (или наоборот) в
  `setScanMode()`
- Убедись, что рядом действительно есть BLE-источники — можно проверить
  приложением nRF Connect на телефоне: если оно видит устройства, а BW16 нет,
  проблема в скетче/плате, а не в окружении

## Полезные ссылки

- Официальный пример BLEScan (на основе которого сделаны эти скетчи):
  https://github.com/Ameba-AIoT/ameba-arduino-d/blob/dev/Arduino_package/hardware/libraries/BLE/examples/BLEScan/BLEScan.ino
- Документация классов AmebaBLE (BLEDevice, BLEScan, BLEAdvertData):
  https://www.amebaiot.com/en/amebad-arduino-ble-scan/
- Форум разработчиков Ameba: https://forum.amebaiot.com/
