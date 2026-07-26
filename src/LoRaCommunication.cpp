/**
 * @file LoRaCommunication.cpp
 * @brief Implementation of LoRa communication with buoys
 * @author Philippe Hubert
 * @date 2025
 */

#include "LoRaCommunication.h"
#include "Logger.h"
#include "DisplayManager.h"

#ifndef LORA_USE_SOFTWARE_M0M1
#include <M5Unified.h>  // Needed for button press detection in jumper mode
#endif

static bool ecrLORACommunication = false;

// Forward declarations for conversion functions
static BuoyState convertLoraToState(const BuoyStateLora& loraState);
static BuoyInfo convertLoraInfoToInfo(const BuoyInfoLora& loraInfo);

LoRaCommunication::LoRaCommunication() {
    buoyCount = 0;
    newDataAvailable = false;
    lastRssi = 0;
    lastSnr = 0.0f;
    
    // Initialize sequential polling state
    currentPollIndex = 0;
    lastPollTime = 0;
    pollInterval = 1000;        // Poll every 1 second
    responseTimeout = 1000;     // Wait 1000ms for response (WOR_500MS + marge)
    selectedBuoyId = 0;         // Bouée sélectionnée par défaut (0)
    pollOnlySelected = true;    // Mode simple: toujours la bouée sélectionnée uniquement
    
    // Initialize buoy array
    for (int i = 0; i < MAX_BUOYS; i++) {
        buoys[i].registered = false;
        buoys[i].buoyId = i;
        buoys[i].lastUpdateTime = UINT32_MAX;  // Force disconnected au démarrage
        buoys[i].lastRssi = 0;
        buoys[i].lastSnr = 0.0f;
    }
    
    // Initialize command retry mechanism
    pendingCommandCount = 0;
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        pendingCommands[i].ackReceived = true;  // Mark as completed initially
        pendingCommands[i].retryCount = 0;
    }
    
    // Initialize display manager pointer
    displayManager = nullptr;
    
    // Créer le mutex pour protéger l'accès au module LoRa
    loraMutex = xSemaphoreCreateMutex();
    if (loraMutex == nullptr) {
        Logger::log("ERROR: Failed to create LoRa mutex!");
    }
}

bool LoRaCommunication::begin() {
    Logger::log("✓ LoRa: Initialisation E220-JP...");
    
#ifdef LORA_MODE_CONFIGURATION
    Logger::log("⚙️  MODE: CONFIGURATION (première installation)");
    Logger::log("");
#else
    Logger::log("📡 MODE: NORMAL (transmission/réception)");
    Logger::log("");
#endif
    
#ifdef LORA_USE_SOFTWARE_M0M1
    // Configure M0 and M1 pins as outputs (si contrôle logiciel activé)
    pinMode(LORA_M0_PIN, OUTPUT);
    pinMode(LORA_M1_PIN, OUTPUT);
    delay(500);  // Wait for mode change
#else
    Logger::log("ℹ️  LoRa: M0/M1 contrôlés par SWITCH sur le module");
#ifdef LORA_MODE_CONFIGURATION
    Logger::log("   → Assurez-vous que le switch est sur ON (config)");
#else
    Logger::log("   → Assurez-vous que le switch est sur OFF (normal)");
#endif
    delay(500);
#endif
    
    // Initialize UART for E220-JP module
    lora.Init(&Serial2, CONFIG_MODE_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    
    Logger::logf("✓ LoRa: UART2 initialisé (RX=GPIO%d, TX=GPIO%d, baud=%d)", 
                 LORA_RX_PIN, LORA_TX_PIN, CONFIG_MODE_BAUD);
    
    // Test if Serial2 is available
    if (!Serial2) {
        Logger::log("✗ LoRa: Serial2 non disponible!");
        return false;
    }
    
    // Clear any pending data in buffer
    while (Serial2.available()) {
        Serial2.read();
    }
    delay(100);
    
    Logger::log("✓ LoRa: Port série vérifié et nettoyé");
    Logger::log("");
    
#ifdef LORA_MODE_CONFIGURATION
    // === MODE CONFIGURATION: Programmer le module ===
    Logger::log("⚙️  CONFIGURATION DU MODULE LoRa...");
    Logger::log("");
    
    // === TEST DE COMMUNICATION UART (rapide) ===
    Logger::log("🔍 Test de communication UART...");
    
    // Quick test with known working configuration
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.flush();
    delay(300);
    
    if (Serial2.available()) {
        Logger::log("✓ Module LoRa E220-JP répond correctement!");
        // Clear the response
        while (Serial2.available()) Serial2.read();
    } else {
        Logger::log("⚠️  Pas de réponse immédiate (normal si module occupé)");
    }
    
    Logger::log("");
    
    // Set default configuration values
    lora.SetDefaultConfigValue(loraConfig);
    
    // Configure LoRa E220-JP parameters
    //uint16_t ownAddress = (LORA_ADDRESS_H << 8) | LORA_ADDRESS_L;
    loraConfig.own_address = 0x0000;  // Adresse unique du joystick (pour WOR)
    loraConfig.baud_rate = BAUD_9600;
    loraConfig.air_data_rate = BW125K_SF9;
    loraConfig.subpacket_size = SUBPACKET_200_BYTE;
    loraConfig.rssi_ambient_noise_flag = RSSI_AMBIENT_NOISE_ENABLE;
    loraConfig.transmitting_power = TX_POWER_13dBm;
    loraConfig.own_channel = LORA_CHANNEL;
    loraConfig.rssi_byte_flag = RSSI_BYTE_ENABLE;
    loraConfig.transmission_method_type = UART_P2P_MODE;  // Mode transparent (désactive WOR)
    loraConfig.lbt_flag = LBT_DISABLE;
    loraConfig.wor_cycle = WOR_500MS;  // Ignoré en mode TT, mais requis
    loraConfig.encryption_key = 0x1234;
    loraConfig.target_address = 0x0000;  // Broadcast mode (comme dans l'exemple M5Stack)
    loraConfig.target_channel = LORA_CHANNEL;
    
    Logger::log("✓ LoRa: Configuration préparée");
    Logger::logf("   - Adresse joystick: 0x%04X", 0x0000);
    Logger::logf("   - Canal: %d (920.6 MHz)", LORA_CHANNEL);
    Logger::log("   - Air Data Rate: BW125K_SF9");
    Logger::log("   - Mode: UART P2P (Point-to-Point)");
    Logger::log("   - Puissance: 13 dBm");
    Logger::log("");
    Logger::log("⏳ LoRa: Envoi configuration au module...");
    
    // Apply configuration to module (M0=HIGH, M1=HIGH required)
    int result = lora.InitLoRaSetting(loraConfig);
    
    if (result != 0) {
        Logger::logf("✗ LoRa: Échec configuration (code erreur: %d)", result);
        Logger::log("");
        Logger::log("⚠️  Le module répond mais refuse la configuration");
        Logger::log("   Causes possibles:");
        Logger::log("   - M0/M1 pas en position HAUTE");
        Logger::log("   - Module occupé (réinitialisez-le)");
        Logger::log("   - Configuration invalide");
        
        return false;
    }
    
    Logger::log("✓ LoRa: Module E220-JP configuré avec succès!");
    Logger::log("");
    Logger::log("ℹ️  IMPORTANT: Pour les prochains démarrages:");
    Logger::log("   1. Commentez #define LORA_MODE_CONFIGURATION");
    Logger::log("   2. Décommentez #define LORA_MODE_NORMAL");
    Logger::log("   3. Mettez le switch M0/M1 sur OFF (mode normal)");
    Logger::log("");
    
#ifdef LORA_USE_SOFTWARE_M0M1
    // Set M0=LOW and M1=LOW for normal mode (transmission/reception)
    digitalWrite(LORA_M0_PIN, LOW);
    digitalWrite(LORA_M1_PIN, LOW);
    Logger::log("✓ LoRa: Mode normal activé (M0=LOW, M1=LOW via GPIO)");
    delay(200);
#endif
    
#else
    // === MODE NORMAL: Utiliser la configuration existante ===
    Logger::log("📡 MODE NORMAL: Utilisation de la config du module");
    Logger::log("");
    
    // Set default configuration values (juste pour avoir loraConfig cohérent)
    lora.SetDefaultConfigValue(loraConfig);
    
    //uint16_t ownAddress = (LORA_ADDRESS_H << 8) | LORA_ADDRESS_L;
    loraConfig.own_address = 0x0000;  // Adresse unique du joystick (pour WOR)
    loraConfig.baud_rate = BAUD_9600;
    loraConfig.air_data_rate = BW125K_SF9;
    loraConfig.subpacket_size = SUBPACKET_200_BYTE;
    loraConfig.rssi_ambient_noise_flag = RSSI_AMBIENT_NOISE_ENABLE;
    loraConfig.transmitting_power = TX_POWER_13dBm;
    loraConfig.own_channel = 0x00;
    loraConfig.rssi_byte_flag = RSSI_BYTE_ENABLE;
    loraConfig.transmission_method_type = UART_P2P_MODE;  // Point-to-Point avec adressage
    loraConfig.lbt_flag = LBT_DISABLE;
    loraConfig.wor_cycle = WOR_500MS;  // Cycle rapide
    loraConfig.encryption_key = 0x1234;  // CLÉ VALIDÉE PAR TEST!
    loraConfig.target_address = 0x0000;  // Broadcast mode (comme dans l'exemple M5Stack)
    loraConfig.target_channel = 0x00;
    
    Logger::log("✓ LoRa: Configuration locale préparée");
    Logger::logf("   - Adresse joystick: 0x%04X", 0x0001);
    Logger::logf("   - Canal: %d (920.6 MHz)", LORA_CHANNEL);
    Logger::log("");
    
    // IMPORTANT: Appeler InitLoRaSetting même en mode NORMAL pour initialiser
    // le mutex interne de la bibliothèque (requis pour SendFrame/RecieveFrame)
    // Le module refusera la config (normal, switch sur OFF), mais le mutex sera créé
    Logger::log("⚙️  LoRa: Initialisation mutex interne...");
    int result = lora.InitLoRaSetting(loraConfig);
    
    if (result != 0) {
        // C'est normal en mode NORMAL - le module refuse car M0/M1 = LOW
        Logger::log("   (Module refuse config - normal en mode transmission)");
    } else {
        Logger::log("✓ Module accepte la reconfig (switch était sur ON?)");
    }
    Logger::log("✓ LoRa: Mutex initialisé, prêt pour communication");
    Logger::log("");
#endif
    
    Logger::log("✓ LoRa: Prêt à recevoir");
    Logger::log("");
    
    return true;
}

void LoRaCommunication::update() {
    // Méthode obsolète - le polling REQUEST/RESPONSE n'est plus utilisé
    // On garde la méthode pour compatibilité interface mais elle ne fait rien
    // Utilisez listenForResponses() à la place
}

void LoRaCommunication::listenForResponses()
{

    if (ecrLORACommunication)
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(true);
    }
    else
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(false);
    }

    // Prendre le mutex (attente max 10ms pour éviter blocage)
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        // Mutex non disponible, quelqu'un d'autre utilise le LoRa
        return;
    }
    
    // Écoute non-bloquante des ACK envoyés par les bouées
    // après réception de COMMAND ou heartbeat

    if (Serial2.available() > 0)
    {
        RecvFrame_t recvFrame;
        if (lora.RecieveFrame(&recvFrame) == 0)
        {
            // Frame reçue
            lastRssi = recvFrame.rssi;
            lastSnr = 0.0f;

            // Vérifier le type de message
            if (recvFrame.recv_data_len >= sizeof(LoRaMessageType))
            {
                LoRaMessageType *msgType = (LoRaMessageType *)recvFrame.recv_data;
                
                Logger::logf("📥 LoRa: Paquet reçu - type=%d, taille=%d bytes", *msgType, recvFrame.recv_data_len);

                // Traiter ACK (enrichi avec état)
                if (*msgType == LoRaMessageType::ACK &&
                         recvFrame.recv_data_len == sizeof(AckWithStatePacketLora))
                {
                    AckWithStatePacketLora *ack = (AckWithStatePacketLora *)recvFrame.recv_data;
                    
                    Logger::logf("📥 ACK+State reçu de Bouée #%d (RSSI=%d dBm)",
                                 ack->buoyId, lastRssi);
                    
                    // Traiter l'ACK enrichi
                    processAck(*ack);
                }
                // Support legacy simple ACK (taille AckPacketLora)
                else if (*msgType == LoRaMessageType::ACK &&
                         recvFrame.recv_data_len == sizeof(AckPacketLora))
                {
                    AckPacketLora *legacyAck = (AckPacketLora *)recvFrame.recv_data;
                    
                    Logger::logf("📥 ACK simple (legacy) reçu de Bouée #%d (RSSI=%d dBm)",
                                 legacyAck->buoyId, lastRssi);
                    
                    // Convertir en AckWithStatePacketLora (sans données d'état)
                    AckWithStatePacketLora enrichedAck;
                    memset(&enrichedAck, 0, sizeof(enrichedAck));
                    enrichedAck.messageType = legacyAck->messageType;
                    enrichedAck.buoyId = legacyAck->buoyId;
                    enrichedAck.commandTimestamp = legacyAck->commandTimestamp;
                    enrichedAck.commandType = legacyAck->commandType;
                    processAck(enrichedAck);
                }
            }
        }
    }
    
    // Libérer le mutex
    xSemaphoreGive(loraMutex);
}

//Méthode deprecated - remplacée par le polling séquentiel dans update()
/* bool LoRaCommunication::pollBuoy(uint8_t buoyId, uint32_t timeoutMs) {
        
    
    // Create REQUEST packet
    RequestPacketLora request;
    request.messageType = LoRaMessageType::REQUEST;
    request.targetBuoyId = buoyId;
    request.timestamp = millis();
    
    // Send REQUEST to buoy
    loraConfig.target_address = 0x0000;  // Broadcast mode (comme dans l'exemple M5Stack)
    loraConfig.target_channel = LORA_CHANNEL;
    
    // LOG DÉTAILLÉ DU PAQUET REQUEST
    Logger::log("📤 ========== ENVOI REQUEST LoRa ==========");
    Logger::logf("   Taille paquet : %d bytes", sizeof(request));
    Logger::logf("   messageType   : %d (0x%02X)", (uint8_t)request.messageType, (uint8_t)request.messageType);
    Logger::logf("   targetBuoyId  : %d", request.targetBuoyId);
    Logger::logf("   timestamp     : %lu", request.timestamp);
    
    // Affichage hexadécimal du paquet complet
    Logger::log("   Données brutes (hex):");
    uint8_t* data = (uint8_t*)&request;
    char hexStr[50];
    for (size_t i = 0; i < sizeof(request); i++) {
        sprintf(hexStr + (i*3), "%02X ", data[i]);
    }
    Logger::logf("   %s", hexStr);
    Logger::log("==========================================");
    
    // Prendre le mutex
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Logger::log("✗ LoRa: Timeout acquisition mutex pour pollBuoy");
        return false;
    }
    
    int result = lora.SendFrame(loraConfig, (uint8_t*)&request, sizeof(request));
    
    if (result != 0) {
        xSemaphoreGive(loraMutex);  // Libérer avant de sortir
        Logger::logf("✗ LoRa: Échec envoi REQUEST à Bouée #%d (err=%d)", buoyId, result);
        return false;
    }
    
    Logger::logf("   ✓ REQUEST envoyé, attente réponse (%d ms max)...", timeoutMs);
    
    // Wait for RESPONSE with timeout
    uint32_t startTime = millis();

    while (millis() - startTime < timeoutMs)
    {
        // Check if data is available before calling RecieveFrame
        // (RecieveFrame bloque en interne si pas de données)
        if (Serial2.available() > 0)
        {

            RecvFrame_t recvFrame;

            if (lora.RecieveFrame(&recvFrame) == 0)
            {
                // Frame received
                lastRssi = recvFrame.rssi;
                lastSnr = 0.0f;

                Logger::logf("   📶 Réception LoRa: RSSI=%d dBm, SNR=%.1f dB, size=%d bytes",
                             lastRssi, lastSnr, recvFrame.recv_data_len);

                // Check if this is a RESPONSE packet
                if (recvFrame.recv_data_len >= sizeof(LoRaMessageType))
                {
                    LoRaMessageType *msgType = (LoRaMessageType *)recvFrame.recv_data;

                    Logger::logf("📥 Réception paquet LoRa: type=%d, size=%d bytes",
                                 *msgType, recvFrame.recv_data_len);

                    if (*msgType == LoRaMessageType::RESPONSE &&
                        recvFrame.recv_data_len == sizeof(ResponsePacketLora))
                    {

                        ResponsePacketLora *response = (ResponsePacketLora *)recvFrame.recv_data;

                        // Check if response is from the buoy we polled
                        if (response->state.buoyId == buoyId)
                        {
                            // Process the response
                            processReceivedMessage((uint8_t *)&response->state, sizeof(BuoyStateLora));
                            xSemaphoreGive(loraMutex);  // Libérer avant de sortir
                            return true;
                        }
                    }
                }
            }
        }
        delay(10); // Small delay to avoid CPU hogging
    }
    
    // Libérer le mutex avant de sortir
    xSemaphoreGive(loraMutex);

    // Timeout - mark buoy as potentially disconnected
    int8_t index = findBuoyById(buoyId);
    if (index >= 0) {
        // Don't unregister immediately, just update timestamp
        // isBuoyConnected() will handle timeout logic
    }
    
    return false;
} */

bool LoRaCommunication::sendCommand(uint8_t buoyId, const Command& cmd) {

    if (ecrLORACommunication)
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(true);
    }
    else
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(false);
    }

    if (buoyId >= MAX_BUOYS) {
        Logger::logf("✗ LoRa: ID bouée invalide %d", buoyId);
        return false;
    }
    
    // Create LoRa COMMAND packet
    CommandPacketLora packet;
    packet.messageType = LoRaMessageType::COMMAND;
    packet.targetBuoyId = buoyId;
    packet.command = cmd.type;
    packet.timestamp = millis();
    
    // LOG DÉTAILLÉ DU PAQUET ENVOYÉ
    Logger::log("📤 ========== ENVOI COMMANDE LoRa ==========");
    Logger::logf("   Taille paquet : %d bytes", sizeof(packet));
    Logger::logf("   messageType   : %d (0x%02X)", packet.messageType, packet.messageType);
    Logger::logf("   targetBuoyId  : %d", packet.targetBuoyId);
    Logger::logf("   command       : %d (0x%02X)", packet.command, packet.command);
    Logger::logf("   timestamp     : %lu", packet.timestamp);
    
    // Affichage hexadécimal du paquet complet
    Logger::log("   Données brutes (hex):");
    uint8_t* data = (uint8_t*)&packet;
    char hexStr[100];
    for (size_t i = 0; i < sizeof(packet); i++) {
        sprintf(hexStr + (i*3), "%02X ", data[i]);
    }
    Logger::logf("   %s", hexStr);
    Logger::log("==========================================");
    
    // Send command via LoRa
    bool sent = sendCommandPacket(packet);
    
    if (sent) {
        // Add to pending commands queue (except for heartbeat)
        if (cmd.type != CMD_HEARTBEAT) {
            if (addPendingCommand(packet)) {
                Logger::logf("✓ LoRa: Commande ajoutée à la queue (en attente d'ACK)");
                // Notifier le display : commande envoyée (Bleu)
                if (displayManager != nullptr) {
                    displayManager->setCommandStatus(CommandStatus::SENDING);
                }
            } else {
                Logger::log("⚠️  LoRa: Queue pleine, commande envoyée sans attente d'ACK");
            }
        }
        return true;
    }
    
    return false;
}

BuoyState LoRaCommunication::getLastBuoyState() {
    // Return the most recently updated buoy state
    uint32_t mostRecent = 0;
    int8_t mostRecentIndex = -1;
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].lastUpdateTime > mostRecent) {
            mostRecent = buoys[i].lastUpdateTime;
            mostRecentIndex = i;
        }
    }
    
    if (mostRecentIndex >= 0) {
        return convertLoraToState(buoys[mostRecentIndex].lastState);
    }
    
    // Return empty state if no buoy found
    BuoyState emptyState = {};
    return emptyState;
}

BuoyState LoRaCommunication::getBuoyState(uint8_t buoyId) {
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        return convertLoraToState(buoys[index].lastState);
    }
    
    // Return empty state if buoy not found
    BuoyState emptyState = {};
    return emptyState;
}

bool LoRaCommunication::hasNewData() {
    return newDataAvailable;
}

void LoRaCommunication::clearNewData() {
    newDataAvailable = false;
}

uint8_t LoRaCommunication::getBuoyCount() const {
    return buoyCount;
}

bool LoRaCommunication::isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs) {
    int8_t index = findBuoyById(buoyId);
    if (index < 0) {
        return false;
    }
    
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - buoys[index].lastUpdateTime;
    return elapsed < timeoutMs;
}

BuoyInfo* LoRaCommunication::getBuoyInfo(uint8_t buoyId) {
    static BuoyInfo convertedInfo[MAX_BUOYS];
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        convertedInfo[index] = convertLoraInfoToInfo(buoys[index]);
        return &convertedInfo[index];
    }
    
    return nullptr;
}

BuoyInfo* LoRaCommunication::getAllBuoys() {
    static BuoyInfo convertedBuoys[MAX_BUOYS];
    for (int i = 0; i < MAX_BUOYS; i++) {
        convertedBuoys[i] = convertLoraInfoToInfo(buoys[i]);
    }
    return convertedBuoys;
}

int16_t LoRaCommunication::getLastRssi() const {
    return lastRssi;
}

float LoRaCommunication::getLastSnr() const {
    return lastSnr;
}

int8_t LoRaCommunication::findBuoyById(uint8_t buoyId) {
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].buoyId == buoyId) {
            return i;
        }
    }
    return -1;
}

int8_t LoRaCommunication::addOrUpdateBuoy(uint8_t buoyId) {
    // Check if buoy already exists
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        return index; // Buoy already registered
    }
    
    // Find free slot
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (!buoys[i].registered) {
            buoys[i].registered = true;
            buoys[i].buoyId = buoyId;
            buoyCount++;
            
            Logger::logf("✓ LoRa: Nouvelle bouée découverte - ID #%d (total: %d)", 
                         buoyId, buoyCount);
            
            return i;
        }
    }
    
    Logger::logf("✗ LoRa: Impossible d'ajouter Bouée #%d - Slots pleins", buoyId);
    return -1;
}

void LoRaCommunication::processReceivedMessage(const uint8_t* data, size_t len) {

    if (ecrLORACommunication)
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(true);
    }
    else
    {
        Logger::setLcdOutput(false);
        Logger::setSerialOutput(false);
    }
    
    // Check if this is a buoy state message
    if (len == sizeof(BuoyStateLora)) {
        BuoyStateLora* state = (BuoyStateLora*)data;
        
        // Validate buoy ID
        if (state->buoyId >= MAX_BUOYS) {
            Logger::logf("✗ LoRa: ID bouée invalide %d", state->buoyId);
            return;
        }
        
        // Add or update buoy
        int8_t index = addOrUpdateBuoy(state->buoyId);
        
        if (index >= 0) {
            // Update buoy state
            buoys[index].lastState = *state;
            buoys[index].lastUpdateTime = millis();
            buoys[index].lastRssi = lastRssi;
            buoys[index].lastSnr = lastSnr;
            
            // Set new data flag
            newDataAvailable = true;
            
            Logger::logf("📊 Bouée #%d: Mode=%d, Nav=%d, GPS=%s, Batt=%.0f mAh", 
                         state->buoyId,
                         state->generalMode,
                         state->navigationMode,
                         state->gpsOk ? "OK" : "NOK",
                         state->remainingCapacity);
        }
    } else {
        Logger::logf("⚠️  LoRa: Taille message invalide (%d bytes, attendu %d)", 
                     len, sizeof(BuoyStateLora));
    }
}

// Helper function to convert BuoyStateLora to BuoyState
static BuoyState convertLoraToState(const BuoyStateLora& loraState) {
    BuoyState state;
    state.buoyId = loraState.buoyId;
    state.timestamp = loraState.timestamp;
    state.generalMode = loraState.generalMode;
    state.navigationMode = loraState.navigationMode;
    state.gpsOk = loraState.gpsOk;
    state.headingOk = loraState.headingOk;
    state.yawRateOk = loraState.yawRateOk;
    state.temperature = loraState.temperature;
    state.remainingCapacity = loraState.remainingCapacity;
    state.distanceToCons = loraState.distanceToCons;
    state.autoPilotThrottleCmde = loraState.autoPilotThrottleCmde;
    state.autoPilotTrueHeadingCmde = loraState.autoPilotTrueHeadingCmde;
    return state;
}

// Helper function to convert BuoyInfoLora to BuoyInfo
static BuoyInfo convertLoraInfoToInfo(const BuoyInfoLora& loraInfo) {
    BuoyInfo info;
    info.registered = loraInfo.registered;
    info.buoyId = loraInfo.buoyId;
    info.lastState = convertLoraToState(loraInfo.lastState);
    info.lastUpdateTime = loraInfo.lastUpdateTime;
    info.lastRssi = loraInfo.lastRssi;
    info.lastSnr = loraInfo.lastSnr;
    return info;
}

const char* LoRaCommunication::getModeName() const {
    return "LoRa 920";
}

void LoRaCommunication::setSelectedBuoy(uint8_t buoyId) {
    if (buoyId < MAX_BUOYS) {
        selectedBuoyId = buoyId;
        Logger::logf("📡 LoRa: Bouée sélectionnée #%d", buoyId);
    }
}

void LoRaCommunication::setPollMode(bool onlySelected) {
    pollOnlySelected = onlySelected;
    if (onlySelected) {
        Logger::logf("📡 LoRa: Mode sélectif activé (bouée #%d)", selectedBuoyId);
    } else {
        Logger::log("📡 LoRa: Mode découverte activé (toutes les bouées)");
    }
}

void LoRaCommunication::removeInactiveBuoys(uint32_t timeoutMs) {
    uint32_t currentTime = millis();
    uint8_t removedCount = 0;
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered) {
            // Check if buoy has timed out
            if (currentTime - buoys[i].lastUpdateTime > timeoutMs) {
                Logger::logf("🗑️  LoRa: Suppression Bouée #%d (inactive depuis %d ms)", 
                             buoys[i].buoyId, currentTime - buoys[i].lastUpdateTime);
                
                buoys[i].registered = false;
                buoyCount--;
                removedCount++;
            }
        }
    }
    
    if (removedCount > 0) {
        Logger::logf("✓ LoRa: %d bouée(s) inactive(s) supprimée(s)", removedCount);
    }
}

/**
 * @brief Send command packet via LoRa
 */
bool LoRaCommunication::sendCommandPacket(const CommandPacketLora& packet) {
    // Prendre le mutex (attente max 50ms)
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Logger::log("✗ LoRa: Timeout acquisition mutex pour envoi");
        return false;
    }
    
    // Broadcast COMMAND to all buoys at address 0x0000
    loraConfig.target_address = 0x0000;  // Broadcast
    loraConfig.target_channel = LORA_CHANNEL;
    
    // Send packet using E220-JP SendFrame()
    int result = lora.SendFrame(loraConfig, (uint8_t*)&packet, sizeof(packet));
    
    // Libérer le mutex
    xSemaphoreGive(loraMutex);
    
    if (result == 0) {
        Logger::logf("✓ LoRa: Commande envoyée à Bouée #%d", packet.targetBuoyId);
        return true;
    } else {
        Logger::logf("✗ LoRa: Échec envoi commande (err=%d)", result);
        return false;
    }
}

/**
 * @brief Add command to pending queue
 */
bool LoRaCommunication::addPendingCommand(const CommandPacketLora& command) {
    // Find a free slot or replace oldest completed command
    int8_t freeSlot = -1;
    
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (pendingCommands[i].ackReceived) {
            freeSlot = i;
            break;
        }
    }
    
    if (freeSlot < 0) {
        Logger::log("⚠️  LoRa: Queue de commandes pleine");
        return false;
    }
    
    // Add command to queue
    pendingCommands[freeSlot].command = command;
    pendingCommands[freeSlot].sentTime = millis();
    pendingCommands[freeSlot].retryCount = 0;
    pendingCommands[freeSlot].ackReceived = false;
    
    pendingCommandCount++;
    
    return true;
}

/**
 * @brief Process ACK packet enriched with buoy state
 */
void LoRaCommunication::processAck(const AckWithStatePacketLora& ack) {
    Logger::logf("✅ ACK+State reçu de Bouée #%d pour commande type=%d (ts=%lu)", 
                 ack.buoyId, ack.commandType, ack.commandTimestamp);
    
    // Find matching pending command
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (!pendingCommands[i].ackReceived &&
            pendingCommands[i].command.targetBuoyId == ack.buoyId &&
            pendingCommands[i].command.timestamp == ack.commandTimestamp &&
            pendingCommands[i].command.command == ack.commandType) {
            
            // Mark as acknowledged
            pendingCommands[i].ackReceived = true;
            pendingCommandCount--;
            
            Logger::logf("   ✓ Commande confirmée (retry=%d)", pendingCommands[i].retryCount);
            
            // Notifier le display : ACK reçu (Vert)
            if (displayManager != nullptr) {
                displayManager->setCommandStatus(CommandStatus::ACK_RECEIVED);
            }
            break;
        }
    }
    
    // Update BuoyStateLora from ACK data - immediate display refresh
    if (ack.buoyId >= MAX_BUOYS) {
        Logger::logf("   ⚠️  ACK de Bouée #%d hors limites", ack.buoyId);
        return;
    }
    
    int8_t index = addOrUpdateBuoy(ack.buoyId);
    if (index < 0) {
        Logger::logf("   ⚠️  Impossible d'enregistrer Bouée #%d", ack.buoyId);
        return;
    }
    
    // Copy state data from ACK into stored BuoyStateLora
    BuoyStateLora& state = buoys[index].lastState;
    state.buoyId = ack.buoyId;
    state.timestamp = millis();  // Use current time as update time
    state.generalMode = (tEtatsGeneral)ack.generalMode;
    state.navigationMode = (tEtatsNav)ack.navigationMode;
    state.gpsOk = ack.gpsOk;
    state.headingOk = ack.headingOk;
    state.yawRateOk = ack.yawRateOk;
    state.temperature = ack.temperature;
    state.remainingCapacity = ack.remainingCapacity;
    state.distanceToCons = ack.distanceToCons;
    state.autoPilotThrottleCmde = ack.autoPilotThrottleCmde;
    state.autoPilotTrueHeadingCmde = ack.autoPilotTrueHeadingCmde;
    
    buoys[index].lastUpdateTime = millis();
    buoys[index].lastRssi = lastRssi;
    
    // Signal new data available for immediate display update
    newDataAvailable = true;
    
    // Force immediate display refresh so the user sees the updated state right away
    if (displayManager != nullptr) {
        displayManager->forceRefresh();
    }
    
    Logger::logf("   ✓ État Bouée #%d mis à jour depuis ACK (genMode=%d, navMode=%d, throttle=%d)",
                 ack.buoyId, ack.generalMode, ack.navigationMode, ack.autoPilotThrottleCmde);
}

/**
 * @brief Process command retries
 */
void LoRaCommunication::processCommandRetries() {
    uint32_t currentTime = millis();
    
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (pendingCommands[i].ackReceived) {
            continue;  // Already acknowledged
        }
        
        uint32_t elapsedTime = currentTime - pendingCommands[i].sentTime;
        
        // Check if ACK timeout
        if (elapsedTime >= ACK_TIMEOUT_MS) {
            if (pendingCommands[i].retryCount >= MAX_RETRY_COUNT) {
                // Max retries reached, give up
                Logger::logf("❌ LoRa: Commande timeout après %d tentatives (Bouée #%d, type=%d)", 
                             MAX_RETRY_COUNT + 1,
                             pendingCommands[i].command.targetBuoyId,
                             pendingCommands[i].command.command);
                
                // Notifier le display : timeout (Rouge)
                if (displayManager != nullptr) {
                    displayManager->setCommandStatus(CommandStatus::TIMEOUT);
                }
                
                // Mark as completed (failed)
                pendingCommands[i].ackReceived = true;
                pendingCommandCount--;
            } else {
                // Retry command
                pendingCommands[i].retryCount++;
                pendingCommands[i].sentTime = currentTime;
                
                Logger::logf("🔄 LoRa: Renvoi commande (tentative %d/%d) à Bouée #%d", 
                             pendingCommands[i].retryCount + 1,
                             MAX_RETRY_COUNT + 1,
                             pendingCommands[i].command.targetBuoyId);
                
                sendCommandPacket(pendingCommands[i].command);
            }
        }
    }
}


void LoRaCommunication::setDisplayManager(DisplayManager* display) {
    displayManager = display;
    Logger::log("✓ LoRa: DisplayManager attaché pour feedback visuel");
}