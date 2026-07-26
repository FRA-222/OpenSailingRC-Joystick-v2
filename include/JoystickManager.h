/**
 * @file JoystickManager.h
 * @brief Joystick and button reading management via I2C
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module handles I2C communication with the STM32 to read values
 * from 2 analog joysticks and 4 buttons.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef JOYSTICK_MANAGER_H
#define JOYSTICK_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "HardwareConfig.h"
#include "Logger.h"

// Axis indices
#define AXIS_LEFT_X  0
#define AXIS_LEFT_Y  1
#define AXIS_RIGHT_X 2
#define AXIS_RIGHT_Y 3

// Button indices
#define BTN_LEFT_STICK  0
#define BTN_RIGHT_STICK 1
#define BTN_LEFT        2
#define BTN_RIGHT       3
#define BTN_ATOM_SCREEN 4  ///< Atom S3 built-in screen button

/**
 * @brief Class to manage joysticks and buttons
 */
class JoystickManager {
public:
    /**
     * @brief Constructor
     */
    JoystickManager();

    /**
     * @brief Initialize I2C communication
     * @return true if initialization succeeds
     */
    bool begin();

    /**
     * @brief Update joystick and button values
     * 
     * Reads all axis values and button states from the STM32 via I2C.
     * Should be called regularly in the main loop.
     */
    void update();

    /**
     * @brief Get the raw value of a joystick axis
     * @param axis Axis index (AXIS_LEFT_X, AXIS_LEFT_Y, etc.)
     * @return 12-bit value (0-4095)
     */
    uint16_t getAxisValue(uint8_t axis);

    /**
     * @brief Get the centered value of an axis (-2048 to +2047)
     * @param axis Axis index
     * @return Signed value centered on 0
     */
    int16_t getAxisCentered(uint8_t axis);

    /**
     * @brief Check if a button is currently pressed
     * @param button Button index (BTN_LEFT_STICK, BTN_RIGHT_STICK, BTN_LEFT, BTN_RIGHT)
     * @return true if pressed
     */
    bool isButtonPressed(uint8_t button);

    /**
     * @brief Check if a button was just pressed (rising edge detection)
     * @param button Button index
     * @return true if just pressed (transition from released to pressed)
     */
    bool wasButtonPressed(uint8_t button);

    /**
     * @brief Check if a button was just released (falling edge detection)
     * @param button Button index
     * @return true if just released
     */
    bool wasButtonReleased(uint8_t button);

    /**
     * @brief Check if Atom S3 screen button is pressed
     * @return true if the screen button is currently pressed
     */
    bool isAtomScreenPressed();

    /**
     * @brief Check if Atom S3 screen button was just pressed
     * @return true if the screen button was just pressed
     */
    bool wasAtomScreenPressed();

    /**
     * @brief Check if Atom S3 screen button was just released
     * @return true if the screen button was just released
     */
    bool wasAtomScreenReleased();

    /**
     * @brief Check if Atom S3 screen button is held for a duration
     * @param durationMs Duration in milliseconds
     * @return true if button held for specified duration
     */
    bool isAtomScreenHeld(uint32_t durationMs = 1000);

    /**
     * @brief Get battery 1 voltage
     * @return Voltage in volts
     */
    float getBattery1Voltage();

    /**
     * @brief Get battery 2 voltage
     * @return Voltage in volts
     */
    float getBattery2Voltage();

    /**
     * @brief Get the index of a ByteButton key newly pressed this update
     * @return 0-7 if a key was just pressed (rising edge), -1 otherwise
     */
    int8_t getByteButtonPressedIndex();

    /**
     * @brief Get the raw ByteButton mask (bits active LOW, 0xFF = all released)
     */
    uint8_t getByteButtonMask();

    /**
     * @brief Light the ByteButton LED of the selected buoy (green), others off
     * @param index Buoy/LED index 0-7 (0xFF = all off)
     * The choice is remembered and re-applied if the ByteButton appears later.
     */
    void setBuoySelectionLed(uint8_t index);

    /**
     * @brief Run full hardware diagnostic (I2C scan + GPIO check)
     * Can be called after begin() to get a detailed HardwareDiagResult.
     * @return HardwareDiagResult structure with per-module status
     */
    HardwareDiagResult performDiagnostic();

private:
    uint16_t axisValues[4];          ///< Raw axis values
    bool buttonState[5];              ///< Current button states (including Atom screen)
    bool buttonPrevState[5];          ///< Previous button states (including Atom screen)
    float batteryVoltage[2];          ///< Battery voltages
    uint32_t atomScreenPressTime;     ///< Timestamp when Atom screen was pressed
    uint8_t byteButtonAddrEffective;  ///< Detected ByteButton I2C address (0xFF if absent)
    uint8_t byteButtonSdaEffective;   ///< Detected ByteButton bus SDA pin
    uint8_t byteButtonSclEffective;   ///< Detected ByteButton bus SCL pin
    uint32_t byteButtonNextProbeMs;   ///< Next allowed lazy re-detection timestamp
    uint8_t byteMaskCurrent;          ///< Current ByteButton mask (active LOW)
    uint8_t byteMaskPrev;             ///< Previous ByteButton mask
    uint8_t stickBtnIdle[2];          ///< Idle button byte of each Unit Joystick (L, R)
    uint8_t ledSelectedIndex;         ///< LED currently lit for buoy selection (0xFF = none)
    TwoWire* i2cBus;

    /**
     * @brief Put ByteButton LEDs in host-controlled mode and apply selection
     */
    void initByteButtonLeds();

    /**
     * @brief Read the left M5 Unit Joystick on the hardware bus (Port A)
     * @param buf Output buffer of 3 bytes: X, Y, button
     * @return true if the read succeeded
     */
    bool readUnitJoystickHW(uint8_t* buf);

    /**
     * @brief Read ByteButton state mask
     * @return 8-bit state mask
     */
    uint8_t readByteButtonMask();
};

#endif // JOYSTICK_MANAGER_H
