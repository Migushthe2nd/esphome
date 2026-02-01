#include "installation_mode_select.h"

namespace esphome::ld2460 {

void InstallationModeSelect::control(const std::string &value, size_t index) {
  this->publish_state(value);
  this->parent_->set_installation_mode(value.c_str());
}

}  // namespace esphome::ld2460
