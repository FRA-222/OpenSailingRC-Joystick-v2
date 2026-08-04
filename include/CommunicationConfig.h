/**
 * @file CommunicationConfig.h
 * @brief Configuration for communication mode selection
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

#ifndef COMMUNICATION_CONFIG_H
#define COMMUNICATION_CONFIG_H

/**
 * @brief Communication mode enumeration
 */
enum class CommMode {
    ESP_NOW,    ///< ESP-NOW communication (2.4 GHz, short range)
    LORA_920,   ///< LoRa communication, 920 MHz module (E220-JP)
    LORA_433    ///< LoRa communication, 433 MHz module (E220-400T22S)
};

/**
 * @brief Communication configuration class
 */
class CommunicationConfig {
public:
    /**
     * @brief Get current communication mode
     * @return Current CommMode
     */
    static CommMode getMode();
    
    /**
     * @brief Set communication mode
     * @param mode New communication mode
     */
    static void setMode(CommMode mode);
    
    /**
     * @brief Get mode name as string
     * @return Mode name ("ESP-NOW", "LoRa 920 MHz" or "LoRa 433 MHz")
     */
    static const char* getModeName();

    /**
     * @brief Tell whether a mode uses the LoRa transport
     *
     * Both LoRa bands share the whole protocol stack and differ only by the
     * radio configuration: use this for everything common, and an explicit test
     * on LORA_920 / LORA_433 only where the band actually matters.
     *
     * @param mode Mode to test
     * @return true for LORA_920 and LORA_433
     */
    static constexpr bool isLoRa(CommMode mode) {
        return mode == CommMode::LORA_920 || mode == CommMode::LORA_433;
    }
    
private:
    static CommMode currentMode;
};

#endif // COMMUNICATION_CONFIG_H
