/**
 * @file CommunicationConfig.cpp
 * @brief Implementation of communication configuration
 * @author Philippe Hubert
 * @date 2025
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "CommunicationConfig.h"

// Default mode: ESP-NOW
// Change this to CommMode::LORA to use LoRa by default
CommMode CommunicationConfig::currentMode = CommMode::LORA;

CommMode CommunicationConfig::getMode() {
    return currentMode;
}

void CommunicationConfig::setMode(CommMode mode) {
    currentMode = mode;
}

const char* CommunicationConfig::getModeName() {
    switch (currentMode) {
        case CommMode::ESP_NOW:
            return "ESP-NOW";
        case CommMode::LORA:
            return "LoRa";
        default:
            return "Unknown";
    }
}
