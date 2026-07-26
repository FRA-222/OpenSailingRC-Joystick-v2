/**
 * @file CommandManager.h
 * @brief Command management for sending to buoys
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module translates joystick inputs into commands for autonomous buoys.
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
    CMD_HEARTBEAT             ///< Heartbeat to keep connection alive
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
