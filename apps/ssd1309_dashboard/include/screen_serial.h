#pragma once

#include <Arduino.h>

#include "screen_control.h"

namespace screen_serial {

void printHelp();

bool handleCommand(screen_control::ControlContext& ctx,
                   const String& cmd,
                   uint32_t nowMs);

}  // namespace screen_serial

