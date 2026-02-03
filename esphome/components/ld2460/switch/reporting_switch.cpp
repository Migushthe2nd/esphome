#include "reporting_switch.h"

namespace esphome {
namespace ld2460 {

void ReportingSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->enable_reporting(state);
}

}  // namespace ld2460
}  // namespace esphome
