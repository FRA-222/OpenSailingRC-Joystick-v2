/**
 * @file HardwareConfig.h
 * @brief Hardware mapping for OpenSailingRC Joystick v2 (Core2 + ExtPort)
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

// Core2 Port A default I2C pins
static const uint8_t CORE2_I2C_SDA_PIN = 32;
static const uint8_t CORE2_I2C_SCL_PIN = 33;
static const uint32_t CORE2_I2C_FREQ_HZ = 100000;

// ExtPort Port C used for ByteButton (yellow=SDA, white=SCL)
static const uint8_t BYTEBUTTON_I2C_SDA_PIN = 14;
static const uint8_t BYTEBUTTON_I2C_SCL_PIN = 13;

// M5 Unit Joystick (U024 v1.1) : adresse fixe 0x52, identique pour toutes
// les unités. Les deux joysticks doivent donc être sur des bus séparés :
//   - Gauche : Port A (bus matériel, SDA=32/SCL=33)
//   - Droit  : Port C (bus logiciel, SDA=14/SCL=13), chaîné avec le ByteButton
// Protocole : lecture directe de 3 octets (X, Y, bouton), valeurs 8 bits.
static const uint8_t I2C_ADDR_JOYSTICK = 0x52;
static const uint8_t I2C_ADDR_BYTEBUTTON = 0x47;

// ByteButton : 8 touches (bits actifs à LOW) = sélection directe de la
// bouée 0-7, ordonnées de gauche à droite (touche la plus à gauche ->
// bouée #1). Attention : le bit 0 du masque est la touche la plus à DROITE,
// l'inversion est faite par byteButtonSlot() dans JoystickManager.cpp.
// Une LED RGB par touche, pilotée par registres (librairie M5Unit-ByteButton) :
static const uint8_t BYTEBUTTON_REG_LED_BRIGHTNESS = 0x10;  // + n (1 octet/LED)
static const uint8_t BYTEBUTTON_REG_LED_MODE = 0x19;        // 0 = piloté par l'hôte
static const uint8_t BYTEBUTTON_REG_RGB888 = 0x20;          // + n*4 (4 octets/LED)
static const uint32_t BYTEBUTTON_COLOR_SELECTED = 0x00FF00; // vert
static const uint8_t BYTEBUTTON_LED_BRIGHTNESS = 80;        // 0-255

// Dual button on ExtPort GPIO (update these pins to match final wiring)
static const uint8_t DUAL_BUTTON_1_GPIO = 19;
static const uint8_t DUAL_BUTTON_2_GPIO = 27;

// Core2 display dimensions
static const uint16_t DISPLAY_WIDTH  = 320;
static const uint16_t DISPLAY_HEIGHT = 240;

/**
 * @brief Result of the hardware diagnostic run at boot
 */
struct HardwareDiagResult {
    // I2C bus
    uint8_t sdaPin;
    uint8_t sclPin;
    uint32_t freqHz;
    uint8_t byteButtonSdaPin;
    uint8_t byteButtonSclPin;

    // Joystick Left
    bool joystickLeftFound;
    uint8_t joystickLeftAddr;
    uint8_t joystickLeftAddrScanned;   ///< Effective address seen on bus (0xFF = none)

    // Joystick Right
    bool joystickRightFound;
    uint8_t joystickRightAddr;
    uint8_t joystickRightAddrScanned;

    // ByteButton
    bool byteButtonFound;
    uint8_t byteButtonAddr;
    uint8_t byteButtonAddrScanned;

    // Dual Button GPIO
    bool dualBtn1Reachable;   ///< GPIO configured (true = init OK)
    bool dualBtn2Reachable;
    uint8_t dualBtn1Gpio;
    uint8_t dualBtn2Gpio;

    // Overall go/no-go
    bool allCriticalOk;       ///< At least both joysticks found

    // Raw I2C bus scan results (all addresses that responded)
    uint8_t scannedAddrs[16];
    uint8_t scannedCount;     ///< Number of devices found by full bus scan
};

#endif // HARDWARE_CONFIG_H
