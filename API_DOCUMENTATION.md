# 📚 OpenSailingRC BuoyJoystick - API Documentation

## Overview

This document provides comprehensive API documentation for the OpenSailingRC BuoyJoystick project. The system is designed to control autonomous GPS buoys using a dual joystick controller with an LCD display, communicating via ESP-NOW protocol.

---

## Table of Contents

1. [JoystickManager](#joystickmanager)
2. [ESPNowCommunication](#espnowcommunication)
3. [BuoyStateManager](#buoystatemanager)
4. [DisplayManager](#displaymanager)
5. [CommandManager](#commandmanager)
6. [Data Structures](#data-structures)
7. [Constants & Enums](#constants--enums)

---

## JoystickManager

**File:** `include/JoystickManager.h`, `src/JoystickManager.cpp`

**Purpose:** Manages I2C communication with the STM32F030F4P6 coprocessor to read dual joystick axes and button states.

### Public Methods

#### `JoystickManager()`
Constructor. Initializes internal state arrays.

#### `bool begin()`
Initialize I2C communication.
- **Returns:** `true` if initialization succeeds
- **I2C Settings:** 400kHz, SDA=GPIO38, SCL=GPIO39, Address=0x59

#### `void update()`
Update all joystick and button values.
- **Description:** Reads all axis values and button states from the STM32 via I2C. Should be called regularly in the main loop (typically 10Hz or faster).
- **Reads:** 4 axes (12-bit each), 4 buttons (1-bit each), 2 battery voltages

#### `uint16_t getAxisValue(uint8_t axis)`
Get the raw value of a joystick axis.
- **Parameters:**
  - `axis`: Axis index (`AXIS_LEFT_X`, `AXIS_LEFT_Y`, `AXIS_RIGHT_X`, `AXIS_RIGHT_Y`)
- **Returns:** 12-bit value (0-4095), centered around 2048

#### `int16_t getAxisCentered(uint8_t axis)`
Get the centered value of an axis.
- **Parameters:**
  - `axis`: Axis index
- **Returns:** Signed value (-2048 to +2047), centered on 0
- **Note:** Returns `rawValue - 2048`

#### `bool isButtonPressed(uint8_t button)`
Check if a button is currently pressed.
- **Parameters:**
  - `button`: Button index (`BTN_LEFT_STICK`, `BTN_RIGHT_STICK`, `BTN_LEFT`, `BTN_RIGHT`)
- **Returns:** `true` if currently pressed

#### `bool wasButtonPressed(uint8_t button)`
Check if a button was just pressed (rising edge detection).
- **Parameters:**
  - `button`: Button index
- **Returns:** `true` if just pressed (transition from released to pressed)
- **Note:** Use this for one-shot button actions

#### `float getBattery1Voltage()`
Get battery 1 voltage.
- **Returns:** Voltage in volts (typically 3.0-4.2V for LiPo)

#### `float getBattery2Voltage()`
Get battery 2 voltage.
- **Returns:** Voltage in volts

### Hardware Mapping

| Hardware Button | Constant | Index | I2C Register |
|----------------|----------|-------|--------------|
| Yellow L (top left) | `BTN_LEFT_STICK` | 0 | 0x70 |
| Yellow R (top right) | `BTN_RIGHT_STICK` | 1 | 0x71 |
| Left joystick press | `BTN_LEFT` | 2 | 0x72 |
| Right joystick press | `BTN_RIGHT` | 3 | 0x73 |

| Joystick Axis | Constant | Index | I2C Register |
|--------------|----------|-------|--------------|
| Left X | `AXIS_LEFT_X` | 0 | 0x00 |
| Left Y | `AXIS_LEFT_Y` | 1 | 0x02 |
| Right X | `AXIS_RIGHT_X` | 2 | 0x20 |
| Right Y | `AXIS_RIGHT_Y` | 3 | 0x22 |

### Example Usage

```cpp
JoystickManager joystick;

void setup() {
    joystick.begin();
}

void loop() {
    joystick.update();
    
    // Read centered axis values
    int16_t leftX = joystick.getAxisCentered(AXIS_LEFT_X);
    int16_t leftY = joystick.getAxisCentered(AXIS_LEFT_Y);
    
    // Check button press
    if (joystick.wasButtonPressed(BTN_LEFT_STICK)) {
        Serial.println("Yellow L button pressed!");
    }
    
    // Read battery
    float bat1 = joystick.getBattery1Voltage();
    Serial.printf("Battery 1: %.2fV\n", bat1);
    
    delay(100);
}
```

---

## ESPNowCommunication

**File:** `include/ESPNowCommunication.h`, `src/ESPNowCommunication.cpp`

**Purpose:** Manages bidirectional ESP-NOW communication with up to 6 autonomous GPS buoys.

### Public Methods

#### `ESPNowCommunication()`
Constructor. Initializes peer list and flags.

#### `bool begin()`
Initialize ESP-NOW and WiFi.
- **Returns:** `true` if initialization succeeds
- **Note:** Sets WiFi to STA mode, initializes ESP-NOW callbacks

#### `bool addBuoy(uint8_t buoyId, const uint8_t* macAddress)`
Add a buoy to the peer list.
- **Parameters:**
  - `buoyId`: Buoy ID (0-5)
  - `macAddress`: 6-byte MAC address array
- **Returns:** `true` if addition succeeds
- **Note:** Automatically registers the peer with ESP-NOW

#### `bool sendCommand(uint8_t buoyId, const Command& cmd)`
Send a command to a specific buoy.
- **Parameters:**
  - `buoyId`: Target buoy ID (0-5)
  - `cmd`: Command structure to send
- **Returns:** `true` if send succeeds
- **Note:** Creates a `CommandPacket` and sends via ESP-NOW

#### `BuoyState getLastBuoyState()`
Get the state of the last buoy that sent data.
- **Returns:** `BuoyState` structure with most recent data

#### `BuoyState getBuoyState(uint8_t buoyId)`
Get the state of a specific buoy.
- **Parameters:**
  - `buoyId`: Buoy ID (0-5)
- **Returns:** `BuoyState` structure for specified buoy

#### `bool hasNewData()`
Check if new data has been received.
- **Returns:** `true` if new data is available since last `clearNewData()`

#### `void clearNewData()`
Reset the new data flag.

#### `uint8_t getBuoyCount()`
Get the number of registered buoys.
- **Returns:** Number of buoys (0-6)

#### `bool isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs = 5000)`
Check if a buoy is connected (has sent recent data).
- **Parameters:**
  - `buoyId`: Buoy ID (0-5)
  - `timeoutMs`: Timeout in milliseconds (default: 5000)
- **Returns:** `true` if the buoy has sent data within the timeout period

#### `const uint8_t* getLocalMacAddress()`
Get local MAC address.
- **Returns:** Pointer to 6-byte array containing local MAC address

### Example Usage

```cpp
ESPNowCommunication espNow;

void setup() {
    espNow.begin();
    
    // Add buoys
    const uint8_t buoy1Mac[] = {0x48, 0xE7, 0x29, 0x9E, 0x2B, 0xAC};
    espNow.addBuoy(0, buoy1Mac);
    
    // Print local MAC
    const uint8_t* mac = espNow.getLocalMacAddress();
    Serial.printf("Local MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void loop() {
    // Check for new data
    if (espNow.hasNewData()) {
        BuoyState state = espNow.getLastBuoyState();
        Serial.printf("Buoy %d: Lat=%.6f, Lon=%.6f, Hdg=%.1f\n",
                      state.buoyId, state.latitude, state.longitude, state.heading);
        espNow.clearNewData();
    }
    
    // Check connection status
    if (espNow.isBuoyConnected(0)) {
        Serial.println("Buoy 0 is connected");
    }
    
    delay(100);
}
```

---

## BuoyStateManager

**File:** `include/BuoyStateManager.h`, `src/BuoyStateManager.cpp`

**Purpose:** Manages the state of all buoys and handles active buoy selection.

### Public Methods

#### `BuoyStateManager(ESPNowCommunication& espNow)`
Constructor.
- **Parameters:**
  - `espNow`: Reference to ESP-NOW communication instance

#### `void begin()`
Initialize the manager.

#### `void update()`
Update the state of all buoys.
- **Description:** Should be called regularly in the main loop to check for new data from buoys and update connection status.
- **Update Rate:** Throttled to 100ms intervals

#### `void selectNextBuoy()`
Select the next buoy.
- **Description:** Cycles to the next buoy in the list (wraps around to first buoy)

#### `void selectPreviousBuoy()`
Select the previous buoy.
- **Description:** Cycles to the previous buoy in the list (wraps around to last buoy)

#### `void selectBuoy(uint8_t buoyId)`
Select a specific buoy.
- **Parameters:**
  - `buoyId`: Buoy ID to select (0-5)

#### `uint8_t getSelectedBuoyId()`
Get the ID of the currently selected buoy.
- **Returns:** Buoy ID (0-5)

#### `BuoyState getSelectedBuoyState()`
Get the state of the selected buoy.
- **Returns:** `BuoyState` structure with current data

#### `uint8_t getConnectedBuoyCount()`
Get the number of connected buoys.
- **Returns:** Number of buoys with recent data

#### `bool isSelectedBuoyConnected()`
Check if the selected buoy is connected.
- **Returns:** `true` if connected (received data recently)

#### `String getBuoyName(uint8_t buoyId)`
Get the name of a buoy.
- **Parameters:**
  - `buoyId`: Buoy ID (0-5)
- **Returns:** Buoy name (e.g., "Buoy #1")

#### `String getModeName(NavigationMode mode)`
Get the name of a navigation mode.
- **Parameters:**
  - `mode`: Navigation mode enum value
- **Returns:** Mode name (e.g., "HDG", "HOLD", "HOME", "STOP")

### Example Usage

```cpp
ESPNowCommunication espNow;
BuoyStateManager buoyState(espNow);

void setup() {
    espNow.begin();
    buoyState.begin();
}

void loop() {
    buoyState.update();
    
    // Get selected buoy info
    uint8_t id = buoyState.getSelectedBuoyId();
    String name = buoyState.getBuoyName(id);
    BuoyState state = buoyState.getSelectedBuoyState();
    
    Serial.printf("%s: Heading=%.1f°, Speed=%.2fm/s\n",
                  name.c_str(), state.heading, state.speed);
    
    // Switch buoys with buttons
    if (buttonPressed) {
        buoyState.selectNextBuoy();
    }
    
    delay(100);
}
```

---

## DisplayManager

**File:** `include/DisplayManager.h`, `src/DisplayManager.cpp`

**Purpose:** Manages the 128x128 LCD display to show buoy status, navigation data, and system information.

### Public Methods

#### `DisplayManager(BuoyStateManager& buoyManager)`
Constructor.
- **Parameters:**
  - `buoyManager`: Reference to buoy state manager

#### `bool begin()`
Initialize the display.
- **Returns:** `true` if initialization succeeds
- **Note:** Sets up M5Unified display, configures fonts and colors

#### `void update()`
Update the display.
- **Description:** Should be called regularly in the main loop. Updates are throttled by `UPDATE_INTERVAL` (500ms).

#### `void displayMainScreen()`
Display the main screen.
- **Description:** Shows buoy status, navigation mode, heading, speed, battery, GPS, and signal indicators.

#### `void displayError(const String& message)`
Display an error message.
- **Parameters:**
  - `message`: Error message to display

#### `void displayConnecting(const String& message)`
Display a connection message.
- **Parameters:**
  - `message`: Connection message to display

#### `void displayBuoySelection()`
Display buoy selection screen.
- **Description:** Shows "SELECTED: Buoy #X" message briefly when switching buoys.

#### `void setEnabled(bool enabled)`
Enable/disable the display.
- **Parameters:**
  - `enabled`: `true` to enable

#### `void setBrightness(uint8_t brightness)`
Set screen brightness.
- **Parameters:**
  - `brightness`: Brightness level (0-255)

### Display Layout

```
┌─────────────────────────┐
│  Buoy #1          [🔋]  │  Header (Cyan)
├─────────────────────────┤
│  CONNECTED         [📡] │  Status (Green/Red)
│                    [📍] │  Indicators
├─────────────────────────┤
│  Mode: HDG              │  Navigation Mode
│                         │
│  Hdg: 045° ↗           │  Heading & Speed
│  Spd: 1.5 m/s          │
│                         │
└─────────────────────────┘
```

### Color Scheme

- **Cyan** - Header text
- **Green** - Connected status
- **Red** - Disconnected status
- **Yellow** - Warnings
- **White** - Normal text
- **Gray** - Inactive elements

### Example Usage

```cpp
BuoyStateManager buoyState(espNow);
DisplayManager display(buoyState);

void setup() {
    display.begin();
    display.setBrightness(200);
}

void loop() {
    display.update();  // Automatically shows main screen
    
    // Show selection when switching
    if (buttonPressed) {
        buoyState.selectNextBuoy();
        display.displayBuoySelection();
    }
    
    delay(100);
}
```

---

## CommandManager

**File:** `include/CommandManager.h`

**Purpose:** Translates joystick inputs into navigation commands for autonomous buoys.

**Note:** Currently defined but not fully implemented. Reserved for Phase 2.

### Enums

#### `BuoyCommand`
Command types sent to buoys:
- `CMD_INIT_HOME` - Initialize Home with current GPS and start in HDG mode
- `CMD_SET_HEADING` - Set target heading (HDG mode)
- `CMD_SET_THROTTLE` - Set speed
- `CMD_HOLD_POSITION` - Hold current position
- `CMD_RETURN_HOME` - Return to Home position
- `CMD_STOP` - Stop all movements
- `CMD_CHANGE_MODE` - Change navigation mode

#### `NavigationMode`
Buoy navigation modes:
- `MODE_MANUAL` - Direct manual control
- `MODE_CAP` - Heading following
- `MODE_WAYPOINT` - Waypoint following
- `MODE_HOLD` - Position hold
- `MODE_RETURN_HOME` - Return to base
- `MODE_STOPPED` - Stopped

### Planned Implementation

```cpp
// Future joystick mapping:
// - Left joystick Y → throttle (-100 to +100%)
// - Right joystick X → heading adjustment (-180 to +180°)
// - Yellow L button → select previous buoy
// - Yellow R button → select next buoy
// - Left stick press → hold position
// - Right stick press → return home
```

---

## Data Structures

### `BuoyState`

Complete state information received from a buoy via ESP-NOW.

```cpp
struct BuoyState {
    uint8_t buoyId;             // Buoy ID (0-5)
    double latitude;            // GPS latitude (degrees)
    double longitude;           // GPS longitude (degrees)
    float heading;              // Current heading (0-359.9°)
    float speed;                // Speed in m/s
    NavigationMode mode;        // Current navigation mode
    uint8_t batteryLevel;       // Battery level (0-100%)
    int8_t signalQuality;       // LTE signal quality (0-31, -1 if no LTE)
    bool gpsLocked;             // GPS lock status
    uint32_t timestamp;         // Message timestamp (milliseconds)
};
```

**Size:** 37 bytes (well within ESP-NOW 250-byte limit)

### `CommandPacket`

Command packet sent to buoys via ESP-NOW.

```cpp
struct CommandPacket {
    uint8_t targetBuoyId;       // Target buoy ID (0-5)
    BuoyCommand command;        // Command type
    int16_t heading;            // Target heading (-180 to +180°)
    int8_t throttle;            // Target speed (-100 to +100%)
    NavigationMode mode;        // Navigation mode
    uint32_t timestamp;         // Command timestamp (milliseconds)
};
```

**Size:** 11 bytes

### `Command`

Internal command representation (used by CommandManager).

```cpp
struct Command {
    BuoyCommand type;           // Command type
    int16_t heading;            // Target heading (-180 to +180°)
    int8_t throttle;            // Speed (-100 to +100%)
    NavigationMode mode;        // Navigation mode
    uint32_t timestamp;         // Command timestamp
};
```

---

## Constants & Enums

### Hardware Constants

```cpp
// I2C Configuration
#define I2C_ADDRESS 0x59            // STM32 I2C address
#define SDA_PIN 38                   // I2C SDA pin
#define SCL_PIN 39                   // I2C SCL pin
#define I2C_FREQ 400000              // 400kHz

// System Configuration
#define MAX_BUOYS 6                  // Maximum number of buoys
#define LCD_WIDTH 128                // LCD width in pixels
#define LCD_HEIGHT 128               // LCD height in pixels

// Timing
#define MAIN_LOOP_RATE 10            // Hz (100ms period)
#define DISPLAY_UPDATE_RATE 2        // Hz (500ms period)
#define STATE_UPDATE_RATE 10         // Hz (100ms period)
```

### Pin Definitions

```cpp
// M5Stack AtomS3
// - USB-Serial/JTAG: Built-in USB-C port
// - I2C: SDA=GPIO38, SCL=GPIO39
// - Display: 128x128 LCD (M5GFX via M5Unified)
// - Button: GPIO41 (built-in button on AtomS3)
```

---

## Communication Protocol

### ESP-NOW Packet Flow

#### Joystick → Buoy (Command)

```
[CommandPacket 11 bytes]
├─ targetBuoyId (1 byte)
├─ command (1 byte enum)
├─ heading (2 bytes, int16)
├─ throttle (1 byte, int8)
├─ mode (1 byte enum)
└─ timestamp (4 bytes, uint32)
```

#### Buoy → Joystick (State)

```
[BuoyState 37 bytes]
├─ buoyId (1 byte)
├─ latitude (8 bytes, double)
├─ longitude (8 bytes, double)
├─ heading (4 bytes, float)
├─ speed (4 bytes, float)
├─ mode (1 byte enum)
├─ batteryLevel (1 byte)
├─ signalQuality (1 byte, int8)
├─ gpsLocked (1 byte, bool)
└─ timestamp (4 bytes, uint32)
```

### Communication Characteristics

- **Protocol:** ESP-NOW (2.4GHz, peer-to-peer)
- **Range:** 100-200m line-of-sight
- **Max Packet Size:** 250 bytes
- **Latency:** ~5-10ms typical
- **Bidirectional:** Yes
- **Encryption:** Supported (optional)

---

## System Integration

### Main Loop Structure

```cpp
void loop() {
    // 1. Read joysticks and buttons (10Hz)
    joystick.update();
    
    // 2. Update buoy states (10Hz)
    buoyState.update();
    
    // 3. Update display (2Hz)
    display.update();
    
    // 4. Handle button presses
    if (joystick.wasButtonPressed(BTN_LEFT_STICK)) {
        buoyState.selectPreviousBuoy();
    }
    
    // 5. Send commands (future: CommandManager)
    // TODO: Implement command generation and sending
    
    delay(100);  // 10Hz loop rate
}
```

### Initialization Sequence

```cpp
void setup() {
    // 1. Initialize USB serial (ESP32-S3 specific)
    USBSerial.begin(115200);
    delay(2000);  // Wait for USB-JTAG
    
    // 2. Initialize M5Stack
    M5.begin();
    
    // 3. Initialize joystick manager
    joystick.begin();
    
    // 4. Initialize ESP-NOW
    espNow.begin();
    
    // 5. Add buoys
    espNow.addBuoy(0, BUOY1_MAC);
    espNow.addBuoy(1, BUOY2_MAC);
    // ... up to 6 buoys
    
    // 6. Initialize managers
    buoyState.begin();
    display.begin();
}
```

---

## Error Handling

### Common Issues & Solutions

#### I2C Communication Failures
- **Symptom:** Axis values stuck at 0 or 4095
- **Solution:** Check I2C wiring (SDA=38, SCL=39), verify STM32 is powered

#### ESP-NOW Send Failures
- **Symptom:** `sendCommand()` returns `false`
- **Solution:** Verify MAC address is correct, check buoy is powered on and in range

#### Display Not Updating
- **Symptom:** Blank screen or frozen display
- **Solution:** Call `display.update()` in main loop, check M5.begin() was called

#### Serial Not Working
- **Symptom:** No output from `Serial.println()`
- **Solution:** Use `USBSerial` instead of `Serial` on ESP32-S3 with USB-JTAG

---

## Performance Metrics

### Typical Values (Measured)

- **I2C Read Time:** ~2ms for all axes + buttons
- **ESP-NOW Send Time:** ~3-5ms
- **Display Update Time:** ~20-30ms
- **Main Loop Time:** ~5-10ms (without display update)
- **Battery Drain:** ~150mA @ 3.7V (LCD on, WiFi active)

### Memory Usage

- **Flash:** 871,373 bytes (26.1% of 3,342,336 bytes)
- **RAM:** 47,212 bytes (14.4% of 327,680 bytes)

---

## Version History

- **v1.0** - Initial implementation
  - All core modules functional
  - 6 buoy support
  - English interface
  - Full Doxygen documentation

---

## License & Credits

**Project:** OpenSailingRC BuoyJoystick  
**Author:** Philippe Hubert  
**Date:** 2025  
**Hardware:** M5Stack AtomS3 + Atom JoyStick  
**Framework:** Arduino + PlatformIO  
**License:** Open Source

---

## Support & Contributing

For issues, questions, or contributions, please refer to the project repository.

**End of API Documentation**
