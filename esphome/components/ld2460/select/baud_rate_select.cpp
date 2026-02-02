#include "baud_rate_select.h"

namespace esphome::ld2460 {

void BaudRateSelect::control(size_t index) {
  auto value = this->at(index);
  this->publish_state(value.value());
  this->parent_->set_baud_rate(value.value().c_str());
}

}  // namespace esphome::ld2460
