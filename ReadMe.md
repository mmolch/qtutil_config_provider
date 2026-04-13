# **mmolch_qtutil_config_provider**
A lightweight Qt6 utility for **loading, merging, validating, watching, and updating JSON‑based configuration files** — with automatic schema validation, file watching, and UI‑friendly change notifications.

This library is designed for Qt applications that need **strongly typed settings**, **live updates**, and **safe error handling** using `std::expected`.

---

## Features
- **JSON Schema validation** (Draft‑7 subset)
- **Deep, schema‑aware merging** of multiple config layers
  (default, user, runtime, etc.)
- **Automatic file watching** with live reload
- **Auto‑save** with debounced write‑back
- **Thread‑safe access** to the current configuration
- **Qt‑native signals**:
  - `configChanged(diff)`
  - `errorOccurred(message)`
- **Exception‑free API** using `std::expected`
- Integrates cleanly with **Qt Widgets**, **QML**, or backend logic

---

## Example Use Case
The included example demonstrates a full round‑trip:

```
UI → ConfigProvider → JSON file → ConfigProvider → UI
```

You can modify the JSON file while the app is running — changes appear instantly in the UI.

---

## 🛠Installation

```bash
git clone https://github.com/<yourname>/mmolch_qtutil_config_provider
cd mmolch_qtutil_config_provider
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Requires:
- **Qt ≥ 6.4**
- **C++23 compiler**

---

## Quick Start

```cpp
using namespace mmolch::qtutil;

auto provider = ConfigProvider::create(
    "config.schema.json",
    {
        "config.default.json",
        "config.user.json",
        "runtime.json"
    }
);

if (!provider) {
    qWarning() << provider.error();
    return;
}

auto* cfg = provider.value();
connect(cfg, &ConfigProvider::configChanged, [](const QJsonObject& diff){
    qDebug() << "Config updated:" << diff;
});

cfg->updateConfig({{"volume", 42}});
```

---

## API Overview

### **Factory**
```cpp
static std::expected<ConfigProvider*, QString> create(
    const QString& schemaPath,
    const QStringList& configPaths,
    QObject* parent = nullptr
);
```
Validates schema → loads & merges configs → returns ready‑to‑use provider.

---

### **Reading**
```cpp
QJsonObject currentConfig() const;
```
Thread‑safe snapshot of the merged configuration.

---

### **Updating**
```cpp
bool updateConfig(const QJsonObject& diff);
```
Applies partial updates, validates them, merges them, and emits `configChanged`.

---

### **Runtime Controls**
```cpp
bool autoSaveEnabled() const;
void setAutoSaveEnabled(bool);

bool fileWatcherEnabled() const;
void setFileWatcherEnabled(bool);
```

---

### **Manual I/O**
```cpp
bool reload();  // Reload from disk
bool save();    // Flush pending changes immediately
```

---

### **Signals**
```cpp
void configChanged(const QJsonObject& diff);
void errorOccurred(const QString& message);
```

---

## Project Structure
```
mmolch_qtutil_config_provider/
 ├─ include/mmolch/qtutil_config_provider.h
 ├─ src/qtutil_config_provider.cpp
 ├─ libs/mmolch_qtutil_json/   # JSON utilities (load, merge, diff, validate)
 └─ example/                   # Full UI demo
```

---

## Example Screenshot (conceptual)
- Slider updates JSON
- JSON edits update UI
- Mute checkbox syncs both ways

---

## License
MIT License — free to use in commercial and open‑source projects.