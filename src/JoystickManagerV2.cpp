/**
 * @file JoystickManager.cpp
 * @brief Implémentation de la gestion des joysticks et boutons
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

#include "JoystickManager.h"
#include <M5Unified.h>

namespace {

bool probeI2C(TwoWire* bus, uint8_t addr) {
    bus->beginTransmission(addr);
    return (bus->endTransmission() == 0);
}

/**
 * Bus I2C logiciel (bit-bang) pour le ByteButton sur le Port C.
 * L'ESP32 n'a que 2 contrôleurs I2C matériels : le contrôleur 1 est pris
 * par M5Unified (bus interne 21/22 : AXP192, tactile, RTC) et le
 * contrôleur 0 est utilisé pour les joysticks du Port A (32/33).
 * Lignes en pseudo drain ouvert : LOW piloté, HIGH relâché via INPUT_PULLUP.
 */
class SoftI2CBus {
public:
    SoftI2CBus(uint8_t sda, uint8_t scl) : sdaPin(sda), sclPin(scl) {}

    void begin() {
        release(sdaPin);
        release(sclPin);
    }

    bool probe(uint8_t addr) {
        start();
        bool ack = writeByte(addr << 1);
        stop();
        return ack;
    }

    bool readReg(uint8_t addr, uint8_t reg, uint8_t& value) {
        start();
        if (!writeByte(addr << 1)) { stop(); return false; }
        if (!writeByte(reg))       { stop(); return false; }
        start();  // repeated start
        if (!writeByte((addr << 1) | 1)) { stop(); return false; }
        value = readByte(false);
        stop();
        return true;
    }

    // Écriture de len octets à partir d'un registre.
    bool writeReg(uint8_t addr, uint8_t reg, const uint8_t* buf, uint8_t len) {
        start();
        if (!writeByte(addr << 1)) { stop(); return false; }
        if (!writeByte(reg))       { stop(); return false; }
        for (uint8_t i = 0; i < len; i++) {
            if (!writeByte(buf[i])) { stop(); return false; }
        }
        stop();
        return true;
    }

    // Lecture directe de len octets sans pointeur de registre
    // (protocole du M5 Unit Joystick).
    bool readBytes(uint8_t addr, uint8_t* buf, uint8_t len) {
        start();
        if (!writeByte((addr << 1) | 1)) { stop(); return false; }
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = readByte(i + 1 < len);  // ACK sauf sur le dernier octet
        }
        stop();
        return true;
    }

private:
    uint8_t sdaPin;
    uint8_t sclPin;

    void release(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }
    void driveLow(uint8_t pin) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
    void tick() { delayMicroseconds(5); }  // ~100 kHz

    void sclHigh() {
        release(sclPin);
        // Tolère le clock stretching de l'esclave
        uint32_t t0 = micros();
        while (digitalRead(sclPin) == LOW && (micros() - t0) < 1000) {}
    }

    void start() {
        release(sdaPin); sclHigh(); tick();
        driveLow(sdaPin); tick();
        driveLow(sclPin); tick();
    }

    void stop() {
        driveLow(sdaPin); tick();
        sclHigh(); tick();
        release(sdaPin); tick();
    }

    bool writeByte(uint8_t b) {
        for (int8_t i = 7; i >= 0; i--) {
            if (b & (1 << i)) release(sdaPin); else driveLow(sdaPin);
            tick();
            sclHigh(); tick();
            driveLow(sclPin);
        }
        release(sdaPin);  // laisse l'esclave poser l'ACK
        tick();
        sclHigh(); tick();
        bool ack = (digitalRead(sdaPin) == LOW);
        driveLow(sclPin); tick();
        return ack;
    }

    uint8_t readByte(bool ack) {
        uint8_t b = 0;
        release(sdaPin);
        for (int8_t i = 7; i >= 0; i--) {
            sclHigh(); tick();
            if (digitalRead(sdaPin)) b |= (1 << i);
            driveLow(sclPin); tick();
        }
        if (ack) driveLow(sdaPin); else release(sdaPin);
        tick();
        sclHigh(); tick();
        driveLow(sclPin); tick();
        release(sdaPin);
        return b;
    }
};

SoftI2CBus byteButtonBus(BYTEBUTTON_I2C_SDA_PIN, BYTEBUTTON_I2C_SCL_PIN);

const uint8_t kByteButtonCandidates[] = {I2C_ADDR_BYTEBUTTON, 0x44, 0x46, 0x47};

uint8_t detectByteButtonAddr() {
    for (uint8_t i = 0; i < sizeof(kByteButtonCandidates); i++) {
        if (byteButtonBus.probe(kByteButtonCandidates[i])) {
            return kByteButtonCandidates[i];
        }
    }
    return 0xFF;
}

}  // namespace

JoystickManager::JoystickManager() {
    for (int i = 0; i < 5; i++) {  // 5 boutons maintenant (incluant Atom screen)
        if (i < 4) {
            axisValues[i] = 2048;  // Valeur centrée par défaut
        }
        buttonState[i] = false;
        buttonPrevState[i] = false;
    }
    batteryVoltage[0] = 0.0f;
    batteryVoltage[1] = 0.0f;
    atomScreenPressTime = 0;
    byteMaskCurrent = 0xFF;
    byteMaskPrev = 0xFF;
    stickBtnIdle[0] = 0;
    stickBtnIdle[1] = 0;
    ledSelectedIndex = 0xFF;
    byteButtonAddrEffective = I2C_ADDR_BYTEBUTTON;
    byteButtonSdaEffective = BYTEBUTTON_I2C_SDA_PIN;
    byteButtonSclEffective = BYTEBUTTON_I2C_SCL_PIN;
    byteButtonNextProbeMs = 0;
    i2cBus = &Wire;  // Contrôleur I2C 0 pour le Port A (SDA=32/SCL=33).
                     // Le contrôleur 1 (Wire1) est réservé à M5Unified pour le
                     // bus interne du Core2 (AXP192, tactile, RTC sur 21/22).
}

bool JoystickManager::begin() {
    // Bus joystick dédié sur le contrôleur I2C 0 (Port A).
    i2cBus->begin(CORE2_I2C_SDA_PIN, CORE2_I2C_SCL_PIN);
    i2cBus->setClock(CORE2_I2C_FREQ_HZ);

    // Bus ByteButton logiciel (bit-bang) sur le Port C.
    byteButtonBus.begin();

    // Initialise les entrées GPIO du Dual Button (actif à LOW)
    pinMode(DUAL_BUTTON_1_GPIO, INPUT_PULLUP);
    pinMode(DUAL_BUTTON_2_GPIO, INPUT_PULLUP);

    delay(100);

    bool leftOk = probeI2C(i2cBus, I2C_ADDR_JOYSTICK);
    bool rightOk = byteButtonBus.probe(I2C_ADDR_JOYSTICK);

    // Étalonne l'octet bouton de chaque stick au repos (polarité non
    // documentée sur le Unit Joystick) : appuyé = différent de cette valeur.
    uint8_t jsInit[3];
    if (leftOk && readUnitJoystickHW(jsInit)) {
        stickBtnIdle[0] = jsInit[2];
    }
    if (rightOk && byteButtonBus.readBytes(I2C_ADDR_JOYSTICK, jsInit, 3)) {
        stickBtnIdle[1] = jsInit[2];
    }

    // Auto-détection ByteButton sur son bus dédié port C.
    // S'il ne répond pas encore (boot du module plus lent que le Core2),
    // readByteButtonMask() retentera périodiquement.
    byteButtonAddrEffective = detectByteButtonAddr();
    byteButtonSdaEffective = BYTEBUTTON_I2C_SDA_PIN;
    byteButtonSclEffective = BYTEBUTTON_I2C_SCL_PIN;
    byteButtonNextProbeMs = millis() + 500;

    // Un masque 0xFF est normal au repos (boutons actifs à LOW) :
    // la présence du module se juge sur l'ACK de son adresse.
    bool byteButtonOk = (byteButtonAddrEffective != 0xFF);
    if (byteButtonOk) {
        initByteButtonLeds();
    }

    Logger::logf("JoystickManager v2: I2C SDA=%d SCL=%d @ %lu Hz",
                 CORE2_I2C_SDA_PIN, CORE2_I2C_SCL_PIN, CORE2_I2C_FREQ_HZ);
    Logger::logf("JoystickManager v2: joystickL=0x%02X (Port A) %s",
                 I2C_ADDR_JOYSTICK, leftOk ? "OK" : "KO");
    Logger::logf("JoystickManager v2: joystickR=0x%02X (Port C) %s",
                 I2C_ADDR_JOYSTICK, rightOk ? "OK" : "KO");
    Logger::logf("JoystickManager v2: byteButton=0x%02X %s",
                 I2C_ADDR_BYTEBUTTON, byteButtonOk ? "OK" : "KO");
    if (byteButtonAddrEffective != 0xFF) {
        Logger::logf("JoystickManager v2: byteButton detecte sur 0x%02X via SDA=%d SCL=%d",
                     byteButtonAddrEffective, byteButtonSdaEffective, byteButtonSclEffective);
    }
    Logger::logf("JoystickManager v2: dualBtnGPIO=(%d,%d)",
                 DUAL_BUTTON_1_GPIO, DUAL_BUTTON_2_GPIO);

    // Permet de démarrer même si des modules sont absents pour faciliter le diagnostic terrain.
    return leftOk || rightOk || byteButtonOk;
}

void JoystickManager::update() {
    // Lit les 2 Unit Joystick : 3 octets (X, Y, bouton) en 8 bits,
    // remis à l'échelle 12 bits. Fallback au centre si module indisponible.
    for (int i = 0; i < 5; i++) {
        buttonPrevState[i] = buttonState[i];
    }

    uint8_t jsData[3];
    if (readUnitJoystickHW(jsData)) {
        axisValues[AXIS_LEFT_X] = (uint16_t)jsData[0] << 4;
        axisValues[AXIS_LEFT_Y] = (uint16_t)jsData[1] << 4;
        buttonState[BTN_LEFT_STICK] = (jsData[2] != stickBtnIdle[0]);
    } else {
        axisValues[AXIS_LEFT_X] = 2048;
        axisValues[AXIS_LEFT_Y] = 2048;
        buttonState[BTN_LEFT_STICK] = false;
    }
    if (byteButtonBus.readBytes(I2C_ADDR_JOYSTICK, jsData, 3)) {
        axisValues[AXIS_RIGHT_X] = (uint16_t)jsData[0] << 4;
        axisValues[AXIS_RIGHT_Y] = (uint16_t)jsData[1] << 4;
        buttonState[BTN_RIGHT_STICK] = (jsData[2] != stickBtnIdle[1]);
    } else {
        axisValues[AXIS_RIGHT_X] = 2048;
        axisValues[AXIS_RIGHT_Y] = 2048;
        buttonState[BTN_RIGHT_STICK] = false;
    }

    // ByteButton : 8 touches de sélection de bouée (bits actifs à LOW)
    byteMaskPrev = byteMaskCurrent;
    byteMaskCurrent = readByteButtonMask();

    // Dual button câblé en GPIO via ExtPort
    buttonState[BTN_LEFT] = (digitalRead(DUAL_BUTTON_1_GPIO) == LOW);
    buttonState[BTN_RIGHT] = (digitalRead(DUAL_BUTTON_2_GPIO) == LOW);

    // Bouton A de l'appareil M5 (Core2)
    M5.update();
    buttonPrevState[BTN_ATOM_SCREEN] = buttonState[BTN_ATOM_SCREEN];
    buttonState[BTN_ATOM_SCREEN] = M5.BtnA.isPressed();

    // Gère le timestamp de pression pour la détection de maintien
    if (buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = millis();
    } else if (!buttonState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = 0;
    }
    
    // Valeurs placeholders tant que l'acquisition batterie Core2 n'est pas branchée.
    batteryVoltage[0] = 0.0f;
    batteryVoltage[1] = 0.0f;
}

uint16_t JoystickManager::getAxisValue(uint8_t axis) {
    if (axis < 4) {
        return axisValues[axis];
    }
    return 2048;  // Valeur centrée par défaut
}

int16_t JoystickManager::getAxisCentered(uint8_t axis) {
    if (axis < 4) {
        // Convertit 0-4095 en -2048 à +2047
        return (int16_t)axisValues[axis] - 2048;
    }
    return 0;
}

bool JoystickManager::isButtonPressed(uint8_t button) {
    if (button < 5) {  // Supporte maintenant 5 boutons
        return buttonState[button];
    }
    return false;
}

bool JoystickManager::wasButtonPressed(uint8_t button) {
    if (button < 5) {  // Supporte maintenant 5 boutons
        // Détecte un front montant (transition false → true)
        return buttonState[button] && !buttonPrevState[button];
    }
    return false;
}

bool JoystickManager::wasButtonReleased(uint8_t button) {
    if (button < 5) {
        // Détecte un front descendant (transition true → false)
        return !buttonState[button] && buttonPrevState[button];
    }
    return false;
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

/**
 * @brief Convertit un numéro de bouée (0-7) en index matériel de touche/LED
 *
 * Le bit 0 du masque du ByteButton correspond à la touche la plus à DROITE.
 * On inverse pour que la bouée #1 soit la touche la plus à gauche et la
 * bouée #8 la plus à droite. La conversion est involutive : la même fonction
 * sert dans les deux sens (touche -> bouée et bouée -> LED).
 */
static inline uint8_t byteButtonSlot(uint8_t index) {
    if (index >= 8) {
        return 0xFF;  // aucune sélection (ledSelectedIndex = 0xFF) : aucune LED allumée
    }
    return 7 - index;
}

void JoystickManager::initByteButtonLeds() {
    if (byteButtonAddrEffective == 0xFF) {
        return;
    }
    uint8_t mode = 0;  // LEDs pilotées par l'hôte (mode "user defined")
    byteButtonBus.writeReg(byteButtonAddrEffective, BYTEBUTTON_REG_LED_MODE, &mode, 1);
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t brightness = BYTEBUTTON_LED_BRIGHTNESS;
        byteButtonBus.writeReg(byteButtonAddrEffective,
                               BYTEBUTTON_REG_LED_BRIGHTNESS + i, &brightness, 1);
        uint32_t color = (i == byteButtonSlot(ledSelectedIndex)) ? BYTEBUTTON_COLOR_SELECTED : 0;
        byteButtonBus.writeReg(byteButtonAddrEffective,
                               BYTEBUTTON_REG_RGB888 + i * 4, (uint8_t*)&color, 4);
    }
}

void JoystickManager::setBuoySelectionLed(uint8_t index) {
    ledSelectedIndex = index;
    if (byteButtonAddrEffective == 0xFF) {
        return;  // sera appliqué à la détection tardive du module
    }
    for (uint8_t i = 0; i < 8; i++) {
        uint32_t color = (i == byteButtonSlot(ledSelectedIndex)) ? BYTEBUTTON_COLOR_SELECTED : 0;
        byteButtonBus.writeReg(byteButtonAddrEffective,
                               BYTEBUTTON_REG_RGB888 + i * 4, (uint8_t*)&color, 4);
    }
}

int8_t JoystickManager::getByteButtonPressedIndex() {
    for (int8_t i = 0; i < 8; i++) {
        bool wasReleased = (byteMaskPrev & (1 << i)) != 0;
        bool isPressed = (byteMaskCurrent & (1 << i)) == 0;
        if (wasReleased && isPressed) {
            return byteButtonSlot(i);  // touche la plus à gauche -> bouée #1
        }
    }
    return -1;
}

uint8_t JoystickManager::getByteButtonMask() {
    return byteMaskCurrent;
}

bool JoystickManager::readUnitJoystickHW(uint8_t* buf) {
    // M5 Unit Joystick : lecture directe de 3 octets (X, Y, bouton),
    // sans pointeur de registre.
    if (i2cBus->requestFrom((int)I2C_ADDR_JOYSTICK, 3) != 3) {
        return false;
    }
    for (uint8_t i = 0; i < 3; i++) {
        buf[i] = i2cBus->read();
    }
    return true;
}

uint8_t JoystickManager::readByteButtonMask() {
    // Plusieurs modules I2C bouton exposent l'état sur le premier registre lisible.
    if (byteButtonAddrEffective == 0xFF) {
        // Le module peut apparaître après le boot : retente au plus toutes les 500 ms.
        uint32_t now = millis();
        if ((int32_t)(now - byteButtonNextProbeMs) < 0) {
            return 0xFF;
        }
        byteButtonNextProbeMs = now + 500;
        byteButtonAddrEffective = detectByteButtonAddr();
        if (byteButtonAddrEffective == 0xFF) {
            return 0xFF;
        }
        Logger::logf("JoystickManager v2: byteButton detecte sur 0x%02X (detection tardive)",
                     byteButtonAddrEffective);
        initByteButtonLeds();
    }

    uint8_t mask;
    if (!byteButtonBus.readReg(byteButtonAddrEffective, 0x00, mask)) {
        return 0xFF;
    }
    return mask;
}

HardwareDiagResult JoystickManager::performDiagnostic() {
    HardwareDiagResult d;
    d.sdaPin  = CORE2_I2C_SDA_PIN;
    d.sclPin  = CORE2_I2C_SCL_PIN;
    d.freqHz  = CORE2_I2C_FREQ_HZ;
    d.byteButtonSdaPin = BYTEBUTTON_I2C_SDA_PIN;
    d.byteButtonSclPin = BYTEBUTTON_I2C_SCL_PIN;
    d.scannedCount = 0;

    // Bus déjà initialisé dans begin(); éviter un second begin()
    // pour supprimer le warning "Bus already started in Master Mode".
    delay(20);  // Laisser le bus se stabiliser

    // --- Scan complet du bus joystick 0x03 → 0x77 ---
    Logger::log("--- Scan I2C joystick 0x03-0x77 ---");
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        i2cBus->beginTransmission(addr);
        uint8_t err = i2cBus->endTransmission();
        if (err == 0) {
            if (d.scannedCount < 16) {
                d.scannedAddrs[d.scannedCount++] = addr;
            }
            Logger::logf("  Trouvé: 0x%02X", addr);
        }
    }
    Logger::logf("  Total joystick bus: %d dispositif(s)", d.scannedCount);

    // --- Scan complet du bus logiciel Port C (ByteButton) ---
    Logger::log("--- Scan I2C Port C 0x03-0x77 ---");
    uint8_t portCCount = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (byteButtonBus.probe(addr)) {
            portCCount++;
            Logger::logf("  Trouvé: 0x%02X", addr);
        }
    }
    Logger::logf("  Total Port C bus: %d dispositif(s)", portCCount);

    // --- Vérifier chaque module contre les résultats du scan ---
    auto foundOnBus = [&](uint8_t addr) -> bool {
        for (uint8_t i = 0; i < d.scannedCount; i++) {
            if (d.scannedAddrs[i] == addr) return true;
        }
        return false;
    };

    // Joystick gauche sur le bus matériel Port A
    d.joystickLeftAddr        = I2C_ADDR_JOYSTICK;
    d.joystickLeftFound       = foundOnBus(I2C_ADDR_JOYSTICK);
    d.joystickLeftAddrScanned = d.joystickLeftFound ? I2C_ADDR_JOYSTICK : 0xFF;

    // Joystick droit sur le bus logiciel Port C (chaîné avec le ByteButton)
    d.joystickRightAddr        = I2C_ADDR_JOYSTICK;
    d.joystickRightFound       = byteButtonBus.probe(I2C_ADDR_JOYSTICK);
    d.joystickRightAddrScanned = d.joystickRightFound ? I2C_ADDR_JOYSTICK : 0xFF;

    // ByteButton (adresse nominale + alternatives) sur son bus logiciel dédié
    d.byteButtonAddr = I2C_ADDR_BYTEBUTTON;
    d.byteButtonAddrScanned = detectByteButtonAddr();
    d.byteButtonFound = (d.byteButtonAddrScanned != 0xFF);
    if (d.byteButtonFound && byteButtonAddrEffective == 0xFF) {
        // Profite du diagnostic pour rattraper une détection manquée au boot.
        byteButtonAddrEffective = d.byteButtonAddrScanned;
    }

    // Dual Button GPIO
    d.dualBtn1Gpio      = DUAL_BUTTON_1_GPIO;
    d.dualBtn2Gpio      = DUAL_BUTTON_2_GPIO;
    d.dualBtn1Reachable = true;  // INPUT_PULLUP configuré dans begin()
    d.dualBtn2Reachable = true;

    // GO/NO-GO : les deux joysticks sont critiques
    d.allCriticalOk = d.joystickLeftFound && d.joystickRightFound;

    Logger::log("--- Diagnostic Hardware ---");
    Logger::logf("  JS Gauche  0x%02X (Port A) : %s", d.joystickLeftAddr,  d.joystickLeftFound  ? "OK" : "KO");
    Logger::logf("  JS Droit   0x%02X (Port C) : %s", d.joystickRightAddr, d.joystickRightFound ? "OK" : "KO");
    Logger::logf("  ByteBtn    0x%02X via SDA=%d SCL=%d : %s",
                 d.byteButtonAddr, d.byteButtonSdaPin, d.byteButtonSclPin,
                 d.byteButtonFound ? "OK" : "KO");
    Logger::logf("  DualBtn    GPIO %d/%d : OK", d.dualBtn1Gpio, d.dualBtn2Gpio);
    Logger::logf("  Statut global  : %s", d.allCriticalOk ? "GO" : "ATTENTION");
    Logger::log("---------------------------");

    return d;
}
