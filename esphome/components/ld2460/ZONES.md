# Software-Based Zones for LD2460

## Overview

Unlike the LD2450 which has hardware zone support (limited to 3 rectangular zones), the LD2460 implements zones **entirely in software** on the ESP32. This approach offers several advantages:

### Benefits of Software Zones

✅ **No Entity Bloat**: Use a single text sensor with JSON data instead of individual sensors per zone  
✅ **Flexible Zones**: Support any number of zones (not limited to 3)  
✅ **Polygon Zones**: Support complex polygon shapes, not just rectangles  
✅ **Easy Configuration**: Define zones via YAML globals or numbers  
✅ **Home Assistant Friendly**: Single JSON sensor integrates cleanly  
✅ **ESP32-Side Processing**: All calculations done locally, no Home Assistant templates needed  

### Comparison: LD2450 vs LD2460 Zones

| Feature | LD2450 (Hardware) | LD2460 (Software) |
|---------|-------------------|-------------------|
| **Zone Limit** | 3 zones max | Unlimited (RAM permitting) |
| **Zone Shape** | Rectangles only | Rectangles, polygons, circles |
| **Configuration** | Sensor commands | YAML globals/numbers |
| **HA Entity Count** | 1 per zone + counts | 1 JSON sensor for all |
| **Flexibility** | Fixed in hardware | Fully customizable |
| **Processing** | Sensor firmware | ESP32 |

---

## Quick Start: Simple Rectangular Zones

### Example 1: Basic 2-Zone Setup

```yaml
esphome:
  name: ld2460-zones

# Define zone coordinates as globals
globals:
  # Zone 1: Living Room (rectangle from -1000,-0 to 1000,3000 mm)
  - id: zone1_x1
    type: int
    initial_value: '-1000'
  - id: zone1_y1
    type: int
    initial_value: '0'
  - id: zone1_x2
    type: int
    initial_value: '1000'
  - id: zone1_y2
    type: int
    initial_value: '3000'

uart:
  tx_pin: GPIO4
  rx_pin: GPIO5
  baud_rate: 115200

ld2460:
  id: ld2460_radar

# Zone occupancy binary sensors
binary_sensor:
  - platform: template
    name: "Living Room Occupancy"
    id: zone1_occupancy
    device_class: occupancy
    filters:
      - delayed_off: 5s

# Calculate zone occupancy on each radar update
interval:
  - interval: 100ms
    then:
      - lambda: |-
          bool zone1_occupied = false;
          
          // Check each target
          for (int i = 0; i < 3; i++) {
            auto target = id(ld2460_radar).get_target(i);
            if (target.valid) {
              int x = target.x;
              int y = target.y;
              
              // Rectangular zone check
              if (x >= id(zone1_x1) && x <= id(zone1_x2) &&
                  y >= id(zone1_y1) && y <= id(zone1_y2)) {
                zone1_occupied = true;
                break;
              }
            }
          }
          
          id(zone1_occupancy).publish_state(zone1_occupied);
```

---

## Advanced: Polygon Zones

For non-rectangular areas, use the point-in-polygon algorithm:

```yaml
# Point-in-polygon helper function
script:
  - id: point_in_polygon
    parameters:
      px: int
      py: int
      poly_x: int[]
      poly_y: int[]
      vertices: int
    then:
      - lambda: |-
          bool inside = false;
          int j = vertices - 1;
          
          for (int i = 0; i < vertices; i++) {
            if ((poly_y[i] > py) != (poly_y[j] > py) &&
                (px < (poly_x[j] - poly_x[i]) * (py - poly_y[i]) / 
                      (poly_y[j] - poly_y[i]) + poly_x[i])) {
              inside = !inside;
            }
            j = i;
          }
          
          return inside;

# Define L-shaped zone as polygon
globals:
  - id: zone_poly_x
    type: int[5]
    initial_value: "{-1000, 1000, 1000, 0, -1000}"
  - id: zone_poly_y
    type: int[5]
    initial_value: "{0, 0, 2000, 2000, 3000}"
```

---

## Cleaner Approach: Single JSON Sensor

Instead of creating individual binary sensors for each zone, use a single text sensor with JSON data:

```yaml
text_sensor:
  - platform: template
    name: "Zone Status"
    id: zone_status
    update_interval: 200ms
    lambda: |-
      // Build JSON with zone occupancy and counts
      std::string json = "{\"zones\":[";
      
      // Zone 1
      int zone1_count = 0;
      for (int i = 0; i < 3; i++) {
        auto target = id(ld2460_radar).get_target(i);
        if (target.valid) {
          if (target.x >= id(zone1_x1) && target.x <= id(zone1_x2) &&
              target.y >= id(zone1_y1) && target.y <= id(zone1_y2)) {
            zone1_count++;
          }
        }
      }
      json += "{\"id\":1,\"name\":\"Living Room\",\"count\":" + 
              std::to_string(zone1_count) + 
              ",\"occupied\":" + (zone1_count > 0 ? "true" : "false") + "}";
      
      // Add more zones...
      json += "]}";
      return json;

# In Home Assistant, use JSON attributes to create individual entities:
# Template sensor:
#   - state: "{{ value_json.zones[0].occupied }}"
#   - attributes:
#       count: "{{ value_json.zones[0].count }}"
```

This approach creates **one entity in Home Assistant** instead of multiple sensors per zone.

---

## Configuration via Number Entities

For runtime-configurable zones, use number entities:

```yaml
number:
  - platform: template
    name: "Zone 1 X1"
    id: zone1_x1_number
    min_value: -6000
    max_value: 6000
    step: 100
    initial_value: -1000
    optimistic: true
    restore_value: true
    unit_of_measurement: "mm"
    
  # ... similar for Y1, X2, Y2

interval:
  - interval: 100ms
    then:
      - lambda: |-
          // Use number entity states for zone boundaries
          int x1 = (int)id(zone1_x1_number).state;
          int y1 = (int)id(zone1_y1_number).state;
          int x2 = (int)id(zone1_x2_number).state;
          int y2 = (int)id(zone1_y2_number).state;
          
          // Zone calculation using dynamic boundaries...
```

This allows zone adjustment from Home Assistant without reflashing.

---

## Complete Example: 4 Zones with JSON Output

See `ld2460-zones-complete-example.yaml` for a full implementation with:
- 4 configurable zones
- Single JSON text sensor
- Zone target counts
- Entry/exit detection
- Delayed off timers
- Home Assistant dashboard integration

---

## Performance Considerations

### Update Frequency

- **Recommended**: 100-250ms interval for zone checks
- **Too Fast** (<50ms): Unnecessary CPU usage
- **Too Slow** (>500ms): Delayed occupancy detection

### Memory Usage

Each zone requires approximately:
- 4 globals (x1, y1, x2, y2) = 16 bytes
- Polygon zones: ~4 bytes per vertex

For 10 rectangular zones: ~160 bytes RAM

### CPU Impact

Zone calculation is lightweight:
- Rectangular zone check: ~10 microseconds
- Polygon zone check: ~50 microseconds per vertex
- 4 zones, 3 targets: ~1ms total per update

---

## Integration with Home Assistant

### Using JSON Sensor

```yaml
# Home Assistant configuration.yaml
template:
  - sensor:
      - name: "Living Room Occupied"
        state: "{{ state_attr('sensor.zone_status', 'zones')[0].occupied }}"
      
      - name: "Living Room Target Count"
        state: "{{ state_attr('sensor.zone_status', 'zones')[0].count }}"
```

### Dashboard Card

```yaml
type: entities
entities:
  - entity: sensor.zone_status
    type: attribute
    attribute: zones
```

---

## Troubleshooting

### Zones Not Detecting Targets

1. **Check Coordinate System**: LD2460 uses millimeters, X (horizontal) and Y (distance)
2. **Verify Zone Bounds**: Use debug sensors to log target positions
3. **Update Interval**: Ensure interval is running (check logs)

### Debug Target Positions

```yaml
sensor:
  - platform: ld2460
    target_1:
      x:
        name: "Debug Target 1 X"
      y:
        name: "Debug Target 1 Y"
```

### Test Zone Logic

```yaml
logger:
  level: DEBUG
  
interval:
  - interval: 1s
    then:
      - lambda: |-
          for (int i = 0; i < 3; i++) {
            auto target = id(ld2460_radar).get_target(i);
            if (target.valid) {
              ESP_LOGD("zones", "Target %d: x=%d, y=%d", i, target.x, target.y);
            }
          }
```

---

## Migration from LD2450

### LD2450 Hardware Zones

```yaml
ld2450:
  zone_1:
    x1: -1000
    y1: 0
    x2: 1000
    y2: 3000
```

### LD2460 Software Zones (Equivalent)

```yaml
globals:
  - id: zone1_x1
    type: int
    initial_value: '-1000'
  # ... etc

interval:
  - interval: 100ms
    then:
      - lambda: |-
          # Zone logic here
```

**Benefits**: More flexible, no hardware limitations, JSON output option

---

## Best Practices

✅ **Use JSON Output** for multiple zones to avoid entity bloat  
✅ **Delayed Off Filters** on occupancy sensors (5-30s typical)  
✅ **Configurable Boundaries** via number entities for easy adjustment  
✅ **Debug Mode** initially to verify zone coordinates  
✅ **Reasonable Update Rate** (100-250ms) balances responsiveness and CPU  

❌ **Avoid** too many zones (>10) - diminishing returns  
❌ **Don't** update faster than sensor reports (~100ms)  
❌ **Don't** use complex polygons (>8 vertices) without testing performance  

---

## Future Enhancements

Potential additions (community contributions welcome):
- Built-in zone helper component (no lambdas required)
- Zone editor via web interface
- Visual zone configuration tool
- Automatic zone learning
- Multi-sensor fusion (combine multiple LD2460s)

---

## Examples

See the `examples/` directory for:
- `basic-2-zones.yaml` - Simple rectangular zones
- `polygon-zones.yaml` - Complex polygon shapes
- `json-zones.yaml` - Single JSON sensor approach
- `configurable-zones.yaml` - Number entity configuration
- `everything-presence-ld2460.yaml` - Full replacement for Everything Presence Lite

---

*For questions or contributions, see the ESPHome community forum or GitHub discussions.*
