# Linky Decoder - ESP32-C3

![](./docs/github_banner.png)

A Wi-Fi-enabled decoder for the French **Linky** electricity meter, built around an ESP32-C3 module.

The firmware reads the meter's **TIC** (_Téléinformation Client_) UART stream, validates and parses each frame, and publishes the decoded labels to Home Assistant over MQTT - with auto-discovery, so every entity (indexes, instant current, apparent power, ...) appears on its own without manual YAML.

## 1. Features

- Reads the **TIC historique** stream (1200 baud, 7E1) from the meter's I1 / I2 terminals
- Decodes every standard historique label: identification, tariff option, indexes, instant / max current, apparent power, current period, HP/HC schedule, overcurrent warning, status word
- Per-group **checksum validation**; drops empty / truncated values before publish
- Per-label **dedupe cache**: a label is republished only when its value changes, so the broker isn't spammed every ~1.5 s
- **Home Assistant MQTT discovery**: entities self-register on every (re)connect, with units / device classes / state classes set so HA shows them as proper energy / current sensors
- LWT-backed **availability**: the device flips to `offline` in HA the second it loses MQTT
- HA -> device **command path** (switches / buttons): `on_command` handlers reach the firmware over the same connection
- Firmware **version sensor** added for free (`sw_version` from `git describe`)
- Split into reusable components:
  - `ha-mqtt` - Home Assistant discovery + entity publishing on top of an esp-mqtt wrapper with LWT
  - `wifi` - thin Wi-Fi station bring-up with a connect callback

## 2. How the TIC stream works

Every Linky meter carries a serial output called the **TIC** (_Téléinformation Client_). In **historique** mode (the default on most installs), the meter sends a frame every ~1.5 s at 1200 baud, 7E1, on the I1 / I2 terminals.

A frame is a sequence of groups framed by STX / ETX, each group laid out as `<LABEL> <VALUE> <CHECKSUM>`. Labels handled here:

```
ADCO  OPTARIF  ISOUSC
HCHC  HCHP     PTEC    HHPHC
IINST IMAX     PAPP
ADPS  MOTDETAT
```

The signal on I1 / I2 is current-loop modulated, so a small front-end (opto-isolator + demodulator) is needed between the meter and the ESP32-C3 UART. Any off-the-shelf téléinfo adapter works.

![tic frame diagram]

## 3. Hardware

### 3.1. Components

- 1 ESP32-C3 module
- 1 TIC-to-UART front-end (opto-isolated current-loop demodulator)
- Wiring to the meter's **I1 / I2** terminals (polarity-insensitive)

### 3.2. UART pinout

The RX pin is configurable via `idf.py menuconfig` (default **GPIO20**). The TIC line is read at 1200 baud, 7-bit even parity, 1 stop bit.

**Safety note:** I1 / I2 are SELV and isolated from mains inside the meter, but always work with the breaker off and keep an isolated front-end between the meter and the ESP if you're not sure.

![schematics]

## 4. Firmware

### 4.1. Architecture

```
Linky meter
   |
   v
TIC front-end -- UART --> uart.c --> tic.c (parser + cache)
                                         |
                                         v
                                       ha.c (discovery + topic resolution)
                                         |
                                         v
                                       mqtt.c (esp-mqtt + LWT) --> MQTT broker --> Home Assistant
                                         ^
                                         |
                                       wifi.c
```

Three project-owned layers:

- **`main`** - app glue: identity, entity table (`ha_entities.c`), TIC decoder
- **`components/ha-mqtt`** - Home Assistant discovery + esp-mqtt wrapper with LWT
- **`components/wifi`** - Wi-Fi station bring-up with a connect callback (`wifi_init(on_connected, arg)`)

### 4.2. Configuration

```
idf.py menuconfig
```

Navigate to:

```
Linky Decoder    -> UART RX pin + device identity (id, name, manufacturer, model)
Home Assistant   -> max entity / command counts
MQTT             -> broker URI
Wifi             -> SSID / password
```

### 4.3. Build and flash

A `Makefile` wraps `idf.py` so you don't have to remember the version-header step:

```
make build
make flash PORT=/dev/ttyACM0
make flash-monitor PORT=/dev/ttyACM0
```

Or the raw IDF commands:

```
. $IDF_PATH/export.sh
idf.py set-target esp32c3
idf.py menuconfig
idf.py build flash monitor
```

## 5. Home Assistant

Discovery is published retained on every MQTT connect, so once the device is online the entities appear on their own under _Settings -> Devices -> MQTT -> Linky Decoder_. No manual YAML needed.

![ha device screenshot]

Entity highlights:

| Entity                                           | Class            | Unit | Notes                                   |
| ------------------------------------------------ | ---------------- | ---- | --------------------------------------- |
| Off-peak Index (HCHC)                            | `energy`         | Wh   | `total_increasing`, zero / empty masked |
| Peak Index (HCHP)                                | `energy`         | Wh   | `total_increasing`, zero / empty masked |
| Apparent Power (PAPP)                            | `apparent_power` | VA   | `measurement`                           |
| Instant Current (IINST)                          | `current`        | A    | `measurement`                           |
| Subscribed Current (ISOUSC) / Max Current (IMAX) | `current`        | A    | diagnostic                              |
| Tariff Option / Current Period / HP-HC Schedule  | -                | -    | diagnostic                              |
| Overcurrent Warning (ADPS)                       | `current`        | A    | diagnostic                              |
| Compteur ID (ADCO) / Status Word (MOTDETAT)      | -                | -    | diagnostic                              |
| Version (free)                                   | -                | -    | `sw_version` from `git describe`        |

The `total_increasing` indexes carry a `value_template` that drops empty / zero readings, so HA's delta calc doesn't see a `0 -> real-index` jump as one day's worth of energy on every device restart.

_Made with ☕ and too many electrons._
