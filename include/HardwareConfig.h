/**
 * @file HardwareConfig.h
 * @brief Brochage et modules des deux matériels de joystick (v1 AtomS3, v2 Core2)
 *
 * Sélectionné par JOYSTICK_HW (voir JoystickConfiguration.h et platformio.ini).
 * Tout ce qui dépend d'une carte est ici : bus I2C, adresses, pins LoRa,
 * dimensions d'écran, et la correspondance bouton physique → action.
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

#ifndef JOYSTICK_HW
#error "JOYSTICK_HW non défini : compiler avec -e joystick-v1-atoms3 ou -e joystick-v2-core2"
#endif

// Indices d'axes et de boutons — communs aux deux matériels (JoystickManager)
#define AXIS_LEFT_X  0
#define AXIS_LEFT_Y  1
#define AXIS_RIGHT_X 2
#define AXIS_RIGHT_Y 3

#define BTN_LEFT_STICK  0   ///< Appui du stick gauche
#define BTN_RIGHT_STICK 1   ///< Appui du stick droit
#define BTN_LEFT        2   ///< Bouton gauche (v1 : jaune sur STM32 ; v2 : Dual Button rouge)
#define BTN_RIGHT       3   ///< Bouton droit  (v1 : jaune sur STM32 ; v2 : Dual Button bleu)
#define BTN_ATOM_SCREEN 4   ///< Bouton A de l'appareil (écran AtomS3 / BtnA Core2)

#if JOYSTICK_HW == 1
// ============================================================================
// v1 — AtomS3 + carte STM32F030 (BuoyJoystick historique)
// ============================================================================
#define JOYSTICK_HW_NAME "v1 AtomS3"
#define JOYSTICK_WIRING_HINT "STM32 sur I2C Wire1 (SDA=38, SCL=39), LoRa sur G1/G2"

// Bus I2C du Grove AtomS3, contrôleur 1 (le 0 est pris par l'écran/IMU M5)
static const uint8_t JS_I2C_SDA_PIN = 38;
static const uint8_t JS_I2C_SCL_PIN = 39;
static const uint32_t JS_I2C_FREQ_HZ = 400000;

// STM32F030F4P6 : 2 sticks 12 bits, 4 boutons, 2 tensions batterie
static const uint8_t I2C_ADDR_STM32 = 0x59;
#define STM32_LEFT_STICK_X_REG    0x00
#define STM32_LEFT_STICK_Y_REG    0x02
#define STM32_RIGHT_STICK_X_REG   0x20
#define STM32_RIGHT_STICK_Y_REG   0x22
#define STM32_LEFT_STICK_BTN_REG  0x70
#define STM32_RIGHT_STICK_BTN_REG 0x71
#define STM32_LEFT_BTN_REG        0x72
#define STM32_RIGHT_BTN_REG       0x73
#define STM32_BATTERY1_VOLTAGE_REG 0x60
#define STM32_BATTERY2_VOLTAGE_REG 0x62

// LoRa E220 sur le Grove/GPIO de l'AtomS3
#define LORA_RX_PIN 1   // AtomS3 G1 (reçoit du LoRa TX)
#define LORA_TX_PIN 2   // AtomS3 G2 (envoie vers LoRa RX)

// Écran AtomS3
static const uint16_t DISPLAY_WIDTH  = 128;
static const uint16_t DISPLAY_HEIGHT = 128;

// Correspondance bouton physique → action (main.cpp)
// v1 : les deux boutons JAUNES du haut sont câblés sur les registres "stick",
// les appuis de stick sur les registres "button" (héritage de la carte STM32).
#define BTN_ACTION_INIT_HOME        BTN_LEFT_STICK   ///< Jaune gauche
#define BTN_ACTION_HOME_VALIDATION  BTN_RIGHT_STICK  ///< Jaune droit
#define BTN_ACTION_NAV_HOLD         BTN_LEFT         ///< Appui stick gauche
#define BTN_ACTION_NAV_STOP         BTN_RIGHT        ///< Appui stick droit
#define BTN_LABEL_INIT_HOME         "[L] Bouton jaune GAUCHE"
#define BTN_LABEL_HOME_VALIDATION   "[R] Bouton jaune DROIT"
#define BTN_LABEL_NAV_HOLD          "[JS-L] Bouton stick GAUCHE"
#define BTN_LABEL_NAV_STOP          "[JS-R] Bouton stick DROIT"

// Sens de l'axe X du stick droit : +1 = pousser à droite donne une valeur positive
#define RIGHT_STICK_X_SIGN (+1)

#elif JOYSTICK_HW == 2
// ============================================================================
// v2 — Core2 + ExtPort : 2 Unit Joystick, ByteButton, Dual Button
// ============================================================================
#define JOYSTICK_HW_NAME "v2 Core2"
#define JOYSTICK_WIRING_HINT "JS gauche Port A (SDA=32, SCL=33) ; JS droit + ByteButton Port C (SDA=14, SCL=13) ; LoRa Port B"

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
// l'inversion est faite par byteButtonSlot() dans JoystickManagerV2.cpp.
// Une LED RGB par touche, pilotée par registres (librairie M5Unit-ByteButton) :
static const uint8_t BYTEBUTTON_REG_LED_BRIGHTNESS = 0x10;  // + n (1 octet/LED)
static const uint8_t BYTEBUTTON_REG_LED_MODE = 0x19;        // 0 = piloté par l'hôte
static const uint8_t BYTEBUTTON_REG_RGB888 = 0x20;          // + n*4 (4 octets/LED)
static const uint32_t BYTEBUTTON_COLOR_SELECTED = 0x00FF00; // vert
static const uint8_t BYTEBUTTON_LED_BRIGHTNESS = 80;        // 0-255

// Dual button on ExtPort GPIO (update these pins to match final wiring)
static const uint8_t DUAL_BUTTON_1_GPIO = 19;
static const uint8_t DUAL_BUTTON_2_GPIO = 27;

// LoRa E220 sur le Port B du Core2 (ExtPort ou base M5GO : jaune=G26, blanc=G36)
// Avec un câble Grove droit : Core2 TX (G26) → LoRa RX ; Core2 RX (G36) ← LoRa TX
// (G36 est entrée-seule : RX uniquement)
#define LORA_RX_PIN 36  // Core2 Port B blanc (reçoit du LoRa TX)
#define LORA_TX_PIN 26  // Core2 Port B jaune (envoie vers LoRa RX)

// Core2 display dimensions
static const uint16_t DISPLAY_WIDTH  = 320;
static const uint16_t DISPLAY_HEIGHT = 240;

// Correspondance bouton physique → action (main.cpp)
#define BTN_ACTION_INIT_HOME        BTN_RIGHT        ///< Dual Button droit (bleu)
#define BTN_ACTION_HOME_VALIDATION  BTN_LEFT         ///< Dual Button gauche (rouge)
#define BTN_ACTION_NAV_HOLD         BTN_LEFT_STICK   ///< Appui stick gauche
#define BTN_ACTION_NAV_STOP         BTN_RIGHT_STICK  ///< Appui stick droit
#define BTN_LABEL_INIT_HOME         "[DUAL-R] Bouton DualButton DROIT (bleu)"
#define BTN_LABEL_HOME_VALIDATION   "[DUAL-L] Bouton DualButton GAUCHE (rouge)"
#define BTN_LABEL_NAV_HOLD          "[JS-L] Bouton stick GAUCHE"
#define BTN_LABEL_NAV_STOP          "[JS-R] Bouton stick DROIT"

// L'axe X du Unit Joystick droit est inversé : pousser à droite donne une valeur négative
#define RIGHT_STICK_X_SIGN (-1)

#endif // JOYSTICK_HW

// Pins M0/M1/AUX du E220 si pilotées par logiciel (voir LORA_USE_SOFTWARE_M0M1
// dans LoRaCommunication.h). Non câblées sur les deux matériels actuels.
#define LORA_M0_PIN 7
#define LORA_M1_PIN 8
#define LORA_AUX_PIN 41

/**
 * @brief Result of the hardware diagnostic run at boot
 *
 * Structure commune aux deux matériels ; les champs sans équivalent sur un
 * matériel sont laissés à faux / 0xFF (v1 : pas de ByteButton, pas de Dual
 * Button, un seul esclave I2C — le STM32 — qui porte les deux sticks).
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
