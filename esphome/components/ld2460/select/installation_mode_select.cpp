#include "installation_mode_select.h"

namespace esphome::ld2460 {

void InstallationModeSelect::control(size_t index) {
  auto value = this->at(index);
  this->publish_state(value.value());
  this->parent_->set_installation_mode(value.value().c_str());
}

}  // namespace esphome::ld2460
