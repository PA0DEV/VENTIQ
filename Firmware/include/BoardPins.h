#pragma once

#include <stdint.h>

namespace BoardPins {

constexpr uint8_t I2C_SCL = 5;       // D1 / GPIO5
constexpr uint8_t I2C_SDA = 4;       // D2 / GPIO4
constexpr uint8_t FAN1_TACH = 14;    // D5 / GPIO14
constexpr uint8_t FAN2_TACH = 12;    // D6 / GPIO12
constexpr uint8_t ONE_WIRE = 13;     // D7 / GPIO13

constexpr uint8_t ONBOARD_LED = 2;   // D4 / GPIO2
constexpr bool ONBOARD_LED_ACTIVE_LOW = true;

}  // namespace BoardPins