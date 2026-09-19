/**
 * @file JoystickManagerV1.cpp
 * @brief Entrées du joystick v1 : AtomS3 + carte STM32F030 (repris de OpenSailingRC-BuoyJoystick)
 * @author Philippe Hubert
 * @date 2025-2026
 *
 * Implémente l'interface commune JoystickManager.h pour le matériel v1 :
 * les deux sticks (12 bits), les quatre boutons et les deux tensions batterie
 * sont lus dans les registres I2C du STM32 (adresse 0x59, bus Wire1 sur le
 * Grove de l'AtomS3). Le bouton d'écran de l'AtomS3 est M5.BtnA.
 *
 * Le matériel v1 n'a ni ByteButton ni Dual Button : les méthodes
 * correspondantes sont neutres (aucune touche, aucune LED). La sélection de
 * bouée se fait avec le bouton d'écran (bouée suivante).
 *
 * Compilé uniquement dans l'environnement joystick-v1-atoms3 (platformio.ini).
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "JoystickManager.h"
#include <M5Unified.h>

#if JOYSTICK_HW != 1
#error "JoystickManagerV1.cpp ne se compile que pour le matériel v1 (JOYSTICK_HW=1)"
#endif

namespace {

TwoWire& stm32Bus() { return Wire1; }

uint16_t readWord(uint8_t reg) {
    uint8_t buffer[2];

    stm32Bus().beginTransmission(I2C_ADDR_STM32);
    stm32Bus().write(reg);
    stm32Bus().endTransmission(false);

    stm32Bus().requestFrom(I2C_ADDR_STM32, (uint8_t)2);

    if (stm32Bus().available() >= 2) {
        buffer[0] = stm32Bus().read();  // LSB
        buffer[1] = stm32Bus().read();  // MSB
        return (uint16_t)(buffer[0] | (buffer[1] << 8));
    }

    return 2048;  // Valeur par défaut (centre) en cas d'erreur
}

uint8_t readByte(uint8_t reg) {
    stm32Bus().beginTransmission(I2C_ADDR_STM32);
    stm32Bus().write(reg);
    stm32Bus().endTransmission(false);

    stm32Bus().requestFrom(I2C_ADDR_STM32, (uint8_t)1);

    if (stm32Bus().available()) {
        return stm32Bus().read();
    }

    return 0xFF;  // Valeur par défaut (relâché) en cas d'erreur
}

bool probeStm32() {
    stm32Bus().beginTransmission(I2C_ADDR_STM32);
    return (stm32Bus().endTransmission() == 0);
}

}  // namespace

JoystickManager::JoystickManager() {
    for (int i = 0; i < 5; i++) {  // 5 boutons (incluant l'écran AtomS3)
        if (i < 4) {
            axisValues[i] = 2048;  // Valeur centrée par défaut
        }
        buttonState[i] = false;
        buttonPrevState[i] = false;
    }
    batteryVoltage[0] = 0.0f;
    batteryVoltage[1] = 0.0f;
    atomScreenPressTime = 0;

    // Champs v2 sans objet sur ce matériel, laissés à « absent »
    byteButtonAddrEffective = 0xFF;
    byteButtonSdaEffective = 0xFF;
    byteButtonSclEffective = 0xFF;
    byteButtonNextProbeMs = 0;
    byteMaskCurrent = 0xFF;
    byteMaskPrev = 0xFF;
    stickBtnIdle[0] = 0;
    stickBtnIdle[1] = 0;
    ledSelectedIndex = 0xFF;
    i2cBus = &Wire1;
}

bool JoystickManager::begin() {
    // Bus I2C du Grove AtomS3 : contrôleur 1 (SDA=38, SCL=39)
    stm32Bus().begin(JS_I2C_SDA_PIN, JS_I2C_SCL_PIN);
    stm32Bus().setClock(JS_I2C_FREQ_HZ);

    delay(100);

    bool found = probeStm32();
    if (found) {
        Logger::logf("✓ JoystickManager v1: STM32 détecté sur I2C 0x%02X (SDA=%d, SCL=%d)",
                     I2C_ADDR_STM32, JS_I2C_SDA_PIN, JS_I2C_SCL_PIN);
    } else {
        Logger::logf("✗ JoystickManager v1: STM32 absent sur I2C 0x%02X (SDA=%d, SCL=%d)",
                     I2C_ADDR_STM32, JS_I2C_SDA_PIN, JS_I2C_SCL_PIN);
    }
    return found;
}

void JoystickManager::update() {
    // Lit les valeurs des 4 axes (12 bits, 0..4095)
    axisValues[AXIS_LEFT_X] = readWord(STM32_LEFT_STICK_X_REG);
    axisValues[AXIS_LEFT_Y] = readWord(STM32_LEFT_STICK_Y_REG);
    axisValues[AXIS_RIGHT_X] = readWord(STM32_RIGHT_STICK_X_REG);
    axisValues[AXIS_RIGHT_Y] = readWord(STM32_RIGHT_STICK_Y_REG);

    // Lit l'état des boutons (actifs à LOW dans le registre)
    for (int i = 0; i < 4; i++) {
        buttonPrevState[i] = buttonState[i];
    }
    buttonState[BTN_LEFT_STICK] = (readByte(STM32_LEFT_STICK_BTN_REG) & 0x01) == 0;
    buttonState[BTN_RIGHT_STICK] = (readByte(STM32_RIGHT_STICK_BTN_REG) & 0x01) == 0;
    buttonState[BTN_LEFT] = (readByte(STM32_LEFT_BTN_REG) & 0x01) == 0;
    buttonState[BTN_RIGHT] = (readByte(STM32_RIGHT_BTN_REG) & 0x01) == 0;

    // Bouton de l'écran AtomS3
    M5.update();
    buttonPrevState[BTN_ATOM_SCREEN] = buttonState[BTN_ATOM_SCREEN];
    buttonState[BTN_ATOM_SCREEN] = M5.BtnA.isPressed();

    // Horodatage de l'appui pour la détection de maintien
    if (buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = millis();
    } else if (!buttonState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = 0;
    }

    // Tensions des deux batteries (mV dans le registre)
    batteryVoltage[0] = (float)readWord(STM32_BATTERY1_VOLTAGE_REG) / 1000.0f;
    batteryVoltage[1] = (float)readWord(STM32_BATTERY2_VOLTAGE_REG) / 1000.0f;
}

uint16_t JoystickManager::getAxisValue(uint8_t axis) {
    if (axis < 4) {
        return axisValues[axis];
    }
    return 2048;
}

int16_t JoystickManager::getAxisCentered(uint8_t axis) {
    if (axis < 4) {
        return (int16_t)axisValues[axis] - 2048;   // 0..4095 → -2048..+2047
    }
    return 0;
}

bool JoystickManager::isButtonPressed(uint8_t button) {
    return (button < 5) ? buttonState[button] : false;
}

bool JoystickManager::wasButtonPressed(uint8_t button) {
    return (button < 5) ? (buttonState[button] && !buttonPrevState[button]) : false;
}

bool JoystickManager::wasButtonReleased(uint8_t button) {
    return (button < 5) ? (!buttonState[button] && buttonPrevState[button]) : false;
}

bool JoystickManager::isAtomScreenPressed() {
    return buttonState[BTN_ATOM_SCREEN];
}

bool JoystickManager::wasAtomScreenPressed() {
    return buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN];
}

bool JoystickManager::wasAtomScreenReleased() {
    return !buttonState[BTN_ATOM_SCREEN] && buttonPrevState[BTN_ATOM_SCREEN];
}

bool JoystickManager::isAtomScreenHeld(uint32_t durationMs) {
    if (!buttonState[BTN_ATOM_SCREEN] || atomScreenPressTime == 0) {
        return false;
    }
    return (millis() - atomScreenPressTime) >= durationMs;
}

float JoystickManager::getBattery1Voltage() {
    return batteryVoltage[0];
}

float JoystickManager::getBattery2Voltage() {
    return batteryVoltage[1];
}

// ── Modules absents du matériel v1 : interface neutre ───────────────────────

int8_t JoystickManager::getByteButtonPressedIndex() {
    return -1;   // pas de ByteButton : la sélection passe par le bouton d'écran
}

uint8_t JoystickManager::getByteButtonMask() {
    return 0xFF;
}

void JoystickManager::setBuoySelectionLed(uint8_t index) {
    ledSelectedIndex = index;   // mémorisé, aucune LED à piloter
}

void JoystickManager::initByteButtonLeds() {}

bool JoystickManager::readUnitJoystickHW(uint8_t* buf) {
    (void)buf;
    return false;
}

uint8_t JoystickManager::readByteButtonMask() {
    return 0xFF;
}

HardwareDiagResult JoystickManager::performDiagnostic() {
    HardwareDiagResult d;
    memset(&d, 0, sizeof(d));
    d.sdaPin = JS_I2C_SDA_PIN;
    d.sclPin = JS_I2C_SCL_PIN;
    d.freqHz = JS_I2C_FREQ_HZ;
    d.byteButtonSdaPin = 0xFF;
    d.byteButtonSclPin = 0xFF;

    delay(20);

    // Scan complet du bus Grove 0x03 → 0x77
    Logger::log("--- Scan I2C Grove 0x03-0x77 ---");
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        stm32Bus().beginTransmission(addr);
        if (stm32Bus().endTransmission() == 0) {
            if (d.scannedCount < 16) {
                d.scannedAddrs[d.scannedCount++] = addr;
            }
            Logger::logf("  Trouvé: 0x%02X", addr);
        }
    }
    Logger::logf("  Total: %d dispositif(s)", d.scannedCount);

    // Le STM32 porte les DEUX sticks : un seul esclave, deux lignes de diagnostic
    bool stm32Found = false;
    for (uint8_t i = 0; i < d.scannedCount; i++) {
        if (d.scannedAddrs[i] == I2C_ADDR_STM32) stm32Found = true;
    }
    d.joystickLeftAddr         = I2C_ADDR_STM32;
    d.joystickLeftFound        = stm32Found;
    d.joystickLeftAddrScanned  = stm32Found ? I2C_ADDR_STM32 : 0xFF;
    d.joystickRightAddr        = I2C_ADDR_STM32;
    d.joystickRightFound       = stm32Found;
    d.joystickRightAddrScanned = stm32Found ? I2C_ADDR_STM32 : 0xFF;

    d.byteButtonAddr = 0xFF;
    d.byteButtonAddrScanned = 0xFF;
    d.byteButtonFound = false;
    d.dualBtn1Reachable = false;
    d.dualBtn2Reachable = false;
    d.dualBtn1Gpio = 0xFF;
    d.dualBtn2Gpio = 0xFF;

    d.allCriticalOk = stm32Found;

    Logger::log("--- Diagnostic Hardware v1 ---");
    Logger::logf("  STM32 sticks+boutons 0x%02X : %s", I2C_ADDR_STM32, stm32Found ? "OK" : "KO");
    Logger::logf("  Batteries : %.2f V / %.2f V", batteryVoltage[0], batteryVoltage[1]);
    Logger::logf("  Statut global  : %s", d.allCriticalOk ? "GO" : "ATTENTION");
    Logger::log("------------------------------");

    return d;
}
