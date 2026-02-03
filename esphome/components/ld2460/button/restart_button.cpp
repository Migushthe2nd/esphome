#include "restart_button.h"

namespace esphome::ld2460 {

void RestartButton::press_action() { this->parent_->restart(); }

}  // namespace esphome::ld2460
