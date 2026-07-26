/**
 * @file CommunicationConfig.cpp
 * @brief Implementation of communication configuration
 * @author Philippe Hubert
 * @date 2025
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
