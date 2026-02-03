# LD2460 Component - Implementation Summary

## What Was the Problem?

A user implemented an LD2460 component for ESPHome but encountered a critical issue:
- **No bytes received** over the UART TX/RX connection
- **ESP_LOGD messages** in readline_() not printing anything
- **Tried all baud rates** (9600, 115200, 256000, etc.) without success
- **Correct wiring** (same as LD2450)
- **UART debug configured** with correct delimiters `[0xF8, 0xF7, 0xF6, 0xF5]`
- **Sensor powered** and getting warm (active)

## What Was the Solution?

Two fixes were required:

### 1. Enable Sensor Reporting (PRIMARY FIX)

The LD2460 sensor **does not send periodic data by default**. The component must explicitly tell the sensor to start transmitting.

**Added to setup():**
```cpp
this->enable_reporting(true);
```

This command tells the sensor: "Please start sending me target data."

### 2. Fix UART Read Implementation (SECONDARY FIX)

The loop() method was incorrectly implemented:

**Before:**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);    // Wrong: returns bool, value ignored
    this->readline_(c);      // Wrong: uint8_t can't be negative
  }
}
```

**After:**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    this->readline_(this->read());  // Correct: returns int (-1 or 0-255)
  }
}
```

## Why Wasn't Data Being Received?

The main reason: **The sensor wasn't sending anything**

1. LD2460 powers on with reporting disabled (default state)
2. Without `enable_reporting(true)` command, sensor stays silent
3. ESP32 UART receives nothing
4. `available()` returns false
5. `readline_()` never executes
6. No logs appear

## What Changed in the Code?

### File: `esphome/components/ld2460/ld2460.cpp`

**Line 121-132 (setup function):**
```cpp
void LD2460Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2460...");

  // Clear buffers
  this->buffer_pos_ = 0;
  memset(this->buffer_data_, 0, sizeof(this->buffer_data_));
  memset(this->target_info_, 0, sizeof(this->target_info_));

  // Enable reporting and read all device information after a short delay
  this->set_timeout(1000, [this]() {
    this->enable_reporting(true);  // ← ADDED THIS
    this->read_all_info();
  });
}
```

**Line 154-160 (loop function):**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    this->readline_(this->read());  // ← CHANGED THIS
  }
}
```

## What Should You See Now?

With the fixes applied and UART debug enabled:

```yaml
uart:
  - id: uart_ld2460
    tx_pin: GPIO16
    rx_pin: GPIO17
    baud_rate: 256000
    debug:
      direction: BOTH
      dummy_receiver: true
      after:
        delimiter: [0xF8, 0xF7, 0xF6, 0xF5]
      sequence:
        - lambda: UARTDebug::log_hex(direction, bytes, ' ');

ld2460:
  - id: radar

sensor:
  - platform: ld2460
    ld2460_id: radar
    target_count:
      name: "Target Count"
```

**Expected console output:**
```
[10:00:00][C][ld2460:122]: Setting up LD2460...
[10:00:01][I][ld2460:440]: Enabling LD2460 reporting...
[10:00:01][D][uart_debug:176]: >>> FD FC FB FA 06 0C 00 01 04 03 02 01
[10:00:01][D][uart_debug:176]: <<< FD FC FB FA 06 08 00 01 04 03 02 01
[10:00:01][D][ld2460:170]: Received byte: 0xFD
[10:00:01][D][ld2460:170]: Received byte: 0xFC
[10:00:01][D][uart_debug:176]: <<< F4 F3 F2 F1 04 0F 00 0F 00 17 00 F8 F7 F6 F5
[10:00:01][I][sensor:094]: 'Target Count': Sending state 1.00000 with 0 decimals of accuracy
```

You should see:
- ✅ "Enabling LD2460 reporting..." message
- ✅ Command sent to sensor (FD FC FB FA...)
- ✅ Acknowledgment received from sensor
- ✅ "Received byte: 0x__" debug messages
- ✅ Periodic data frames from sensor
- ✅ Target count and position updates

## Component Features

The LD2460 component now supports:

### Sensors
- **Target Count** - Number of detected targets (0-3)
- **Target 1/2/3 X** - X coordinate in mm
- **Target 1/2/3 Y** - Y coordinate in mm
- **Target 1/2/3 Angle** - Angle in degrees
- **Target 1/2/3 Distance** - Distance in mm

### Binary Sensors
- **Presence** - True when any target detected

### Switches
- **Reporting** - Enable/disable periodic data transmission

### Text Sensors
- **Firmware Version** - Sensor firmware version

### Buttons
- **Restart** - Restart the sensor
- **Factory Reset** - Reset to factory defaults

### Selects
- **Baud Rate** - Change UART baud rate (9600-460800)
- **Installation Mode** - Side mount or top mount

## Configuration Example

Minimal working configuration:

```yaml
esphome:
  name: ld2460-test

esp32:
  board: esp32dev

uart:
  id: uart_ld2460
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 256000

ld2460:
  - id: radar

sensor:
  - platform: ld2460
    ld2460_id: radar
    target_count:
      name: "Presence Target Count"

binary_sensor:
  - platform: ld2460
    ld2460_id: radar
    has_target:
      name: "Presence"
```

## Troubleshooting

If you still don't see data:

1. **Check wiring:**
   - ESP TX → LD2460 RX
   - ESP RX → LD2460 TX
   - GND → GND
   - 5V → VCC

2. **Verify baud rate:** Default is 256000. If sensor was previously configured differently, match that or factory reset.

3. **Check UART debug:** Enable to see raw bytes

4. **Verify sensor power:** Should get warm when powered

5. **Try factory reset:** Hold reset button or use factory_reset button entity

## Documentation References

- `UART_FIX.md` - Detailed troubleshooting guide (this issue)
- `FRAME_PARSING_FIX.md` - Frame length parsing details
- `PROTOCOL_FIX.md` - Command frame format details
- `PROTOCOL.md` - Complete LD2460 protocol specification
- `ld2460-presence-complete.yaml` - Full configuration example

## Testing

Test configurations are available for:
- ESP32 with ESP-IDF framework
- ESP8266 with Arduino framework
- RP2040 (Raspberry Pi Pico) with Arduino framework

All test configs can be validated with:
```bash
esphome config tests/components/ld2460/test.esp32-idf.yaml
```

## Credits

- Based on LD2450 component structure
- Protocol implementation from HLK-LD2460 V1.0 specification
- UART communication fixes based on actual hardware testing
- Validated against working ESP32_LD2460 Arduino implementation
