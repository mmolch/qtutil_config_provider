# **mmolch_qtutil_config_provider**
### *Schema‑validated, layered, auto‑reloading JSON configuration for Qt6*

`mmolch_qtutil_config_provider` is a lightweight Qt6 utility library that loads, validates, merges, and watches JSON configuration files. It is built on top of `mmolch_qtutil_json`, providing a high‑level, thread‑safe configuration provider suitable for Qt applications, daemons, and tools.

## Features

- **Layered configuration loading**
  - Application, system, user, and ephemeral config layers
  - Deep merge with schema‑aware merge strategies

- **JSON Schema validation**
  - Validates every config file before applying changes

- **Automatic file watching**
  - Reloads configuration when files change on disk

- **Auto‑save support**
  - In‑memory updates are flushed to disk automatically

- **Diff‑based change notifications**
  - Emits only the keys that actually changed

- **Requirements**
  - **Qt ≥ 6.4**
  - **C++23 compiler**

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

## API Overview

### **Factory**
```cpp
static std::expected<ConfigProvider*, QString> create(
    const QString& schemaPath,
    const QStringList& configPaths,
    std::unique_ptr<ConfigValidator> validator = nullptr,
    QObject* parent = nullptr
);
```
Validates schema → loads & merges configs → returns ready‑to‑use provider.

### **Reading**
```cpp
QJsonObject currentConfig() const;
```

### **Updating**
```cpp
bool updateConfig(const QJsonObject& diff);
```
Applies partial updates, validates them, merges them, and emits `configChanged`.

### **Runtime Controls**
```cpp
bool autoSaveEnabled() const;
void setAutoSaveEnabled(bool);

bool fileWatcherEnabled() const;
void setFileWatcherEnabled(bool);
```

### **Manual I/O**
```cpp
bool reload();  // Reload from disk
bool save();    // Flush pending changes immediately
```

### **Signals**
```cpp
void configChanged(const QJsonObject& diff);
void errorOccurred(const QString& message);
```
## License
MIT License — free to use in commercial and open‑source projects.