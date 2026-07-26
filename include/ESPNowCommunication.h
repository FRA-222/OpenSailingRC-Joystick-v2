/**
 * @file ESPNowCommunication.h
 * @brief ESP-NOW communication management with buoys
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module handles bidirectional ESP-NOW communication between the joystick
 * and autonomous GPS buoys.
 */

#ifndef ESPNOW_COMMUNICATION_H
#define ESPNOW_COMMUNICATION_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "ICommunication.h"
#include "CommandManager.h"

// Forward declaration
class DisplayManager;

// Maximum number of manageable buoys
#define MAX_BUOYS 8

/**
 * @brief Command packet structure (sent)
 * Uses __attribute__((packed)) to ensure identical memory layout across all devices
 */
struct __attribute__((packed)) CommandPacket {
    uint8_t targetBuoyId;       ///< Target buoy ID
    BuoyCommand command;        ///< Command type
    uint32_t timestamp;         ///< Timestamp
    uint16_t sequenceNumber;    ///< Sequence number for deduplication
    uint8_t ttl;                ///< Time-To-Live: 1=original, 0=relayed by Hub
};

/**
 * @brief ACK with buoy state packet (received from Buoy after command processing)
 * Uses __attribute__((packed)) to ensure identical memory layout on both sides
 */
struct __attribute__((packed)) AckWithStatePacket {
    // ACK identification
    uint8_t buoyId;                     ///< Buoy ID sending the ACK
    uint32_t commandTimestamp;          ///< Timestamp of the acknowledged command
    uint8_t commandType;                ///< BuoyCommand type acknowledged
    
    // Buoy state data
    uint8_t generalMode;                ///< tEtatsGeneral as uint8_t
    uint8_t navigationMode;             ///< tEtatsNav as uint8_t
    bool gpsOk;                         ///< GPS sensor status
    bool headingOk;                     ///< Heading sensor status
    bool yawRateOk;                     ///< Yaw rate sensor status
    float temperature;                  ///< Temperature in °C
    float remainingCapacity;            ///< Remaining battery capacity
    float distanceToCons;               ///< Distance to consigne in meters
    int8_t autoPilotThrottleCmde;       ///< Autopilot throttle command
    int16_t autoPilotTrueHeadingCmde;   ///< Autopilot heading command (0-359)

    // v2 addition for Hub relay
    uint8_t ttl;                        ///< Time-To-Live: 1=original, 0=relayed by Hub
};

/**
 * @brief Pending command structure for retry mechanism
 */
struct PendingCommandESPNow {
    CommandPacket command;      ///< Command to send/retry
    uint32_t sentTime;          ///< Time when command was last sent
    uint8_t retryCount;         ///< Number of retries attempted
    bool ackReceived;           ///< Has ACK been received?
};

/**
 * @brief Class to manage ESP-NOW communication with buoys
 */
class ESPNowCommunication : public ICommunication {
public:
    /**
     * @brief Constructor
     */
    ESPNowCommunication();

    // ICommunication interface implementation
    bool begin() override;
    void update() override;
    bool sendCommand(uint8_t buoyId, const Command& cmd) override;
    BuoyState getLastBuoyState() override;
    BuoyState getBuoyState(uint8_t buoyId) override;
    bool hasNewData() override;
    void clearNewData() override;
    uint8_t getBuoyCount() const override;
    BuoyInfo* getBuoyInfo(uint8_t buoyId) override;
    BuoyInfo* getAllBuoys() override;
    int16_t getLastRssi() const override;
    float getLastSnr() const override;
    const char* getModeName() const override;
    void removeInactiveBuoys(uint32_t timeoutMs) override;

    // Additional ESP-NOW specific methods (override required for interface)
    bool isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs = 120000) override;
    
    // ESP-NOW only methods
    bool addBuoy(uint8_t buoyId, const uint8_t* macAddress);
    bool addBuoyDynamically(const uint8_t* macAddress, uint8_t buoyId);
    const uint8_t* getLocalMacAddress();
    
    /**
     * @brief Vérifie et renvoie les commandes en attente d'ACK
     * Appelé régulièrement dans la boucle principale
     */
    void processCommandRetries();
    
    /**
     * @brief Set display manager for visual feedback
     * @param display Pointer to DisplayManager
     */
    void setDisplayManager(DisplayManager* display);

private:
    struct BuoyPeer {
        uint8_t buoyId;
        uint8_t macAddress[6];
        bool registered;
        BuoyState lastState;
        uint32_t lastUpdateTime;    ///< Last update timestamp (millis)
        int16_t lastRssi;           ///< Last RSSI value
        float lastSnr;              ///< Last SNR value
    };

    BuoyPeer buoys[MAX_BUOYS];
    uint8_t buoyCount;
    bool newDataAvailable;
    uint8_t localMac[6];
    
    // Command retry mechanism
    static const uint8_t MAX_PENDING_COMMANDS = 10;  ///< Maximum pending commands
    static const uint8_t MAX_RETRY_COUNT = 3;        ///< Maximum retry attempts
    static const uint32_t ACK_TIMEOUT_MS = 2000;     ///< Timeout for ACK (ms)
    static const uint32_t RETRY_INTERVAL_MS = 500;   ///< Interval between retries (ms)
    PendingCommandESPNow pendingCommands[MAX_PENDING_COMMANDS]; ///< Queue of pending commands
    uint8_t pendingCommandCount;                     ///< Number of pending commands
    uint16_t commandSequenceCounter;                  ///< Sequence counter for command packets
    DisplayManager* displayManager;                  ///< Pointer to display manager for visual feedback
    
    static ESPNowCommunication* instance;  ///< Instance for callback

    /**
     * @brief Callback for received data (static)
     * @param mac Sender MAC address
     * @param data Received data buffer
     * @param len Data length
     */
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);

    /**
     * @brief Callback for data sent (static)
     * @param mac Receiver MAC address
     * @param status Send status
     */
    static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);

    /**
     * @brief Process received data (instance method)
     * @param mac Sender MAC address
     * @param data Received data buffer
     * @param len Data length
     */
    void handleReceivedData(const uint8_t* mac, const uint8_t* data, int len);

    /**
     * @brief Find buoy index by ID
     * @param buoyId Buoy ID
     * @return Index in array or -1 if not found
     */
    int8_t findBuoyIndex(uint8_t buoyId);

    /**
     * @brief Find buoy index by MAC address
     * @param mac MAC address
     * @return Index in array or -1 if not found
     */
    int8_t findBuoyByMac(const uint8_t* mac);
    
    /**
     * @brief Process received ACK with buoy state
     * @param ack ACK with state packet received
     */
    void processAck(const AckWithStatePacket& ack);
    
    /**
     * @brief Add command to pending queue
     * @param command Command packet to add
     * @return true if added successfully
     */
    bool addPendingCommand(const CommandPacket& command);
    
    /**
     * @brief Send command packet via ESP-NOW
     * @param packet Command packet to send
     * @return true if sent successfully
     */
    bool sendCommandPacket(const CommandPacket& packet);
};

#endif // ESPNOW_COMMUNICATION_H
