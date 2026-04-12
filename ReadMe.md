# **mmolch_qtutil_config_provider**
### *Schema‑validated, layered, auto‑reloading JSON configuration for Qt6*

`mmolch_qtutil_config_provider` is a lightweight Qt6 utility library that loads, validates, merges, and watches JSON configuration files. It is built on top of `mmolch_qtutil_json`, providing a high‑level, thread‑safe configuration provider suitable for Qt applications, daemons, and tools.

---

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

- **Thread‑safe access**
  - Uses `QReadWriteLock` for concurrent reads and safe writes

- **Diff‑based change notifications**
  - Emits only the keys that actually changed

---

## Installation

Add the library to your CMake project:

```cmake
add_subdirectory(mmolch_qtutil_config_provider)
target_link_libraries(your_target PRIVATE mmolch_qtutil_config_provider)
```

Requires:

- **Qt ≥ 6.4**
- **C++23**

---

## Basic Usage

```cpp
using namespace mmolch::qtutil;

ConfigProvider provider{
    "schema.json",
    {
        "/etc/myapp/config.json",
        "~/.config/myapp/config.json",
        "runtime.json"
    },
    ConfigProvider::DefaultOptions
};

connect(&provider, &ConfigProvider::configChanged, [](const QJsonObject &diff){
    qInfo() << "Config updated:" << diff;
});

auto cfg = provider.currentConfig();
```

To update configuration in memory:

```cpp
provider.updateConfig({
    {"window", QJsonObject{{"width", 900}}}
});
```

---

## Example

A full working example is included under `example/`, demonstrating:

- Schema validation
- Layered config merging
- Auto‑reload via `QFileSystemWatcher`
- Diff‑based UI updates

```cpp
Contact contact;
contact.configProvider().updateConfig({{"first_name", "Dieter"}});
```

---

## How It Works

`ConfigProvider` internally:

- Loads all configured JSON files
- Validates each file against the provided schema
- Merges them using `json_merge_inplace_with_schema()`
- Watches files for changes
- Emits:
  - `configChanged(diff)` on success
  - `errorOccurred(message)` on validation or parse errors

From the document:
> “Updates config in memory and schedules save if EnableAutoSave is set.”
> “Emitted when the configuration changes… containing ONLY the keys that were altered.”

---

## Building

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Enable tests:

```bash
cmake -DBUILD_TESTING=ON ..
```

---

## License

MIT License — see `LICENSE` for details.
