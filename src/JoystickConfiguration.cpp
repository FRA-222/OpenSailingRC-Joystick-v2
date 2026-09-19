/**
 * @file JoystickConfiguration.cpp
 * @brief Accès lisible à la configuration du joystick
 * @author Philippe Hubert
 * @date 2026
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "JoystickConfiguration.h"
#include "HardwareConfig.h"
#include "Logger.h"

const char* JoystickConfiguration::hardwareName() {
    return isV1() ? "v1 AtomS3" : "v2 Core2";
}

const char* JoystickConfiguration::productName() {
    return isV1() ? "Joystick v1" : "Joystick v2";
}

const char* JoystickConfiguration::wiringHint() {
    return JOYSTICK_WIRING_HINT;
}

void JoystickConfiguration::logSummary() {
    Logger::log("--- Configuration joystick ---");
    Logger::logf("  Materiel      : %s (JOYSTICK_HW=%d)", hardwareName(), (int)JOYSTICK_HARDWARE);
    Logger::logf("  Firmware      : %s", JOYSTICK_FIRMWARE_VERSION);
    Logger::logf("  Communication : %s", CommunicationConfig::getModeName());
    if (CommunicationConfig::isLoRa(COMM_MODE)) {
        Logger::logf("  LoRa UART     : RX=GPIO%d TX=GPIO%d", LORA_RX_PIN, LORA_TX_PIN);
        Logger::logf("  ESP-NOW passif: %s", JOYSTICK_ESPNOW_PASSIVE ? "actif" : "coupe");
    }
    Logger::logf("  Ecran         : %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    Logger::log("------------------------------");
}
