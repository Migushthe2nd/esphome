#include "baud_rate_select.h"

namespace esphome::ld2460 {

void BaudRateSelect::control(const std::string &value, size_t index) {
  this->publish_state(value);
  this->parent_->set_baud_rate(value.c_str());
}

}  // namespace esphome::ld2460
