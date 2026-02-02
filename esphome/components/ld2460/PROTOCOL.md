# HLK-LD2460 Protocol Documentation

## Document Information
- **Product**: HLK-LD2460 Multi-Target Tracking Radar
- **Version**: V1.0
- **Company**: Shenzhen Hi-Link Electronic Co., Ltd.

---

## 1. Protocol Overview

The LD2460 uses a serial UART protocol to communicate with the host system. The protocol consists of:
- **Header**: Fixed 4-byte sequence
- **Function Code**: 1 byte identifying the command/response type
- **Length**: 2 bytes (little-endian) indicating packet length
- **Data**: Variable length payload
- **Footer**: Fixed 4-byte sequence

### Frame Structure

All communication frames follow this structure:

| Field | Size | Description |
|-------|------|-------------|
| Header | 4 bytes | Frame start marker |
| Function Code | 1 byte | Command/response identifier |
| Length | 2 bytes | Packet length (little-endian) |
| Data | Variable | Payload data |
| Footer | 4 bytes | Frame end marker |

---

## 2. Frame Headers and Footers

### Periodic Data Frame (Radar → Host)
- **Header**: `F4 F3 F2 F1`
- **Footer**: `F8 F7 F6 F5`

### Command/Response Frame (Host ↔ Radar)
- **Header**: `FD FC FB FA`
- **Footer**: `04 03 02 01`

---

## 3. Function Codes

| Code | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x04 | Target Data | Radar → Host | Periodic target position data |
| 0x06 | Enable/Disable Reporting | Host → Radar | Control radar data reporting |
| 0x07 | Set Detection Parameters | Host → Radar | Configure detection distance and angle |
| 0x08 | Read Detection Parameters | Host → Radar | Query current detection settings |
| 0x09 | Set Installation Mode | Host → Radar | Configure side-mount or top-mount |
| 0x0A | Read Installation Mode | Host → Radar | Query installation mode |
| 0x0B | Read Version | Host → Radar | Query firmware version |
| 0x0D | Restart | Host → Radar | Restart the radar module |
| 0x0E | Set Baud Rate | Host → Radar | Configure UART baud rate |
| 0x10 | Factory Reset | Host → Radar | Reset to factory defaults |

---

## 4. Detailed Command Specifications

### 4.1 Periodic Target Data (0x04)

**Direction**: Radar → Host (automatic reporting)

**Frame Format**:
```
F4 F3 F2 F1  04  [len_low] [len_high]  [target_data...]  F8 F7 F6 F5
```

**Data Format**:
- Length = number_of_targets * 4 + 11
- Each target: 4 bytes (X coordinate + Y coordinate)
  - X: 2 bytes (little-endian), signed, unit: 0.1 meters
  - Y: 2 bytes (little-endian), signed, unit: 0.1 meters

**Example**: One target at position (1.5m, 2.3m)
```
F4 F3 F2 F1  04  0F 00  0F 00  17 00  F8 F7 F6 F5
```
- 0x000F = 15 = 1.5m × 10 (0.1m units)
- 0x0017 = 23 = 2.3m × 10 (0.1m units)

---

### 4.2 Enable/Disable Reporting (0x06)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  06  0C 00  [enable]  04 03 02 01
```

**Parameters**:
- `enable`: 1 byte
  - `0x00` = Disable reporting
  - `0x01` = Enable reporting

**Response Frame**:
```
FD FC FB FA  06  0C 00  [status]  04 03 02 01
```

**Status Values**:
- Upper 4 bits: Operation result
  - `0x00` = Disable failed
  - `0x10` = Disable success
  - `0x01` = Enable failed
  - `0x11` = Enable success
- Lower 4 bits: Operation content

**Examples**:
- Disable: `FD FC FB FA 06 0C 00 00 04 03 02 01`
- Response (disable success): `FD FC FB FA 06 0C 00 10 04 03 02 01`

---

### 4.3 Set Detection Parameters (0x07)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  07  0F 00  [dist_low] [dist_high] [angle_low] [angle_high]  04 03 02 01
```

**Parameters**:
- Distance: 2 bytes (little-endian), unit: meters × 100
- Angle: 2 bytes (little-endian), unit: degrees × 100

**Response Frame**:
```
FD FC FB FA  07  0C 00  [status]  04 03 02 01
```

**Status Values**:
- `0x00` = Failed to set parameters
- `0x01` = Successfully set parameters

**Example**: Set distance to 2.6m and angle to 30°
```
Command:  FD FC FB FA 07 0F 00 04 01 B8 0B 04 03 02 01
```
- Distance: 0x0104 = 260 = 2.6m × 100
- Angle: 0x0BB8 = 3000 = 30° × 100

**Response (success)**:
```
FD FC FB FA 07 0C 00 01 04 03 02 01
```

**Note**: These parameters only apply to side-mount installation mode.

---

### 4.4 Read Detection Parameters (0x08)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  08  0C 00  01  04 03 02 01
```

**Response Frame**:
```
FD FC FB FA  08  0F 00  [dist_low] [dist_high] [angle_low] [angle_high]  04 03 02 01
```

**Response Data**:
- Distance: 2 bytes (little-endian), unit: meters × 100
- Angle: 2 bytes (little-endian), unit: degrees × 100

**Example**:
```
Command:  FD FC FB FA 08 0C 00 01 04 03 02 01
Response: FD FC FB FA 08 0F 00 04 01 B8 0B 04 03 02 01
```
- Distance: 0x0104 = 260 = 2.6m
- Angle: 0x0BB8 = 3000 = 30°

**Note**: These parameters are only readable for side-mount installation.

---

### 4.5 Set Installation Mode (0x09)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  09  0C 00  [mode]  04 03 02 01
```

**Parameters**:
- `mode`: 1 byte
  - `0x01` = Side-mount
  - `0x02` = Top-mount

**Response Frame**:
```
FD FC FB FA  09  0C 00  [status]  04 03 02 01
```

**Status Values**:
- Upper 4 bits: Operation result
  - `0x10` = Successfully set side-mount
  - `0x11` = Successfully set top-mount
- Lower 4 bits: Operation content
  - `0x01` = Side-mount setting
  - `0x02` = Top-mount setting

**Examples**:
- Set side-mount: `FD FC FB FA 09 0C 00 01 04 03 02 01`
- Response (success): `FD FC FB FA 09 0C 00 11 04 03 02 01`

---

### 4.6 Read Installation Mode (0x0A)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  0A  0C 00  01  04 03 02 01
```

**Response Frame**:
```
FD FC FB FA  0A  0C 00  [mode]  04 03 02 01
```

**Response Values**:
- `0x01` = Side-mount
- `0x02` = Top-mount

**Example**:
```
Command:  FD FC FB FA 0A 0C 00 01 04 03 02 01
Response: FD FC FB FA 0A 0C 00 02 04 03 02 01  (Top-mount)
```

---

### 4.7 Read Firmware Version (0x0B)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  0B  0C 00  01  04 03 02 01
```

**Response Frame**:
```
FD FC FB FA  0B  10 00  [mode] [year] [month] [major] [minor]  04 03 02 01
```

**Response Data**:
- Installation mode: 1 byte (0x01 = side-mount, 0x02 = top-mount)
- Year: 1 byte (e.g., 0x19 = 2025)
- Month: 1 byte (e.g., 0x02 = February)
- Major version: 1 byte
- Minor version: 1 byte

**Example**: Side-mount, firmware dated 2025-02, version 1.2
```
Command:  FD FC FB FA 0B 0C 00 01 04 03 02 01
Response: FD FC FB FA 0B 10 00 02 19 02 01 02 04 03 02 01
```
- Mode: 0x02 = Top-mount
- Date: 2025/02 (0x19 = 25, 0x02 = 2)
- Version: V1.2 (0x01, 0x02)

---

### 4.8 Restart Radar (0x0D)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  0D  0C 00  01  04 03 02 01
```

**Description**: Restarts the radar module.

**Example**:
```
FD FC FB FA 0D 0C 00 01 04 03 02 01
```

---

### 4.9 Set Baud Rate (0x0E)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  0E  0C 00  [rate_code]  04 03 02 01
```

**Baud Rate Codes**:
| Code | Baud Rate |
|------|-----------|
| 0x00 | 9600 |
| 0x01 | 19200 |
| 0x02 | 38400 |
| 0x03 | 57600 |
| 0x04 | 115200 |
| 0x05 | 230400 |
| 0x06 | 256000 |
| 0x07 | 460800 |

**Response Frame**:
```
FD FC FB FA  0E  0C 00  [status]  04 03 02 01
```

**Status Values**:
- `0x00` = Failed to set baud rate
- `0x01` = Successfully set baud rate

**Example**: Set baud rate to 115200
```
Command:  FD FC FB FA 0E 0C 00 04 04 03 02 01
Response: FD FC FB FA 0E 0C 00 01 04 03 02 01  (Success)
```

---

### 4.10 Factory Reset (0x10)

**Direction**: Host → Radar

**Command Frame**:
```
FD FC FB FA  10  0C 00  01  04 03 02 01
```

**Description**: Resets all settings to factory defaults:
- Baud rate: 115200
- Installation mode: Side-mount
- Detection distance: 2.6m
- Detection angle: 30°
- Side-mount detection range: 6m, ±60°
- Top-mount detection range: 4m, 0-360°

**Response Frame**:
```
FD FC FB FA  10  0C 00  [status]  04 03 02 01
```

**Status Values**:
- `0x00` = Factory reset failed
- `0x01` = Factory reset successful

**Example**:
```
Command:  FD FC FB FA 10 0C 00 01 04 03 02 01
Response: FD FC FB FA 10 0C 00 01 04 03 02 01  (Success)
```

---

## 5. Data Encoding Details

### 5.1 Coordinate System

Target positions are reported in Cartesian coordinates:
- **X-axis**: Horizontal position
- **Y-axis**: Distance from radar

### 5.2 Value Encoding

- **Coordinates**: 
  - Signed 16-bit integers (little-endian)
  - Unit: 0.1 meters (multiply raw value by 10 to get millimeters)
  - Range: -3276.8m to +3276.7m

- **Distance (Detection Parameters)**:
  - Unsigned 16-bit integer (little-endian)
  - Unit: 0.01 meters (multiply by 100 to get protocol value)
  - Example: 2.6m → 260 → 0x0104

- **Angle (Detection Parameters)**:
  - Unsigned 16-bit integer (little-endian)
  - Unit: 0.01 degrees (multiply by 100 to get protocol value)
  - Example: 30° → 3000 → 0x0BB8

### 5.3 Little-Endian Format

All multi-byte values are transmitted in little-endian format (LSB first).

Example: Value 260 (0x0104)
- Byte 0: 0x04 (low byte)
- Byte 1: 0x01 (high byte)

---

## 6. Default Configuration

The radar ships with these default settings:

| Parameter | Default Value |
|-----------|---------------|
| Baud Rate | 115200 |
| Installation Mode | Side-mount |
| Detection Distance | 2.6 meters |
| Detection Angle | 30 degrees |
| Side-mount Range | 6m, ±60° |
| Top-mount Range | 4m, 0-360° |

---

## 7. Implementation Notes

### 7.1 Frame Validation

Always validate:
1. Header matches expected value (F4/F3/F2/F1 or FD/FC/FB/FA)
2. Footer matches expected value (F8/F7/F6/F5 or 04/03/02/01)
3. Length field is consistent with actual frame size
4. Function code is recognized

### 7.2 Command Sequencing

- Wait for response before sending next command
- Commands should be sent with appropriate delays
- The radar automatically sends periodic data when reporting is enabled

### 7.3 Detection Parameters

- Detection distance and angle parameters **only apply to side-mount mode**
- These parameters cannot be set or read in top-mount mode
- Both parameters must be sent together in command 0x07

### 7.4 Coordinate Calculation

From target X,Y coordinates, calculate:
- **Distance**: `sqrt(x² + y²)`
- **Angle**: `atan2(y, x)` or `acos(y / distance)` for angle from forward axis

---

## 8. Implementation Validation Checklist

### Frame Handling
- [ ] Correctly parse data frame header (F4 F3 F2 F1)
- [ ] Correctly parse data frame footer (F8 F7 F6 F5)
- [ ] Correctly parse command frame header (FD FC FB FA)
- [ ] Correctly parse command frame footer (04 03 02 01)
- [ ] Handle variable-length frames based on length field

### Target Data
- [ ] Parse target X coordinates (little-endian, signed)
- [ ] Parse target Y coordinates (little-endian, signed)
- [ ] Scale coordinates correctly (multiply by 10 for mm)
- [ ] Calculate distance from X,Y
- [ ] Calculate angle from X,Y
- [ ] Handle up to 3 targets

### Commands
- [ ] Enable/disable reporting (0x06)
- [ ] Set detection parameters (0x07) with proper encoding
- [ ] Read detection parameters (0x08)
- [ ] Set installation mode (0x09)
- [ ] Read installation mode (0x0A)
- [ ] Read version (0x0B)
- [ ] Restart (0x0D)
- [ ] Set baud rate (0x0E)
- [ ] Factory reset (0x10)

### Data Encoding
- [ ] Little-endian encoding for all multi-byte values
- [ ] Distance: meters × 100
- [ ] Angle: degrees × 100
- [ ] Coordinates: 0.1m units (raw × 10 = mm)
- [ ] Proper rounding for float-to-int conversion

### Response Handling
- [ ] Parse and validate all response frames
- [ ] Extract status codes correctly
- [ ] Update internal state from read responses
- [ ] Log appropriate messages for success/failure

---

## 9. Protocol Example Sequences

### 9.1 Initialization Sequence

```
1. Host → Radar: Read Version (0x0B)
   FD FC FB FA 0B 0C 00 01 04 03 02 01

2. Radar → Host: Version Response
   FD FC FB FA 0B 10 00 01 19 02 01 00 04 03 02 01
   (Side-mount, 2025/02, V1.0)

3. Host → Radar: Read Detection Parameters (0x08)
   FD FC FB FA 08 0C 00 01 04 03 02 01

4. Radar → Host: Parameters Response
   FD FC FB FA 08 0F 00 04 01 B8 0B 04 03 02 01
   (2.6m, 30°)

5. Host → Radar: Enable Reporting (0x06)
   FD FC FB FA 06 0C 00 01 04 03 02 01

6. Radar → Host: Enable Response
   FD FC FB FA 06 0C 00 11 04 03 02 01
   (Success)

7. Radar → Host: Periodic Target Data (every cycle)
   F4 F3 F2 F1 04 0F 00 0F 00 17 00 F8 F7 F6 F5
   (One target at 1.5m, 2.3m)
```

### 9.2 Configuration Change Sequence

```
1. Host → Radar: Set Detection Parameters (0x07)
   FD FC FB FA 07 0F 00 F4 01 14 11 04 03 02 01
   (5.0m, 45°)

2. Radar → Host: Set Response
   FD FC FB FA 07 0C 00 01 04 03 02 01
   (Success)

3. Host → Radar: Read Detection Parameters (0x08)
   FD FC FB FA 08 0C 00 01 04 03 02 01

4. Radar → Host: Parameters Response
   FD FC FB FA 08 0F 00 F4 01 14 11 04 03 02 01
   (Confirmed: 5.0m, 45°)
```

---

## 10. Troubleshooting

### Common Issues

1. **No response from radar**
   - Check UART connections (TX/RX)
   - Verify baud rate (default: 115200)
   - Ensure power supply is adequate

2. **Invalid frame data**
   - Verify header/footer bytes
   - Check length field matches frame size
   - Validate checksum if implemented

3. **Detection parameters not working**
   - Ensure radar is in side-mount mode
   - Parameters only apply to side-mount installation
   - Verify values are within valid ranges

4. **No target data**
   - Enable reporting with command 0x06
   - Check that targets are within detection range
   - Verify installation mode matches physical setup

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024 | Initial protocol specification |

---

*End of Protocol Documentation*
