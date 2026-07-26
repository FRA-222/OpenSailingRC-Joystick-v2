/**
 * @file DisplayManager.h
 * @brief LCD display management for the joystick
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module manages the display of information on the M5AtomS3's 128x128 LCD:
 * - Selected buoy
 * - Navigation state
 * - Battery level
 * - Heading and speed
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <M5Unified.h>
#include "BuoyStateManager.h"
#include "CommandManager.h"
#include "HardwareConfig.h"

/**
 * @brief Command status for visual feedback
 */
enum class CommandStatus {
    IDLE,          ///< No command pending
    SENDING,       ///< Command sent, waiting for ACK (Blue)
    ACK_RECEIVED,  ///< ACK received (Green)
    TIMEOUT        ///< No ACK after max retries (Red)
};

/**
 * @brief Class to manage the display
 */
class DisplayManager {
public:
    /**
     * @brief Constructor
     * @param buoyManager Reference to buoy state manager
     */
    DisplayManager(BuoyStateManager& buoyManager);

    /**
     * @brief Initialize the display
     * @return true if initialization succeeds
     */
    bool begin();

    /**
     * @brief Update the display
     * 
     * Should be called regularly in the main loop.
     * Updates are throttled by UPDATE_INTERVAL.
     */
    void update();

    /**
     * @brief Display the main screen
     * 
     * Shows buoy status, navigation mode, heading, speed, battery, GPS, and signal.
     */
    void displayMainScreen();

    /**
     * @brief Display an error message
     * @param message Message to display
     */
    void displayError(const String& message);

    /**
     * @brief Display a connection message
     * @param message Message to display
     */
    void displayConnecting(const String& message);

    /**
     * @brief Display buoy selection screen
     * 
     * Shows "SELECTED: Buoy #X" message briefly when switching buoys.
     */
    void displayBuoySelection();

    /**
     * @brief Display detailed boot hardware diagnostic screen
     * Shows I2C bus config, per-module status (OK/KO + address), GPIO Dual Button,
     * and a final GO/NO-GO banner. Stays visible for durationMs (0 = wait for BtnA).
     * @param diag Result from JoystickManager::performDiagnostic()
     * @param durationMs How long to show the screen (ms). 0 = hold until BtnA pressed.
     */
    void displayBootDiagnostic(const HardwareDiagResult& diag, uint32_t durationMs = 4000);

    /**
     * @brief Force refresh of the display
     * 
     * Clears the cache and forces a complete redraw on next update.
     */
    void forceRefresh();

    /**
     * @brief Enable/disable the display
     * @param enabled true to enable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Set screen brightness
     * @param brightness Brightness level (0-255)
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief Set command status for visual feedback
     * @param status Command status (IDLE, SENDING, ACK_RECEIVED, TIMEOUT)
     * 
     * Updates the header color based on command status:
     * - SENDING: Blue (waiting for ACK)
     * - ACK_RECEIVED: Green (command confirmed)
     * - TIMEOUT: Red (no ACK after retries)
     * - IDLE: Normal color
     */
    void setCommandStatus(CommandStatus status);

private:
    BuoyStateManager& buoyMgr;
    bool displayEnabled;
    uint32_t lastUpdateTime;
    uint8_t currentBrightness;
    
    // Command status for visual feedback (volatile: written from ESP-NOW callback on Core 0)
    volatile CommandStatus commandStatus;
    volatile uint32_t commandStatusTime;  ///< Time when status was last changed
    static const uint32_t STATUS_DISPLAY_DURATION = 3000;  ///< Display status for 3 seconds
    
    // Buoy selection overlay (non-blocking)
    volatile bool showingBuoySelection;    ///< True while selection overlay is displayed
    volatile uint32_t buoySelectionTime;   ///< Time when selection was shown
    static const uint32_t BUOY_SELECTION_DURATION = 500;  ///< Show selection for 500ms
    
    // Cache pour éviter le flickering
    struct DisplayCache {
        uint8_t buoyId = 255;
        bool connected = false;
        bool usingESPNow = false;  ///< Source de données : true=ESP-NOW, false=LoRa
        tEtatsGeneral generalMode = INIT;
        tEtatsNav navigationMode = NAV_STOP;
        bool gpsOk = false;
        bool headingOk = false;
        bool yawRateOk = false;
        uint8_t temperature = 0;
        uint8_t batteryPercent = 0;
        uint8_t distanceToCons = 0;
        float autoPilotTrueHeadingCmde = 0;
        int8_t autoPilotThrottleCmde = 0;
        bool firstUpdate = true;
    } cache;
    
    static const uint32_t UPDATE_INTERVAL = 500;  ///< Update interval in ms
    static const uint8_t DEFAULT_BRIGHTNESS = 128;
    
    /**
     * @brief Draw header with buoy name
     * @param connected Connection status
     * @param usingESPNow true if data comes from ESP-NOW passive listener
     * 
     * Displays buoy name in:
     * - Cyan if connected via ESP-NOW data
     * - Green if connected via LoRa only
     * - Red if disconnected
     * Command status colors (blue/green/red) take priority.
     */
    void drawHeader(bool connected, bool usingESPNow = false);

    /**
     * @brief Draw sensor status LEDs
     * @param state Buoy state
     * 
     * Displays three LED indicators for GPS, MAG (heading), and YAW sensors.
     * Green = OK, Red = KO.
     */
    void drawSensorLEDs(const BuoyState& state);
    
    /**
     * @brief Convertit les couleurs RGB565 pour compenser la permutation de l'écran AtomS3
     * L'écran fait une rotation circulaire: R→G, G→B, B→R
     * @param rgb565 Couleur RGB565 normale
     * @return Couleur RGB565 compensée
     */
    uint16_t swapColorChannels(uint16_t rgb565);

    /**
     * @brief Draw temperature and battery percentage
     * @param state Buoy state
     * 
     * Displays temperature in Celsius and battery level as percentage.
     */
    void drawTempBattery(const BuoyState& state);

    /**
     * @brief Draw navigation state
     * @param state Buoy state
     * 
     * Shows general mode and navigation mode.
     */
    void drawNavigationState(const BuoyState& state);

    /**
     * @brief Draw distance to consigne and throttle
     * @param state Buoy state
     * 
     * Displays distance to waypoint and autopilot throttle command.
     */
    void drawDistanceThrottle(const BuoyState& state);

    /**
     * @brief Draw battery indicator
     * @param batteryLevel Battery level (0-100%)
     * @param x X position
     * @param y Y position
     */
    void drawBattery(uint8_t batteryLevel, int16_t x, int16_t y);

    /**
     * @brief Draw LTE signal indicator
     * @param signalQuality Signal quality (-1 to 31)
     * @param x X position
     * @param y Y position
     */
    void drawSignal(int8_t signalQuality, int16_t x, int16_t y);

    /**
     * @brief Draw GPS indicator
     * @param locked GPS locked status
     * @param x X position
     * @param y Y position
     */
    void drawGPS(bool locked, int16_t x, int16_t y);

    /**
     * @brief Display heading and speed
     * @param heading Heading in degrees
     * @param speed Speed in m/s
     */
    void drawHeadingSpeed(float heading, float speed);

    /**
     * @brief Get color based on battery level
     * @param batteryLevel Battery level (0-100%)
     * @return Color (RGB565)
     */
    uint16_t getBatteryColor(uint8_t batteryLevel);

    /**
     * @brief Get color based on navigation mode
     * @param mode Navigation mode
     * @return Color (RGB565)
     */
    uint16_t getNavModeColor(tEtatsNav mode);

    /**
     * @brief Get color based on general mode
     * @param mode General mode
     * @return Color (RGB565)
     */
    uint16_t getGeneralModeColor(tEtatsGeneral mode);
};

#endif // DISPLAY_MANAGER_H
