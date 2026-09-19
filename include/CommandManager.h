/**
 * @file CommandManager.h
 * @brief Command management for sending to buoys
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module translates joystick inputs into commands for autonomous buoys.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <Arduino.h>
#include "BuoyEnums.h"

// Forward declaration
class ICommunication;

/**
 * @brief Command types sent to buoys
 * IMPORTANT: uint8_t to ensure 1 byte size
 */
enum BuoyCommand : uint8_t {
    CMD_INIT_HOME = 0,        ///< Initialize Home with current GPS position
    CMD_THROTTLE_INCREASE,    ///< Increase throttle (buoy decides increment)
    CMD_THROTTLE_DECREASE,    ///< Decrease throttle (buoy decides increment)
    CMD_HEADING_INCREASE,     ///< Increase heading (buoy decides increment)
    CMD_HEADING_DECREASE,     ///< Decrease heading (buoy decides increment)
    CMD_NAV_HOLD,             ///< Hold current position
    CMD_NAV_CAP,              ///< Navigate by heading (cap mode)
    CMD_NAV_HOME,             ///< Navigate to Home position
    CMD_NAV_STOP,             ///< Stop all movements
    CMD_HOME_VALIDATION,      ///< Validate Home position
    CMD_HEARTBEAT,            ///< Heartbeat to keep connection alive
    CMD_MAINTENANCE_ENTER,    ///< Enter MAINTENANCE mode (maps to dashboard code "4")
    CMD_MAINTENANCE_EXIT,     ///< Exit MAINTENANCE mode, return to READY (maps to dashboard code "5")

    // ── Passerelle / dashboard (GATEWAY_DESIGN.md §3.2, LORA_PROTOCOL.md §5.2) ──
    // Allocation partagée bouée / joystick / passerelle : ne jamais réordonner.
    CMD_POLL = 13,                    ///< Demande d'état + battement de cœur (no-op navigation)
    CMD_OBSERVABLE = 14,              ///< Demande de trame OBSERVABLE
    CMD_SET_HOME = 15,                ///< Home à la position transmise (trame COMMAND_POS)
    CMD_SET_TARGET = 16,              ///< Cible à la position transmise (trame COMMAND_POS)
    CMD_SET_DEGT = 17,                ///< Point de dégagement à la position transmise (trame COMMAND_POS)
    CMD_OBSERVABLE2 = 18,             ///< Demande de trame OBSERVABLE2 (gains PID, moteurs)
    CMD_OBSERVABLE_GPS = 19,          ///< Demande de trame OBSERVABLE_GPS (dérive GPS)
    CMD_SERVICE_LIFE_INCREASE = 20,   ///< Durée de service +½ h (code dashboard "L")
    CMD_SERVICE_LIFE_DECREASE = 21,   ///< Durée de service −½ h (code "M")
    CMD_CURRENT_BUOY_INCREASE = 22,   ///< Identité préparée +1 (code "1") — jamais en diffusion
    CMD_CURRENT_BUOY_DECREASE = 23,   ///< Identité préparée −1 (code "2") — jamais en diffusion
    CMD_CURRENT_BUOY_VALIDATION = 24, ///< Valider l'identité préparée (code "3") — jamais en diffusion
    CMD_BATTERY_LIPO = 25,            ///< Batterie LiPo (code "I")
    CMD_BATTERY_LIFE = 26,            ///< Batterie LiFe (code "J")
    CMD_CALIBRATION = 27,             ///< Calibration magnétomètre (code "F") — jamais en diffusion
    CMD_NAV_BASIC = 28,               ///< Navigation basique (code "04Z")
    CMD_NAV_TARGET = 29,              ///< Navigation vers la cible (code "06Z")
    CMD_TEST_GPS_KO_ON = 30,          ///< Essai : GPS KO (code "71Z")
    CMD_TEST_GPS_KO_OFF = 31,         ///< (code "70Z")
    CMD_TEST_DL_TOTAL_KO_ON = 32,     ///< Essai : datalink total KO (code "D1Z")
    CMD_TEST_DL_TOTAL_KO_OFF = 33,    ///< (code "D0Z")
    CMD_TEST_DL_PARTIAL_KO_ON = 34,   ///< Essai : datalink partiel KO (code "G1Z")
    CMD_TEST_DL_PARTIAL_KO_OFF = 35,  ///< (code "G0Z")
    CMD_TEST_YAWRATE_KO_ON = 36,      ///< Essai : lacet KO (code "91Z")
    CMD_TEST_YAWRATE_KO_OFF = 37,     ///< (code "90Z")
    CMD_TEST_MAG_KO_ON = 38,          ///< Essai : magnétomètre KO (code "81Z")
    CMD_TEST_MAG_KO_OFF = 39,         ///< (code "80Z")
    CMD_TEST_AUTOPILOT_ON = 40,       ///< Essai : pilote automatique (code "H1Z")
    CMD_TEST_AUTOPILOT_OFF = 41,      ///< (code "H0Z")
    CMD_MONITORING_HEADING_ERROR_ON = 42,  ///< Surveillance de l'écart de cap (code "K1Z")
    CMD_MONITORING_HEADING_ERROR_OFF = 43  ///< (code "K0Z")
};

/**
 * @brief Complete command structure
 */
struct Command {
    uint8_t targetBuoyId;   ///< Target buoy ID
    BuoyCommand type;       ///< Command type
    uint32_t timestamp;     ///< Command timestamp
};

/**
 * @brief Class to manage buoy commands
 */
class CommandManager {
public:
    /**
     * @brief Constructor
     * @param comm Reference to ICommunication instance
     */
    CommandManager(ICommunication& comm);

    /**
     * @brief Update commands based on joystick inputs
     * @param leftX Left X axis (-2048 to +2047)
     * @param leftY Left Y axis (-2048 to +2047)
     * @param rightX Right X axis (-2048 to +2047)
     * @param rightY Right Y axis (-2048 to +2047)
     * @param btnLeft Left button pressed
     * @param btnRight Right button pressed
     * @param btnLeftStick Left stick button pressed
     * @param btnRightStick Right stick button pressed
     */
    void update(int16_t leftX, int16_t leftY, int16_t rightX, int16_t rightY,
                bool btnLeft, bool btnRight, bool btnLeftStick, bool btnRightStick);

    /**
     * @brief Get the last generated command
     * @return Command structure
     */
    Command getCommand();

    /**
     * @brief Check if a new command is available
     * @return true if command is available
     */
    bool hasNewCommand();

    /**
     * @brief Reset the new command flag
     */
    void clearNewCommand();

    /**
     * @brief Get the current calculated heading
     * @return Heading in degrees (-180 to +180)
     */
    int16_t getCurrentHeading();

    /**
     * @brief Get the current throttle
     * @return Throttle (-100 to +100%)
     */
    int8_t getCurrentThrottle();

    /**
     * @brief Get the current navigation mode
     * @return Navigation mode
     */
    tEtatsNav getCurrentMode();

    /**
     * @brief Generate and send INIT_HOME command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateInitHomeCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send HOME_VALIDATION command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateHomeValidationCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send NAV_CAP command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateNavCapCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send NAV_HOME command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateNavHomeCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send NAV_HOLD command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateNavHoldCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send NAV_STOP command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateNavStopCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send THROTTLE_INCREASE command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateThrottleIncreaseCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send THROTTLE_DECREASE command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateThrottleDecreaseCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send HEADING_INCREASE command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateHeadingIncreaseCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send HEADING_DECREASE command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateHeadingDecreaseCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send MAINTENANCE_ENTER command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateMaintenanceEnterCommand(uint8_t targetBuoyId);

    /**
     * @brief Generate and send MAINTENANCE_EXIT command to a buoy
     * @param targetBuoyId ID of the buoy to send the command to
     * @return true if command was sent successfully
     */
    bool generateMaintenanceExitCommand(uint8_t targetBuoyId);

    /**
     * @brief Send heartbeat to all active buoys
     * Should be called periodically (every 5 seconds recommended)
     * @return Number of heartbeats sent successfully
     */
    uint8_t sendHeartbeatToAllBuoys();
    
    /**
     * @brief Generate and send HEARTBEAT command to a specific buoy
     * @param targetBuoyId ID of the buoy to send the heartbeat to
     * @return true if command was sent successfully
     */
    bool generateHeartbeatCommand(uint8_t targetBuoyId);

    /**
     * @brief Demande la trame OBSERVABLE (température, batterie, route, vitesse…) à une bouée
     *
     * Protocole LoRa v2 uniquement : la bouée répond par un ObservablePacketLora
     * (11 o) au lieu du BUOY_STATUS. Sans effet sur la navigation, jamais acquittée.
     * @param targetBuoyId ID of the buoy to query
     * @return true if command was sent successfully
     */
    bool generateObservableCommand(uint8_t targetBuoyId);

private:
    ICommunication& comm;  ///< Reference to communication interface
    
    Command currentCommand;
    bool newCommandAvailable;
    
    int16_t currentHeading;      ///< Current heading
    int8_t currentThrottle;      ///< Current throttle
    tEtatsNav currentMode;  ///< Current mode
    
    uint32_t lastHeadingChange;  ///< Last heading change timestamp
    uint32_t lastModeChange;     ///< Last mode change timestamp
    
    static const int16_t HEADING_INCREMENT = 10;  ///< Heading increment in degrees
    static const uint32_t HEADING_DELAY = 200;    ///< Delay between heading changes (ms)
    static const int16_t STICK_DEADZONE = 300;    ///< Joystick deadzone
    
    /**
     * @brief Apply deadzone to joystick value
     * @param value Joystick value
     * @return Value with deadzone applied
     */
    int16_t applyDeadzone(int16_t value);

    /**
     * @brief Generate heading change command
     * @param rightX Right X axis of joystick
     */
    void updateHeading(int16_t rightX);

    /**
     * @brief Generate throttle command
     * @param leftY Left Y axis of joystick
     */
    void updateThrottle(int16_t leftY);
};

#endif // COMMAND_MANAGER_H
