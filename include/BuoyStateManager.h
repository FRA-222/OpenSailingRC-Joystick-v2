/**
 * @file BuoyStateManager.h
 * @brief State management for all buoys and active buoy selection
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module manages the state of all connected buoys and the selection
 * of the currently controlled buoy.
 */

#ifndef BUOY_STATE_MANAGER_H
#define BUOY_STATE_MANAGER_H

#include <Arduino.h>
#include "ICommunication.h"

// Forward declaration
class DisplayManager;

/**
 * @brief Class to manage buoy states
 */
class BuoyStateManager {
public:
    /**
     * @brief Constructor
     * @param comm Reference to ICommunication instance
     */
    BuoyStateManager(ICommunication& comm);

    /**
     * @brief Initialize the manager
     */
    void begin();

    /**
     * @brief Update the state of all buoys
     * 
     * Should be called regularly in the main loop to check for new data
     * from buoys and update connection status.
     */
    void update();

    /**
     * @brief Set the display manager
     * @param display Pointer to DisplayManager instance
     */
    void setDisplayManager(DisplayManager* display);

    /**
     * @brief Select the next buoy
     * 
     * Cycles to the next buoy in the list (wraps around to first buoy)
     */
    void selectNextBuoy();

    /**
     * @brief Select the previous buoy
     * 
     * Cycles to the previous buoy in the list (wraps around to last buoy)
     */
    void selectPreviousBuoy();

    /**
     * @brief Select a specific buoy
     * @param buoyId Buoy ID to select (0-5)
     */
    void selectBuoy(uint8_t buoyId);

    /**
     * @brief Get the ID of the currently selected buoy
     * @return Buoy ID (0-5)
     */
    uint8_t getSelectedBuoyId();

    /**
     * @brief Get the state of the selected buoy
     * @return BuoyState structure with current data
     */
    BuoyState getSelectedBuoyState();

    /**
     * @brief Get the number of connected buoys
     * @return Number of buoys with recent data
     */
    uint8_t getConnectedBuoyCount();

    /**
     * @brief Check if the selected buoy is connected
     * @return true if connected (received data recently)
     */
    bool isSelectedBuoyConnected();

    /**
     * @brief Get the name of a buoy
     * @param buoyId Buoy ID (0-5)
     * @return Buoy name (e.g., "Buoy #1")
     */
    String getBuoyName(uint8_t buoyId);

    /**
     * @brief Get the name of a navigation mode
     * @param mode Navigation mode
     * @return Mode name (e.g., "HDG", "HOLD", etc.)
     */
    String getNavModeName(tEtatsNav mode);

    /**
     * @brief Get the name of a general mode
     * @param mode General mode
     * @return Mode name (e.g., "INIT", "READY", "NAV", etc.)
     */
    String getGeneralModeName(tEtatsGeneral mode);

    /**
     * @brief Check if new communication data is available
     * @return true if new data has been received (from primary or ESP-NOW listener)
     */
    bool hasNewData();

    /**
     * @brief Clear the new data flag (on both primary and ESP-NOW listener)
     */
    void clearNewData();

    /**
     * @brief Set optional ESP-NOW listener for passive data reception in LoRa mode
     * When set, BuoyStateManager will also use ESP-NOW data to provide
     * the freshest available state for each buoy.
     * @param listener Pointer to ESPNowCommunication instance (or nullptr to disable)
     */
    void setESPNowListener(ICommunication* listener);

    /**
     * @brief Check if the display is currently using ESP-NOW data for the selected buoy
     * @return true if ESP-NOW is the active data source for the selected buoy
     */
    bool isUsingESPNowData();

private:
    /**
     * @brief Check if ESP-NOW is actively receiving data for a buoy
     * "Active" means data received within the last ESPNOW_ACTIVE_TIMEOUT_MS.
     * @param buoyId Buoy ID to check
     * @return true if ESP-NOW data is recent enough to be considered active
     */
    bool isESPNowActiveForBuoy(uint8_t buoyId);

    ICommunication& comm;
    ICommunication* espNowListener;  ///< Optional ESP-NOW passive listener (used in LoRa mode)
    DisplayManager* displayMgr;
    uint8_t selectedBuoyId;
    uint8_t lastConnectedBuoyId;
    uint32_t lastUpdateTime;
    
    static const uint32_t UPDATE_INTERVAL = 100;  ///< Update interval in ms
    static const uint32_t ESPNOW_ACTIVE_TIMEOUT_MS = 10000;  ///< ESP-NOW data valid for 10s
};

#endif // BUOY_STATE_MANAGER_H
