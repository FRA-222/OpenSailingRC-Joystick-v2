/**
 * @file LoRaCommunication.h
 * @brief LoRa communication management with buoys using M5Stack LoRa 920 module
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module handles bidirectional LoRa communication between the joystick
 * and autonomous GPS buoys using the M5Stack LoRa 920MHz module.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef LORA_COMMUNICATION_H
#define LORA_COMMUNICATION_H

#include <Arduino.h>
#include <M5_LoRa_E220_JP.h>
#include "CommandManager.h"
#include "ICommunication.h"

// Forward declaration
class DisplayManager;

// LoRa E220-JP module uses UART communication
// Configuration pour M5Stack Core2 + Unit LoRaE220-920 sur PORT B
// (ExtPort For Core2 ou base M5GO : jaune=G26, blanc=G36)
//
// Unit LoRaE220-920 : UART_RX (jaune), UART_TX (blanc)
// Avec un câble Grove droit :
//   Core2 TX (G26, jaune) → LoRa RX
//   Core2 RX (G36, blanc) ← LoRa TX   (G36 est entrée-seule : RX uniquement)
//
// IMPORTANT: M0/M1 pins sur le module M5Stack LoRa E220-JP
// sont contrôlées par un SWITCH sur le module:
// - Pour CONFIG: Switch sur ON (M0=HIGH, M1=HIGH)
// - Pour NORMAL: Switch sur OFF (M0=LOW, M1=LOW)
//
// CONFIGURATION DU MODE DE DÉMARRAGE
// Décommentez UNE SEULE des deux lignes suivantes:
#define LORA_MODE_NORMAL          // Mode normal (transmission/réception) - À UTILISER EN PRODUCTION
//#define LORA_MODE_CONFIGURATION   // Mode configuration (première fois ou changement de paramètres)
//
// Si votre module n'a pas de switch, décommentez LORA_USE_SOFTWARE_M0M1
// et connectez M0/M1 aux GPIOs indiqués ci-dessous
//
// #define LORA_USE_SOFTWARE_M0M1  // Décommentez si M0/M1 sont connectés aux GPIOs

#define LORA_RX_PIN 36  // Core2 Port B blanc (reçoit du LoRa TX)
#define LORA_TX_PIN 26  // Core2 Port B jaune (envoie vers LoRa RX)

#ifdef LORA_USE_SOFTWARE_M0M1
#define LORA_M0_PIN 7   // Mode control pin 0 (si connecté)
#define LORA_M1_PIN 8   // Mode control pin 1 (si connecté)
#define LORA_AUX_PIN 41 // Auxiliary pin (optionnel)
#endif

// Maximum number of manageable buoys
#define MAX_BUOYS 8

// ── Bandes LoRa supportées ───────────────────────────────────────────────────
// Le même module E220 (et la même bibliothèque) est utilisé dans les deux cas :
// seule la configuration radio change. La fréquence n'est PAS détectable par
// logiciel, c'est donc le mode de communication (CommMode::LORA_920 / LORA_433)
// qui détermine la bande, et le switch M0/M1 du module qui gère sa
// configuration. Les deux extrémités (joystick et bouée) doivent être réglées
// sur la même bande, sinon la liaison est silencieusement inexistante.
enum class LoRaBand {
    BAND_920,   ///< Module E220-900T22S(JP) : F = 920.6 + canal * 0.2 MHz
    BAND_433    ///< Module E220-400T22S     : F = 410.125 + canal * 1 MHz
};

// LoRa E220 configuration
// IMPORTANT : le canal doit être IDENTIQUE côté joystick et côté bouée.
#define LORA_CHANNEL_920 0x00       // 920.600 MHz (bande ISM japonaise)
#define LORA_CHANNEL_433 23         // 433.125 MHz (bande ISM européenne)
#define LORA_ADDRESS_H 0x00         // High byte of address
#define LORA_ADDRESS_L 0x07         // Low byte of address (Joystick) - Modifié pour correspondre à la Bouée
#define LORA_NETID 0x00             // Network ID
#define LORA_UART_BAUD 9600         // UART baud rate (default)

/**
 * @brief Buoy state structure (received via LoRa)
 */
struct BuoyStateLora {
    uint8_t buoyId;                     ///< Buoy ID (0-5)
    uint32_t timestamp;                 ///< Message timestamp
    
    // General state
    tEtatsGeneral generalMode;          ///< General state (INIT, READY, MAINTENANCE, HOME_DEFINITION, NAV)
    tEtatsNav navigationMode;           ///< Current navigation mode
    
    // Sensor status
    bool gpsOk;                         ///< GPS sensor status
    bool headingOk;                     ///< Heading sensor status
    bool yawRateOk;                     ///< Yaw rate sensor status
    
    // Environmental data
    uint8_t temperature;                  ///< Temperature in °C
    
    // Battery data
    uint8_t remainingCapacity;            ///< Remaining battery capacity in %
    
    // Navigation data
    uint8_t distanceToCons;               ///< Distance to consigne/waypoint in meters
    
    // Autopilot commands
    int8_t autoPilotThrottleCmde;       ///< Autopilot throttle command (-100 to +100%)
    int16_t autoPilotTrueHeadingCmde;    ///< Autopilot heading command in degrees (0-359)
};

/**
 * @brief Message types for LoRa protocol
 */
enum class LoRaMessageType : uint8_t {
    REQUEST = 0x01,    ///< Request buoy status (Joystick -> Buoy)
    RESPONSE = 0x02,   ///< Response with buoy state (Buoy -> Joystick)
    COMMAND = 0x03,    ///< Command to buoy (Joystick -> Buoy)
    ACK = 0x04         ///< Acknowledgment (Buoy -> Joystick)
};

/**
 * @brief Request packet structure (Joystick -> Buoy)
 * Used to poll a specific buoy for its current state
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) RequestPacketLora {
    LoRaMessageType messageType;  ///< Message type (REQUEST)
    uint8_t targetBuoyId;         ///< Target buoy ID to poll
    uint32_t timestamp;           ///< Request timestamp
};

/**
 * @brief Response packet structure (Buoy -> Joystick)
 * Contains the full buoy state as response to a REQUEST
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) ResponsePacketLora {
    LoRaMessageType messageType;  ///< Message type (RESPONSE)
    BuoyStateLora state;          ///< Complete buoy state
};

/**
 * @brief Command packet structure (sent via LoRa)
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) CommandPacketLora {
    LoRaMessageType messageType;  ///< Message type (COMMAND)
    uint8_t targetBuoyId;         ///< Target buoy ID
    BuoyCommand command;          ///< Command type
    uint32_t timestamp;           ///< Timestamp
};

/**
 * @brief ACK packet structure (received from Buoy) - LEGACY simple ACK
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) AckPacketLora {
    LoRaMessageType messageType;  ///< Message type (ACK)
    uint8_t buoyId;               ///< Buoy ID sending the ACK
    uint32_t commandTimestamp;    ///< Timestamp of the acknowledged command
    BuoyCommand commandType;      ///< Type of acknowledged command
};

/**
 * @brief ACK with buoy state packet (received from Buoy)
 * Enriched ACK containing full buoy state for immediate display update.
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) AckWithStatePacketLora {
    // ACK identification
    LoRaMessageType messageType;        ///< Message type (ACK)
    uint8_t buoyId;                     ///< Buoy ID sending the ACK
    uint32_t commandTimestamp;          ///< Timestamp of the acknowledged command
    BuoyCommand commandType;            ///< Type of acknowledged command
    
    // Buoy state data (same fields as BuoyStateLora)
    uint8_t generalMode;                ///< tEtatsGeneral as uint8_t
    uint8_t navigationMode;             ///< tEtatsNav as uint8_t
    bool gpsOk;                         ///< GPS sensor status
    bool headingOk;                     ///< Heading sensor status
    bool yawRateOk;                     ///< Yaw rate sensor status
    uint8_t temperature;                ///< Temperature in °C
    uint8_t remainingCapacity;          ///< Remaining battery capacity in %
    uint8_t distanceToCons;             ///< Distance to consigne in meters
    int8_t autoPilotThrottleCmde;       ///< Autopilot throttle command
    int16_t autoPilotTrueHeadingCmde;   ///< Autopilot heading command (0-359)
};

/**
 * @brief Pending command structure for retry mechanism
 */
struct PendingCommand {
    CommandPacketLora command;    ///< Command to send/retry
    uint32_t sentTime;            ///< Time when command was last sent
    uint8_t retryCount;           ///< Number of retries attempted
    bool ackReceived;             ///< Has ACK been received?
};

/**
 * @brief Buoy information structure
 */
struct BuoyInfoLora {
    bool registered;            ///< Is this buoy registered?
    uint8_t buoyId;            ///< Buoy ID (0-5)
    BuoyStateLora lastState;   ///< Last received state
    uint32_t lastUpdateTime;   ///< Last update timestamp
    int16_t lastRssi;          ///< Last RSSI value
    float lastSnr;             ///< Last SNR value
};

/**
 * @brief Class to manage LoRa communication with buoys
 */
class LoRaCommunication : public ICommunication {
public:
    /**
     * @brief Constructor
     * @param band Radio band of the plugged module (default: 920 MHz)
     *
     * The band cannot be probed at runtime: it must match the module actually
     * connected, hence the explicit choice through CommMode in main.cpp.
     */
    explicit LoRaCommunication(LoRaBand band = LoRaBand::BAND_920);

    /**
     * @brief Set the radio band before begin()
     * @param band Band of the plugged module
     *
     * Has no effect once begin() has run: the configuration is pushed to the
     * module during initialization.
     */
    void setBand(LoRaBand band);

    /**
     * @brief Get the channel number used for the current band
     */
    uint8_t getChannel() const;

    /**
     * @brief Get the center frequency of the current band/channel, in MHz
     */
    float getFrequencyMHz() const;

    /**
     * @brief Get the transmitting power register value for the current band
     *
     * ATTENTION : l'énumération TX_POWER_* de la bibliothèque est étiquetée pour
     * le variant JP. Le champ fait 2 bits dans REG1 et sa table de correspondance
     * dépend de la bande :
     *   - E220-900T22S(JP) : 0b00 = 13 dBm (maxi ARIB), 0b01 = 12, 0b10 = 7, 0b11 = 0
     *   - E220-400T22S     : 0b00 = 22 dBm, 0b01 = 17, 0b10 = 13, 0b11 = 10 dBm
     * Écrire TX_POWER_13dBm (0b00) sur un module 433 le ferait donc émettre à
     * 22 dBm. On sélectionne 0b11 = 10 dBm, compatible avec la limite de 10 mW
     * de la bande ISM 433 européenne.
     * À confirmer contre la datasheet du module 433 avant émission prolongée.
     */
    uint8_t getTxPower() const;

    /**
     * @brief Get the "air data rate" field (REG0) for the current band
     *
     * PIÈGE : le registre REG0 n'a PAS la même structure selon la variante.
     *   - E220-900T22S(JP) : bits[4:0] encodent le couple SF/BW (firmware JP
     *     spécifique). BW125K_SF9 = 0b10000 = 1758 bps.
     *   - E220-400T22S (et toute la famille EBYTE standard) :
     *       bits[4:3] = parité UART (00 = 8N1)
     *       bits[2:0] = débit air (0b010 = 2.4 kbps = SF9/BW125, défaut usine)
     *
     * Écrire BW125K_SF9 (0b10000) sur un module 433 met donc les bits de parité
     * à 0b10, soit 8E1, alors que l'ESP32 continue d'émettre en 8N1 : le module
     * devient sourd dès la fin de la configuration et plus aucune trame ne
     * passe. On écrit 0b010 en 433, qui donne 8N1 + 2.4 kbps, soit la même
     * modulation physique SF9/BW125 que le 920.
     *
     * Note de récupération : en mode configuration (switch M0/M1 sur ON) le
     * module force son UART à 9600 8N1, un module déjà mal configuré reste donc
     * reprogrammable.
     */
    uint8_t getAirDataRate() const;

    // ICommunication interface implementation
    bool begin() override;
    void update() override;  // Obsolète - gardé pour compatibilité
    bool sendCommand(uint8_t buoyId, const Command& cmd) override;
    BuoyState getLastBuoyState() override;
    BuoyState getBuoyState(uint8_t buoyId) override;
    bool hasNewData() override;
    void clearNewData() override;
    uint8_t getBuoyCount() const override;
    bool isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs = 120000) override;
    BuoyInfo* getBuoyInfo(uint8_t buoyId) override;
    BuoyInfo* getAllBuoys() override;
    int16_t getLastRssi() const override;
    float getLastSnr() const override;
    const char* getModeName() const override;
    void removeInactiveBuoys(uint32_t timeoutMs) override;
    
    /**
     * @brief Écoute passive des RESPONSE (non-bloquant)
     * Les RESPONSE sont envoyées par les bouées après COMMAND ou heartbeat
     */
    void listenForResponses();
    
    /**
     * @brief Vérifie et renvoie les commandes en attente d'ACK
     * Appelé régulièrement dans la boucle principale
     */
    void processCommandRetries();
    
    /**
     * @brief Set which buoy to poll in selective mode
     * @param buoyId Buoy ID to select (0-5)
     */
    void setSelectedBuoy(uint8_t buoyId);
    
    /**
     * @brief Set polling mode
     * @param onlySelected true = poll only selected buoy, false = poll all buoys
     */
    void setPollMode(bool onlySelected);
    
    /**
     * @brief Set display manager for visual feedback
     * @param display Pointer to DisplayManager
     */
    void setDisplayManager(DisplayManager* display);

private:
    LoRaBand band;                    ///< Radio band of the plugged module
    LoRa_E220_JP lora;                ///< LoRa E220 module instance
    LoRaConfigItem_t loraConfig;      ///< LoRa configuration structure
    BuoyInfoLora buoys[MAX_BUOYS];    ///< Array of buoy information
    uint8_t buoyCount;                ///< Number of registered buoys
    bool newDataAvailable;            ///< New data flag
    int16_t lastRssi;                 ///< Last received RSSI
    float lastSnr;                    ///< Last received SNR
    SemaphoreHandle_t loraMutex;      ///< Mutex pour protéger l'accès au module LoRa
    
    // Sequential polling state
    uint8_t currentPollIndex;         ///< Current buoy index being polled
    uint32_t lastPollTime;            ///< Last poll timestamp
    uint32_t pollInterval;            ///< Interval between polls (ms)
    uint32_t responseTimeout;         ///< Timeout waiting for response (ms)
    uint8_t selectedBuoyId;           ///< Selected buoy ID for selective polling
    bool pollOnlySelected;            ///< If true, poll only selected buoy
    
    // Command retry mechanism
    static const uint8_t MAX_PENDING_COMMANDS = 10;  ///< Maximum pending commands
    static const uint8_t MAX_RETRY_COUNT = 3;        ///< Maximum retry attempts
    static const uint32_t ACK_TIMEOUT_MS = 2000;     ///< Timeout for ACK (ms)
    static const uint32_t RETRY_INTERVAL_MS = 500;   ///< Interval between retries (ms)
    PendingCommand pendingCommands[MAX_PENDING_COMMANDS]; ///< Queue of pending commands
    uint8_t pendingCommandCount;                     ///< Number of pending commands
    DisplayManager* displayManager;                  ///< Pointer to display manager for visual feedback

    /**
     * @brief Poll a specific buoy for its state (Request/Response model)
     * @param buoyId Buoy ID to poll
     * @param timeoutMs Timeout to wait for response (default: 500ms)
     * @return true if buoy responded, false if timeout
     */
    bool pollBuoy(uint8_t buoyId, uint32_t timeoutMs = 500);

    /**
     * @brief Find a buoy by ID
     * @param buoyId Buoy ID
     * @return Index in array or -1 if not found
     */
    int8_t findBuoyById(uint8_t buoyId);

    /**
     * @brief Add or update a buoy
     * @param buoyId Buoy ID
     * @return Index in array
     */
    int8_t addOrUpdateBuoy(uint8_t buoyId);

    /**
     * @brief Process received LoRa message
     * @param data Received data buffer
     * @param len Length of received data
     */
    void processReceivedMessage(const uint8_t* data, size_t len);
    
    /**
     * @brief Process received ACK (enriched with buoy state)
     * @param ack ACK+State packet received
     */
    void processAck(const AckWithStatePacketLora& ack);
    
    /**
     * @brief Add command to pending queue
     * @param command Command packet to add
     * @return true if added successfully
     */
    bool addPendingCommand(const CommandPacketLora& command);
    
    /**
     * @brief Send command packet via LoRa
     * @param packet Command packet to send
     * @return true if sent successfully
     */
    bool sendCommandPacket(const CommandPacketLora& packet);
};

#endif // LORA_COMMUNICATION_H
