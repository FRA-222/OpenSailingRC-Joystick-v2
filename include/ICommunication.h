/**
 * @file ICommunication.h
 * @brief Abstract interface for communication with buoys
 * @author Philippe Hubert  
 * @date 2025
 * 
 * This interface allows BuoyStateManager and CommandManager to work
 * with both ESP-NOW and LoRa communication modes.
 */

#ifndef ICOMMUNICATION_H
#define ICOMMUNICATION_H

#include <Arduino.h>
#include "BuoyEnums.h"

// Forward declaration for Command (defined in CommandManager.h)
struct Command;

/**
 * @brief Buoy state structure (unified for all communication modes)
 */
struct BuoyState {
    uint8_t buoyId;
    uint32_t timestamp;
    tEtatsGeneral generalMode;
    tEtatsNav navigationMode;
    bool gpsOk;
    bool headingOk;
    bool yawRateOk;
    double latitude;                    ///< Latitude in degrees
    double longitude;                   ///< Longitude in degrees
    float temperature;
    float remainingCapacity;
    float distanceToCons;
    int8_t autoPilotThrottleCmde;
    float autoPilotTrueHeadingCmde;     ///< Heading command in degrees (float to match Buoy)

    // v2 additions for Hub relay
    uint16_t sequenceNumber;            ///< Sequence number for deduplication
    uint8_t ttl;                        ///< Time-To-Live: 1=original, 0=relayed by Hub
};

/**
 * @brief Buoy information structure
 */
struct BuoyInfo {
    bool registered;
    uint8_t buoyId;
    BuoyState lastState;
    uint32_t lastUpdateTime;
    int16_t lastRssi;
    float lastSnr;
};

// Maximum number of buoys
#ifndef MAX_BUOYS
#define MAX_BUOYS 8
#endif

/**
 * @brief Abstract communication interface
 */
class ICommunication {
public:
    virtual ~ICommunication() = default;
    
    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual bool sendCommand(uint8_t buoyId, const Command& cmd) = 0;
    virtual BuoyState getLastBuoyState() = 0;
    virtual BuoyState getBuoyState(uint8_t buoyId) = 0;
    virtual bool hasNewData() = 0;
    virtual void clearNewData() = 0;
    virtual uint8_t getBuoyCount() const = 0;
    virtual bool isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs = 120000) = 0;
    virtual BuoyInfo* getBuoyInfo(uint8_t buoyId) = 0;
    virtual BuoyInfo* getAllBuoys() = 0;
    virtual int16_t getLastRssi() const = 0;
    virtual float getLastSnr() const = 0;
    virtual const char* getModeName() const = 0;
    virtual void removeInactiveBuoys(uint32_t timeoutMs) = 0;
};

#endif // ICOMMUNICATION_H
