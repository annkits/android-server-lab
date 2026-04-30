# Backend + Android приложение для сбора данных о сотовой связи и местоположении

Учебный проект, состоящий из Android-приложения и C++ backend-сервера с графическим интерфейсом.

## Описание проекта

Приложение собирает данные о местоположении устройства и информации о ближайших сотовых вышках (LTE, GSM, NR), передаёт их по протоколу ZeroMQ на сервер.  
Сервер сохраняет данные в базу PostgreSQL, отображает текущую информацию и строит графики изменения уровня сигнала в реальном времени.

## Возможности

### Android-приложение (https://github.com/annkits/VP_HCI.git)

- **Сбор данных в фоне** через foreground-сервис (`DataCollectorService`)
- Получение координат (GPS + сеть)
- Получение информации о сотовых вышках:
  - LTE (4G): RSRP, RSRQ, RSSNR, dBm, PCI, CI, TAC и др.
  - GSM (2G): RSSI (dBm), LAC, CID
  - NR (5G): ssRSRP, ssRSRQ, ssSINR, PCI, NCI
- Сохранение данных в JSON-файлы (`location.json`, `telephony.json`)
- Передача данных на сервер по ZeroMQ (TCP)
- Отдельные экраны:
  - Калькулятор
  - Музыкальный плеер
  - Просмотр текущего местоположения
  - Просмотр информации о сотовых вышках
  - Тестирование сокетов

### Backend (C++) (https://github.com/annkits/android-server-lab.git)

- Приём данных по ZeroMQ (порт 6000)
- Парсинг JSON от Android
- Сохранение данных в базу **PostgreSQL**
- Хранение истории пакетов в памяти
- Графический интерфейс на **Dear ImGui + ImPlot**:
  - Отображение текущего местоположения
  - Отображение информации о всех видимых вышках
  - **Три графика в реальном времени**:
    - RSRP (Reference Signal Received Power)
    - dBm / RSSI (общая мощность сигнала)
    - SINR (Signal to Interference plus Noise Ratio)
  - **Интерактивная карта OpenStreetMap**:
    - Отображение трека передвижения
    - Поддержка зума и панорамирования
    - Автоматическая загрузка тайлов с OSM сервера
    - Кэширование тайлов на диск
    - Асинхронная загрузка в фоновых потоках
- Поддержка нескольких сот одновременно (разные цвета линий по PCI)
- Обработка сигналов завершения (Ctrl+C)

## Структура проекта
```
android-server-lab/
├── src/                    # C++ исходники
│   ├── main.cpp
│   ├── server.cpp
│   ├── gui.cpp
│   ├── osm_map.cpp
│   ├── map.cpp
│   ├── curl_utils.cpp
│   ├── tile_manager.cpp
│   └── shared.h
├── include/                    
│   ├── json.hpp
│   ├── zmq.hpp
│   ├── osm_map.h
│   ├── curl_utils.h
│   ├── tile_manager.h
│   ├── stb_image.h
│   └── shared.h
├── include/                    
│   ├── json.hpp
│   ├── zmq.hpp
│   └── shared.h
├── CMakeLists.txt
├── third_party/            # ImGui, ImPlot
├── assets/fonts/           # JetBrains Mono
│
└── Android-приложение/
├── app/src/main/java/com/example/myapplication/
│   ├── MainActivity.kt
│   ├── CalculatorActivity.kt
│   ├── MusicPlayerActivity.kt
│   ├── LocationActivity.kt
│   ├── TelephonyActivity.kt
│   ├── SocketsActivity.kt
│   └── DataCollectorService.kt
└── ...
```


## Технологии

**Backend:**
- C++20
- ZeroMQ (libzmq)
- nlohmann/json
- libpq (PostgreSQL)
- Dear ImGui + ImPlot
- SDL2 + OpenGL
- libcurl
- STB Image

**Android:**
- Kotlin
- ZeroMQ (org.zeromq:jeromq)
- Google Play Services Location
- TelephonyManager

**База данных:**
- PostgreSQL

## Как запустить

### 1. Backend

```bash
sudo apt install libzmq3-dev libpq-dev libsdl2-dev libglew-dev

mkdir build && cd build
cmake ..
make -j4

./backend
```

### 2. Android-приложение

- Открыть проект в Android Studio
- Запустить на устройстве или эмуляторе (Android 12+ рекомендуется)
- Разрешить все необходимые разрешения

### 3. База данных

```
createdb telecom_data
psql -d telecom_data -c "CREATE USER postgres WITH PASSWORD 'telecom_pass';"
```
(или изменить пароль в server.cpp в функции init_database())

## Что отображается в GUI

- Текущее местоположение (широта, долгота, высота, точность)
- Список всех видимых сотовых вышек с детальной информацией
- Три графика в реальном времени:
    - RSRP — качество reference-сигнала
    - dBm — общая мощность сигнала
    - SINR — отношение сигнал/шум
- Интерактивная карта OpenStreetMap:
    - Отображение текущего местоположения
    - Зум колесиком мыши, панорамирование перетаскиванием
    - Автоматическая подгрузка тайлов при перемещении
    - Кэширование тайлов для быстрого доступа