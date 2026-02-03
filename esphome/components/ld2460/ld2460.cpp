#include "ld2460.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cmath>
#include <numbers>

namespace esphome::ld2460 {

static const char *const TAG = "ld2460";

enum BaudRate : uint8_t {
  BAUD_RATE_9600 = 0,
  BAUD_RATE_19200 = 1,
  BAUD_RATE_38400 = 2,
  BAUD_RATE_57600 = 3,
  BAUD_RATE_115200 = 4,
  BAUD_RATE_230400 = 5,
  BAUD_RATE_256000 = 6,
  BAUD_RATE_460800 = 7,
};

enum InstallationMode : uint8_t {
  INSTALLATION_SIDE_MOUNT = 1,
  INSTALLATION_TOP_MOUNT = 2,
};

// Frame headers and footers
enum FrameMarkers : uint8_t {
  // Periodic data frame (radar -> host)
  DATA_FRAME_HEADER_0 = 0xF4,
  DATA_FRAME_HEADER_1 = 0xF3,
  DATA_FRAME_HEADER_2 = 0xF2,
  DATA_FRAME_HEADER_3 = 0xF1,
  DATA_FRAME_FOOTER_0 = 0xF8,
  DATA_FRAME_FOOTER_1 = 0xF7,
  DATA_FRAME_FOOTER_2 = 0xF6,
  DATA_FRAME_FOOTER_3 = 0xF5,

  // Command frame (host -> radar)
  CMD_FRAME_HEADER_0 = 0xFD,
  CMD_FRAME_HEADER_1 = 0xFC,
  CMD_FRAME_HEADER_2 = 0xFB,
  CMD_FRAME_HEADER_3 = 0xFA,
  CMD_FRAME_FOOTER_0 = 0x04,
  CMD_FRAME_FOOTER_1 = 0x03,
  CMD_FRAME_FOOTER_2 = 0x02,
  CMD_FRAME_FOOTER_3 = 0x01,
};

// Command codes
enum Command : uint8_t {
  CMD_ENABLE_REPORTING = 0x06,
  CMD_SET_DETECTION_PARAMS = 0x07,
  CMD_READ_DETECTION_PARAMS = 0x08,
  CMD_SET_INSTALLATION_MODE = 0x09,
  CMD_READ_INSTALLATION_MODE = 0x0A,
  CMD_READ_VERSION = 0x0B,
  CMD_RESTART = 0x0D,
  CMD_SET_BAUD_RATE = 0x0E,
  CMD_FACTORY_RESET = 0x10,
};

// Function code in periodic data
enum FunctionCode : uint8_t {
  FUNCTION_TARGET_DATA = 0x04,
};

// Memory-efficient lookup tables
struct StringToUint8 {
  const char *str;
  const uint8_t value;
};

struct Uint8ToString {
  const uint8_t value;
  const char *str;
};

constexpr StringToUint8 BAUD_RATES_BY_STR[] = {
    {"9600", BAUD_RATE_9600},     {"19200", BAUD_RATE_19200},   {"38400", BAUD_RATE_38400},
    {"57600", BAUD_RATE_57600},   {"115200", BAUD_RATE_115200}, {"230400", BAUD_RATE_230400},
    {"256000", BAUD_RATE_256000}, {"460800", BAUD_RATE_460800},
};

constexpr StringToUint8 INSTALLATION_MODE_BY_STR[] = {
    {"Side Mount", INSTALLATION_SIDE_MOUNT},
    {"Top Mount", INSTALLATION_TOP_MOUNT},
};

constexpr Uint8ToString INSTALLATION_MODE_BY_UINT[] = {
    {INSTALLATION_SIDE_MOUNT, "Side Mount"},
    {INSTALLATION_TOP_MOUNT, "Top Mount"},
};

// Baud rates in the same order as BAUD_RATES_BY_STR for index-based lookup
constexpr uint32_t BAUD_RATES[] = {9600, 19200, 38400, 57600, 115200, 230400, 256000, 460800};

// Helper functions for lookups
template<size_t N> uint8_t find_uint8(const StringToUint8 (&arr)[N], const std::string &str) {
  for (const auto &entry : arr) {
    if (str == entry.str)
      return entry.value;
  }
  return 0xFF;  // Not found
}

template<size_t N> const char *find_str(const Uint8ToString (&arr)[N], uint8_t value) {
  for (const auto &entry : arr) {
    if (entry.value == value)
      return entry.str;
  }
  return nullptr;  // Not found
}

void LD2460Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LD2460...");

  // Clear buffers
  this->buffer_pos_ = 0;
  memset(this->buffer_data_, 0, sizeof(this->buffer_data_));
  memset(this->target_info_, 0, sizeof(this->target_info_));

  // Enable reporting and read all device information after a short delay
  this->set_timeout(1000, [this]() {
    this->enable_reporting(true);
    this->read_all_info();
  });
}

void LD2460Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2460:");

#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Target", this->target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "Target Count", this->target_count_sensor_);
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    ESP_LOGCONFIG(TAG, "  Target %d:", i + 1);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "X", this->target_x_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "Y", this->target_y_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "Angle", this->target_angle_sensors_[i]);
    LOG_SENSOR_WITH_DEDUP_SAFE("    ", "Distance", this->target_distance_sensors_[i]);
  }
#endif
}

void LD2460Component::loop() {
  while (this->available()) {
    this->readline_(this->read());
  }
}

void LD2460Component::readline_(int readch) {
  if (readch < 0) {
    return;
  }

  uint8_t c = (uint8_t) readch;

  // Debug: log every received byte
  ESP_LOGD(TAG, "Received byte: 0x%02X", c);

  // Add to buffer
  if (this->buffer_pos_ < sizeof(this->buffer_data_)) {
    this->buffer_data_[this->buffer_pos_++] = c;
  } else {
    // Buffer overflow - reset
    this->buffer_pos_ = 0;
    return;
  }

  // Check for periodic data frame (F4 F3 F2 F1 ... F8 F7 F6 F5)
  if (this->buffer_pos_ >= 4) {
    if (this->buffer_data_[0] == DATA_FRAME_HEADER_0 && this->buffer_data_[1] == DATA_FRAME_HEADER_1 &&
        this->buffer_data_[2] == DATA_FRAME_HEADER_2 && this->buffer_data_[3] == DATA_FRAME_HEADER_3) {
      // We have a data frame header, check if we have enough data for function + length
      if (this->buffer_pos_ >= 7) {
        // Frame structure: Header(4) + Function(1) + Length(2) + Data + Footer(4)
        // Length field is at bytes 5-6 (little endian) and represents total frame length
        uint16_t total_len = this->buffer_data_[5] | (this->buffer_data_[6] << 8);

        if (this->buffer_pos_ >= total_len) {
          // Check footer at end of frame
          uint16_t footer_offset = total_len - 4;
          if (this->buffer_data_[footer_offset] == DATA_FRAME_FOOTER_0 &&
              this->buffer_data_[footer_offset + 1] == DATA_FRAME_FOOTER_1 &&
              this->buffer_data_[footer_offset + 2] == DATA_FRAME_FOOTER_2 &&
              this->buffer_data_[footer_offset + 3] == DATA_FRAME_FOOTER_3) {
            // Valid data frame
            this->handle_periodic_data_(this->buffer_data_, total_len);
          }
          this->buffer_pos_ = 0;
        }
      }
    } else if (this->buffer_data_[0] == CMD_FRAME_HEADER_0 && this->buffer_data_[1] == CMD_FRAME_HEADER_1 &&
               this->buffer_data_[2] == CMD_FRAME_HEADER_2 && this->buffer_data_[3] == CMD_FRAME_HEADER_3) {
      // Command frame (acknowledgment)
      // Same structure: Header(4) + Function(1) + Length(2) + Data + Footer(4)
      if (this->buffer_pos_ >= 7) {
        uint16_t total_len = this->buffer_data_[5] | (this->buffer_data_[6] << 8);

        if (this->buffer_pos_ >= total_len) {
          uint16_t footer_offset = total_len - 4;
          if (this->buffer_data_[footer_offset] == CMD_FRAME_FOOTER_0 &&
              this->buffer_data_[footer_offset + 1] == CMD_FRAME_FOOTER_1 &&
              this->buffer_data_[footer_offset + 2] == CMD_FRAME_FOOTER_2 &&
              this->buffer_data_[footer_offset + 3] == CMD_FRAME_FOOTER_3) {
            this->handle_ack_data_(this->buffer_data_, total_len);
          }
          this->buffer_pos_ = 0;
        }
      }
    } else {
      // Not a valid frame start - shift buffer
      if (this->buffer_pos_ >= 4) {
        memmove(this->buffer_data_, this->buffer_data_ + 1, this->buffer_pos_ - 1);
        this->buffer_pos_--;
      }
    }
  }
}

void LD2460Component::handle_periodic_data_(const uint8_t *buffer, uint8_t len) {
  // Frame: [F4 F3 F2 F1] [len_low len_high] [func] [target_data...] [F8 F7 F6 F5]
  if (len < 11) {  // Minimum: header(4) + len(2) + func(1) + footer(4)
    return;
  }

  uint8_t function_code = buffer[6];
  if (function_code != FUNCTION_TARGET_DATA) {
    return;
  }

  // Calculate number of targets: (total_len - header - len - func - footer) / 4
  uint16_t data_len = buffer[4] | (buffer[5] << 8);
  uint8_t num_targets = (data_len - 11) / 4;  // Each target is 4 bytes (X, Y)

  if (num_targets > MAX_TARGETS) {
    num_targets = MAX_TARGETS;
  }

  // Parse target data
  uint8_t active_targets = 0;

  // First, mark all targets as invalid
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    this->target_info_[i].valid = false;
  }

  for (uint8_t i = 0; i < num_targets; i++) {
    uint8_t offset = 7 + (i * 4);  // Start after header(4) + len(2) + func(1)

    if (offset + 3 < len - 4) {  // Ensure we don't read past footer
      int16_t x = (int16_t) ((buffer[offset + 1] << 8) | buffer[offset]);
      int16_t y = (int16_t) ((buffer[offset + 3] << 8) | buffer[offset + 2]);

      // Scale by 0.1 (multiply by 10 to convert to mm)
      this->target_info_[i].x = x * 10;
      this->target_info_[i].y = y * 10;

      // Check if target is valid (non-zero position)
      if (x != 0 || y != 0) {
        this->target_info_[i].valid = true;
        active_targets++;

#ifdef USE_SENSOR
        // Publish X, Y coordinates
        if (this->target_x_sensors_[i] != nullptr) {
          this->target_x_sensors_[i]->publish_state_if_not_dup(this->target_info_[i].x);
        }
        if (this->target_y_sensors_[i] != nullptr) {
          this->target_y_sensors_[i]->publish_state_if_not_dup(this->target_info_[i].y);
        }

        // Calculate and publish distance
        if (this->target_distance_sensors_[i] != nullptr) {
          uint16_t distance = (uint16_t) sqrt(pow(this->target_info_[i].x, 2) + pow(this->target_info_[i].y, 2));
          this->target_distance_sensors_[i]->publish_state_if_not_dup(distance);
        }

        // Calculate and publish angle
        if (this->target_angle_sensors_[i] != nullptr) {
          float angle = atan2((float) this->target_info_[i].y, (float) this->target_info_[i].x) * 180.0f /
                        std::numbers::pi_v<float>;
          this->target_angle_sensors_[i]->publish_state_if_not_dup(angle);
        }
#endif
      }
    }
  }

#ifdef USE_SENSOR
  // Publish target count
  if (this->target_count_sensor_ != nullptr) {
    this->target_count_sensor_->publish_state_if_not_dup(active_targets);
  }
#endif

#ifdef USE_BINARY_SENSOR
  // Update presence
  if (active_targets > 0) {
    this->presence_millis_ = millis();
    if (this->target_binary_sensor_ != nullptr) {
      this->target_binary_sensor_->publish_state(true);
    }
  } else if (this->get_timeout_status_(this->presence_millis_)) {
    if (this->target_binary_sensor_ != nullptr) {
      this->target_binary_sensor_->publish_state(false);
    }
  }
#endif
}

bool LD2460Component::handle_ack_data_(const uint8_t *buffer, uint8_t len) {
  if (len < 12) {
    return false;
  }

  uint8_t command = buffer[6];
  uint8_t status = buffer[7];

  ESP_LOGD(TAG, "ACK received - Command: 0x%02X, Status: 0x%02X", command, status);

  // Handle version response (CMD_READ_VERSION = 0x0B)
  // Protocol format in Table 14: [installation_mode, year, month, major, minor]
  if (command == CMD_READ_VERSION && len >= 16) {
    this->installation_mode_ = buffer[7];
    this->version_[0] = buffer[8];   // Year
    this->version_[1] = buffer[9];   // Month
    this->version_[2] = buffer[10];  // Major version
    this->version_[3] = buffer[11];  // Minor version

    ESP_LOGI(TAG, "Version: 20%02d/%02d V%d.%d, Mode: %s", this->version_[0], this->version_[1], this->version_[2],
             this->version_[3], find_str(INSTALLATION_MODE_BY_UINT, this->installation_mode_));

#ifdef USE_TEXT_SENSOR
    if (this->version_text_sensor_ != nullptr) {
      char version_str[32];
      snprintf(version_str, sizeof(version_str), "20%02d/%02d V%d.%d", this->version_[0], this->version_[1],
               this->version_[2], this->version_[3]);
      this->version_text_sensor_->publish_state(version_str);
    }
#endif
  }

  // Handle installation mode response (CMD_READ_INSTALLATION_MODE = 0x0A)
  if (command == CMD_READ_INSTALLATION_MODE && len >= 12) {
    this->installation_mode_ = buffer[7];
    ESP_LOGI(TAG, "Installation mode: %s", find_str(INSTALLATION_MODE_BY_UINT, this->installation_mode_));
  }

  // Handle detection parameters response (CMD_READ_DETECTION_PARAMS = 0x08)
  // Protocol format in Table 8: response contains distance and angle at specific offsets
  // Frame: [FD FC FB FA] [0x08] [len] [len] [distance_low] [distance_high] [angle_low] [angle_high] ...
  if (command == CMD_READ_DETECTION_PARAMS && len >= 15) {
    // Offset 7-8: distance (little-endian, in cm * 100)
    uint16_t distance_value = buffer[7] | (buffer[8] << 8);
    // Offset 9-10: angle (little-endian, in degrees * 100)
    uint16_t angle_value = buffer[9] | (buffer[10] << 8);

    this->detection_distance_ = distance_value / 100.0f;
    this->detection_angle_ = angle_value / 100.0f;

    ESP_LOGI(TAG, "Detection params: distance=%.2fm, angle=%.0f°", this->detection_distance_, this->detection_angle_);
  }

  // Handle set detection parameters response (CMD_SET_DETECTION_PARAMS = 0x07)
  if (command == CMD_SET_DETECTION_PARAMS && len >= 12) {
    // Status byte indicates success/failure
    // 0x00 = failure, 0x01 = success
    if (status == 0x01) {
      ESP_LOGI(TAG, "Detection parameters set successfully");
    } else {
      ESP_LOGW(TAG, "Failed to set detection parameters");
    }
  }

  return true;
}

void LD2460Component::send_command_(uint8_t command, const uint8_t *data, uint8_t data_len) {
  uint8_t buffer[64];
  uint8_t pos = 0;

  // Header
  buffer[pos++] = CMD_FRAME_HEADER_0;
  buffer[pos++] = CMD_FRAME_HEADER_1;
  buffer[pos++] = CMD_FRAME_HEADER_2;
  buffer[pos++] = CMD_FRAME_HEADER_3;

  // Command/Function code
  buffer[pos++] = command;

  // Length calculation: entire frame size
  // Frame: header(4) + command(1) + length(2) + data(n) + footer(4) = 11 + n
  uint16_t length = 11 + data_len;
  buffer[pos++] = length & 0xFF;
  buffer[pos++] = (length >> 8) & 0xFF;

  // Data
  if (data != nullptr && data_len > 0) {
    memcpy(&buffer[pos], data, data_len);
    pos += data_len;
  }

  // Footer
  buffer[pos++] = CMD_FRAME_FOOTER_0;
  buffer[pos++] = CMD_FRAME_FOOTER_1;
  buffer[pos++] = CMD_FRAME_FOOTER_2;
  buffer[pos++] = CMD_FRAME_FOOTER_3;

  this->write_array(buffer, pos);
  this->flush();

  ESP_LOGV(TAG, "Sent command 0x%02X", command);
}

void LD2460Component::restart() {
  ESP_LOGI(TAG, "Restarting LD2460...");
  uint8_t data = 0x01;
  this->send_command_(CMD_RESTART, &data, 1);
}

void LD2460Component::factory_reset() {
  ESP_LOGI(TAG, "Factory reset LD2460...");
  uint8_t data = 0x01;
  this->send_command_(CMD_FACTORY_RESET, &data, 1);
}

void LD2460Component::enable_reporting(bool enable) {
  ESP_LOGI(TAG, "%s LD2460 reporting...", enable ? "Enabling" : "Disabling");
  uint8_t data = enable ? 0x01 : 0x00;
  this->send_command_(CMD_ENABLE_REPORTING, &data, 1);
}

void LD2460Component::set_baud_rate(const char *state) {
  uint8_t rate = find_uint8(BAUD_RATES_BY_STR, state);
  if (rate != 0xFF) {
    ESP_LOGI(TAG, "Setting baud rate to %s", state);
    this->send_command_(CMD_SET_BAUD_RATE, &rate, 1);
  } else {
    ESP_LOGW(TAG, "Invalid baud rate: %s", state);
  }
}

void LD2460Component::set_installation_mode(const char *state) {
  uint8_t mode = find_uint8(INSTALLATION_MODE_BY_STR, state);
  if (mode != 0xFF) {
    ESP_LOGI(TAG, "Setting installation mode to %s", state);
    this->send_command_(CMD_SET_INSTALLATION_MODE, &mode, 1);
    this->installation_mode_ = mode;
  } else {
    ESP_LOGW(TAG, "Invalid installation mode: %s", state);
  }
}

void LD2460Component::set_detection_distance(float value) {
  // Store the distance value
  this->detection_distance_ = value;
  ESP_LOGI(TAG, "Setting detection distance to %.2f m", value);
  // Send combined detection parameters command
  this->send_detection_params_();
}

void LD2460Component::set_detection_angle(float value) {
  // Store the angle value
  this->detection_angle_ = value;
  ESP_LOGI(TAG, "Setting detection angle to %.0f degrees", value);
  // Send combined detection parameters command
  this->send_detection_params_();
}

void LD2460Component::send_detection_params_() {
  // Protocol: CMD_SET_DETECTION_PARAMS (0x07)
  // Data: 4 bytes total
  //   - 2 bytes: distance in meters * 100 (little-endian)
  //   - 2 bytes: angle in degrees * 100 (little-endian)

  // Validate and convert distance (must be positive)
  float distance = std::max(0.0f, this->detection_distance_);
  uint16_t distance_value = (uint16_t) std::round(distance * 100.0f);

  // Validate and convert angle (must be positive)
  float angle = std::max(0.0f, this->detection_angle_);
  uint16_t angle_value = (uint16_t) std::round(angle * 100.0f);

  uint8_t data[4];
  data[0] = distance_value & 0xFF;         // Distance low byte
  data[1] = (distance_value >> 8) & 0xFF;  // Distance high byte
  data[2] = angle_value & 0xFF;            // Angle low byte
  data[3] = (angle_value >> 8) & 0xFF;     // Angle high byte

  ESP_LOGD(TAG, "Sending detection params: distance=%.2fm (0x%04X), angle=%.0f° (0x%04X)", distance, distance_value,
           angle, angle_value);

  this->send_command_(CMD_SET_DETECTION_PARAMS, data, 4);
}

void LD2460Component::read_version() {
  ESP_LOGD(TAG, "Reading version...");
  uint8_t data = 0x01;
  this->send_command_(CMD_READ_VERSION, &data, 1);
}

void LD2460Component::read_detection_params() {
  ESP_LOGD(TAG, "Reading detection parameters...");
  uint8_t data = 0x01;
  this->send_command_(CMD_READ_DETECTION_PARAMS, &data, 1);
}

void LD2460Component::read_installation_mode() {
  ESP_LOGD(TAG, "Reading installation mode...");
  uint8_t data = 0x01;
  this->send_command_(CMD_READ_INSTALLATION_MODE, &data, 1);
}

void LD2460Component::read_all_info() {
  ESP_LOGD(TAG, "Reading all device information...");
  this->read_version();
  this->read_detection_params();
}

bool LD2460Component::get_timeout_status_(uint32_t check_millis) {
  return (millis() - check_millis) > (this->timeout_ * 1000);
}

#ifdef USE_SENSOR
void LD2460Component::set_target_x_sensor(uint8_t target, sensor::Sensor *s) {
  if (target < MAX_TARGETS) {
    this->target_x_sensors_[target] = new SensorWithDedup<int16_t>(s);
  }
}

void LD2460Component::set_target_y_sensor(uint8_t target, sensor::Sensor *s) {
  if (target < MAX_TARGETS) {
    this->target_y_sensors_[target] = new SensorWithDedup<int16_t>(s);
  }
}

void LD2460Component::set_target_angle_sensor(uint8_t target, sensor::Sensor *s) {
  if (target < MAX_TARGETS) {
    this->target_angle_sensors_[target] = new SensorWithDedup<float>(s);
  }
}

void LD2460Component::set_target_distance_sensor(uint8_t target, sensor::Sensor *s) {
  if (target < MAX_TARGETS) {
    this->target_distance_sensors_[target] = new SensorWithDedup<uint16_t>(s);
  }
}
#endif

}  // namespace esphome::ld2460
