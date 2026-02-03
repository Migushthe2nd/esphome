# LD2460 ESPHome YAML Configurations

This directory contains example YAML configurations for the LD2460 radar sensor component.

## Available Configurations

### 1. `ld2460-presence-complete.yaml` - **RECOMMENDED**

**Production-ready, comprehensive configuration (861 lines)**

Perfect for:
- Production deployments
- Base for custom projects
- UI tool integration
- Everything Presence-style implementations

**Features:**
- ✅ Complete sensor entity exposure
- ✅ Software-based zone system (4 zones, extensible)
- ✅ JSON zone reporting (single sensor, no bloat)
- ✅ Individual zone binary sensors
- ✅ Runtime-configurable zone boundaries (16 number entities)
- ✅ Target position sensors (optional, disabled by default)
- ✅ All device controls (restart, factory reset, baud rate, installation mode)
- ✅ Diagnostics (WiFi, uptime, firmware version)
- ✅ Well-documented with inline comments
- ✅ Customizable via substitutions

**Compared to Everything Presence LD2450:**
- 861 lines vs 1983 lines (56% reduction)
- Cleaner structure
- No entity bloat
- Modern ESPHome patterns

### 2. `ld2460-zones-json-example.yaml`

**Minimal zones demonstration (266 lines)**

Perfect for:
- Learning the zone system
- Quick prototyping
- Understanding JSON approach

**Features:**
- Basic zone configuration example
- Shows JSON zone reporting
- Demonstrates zone boundary numbers
- Simpler, educational focus

## Quick Start

### Option 1: Use Complete Configuration

```bash
# Copy and customize
cp ld2460-presence-complete.yaml my-presence-sensor.yaml

# Edit substitutions at the top:
# - device_name
# - uart_tx_pin / uart_rx_pin
# - zone names and boundaries (optional)

# Create secrets.yaml:
# wifi_ssid: "YourSSID"
# wifi_password: "YourPassword"
# api_encryption_key: "your-32-char-key"
# ota_password: "your-ota-password"

# Flash to ESP32
esphome run my-presence-sensor.yaml
```

### Option 2: Start from Minimal Example

```bash
# Use the simpler example
cp ld2460-zones-json-example.yaml my-sensor.yaml

# Customize and extend as needed
esphome run my-sensor.yaml
```

## Entity Overview

The complete configuration exposes:

### Text Sensors (2)
- `text_sensor.firmware_version` - LD2460 firmware version
- `text_sensor.zone_status_json` - **KEY SENSOR** - All zone data in JSON format

### Binary Sensors (5)
- `binary_sensor.presence` - Overall presence detection
- `binary_sensor.zone_1_occupancy` - Living Room
- `binary_sensor.zone_2_occupancy` - Hallway
- `binary_sensor.zone_3_occupancy` - Kitchen
- `binary_sensor.zone_4_occupancy` - Bedroom

### Sensors (19 - most disabled by default)
- `sensor.wifi_signal` - WiFi signal strength
- `sensor.uptime` - Device uptime
- `sensor.target_count` - Number of detected targets
- `sensor.target_1_x/y/angle/distance` - Target 1 position (disabled by default)
- `sensor.target_2_x/y/angle/distance` - Target 2 position (disabled by default)
- `sensor.target_3_x/y/angle/distance` - Target 3 position (disabled by default)

### Buttons (3)
- `button.restart_esp` - Restart ESP32
- `button.restart_sensor` - Restart LD2460 sensor
- `button.factory_reset_sensor` - Factory reset sensor

### Selects (2)
- `select.baud_rate` - UART baud rate (8 options)
- `select.installation_mode` - Side Mount / Top Mount

### Numbers (16 - Zone Configuration)
- `number.zone_1_x1/y1/x2/y2` - Zone 1 boundaries
- `number.zone_2_x1/y1/x2/y2` - Zone 2 boundaries
- `number.zone_3_x1/y1/x2/y2` - Zone 3 boundaries
- `number.zone_4_x1/y1/x2/y2` - Zone 4 boundaries

**Total:** ~47 entities (only ~20 enabled by default)

## Zone Configuration

### Coordinate System

```
        Y-axis (Far)
           ↑
           |
    6000mm |
           |
    3000mm |
           |
        0mm +---------------→ X-axis
          -3000  0  +3000mm
           (Left)  (Right)
```

### Default Zone Layout

```
┌─────────────────────────────────────┐
│                                     │  6000mm
│         Zone 2: Hallway             │
│                                     │
├──────┬───────────────┬──────────────┤  3000mm
│Zone 4│ Zone 1: Living│ Zone 3:      │
│Bedroom    Room       │ Kitchen      │
│      │               │              │
└──────┴───────────────┴──────────────┘  0mm
 -3000  -1500    0    1500   3000mm
```

### Adjusting Zones

Configure via Home Assistant number entities:
- `Zone X X1` - Left boundary
- `Zone X X2` - Right boundary
- `Zone X Y1` - Near boundary
- `Zone X Y2` - Far boundary

Range: -6000 to +6000mm, 100mm steps

## JSON Format

The `zone_status_json` sensor outputs:

```json
{
  "zones": [
    {
      "id": 1,
      "name": "Living Room",
      "count": 2,
      "occupied": true
    },
    {
      "id": 2,
      "name": "Hallway",
      "count": 0,
      "occupied": false
    },
    {
      "id": 3,
      "name": "Kitchen",
      "count": 1,
      "occupied": true
    },
    {
      "id": 4,
      "name": "Bedroom",
      "count": 0,
      "occupied": false
    }
  ]
}
```

Perfect for:
- UI tool parsing
- Node-RED processing
- Custom automations
- Multi-zone tracking

## UI Tool Integration

The complete configuration is designed for UI tool integration:

### For Zone Visualization
1. Parse `text_sensor.zone_status_json` for zone data
2. Enable and read `sensor.target_X_x` and `sensor.target_X_y` for target positions
3. Display zones and targets on 2D grid

### For Zone Configuration
1. Read current values from `number.zone_X_x1/y1/x2/y2`
2. Display visual zone editor
3. Update number entities when user adjusts zones
4. Zones update in real-time on ESP32

### For Sensor Control
1. Use `select.installation_mode` for mount configuration
2. Use `button.restart_sensor` for sensor control
3. Display `text_sensor.firmware_version` for info

## Adding More Zones

To extend beyond 4 zones:

1. Add globals for new zone:
```yaml
globals:
  - id: zone5_x1
    type: int
    initial_value: '1000'
  # ... y1, x2, y2
```

2. Add number entities:
```yaml
number:
  - platform: template
    name: "Zone 5 X1 (Left)"
    # ... configuration
```

3. Add binary sensor:
```yaml
binary_sensor:
  - platform: template
    name: "Zone 5 Occupancy"
    # ... lambda with zone check
```

4. Update `zone_status_json` lambda to include new zone

## Customization Tips

### Change Update Intervals

Edit substitutions:
```yaml
substitutions:
  zone_update_interval: "500ms"  # Slower updates
  zone_delayed_off: "10s"        # Longer delay before off
```

### Enable Target Position Sensors

In the complete YAML, find target sensors and remove `disabled_by_default: true`

### Change Zone Names

Edit the lambda in `zone_status_json` text sensor:
```cpp
json += "{\"id\":1,\"name\":\"My Custom Name\",\"count\":" + ...
```

### Add Custom Icons

Each zone occupancy sensor has `icon:` parameter - customize as needed:
```yaml
icon: mdi:home  # or any other MDI icon
```

## Troubleshooting

### No Targets Detected
1. Check UART wiring (TX → RX, RX → TX)
2. Verify baud rate matches (default 115200)
3. Check `text_sensor.firmware_version` - should show version
4. Try `button.restart_sensor`

### Zones Not Working
1. Enable target position sensors to see where targets are
2. Adjust zone boundaries using number entities
3. Check that targets are within configured zones
4. View logs for lambda calculation output

### High Update Lag
1. Increase `zone_update_interval` (default 250ms)
2. Disable unused target position sensors
3. Reduce number of zones

## Best Practices

1. **Start with defaults** - Test before customizing
2. **Use entity categories** - Keep config/diagnostic entities organized
3. **Disable unused sensors** - Reduce entity count
4. **Test zone boundaries** - Use target sensors to visualize before finalizing
5. **Document changes** - Add comments when customizing
6. **Backup config** - Save working configuration before changes

## Support

For issues or questions:
- Check ESPHome logs: `esphome logs my-presence-sensor.yaml`
- Review PROTOCOL.md for sensor protocol details
- See ZONES.md for software zones documentation
- Check ARCHITECTURE.md for component design

## See Also

- `PROTOCOL.md` - LD2460 protocol specification
- `ZONES.md` - Software zones implementation guide
- `ARCHITECTURE.md` - Component architecture analysis
- `VERIFICATION.md` - Implementation verification report
