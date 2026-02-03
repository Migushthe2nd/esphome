# LD2460 Initialization Sequence Fix

## Problem
The LD2460 sensor was not sending any data after setup, even though:
- UART was configured correctly
- Baud rate matched (115200)
- Commands were being sent
- Wiring was correct

## Root Cause
**Missing sensor restart during initialization**

The LD2460 setup was calling `enable_reporting()` immediately after boot, but the sensor may be in an unknown state and not responsive.

## Solution

### Before (Broken)
```cpp
void LD2460Component::setup() {
  // ... buffer init ...
  
  this->set_timeout(1000, [this]() {
    this->enable_reporting(true);  // Sensor might not be ready!
    this->read_all_info();
  });
}
```

**Problem**: Sensor might be:
- Still booting
- In a stuck state from previous session
- Not ready to accept commands
- In wrong mode

### After (Fixed)
```cpp
void LD2460Component::setup() {
  // ... buffer init ...
  
  // Step 1: Restart sensor after 1 second
  this->set_timeout(1000, [this]() {
    ESP_LOGI(TAG, "Restarting sensor to clear any stuck state...");
    this->restart();
  });
  
  // Step 2: Enable reporting after sensor boots (2.5s total)
  this->set_timeout(2500, [this]() {
    ESP_LOGI(TAG, "Enabling reporting and reading device info...");
    this->enable_reporting(true);
    this->read_all_info();
  });
}
```

**Benefits**:
1. **Clean slate**: Restart clears any stuck state
2. **Proper timing**: 1.5s delay allows sensor to fully boot
3. **Matches LD2450**: Uses same init pattern as working LD2450
4. **Reliable**: Works consistently across power cycles

## Initialization Sequence

```
Time    Action                          Reason
----    ------                          ------
0ms     setup() called                  Component initializes
1000ms  Send restart command            Clear any stuck state
        Wait for sensor to boot         Sensor restarts internally
2500ms  Enable reporting                Now sensor is ready
        Read device info                Get version, config
```

## Comparison with LD2450

**LD2450 Initialization:**
```cpp
void LD2450Component::setup() {
  this->restart_and_read_all_info();  // Restart + delayed read
}

void LD2450Component::restart_and_read_all_info() {
  this->set_config_mode_(true);   // Enter config mode
  this->restart_();                // Restart sensor
  this->set_timeout(1500, [this]() { 
    this->read_all_info();         // Read after 1.5s
  });
}
```

**LD2460 Initialization (Now):**
```cpp
void LD2460Component::setup() {
  this->set_timeout(1000, [this]() { 
    this->restart();               // Restart sensor
  });
  this->set_timeout(2500, [this]() {
    this->enable_reporting(true);  // Enable after boot
    this->read_all_info();
  });
}
```

**Key Difference:**
- LD2450 uses config mode (0xFF/0xFE commands)
- LD2460 uses enable_reporting (0x06 command)
- Both restart the sensor for clean initialization

## Debugging Tips

If you still have issues after this fix:

1. **Check logs** for restart confirmation:
   ```
   [I][ld2460:xxx]: Restarting sensor to clear any stuck state...
   [I][ld2460:xxx]: Enabling reporting and reading device info...
   ```

2. **Enable VERBOSE logging** to see UART activity:
   ```yaml
   logger:
     level: VERBOSE
     logs:
       ld2460: VERBOSE
   ```

3. **Watch for incoming data**:
   ```
   [V][ld2460:xxx]: UART has X bytes available
   [V][ld2460:xxx]: Read byte from UART: 0xXX
   ```

4. **Check UART debug** shows responses:
   ```
   [D][uart_debug:113]: << FD FC FB FA ...  # Response from sensor
   ```

## Related Files
- `ld2460.cpp`: setup() and loop() implementation
- `UART_FIX.md`: Previous UART communication fixes
- `PROTOCOL.md`: LD2460 command specifications
