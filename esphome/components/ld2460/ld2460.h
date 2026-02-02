#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#include "esphome/components/ld24xx/ld24xx.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"

#include <array>

namespace esphome::ld2460 {

using namespace ld24xx;

// Constants
static constexpr uint8_t DEFAULT_PRESENCE_TIMEOUT = 5;   // Timeout to reset presence status 5 sec.
static constexpr uint8_t MAX_TARGETS = 3;                // Max 3 Targets in LD2460
static constexpr uint16_t MAX_DETECTION_DISTANCE = 600;  // Max detection distance in cm (6m)

// Target coordinate struct
struct Target {
  int16_t x;
  int16_t y;
  bool valid;  // Whether this target has data
};

class LD2460Component : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(target_count, uint8_t)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(detection_distance)
  SUB_NUMBER(detection_angle)
#endif
#ifdef USE_SELECT
  SUB_SELECT(baud_rate)
  SUB_SELECT(installation_mode)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(factory_reset)
  SUB_BUTTON(restart)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void restart();
  void factory_reset();
  void set_baud_rate(const char *state);
  void set_installation_mode(const char *state);
  void set_detection_distance(float value);
  void set_detection_angle(float value);
  void read_version();
  void read_detection_params();
  void read_all_info();  // Convenience: read version + detection params

  // Get target data for use in lambdas (for zone calculations, etc.)
  Target get_target(uint8_t index) const {
    if (index < MAX_TARGETS) {
      return target_info_[index];
    }
    return {0, 0, false};
  }
  uint8_t get_target_count() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
      if (target_info_[i].valid) {
        count++;
      }
    }
    return count;
  }

#ifdef USE_SENSOR
  void set_target_x_sensor(uint8_t target, sensor::Sensor *s);
  void set_target_y_sensor(uint8_t target, sensor::Sensor *s);
  void set_target_angle_sensor(uint8_t target, sensor::Sensor *s);
  void set_target_distance_sensor(uint8_t target, sensor::Sensor *s);
#endif

 protected:
  void send_command_(uint8_t command, const uint8_t *data, uint8_t data_len);
  void send_detection_params_();
  void handle_periodic_data_(const uint8_t *buffer, uint8_t len);
  bool handle_ack_data_(const uint8_t *buffer, uint8_t len);
  void readline_(int readch);
  bool get_timeout_status_(uint32_t check_millis);

  uint32_t presence_millis_ = 0;
  uint16_t timeout_ = DEFAULT_PRESENCE_TIMEOUT;
  uint8_t buffer_data_[128];
  uint8_t buffer_pos_ = 0;
  uint8_t version_[4] = {0, 0, 0, 0};  // Year, Month, Major, Minor
  uint8_t installation_mode_ = 1;      // 1 = side-mount, 2 = top-mount
  float detection_distance_ = 2.6f;    // Default: 2.6 meters
  float detection_angle_ = 30.0f;      // Default: 30 degrees
  Target target_info_[MAX_TARGETS];

#ifdef USE_SENSOR
  std::array<SensorWithDedup<int16_t> *, MAX_TARGETS> target_x_sensors_{};
  std::array<SensorWithDedup<int16_t> *, MAX_TARGETS> target_y_sensors_{};
  std::array<SensorWithDedup<float> *, MAX_TARGETS> target_angle_sensors_{};
  std::array<SensorWithDedup<uint16_t> *, MAX_TARGETS> target_distance_sensors_{};
#endif
};

}  // namespace esphome::ld2460
