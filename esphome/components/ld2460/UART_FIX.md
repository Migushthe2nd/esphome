# LD2460 UART Communication Fix - Troubleshooting Guide

## Problem Description

**Symptom:** "I don't get any bytes over the tx/rx connection, as nothing is printed."

The user implemented an LD2460 component and:
- ✗ No bytes received over UART
- ✗ ESP_LOGD("Received byte: 0x%02X", c) doesn't print anything
- ✗ Tried all baud rates (9600, 115200, 256000, etc.)
- ✗ UART debug with correct delimiters shows no incoming data
- ✓ Pins connected correctly (same as LD2450)
- ✓ Sensor is powered and warm (active)

## Root Causes Identified

### 1. Missing Sensor Reporting Enablement (PRIMARY CAUSE)

**Problem:** The LD2460 sensor does not send periodic data frames by default.

**Evidence:**
- No periodic data frames received from sensor
- UART TX works (commands sent to sensor visible in UART debug)
- UART RX appears silent (no data from sensor)

**Fix:** Call `enable_reporting(true)` during setup

```cpp
void LD2460Component::setup() {
  // ... initialization ...
  this->set_timeout(1000, [this]() {
    this->enable_reporting(true);  // ← Added this
    this->read_all_info();
  });
}
```

### 2. Incorrect loop() Implementation (SECONDARY ISSUE)

**Problem:** The loop() method was using `read_byte(&c)` and passing a `uint8_t` to `readline_()`, which expects an `int`.

**Before (Incorrect):**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);  // Returns bool
    this->readline_(c);    // Passes uint8_t (0-255, never negative)
  }
}

void LD2460Component::readline_(int readch) {
  if (readch < 0) {  // ← This check never triggers!
    return;
  }
  uint8_t c = (uint8_t) readch;
  ESP_LOGD(TAG, "Received byte: 0x%02X", c);  // ← Would still execute
  // ... rest of processing
}
```

**Issue:** When `uint8_t c` is passed to a function expecting `int`, it's promoted to `int` but the value remains 0-255. The check `if (readch < 0)` never triggers because `c` can't be negative.

However, this wouldn't prevent the ESP_LOGD from executing - it would just mean the early return never happens.

**After (Correct):**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    this->readline_(this->read());  // ← Returns int: -1 or 0-255
  }
}
```

**Why this is better:**
- `read()` returns `int`: -1 if no data, or byte value 0-255
- Follows the pattern used by LD2450 and other UART components
- Allows readline_() to properly detect no-data condition

## Why No Bytes Were Received

The primary reason was **missing `enable_reporting(true)` call**:

1. LD2460 sensor powers on with reporting disabled (or in a default state)
2. Without explicit command to enable reporting, sensor doesn't send periodic data
3. Component's loop() never sees any bytes from sensor
4. `available()` always returns false
5. `readline_()` never gets called
6. ESP_LOGD never executes

## Complete Fix Applied

### File: `esphome/components/ld2460/ld2460.cpp`

**Change 1: Enable reporting in setup()**
```cpp
void LD2460Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2460...");
  
  // Clear buffers
  this->buffer_pos_ = 0;
  memset(this->buffer_data_, 0, sizeof(this->buffer_data_));
  memset(this->target_info_, 0, sizeof(this->target_info_));
  
  // Enable reporting and read all device information after a short delay
  this->set_timeout(1000, [this]() {
    this->enable_reporting(true);  // ← ADDED: Tell sensor to start sending data
    this->read_all_info();
  });
}
```

**Change 2: Fix loop() implementation**
```cpp
void LD2460Component::loop() {
  while (this->available()) {
    this->readline_(this->read());  // ← CHANGED: Use read() instead of read_byte()
  }
}
```

## Expected Behavior After Fix

With UART debug enabled:
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
```

You should now see:

**Console output:**
```
[D][ld2460:xxx]: Setting up LD2460...
[I][ld2460:xxx]: Enabling LD2460 reporting...
[D][uart_debug:176]: >>> FD FC FB FA 06 0C 00 01 04 03 02 01    (enable_reporting command)
[D][uart_debug:176]: <<< FD FC FB FA 06 08 00 01 04 03 02 01    (ack from sensor)
[D][ld2460:170]: Received byte: 0xFD                             (bytes being logged!)
[D][ld2460:170]: Received byte: 0xFC
[D][uart_debug:176]: <<< F4 F3 F2 F1 04 0F 00 0F 00 17 00 F8 F7 F6 F5    (periodic data!)
[D][ld2460:xxx]: Target 1: X=15mm, Y=23mm
[I][sensor:xxx]: 'Target Count': Sending state 1.00000
```

## Verification Checklist

After applying the fix, verify:

- [ ] ESP32 sees incoming UART data (check with UART debug)
- [ ] ESP_LOGD prints "Received byte: 0x__" messages
- [ ] Periodic data frames appear every ~100ms
- [ ] Target count sensor updates
- [ ] Target position sensors (X, Y) update
- [ ] Presence binary sensor responds to targets
- [ ] Version command returns firmware info

## Additional Notes

### Frame Structure (Correct in Current Implementation)

The frame parsing was already correct:

**Periodic Data Frame:**
```
Byte Position: 0  1  2  3  4   5  6   7+ ...          end-4 to end-1
               F4 F3 F2 F1 FN  LL HH  [DATA...]       F8 F7 F6 F5
               └─Header──┘ │   └Len┘  └─Payload─┘    └─Footer─┘
                           │
                           └─ Function code (0x04 for target data)
```

**Length field (bytes 5-6):** Total frame size in bytes (little-endian)

The code correctly reads length from `buffer_data_[5]` and `buffer_data_[6]`:
```cpp
uint16_t total_len = this->buffer_data_[5] | (this->buffer_data_[6] << 8);
```

This was one of the "Critical Fixes" mentioned in PR #3, and it's already correct in this implementation.

### Baud Rate

Default baud rate for LD2460: **256000**

If sensor was previously configured to different baud rate:
1. You must match that baud rate in YAML, OR
2. Use factory reset button to restore default settings

### Wiring

Correct connections:
```
ESP32 TX (GPIO17) → LD2460 RX
ESP32 RX (GPIO16) ← LD2460 TX
GND               → GND
5V                → VCC
```

**Important:** TX→RX and RX→TX (crossover connection)

## References

- `FRAME_PARSING_FIX.md` - Details about frame length parsing
- `PROTOCOL_FIX.md` - Details about command frame format
- `PROTOCOL.md` - Complete LD2460 protocol specification
- `ld2460-presence-complete.yaml` - Full configuration example
