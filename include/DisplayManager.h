/**
 * @file DisplayManager.h
 * @brief LCD display management for the joystick
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module manages the display of information on the M5Stack Core2's 320x240 LCD:
 * - Selected buoy
 * - Navigation state
 * - Battery level
 * - Heading and speed
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
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
     * @param firmwareVersion Firmware version string displayed on startup screen
     */
    DisplayManager(BuoyStateManager& buoyManager, const char* firmwareVersion = "2.0.0");

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
    String firmwareVersion;
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
    
    /**
     * @brief Cached state of one text field of the main screen
     *
     * A field is redrawn only when its rendered text or its color changes, which
     * is what actually removes the flickering: comparing raw sensor values isn't
     * enough since several of them jitter below the displayed resolution.
     */
    struct TextField {
        char text[16] = "";
        uint16_t color = 0;
        bool valid = false;    ///< false = nothing drawn yet, next draw is unconditional
    };

    // Cache pour éviter le flickering
    struct DisplayCache {
        uint8_t buoyId = 255;
        bool connected = false;
        bool usingESPNow = false;  ///< Source de données : true=ESP-NOW, false=LoRa
        bool firstUpdate = true;

        // LEDs capteurs : état dessiné (-1 = jamais dessiné)
        int8_t gpsOk = -1;
        int8_t headingOk = -1;
        int8_t yawRateOk = -1;

        // Niveau dessiné dans l'icône batterie (-1 = jamais dessinée)
        int16_t batteryIcon = -1;

        // Champs texte
        TextField joystickBattery;
        TextField buoyName;
        TextField sourceTag;
        TextField temperature;
        TextField battery;
        TextField generalMode;
        TextField navMode;
        TextField distance;
        TextField heading;
        TextField throttle;
    } cache;

    /**
     * @brief Draw a text field only if its content or color changed
     *
     * Uses an opaque text background plus setTextPadding() so the new string
     * overwrites the previous one in a single pass: no fillRect/redraw sequence,
     * hence no visible blink.
     *
     * @param field    Cache entry for this field
     * @param text     Text to display
     * @param color    Foreground color
     * @param font     Font to use
     * @param textSize Text magnification (fractional values allowed, e.g. 1.6)
     * @param datum    Text datum (alignment) for x/y
     * @param x        X anchor
     * @param y        Y anchor
     * @param padWidth Width erased around the text (must cover the widest value)
     * @param force    true to redraw even if unchanged (after a screen clear)
     * @return true if the field was actually repainted
     */
    bool drawTextField(TextField& field, const char* text, uint16_t color,
                       const m5gfx::IFont* font, float textSize, m5gfx::textdatum_t datum,
                       int16_t x, int16_t y, uint16_t padWidth, bool force = false);

    /**
     * @brief Invalidate every cached field so the next draw repaints everything
     *
     * Must be called after any fillScreen()/fillRect() that wipes drawn content.
     */
    void invalidateCachedFields();

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
     * @brief Draw the static parts of the main screen layout
     *
     * Separators, sensor labels (GPS/MAG/YAW) and command labels (DIST/CAP/THR)
     * never change: they are painted once per full redraw so the periodic update
     * only touches the values themselves.
     */
    void drawStaticLayout();

    /**
     * @brief Draw sensor status LEDs
     * @param state Buoy state
     * 
     * Displays three LED indicators for GPS, MAG (heading), and YAW sensors.
     * Green = OK, Red = KO.
     */
    void drawSensorLEDs(const BuoyState& state, bool force);

    /**
     * @brief Draw the joystick's own battery level in the header (top left)
     *
     * Read from M5.Power (Core2 internal battery), not from the buoy telemetry.
     * Prefixed with "JS" to distinguish it from the buoy battery shown in the
     * temperature band.
     *
     * @param force true to redraw even if the value did not change
     */
    void drawJoystickBattery(bool force);
    
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
    void drawTempBattery(const BuoyState& state, bool force);

    /**
     * @brief Draw navigation state
     * @param state Buoy state
     * 
     * Shows general mode and navigation mode.
     */
    void drawNavigationState(const BuoyState& state, bool force);

    /**
     * @brief Draw distance to consigne and throttle
     * @param state Buoy state
     * 
     * Displays distance to waypoint and autopilot throttle command.
     */
    void drawDistanceThrottle(const BuoyState& state, bool force);

    /**
     * @brief Draw battery icon (30x15 px, no text)
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
