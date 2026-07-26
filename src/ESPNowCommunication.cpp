/**
 * @file ESPNowCommunication.cpp
 * @brief Implémentation de la communication ESP-NOW
 * @author Philippe Hubert
 * @date 2025
 */

#include "ESPNowCommunication.h"
#include "Logger.h"
#include "DisplayManager.h"
#include <esp_wifi.h>

// Instance statique pour les callbacks
ESPNowCommunication* ESPNowCommunication::instance = nullptr;

ESPNowCommunication::ESPNowCommunication() {
    buoyCount = 0;
    newDataAvailable = false;
    instance = this;
    
    // Initialise le tableau de bouées
    for (int i = 0; i < MAX_BUOYS; i++) {
        buoys[i].registered = false;
        buoys[i].buoyId = i;
        buoys[i].lastState.buoyId = i;
        buoys[i].lastState.timestamp = 0;
        buoys[i].lastUpdateTime = UINT32_MAX;  // Force disconnected au démarrage
        buoys[i].lastRssi = 0;
        buoys[i].lastSnr = 0.0f;
    }
    
    // Initialize command retry mechanism
    pendingCommandCount = 0;
    commandSequenceCounter = 0;
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        pendingCommands[i].ackReceived = true;  // Mark as completed initially
        pendingCommands[i].retryCount = 0;
    }
    
    // Initialize display manager pointer
    displayManager = nullptr;
}

bool ESPNowCommunication::begin() {
    // Configure WiFi en mode Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Enable ESP-NOW Long Range mode only
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    Logger::log("✓ ESP-NOW: Long Range ONLY mode");
    
    // Configure la puissance TX WiFi au maximum pour portée maximale ESP-NOW
    esp_wifi_set_max_tx_power(84);  // 21 dBm max
    Logger::log("✓ ESP-NOW: Puissance TX réglée à 21 dBm (max)");
    
    // Obtient et affiche l'adresse MAC locale
    WiFi.macAddress(localMac);
    Logger::print("✓ ESP-NOW: Adresse MAC locale: ");
    for (int i = 0; i < 6; i++) {
        Logger::printf("%02X", localMac[i]);
        if (i < 5) Logger::print(":");
    }
    Logger::log();
    
    // Initialise ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Logger::log("✗ ESP-NOW: Échec initialisation");
        return false;
    }
    
    Logger::log("✓ ESP-NOW: Initialisé");
    
    // Enregistre les callbacks
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    
    // Ajoute le peer broadcast pour l'envoi de commandes (permet le relais par le Hub)
    esp_now_peer_info_t broadcastPeer = {};
    memset(broadcastPeer.peer_addr, 0xFF, 6);  // FF:FF:FF:FF:FF:FF
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    if (esp_now_add_peer(&broadcastPeer) != ESP_OK) {
        Logger::log("⚠️  ESP-NOW: Échec ajout peer broadcast (peut-être déjà existant)");
    }
    
    return true;
}

void ESPNowCommunication::update() {
    // ESP-NOW is event-driven via callbacks, no polling needed
    // This function is kept empty but required by ICommunication interface
}

bool ESPNowCommunication::addBuoy(uint8_t buoyId, const uint8_t* macAddress) {
    Logger::log("⚠️  ESP-NOW: addBuoy() est obsolète - utilisation de la découverte automatique");
    return addBuoyDynamically(macAddress, buoyId);
}

bool ESPNowCommunication::addBuoyDynamically(const uint8_t* macAddress, uint8_t buoyId) {
    if (buoyId >= MAX_BUOYS) {
        Logger::logf("✗ ESP-NOW: ID bouée invalide %d", buoyId);
        return false;
    }
    
    // Vérifie si cette bouée existe déjà
    int8_t existingIndex = findBuoyByMac(macAddress);
    if (existingIndex >= 0) {
        // Bouée déjà connue, met à jour son ID si nécessaire
        if (buoys[existingIndex].buoyId != buoyId) {
            Logger::logf("🔄 ESP-NOW: Bouée change d'ID %d -> %d", 
                         buoys[existingIndex].buoyId, buoyId);
            buoys[existingIndex].buoyId = buoyId;
        }
        return true;
    }
    
    // Trouve un slot libre
    int8_t freeSlot = -1;
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (!buoys[i].registered) {
            freeSlot = i;
            break;
        }
    }
    
    if (freeSlot < 0) {
        Logger::logf("✗ ESP-NOW: Aucun slot libre pour Bouée #%d", buoyId);
        return false;
    }
    
    // Configure la nouvelle bouée
    memcpy(buoys[freeSlot].macAddress, macAddress, 6);
    buoys[freeSlot].buoyId = buoyId;
    buoys[freeSlot].registered = true;
    buoys[freeSlot].lastState.buoyId = buoyId;
    buoys[freeSlot].lastState.timestamp = 0;
    
    // Ajoute le peer ESP-NOW
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;  // Canal par défaut  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Logger::logf("✗ ESP-NOW: Échec ajout Bouée #%d", buoyId);
        buoys[freeSlot].registered = false;
        return false;
    }
    
    buoyCount++;
    
    Logger::print("🆕 ESP-NOW: Bouée #");
    Logger::print(String(buoyId));
    Logger::print(" découverte - MAC: ");
    for (int i = 0; i < 6; i++) {
        Logger::printf("%02X", macAddress[i]);
        if (i < 5) Logger::print(":");
    }
    Logger::logf(" (total: %d bouées)", buoyCount);
    
    return true;
}

bool ESPNowCommunication::sendCommand(uint8_t buoyId, const Command& cmd) {
    int8_t index = findBuoyIndex(buoyId);
    if (index < 0) {
        Logger::logf("✗ ESP-NOW: Bouée #%d non trouvée", buoyId);
        return false;
    }
    
    // Prépare le paquet de commande
    CommandPacket packet;
    packet.targetBuoyId = cmd.targetBuoyId;
    packet.command = cmd.type;
    packet.timestamp = cmd.timestamp;
    packet.sequenceNumber = ++commandSequenceCounter;
    packet.ttl = 1; // Original packet, can be relayed once by Hub
    
    // Envoie via ESP-NOW
    bool sent = sendCommandPacket(packet);
    
    if (sent) {
        // Add to pending commands queue (except for heartbeat)
        if (cmd.type != CMD_HEARTBEAT) {
            if (addPendingCommand(packet)) {
                Logger::logf("✓ ESP-NOW: Commande ajoutée à la queue (en attente d'ACK)");
                // Notifier le display : commande envoyée (Bleu)
                if (displayManager != nullptr) {
                    displayManager->setCommandStatus(CommandStatus::SENDING);
                }
            } else {
                Logger::log("⚠️  ESP-NOW: Queue pleine, commande envoyée sans attente d'ACK");
            }
        }
        return true;
    }
    
    return false;
}

BuoyState ESPNowCommunication::getLastBuoyState() {
    // Retourne l'état de la bouée la plus récemment mise à jour (comme LoRa)
    uint32_t mostRecent = 0;
    int8_t mostRecentIndex = -1;
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].lastUpdateTime > mostRecent 
            && buoys[i].lastUpdateTime != UINT32_MAX) {
            mostRecent = buoys[i].lastUpdateTime;
            mostRecentIndex = i;
        }
    }
    
    if (mostRecentIndex >= 0) {
        return buoys[mostRecentIndex].lastState;
    }
    
    // Retourne un état vide si aucune donnée
    BuoyState emptyState = {};
    return emptyState;
}

BuoyState ESPNowCommunication::getBuoyState(uint8_t buoyId) {
    int8_t index = findBuoyIndex(buoyId);
    if (index >= 0) {
        return buoys[index].lastState;
    }
    
    // Retourne un état vide si bouée non trouvée
    BuoyState emptyState = {};
    emptyState.buoyId = buoyId;
    return emptyState;
}

bool ESPNowCommunication::hasNewData() {
    return newDataAvailable;
}

void ESPNowCommunication::clearNewData() {
    newDataAvailable = false;
}

uint8_t ESPNowCommunication::getBuoyCount() const {
    return buoyCount;
}

bool ESPNowCommunication::isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs) {
    int8_t index = findBuoyIndex(buoyId);
    if (index < 0) {
        return false;
    }
    
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - buoys[index].lastUpdateTime;
    return elapsed < timeoutMs;
}

const uint8_t* ESPNowCommunication::getLocalMacAddress() {
    return localMac;
}

void ESPNowCommunication::removeInactiveBuoys(uint32_t timeoutMs) {
    uint8_t removedCount = 0;
    uint32_t now = millis();
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered) {
            // Vérifie si la bouée est inactive (utilise lastUpdateTime comme LoRa)
            if (buoys[i].lastUpdateTime == UINT32_MAX || 
                (now - buoys[i].lastUpdateTime) > timeoutMs) {
                
                // Supprime le peer ESP-NOW
                esp_err_t result = esp_now_del_peer(buoys[i].macAddress);
                if (result == ESP_OK) {
                    Logger::logf("🗑️  ESP-NOW: Bouée #%d supprimée (inactive)", buoys[i].buoyId);
                } else {
                    Logger::logf("⚠️  ESP-NOW: Échec suppression peer Bouée #%d", buoys[i].buoyId);
                }
                
                // Remet à zéro le slot
                buoys[i].registered = false;
                buoys[i].buoyId = i;
                buoys[i].lastState.timestamp = 0;
                memset(buoys[i].macAddress, 0, 6);
                
                buoyCount--;
                removedCount++;
            }
        }
    }
    
    if (removedCount > 0) {
        Logger::logf("🧹 ESP-NOW: %d bouée(s) inactive(s) supprimée(s) (total restant: %d)", 
                     removedCount, buoyCount);
    }
}

// Callback statique pour réception de données
void ESPNowCommunication::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (instance) {
        instance->handleReceivedData(mac, data, len);
    }
}

// Callback statique pour envoi de données
void ESPNowCommunication::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Logger::log("✓ ESP-NOW: Envoi réussi");
    } else {
        Logger::log("✗ ESP-NOW: Échec envoi");
    }
}

// Traite les données reçues
void ESPNowCommunication::handleReceivedData(const uint8_t* mac, const uint8_t* data, int len) {
    // Vérifie si c'est un ACK avec état (enriched ACK)
    if (len == sizeof(AckWithStatePacket)) {
        AckWithStatePacket ack;
        memcpy(&ack, data, sizeof(AckWithStatePacket));
        Logger::logf("📥 ACK+State reçu de Bouée #%d (cmd=%d)", ack.buoyId, ack.commandType);
        processAck(ack);
        return;
    }
    
    // Vérifie la taille des données pour BuoyState
    if (len != sizeof(BuoyState)) {
        Logger::logf("✗ ESP-NOW: Taille invalide reçue %d (attendu %d ou %d)", 
                     len, sizeof(BuoyState), sizeof(AckWithStatePacket));
        return;
    }
    
    // Parse l'état reçu pour obtenir l'ID de la bouée
    BuoyState receivedState;
    memcpy(&receivedState, data, sizeof(BuoyState));
    
    // Validation de l'ID de bouée (comme LoRa)
    if (receivedState.buoyId >= MAX_BUOYS) {
        Logger::logf("✗ ESP-NOW: ID bouée invalide %d", receivedState.buoyId);
        return;
    }
    
    // Trouve la bouée : d'abord par MAC, puis par buoyId (cas du relais Hub)
    int8_t index = findBuoyByMac(mac);
    if (index < 0) {
        // MAC inconnue — peut être relayé par le Hub, chercher par buoyId
        index = findBuoyIndex(receivedState.buoyId);
    }
    if (index < 0) {
        // Bouée vraiment inconnue - tentative de découverte automatique
        Logger::print("📡 ESP-NOW: Nouvelle bouée détectée - MAC: ");
        for (int i = 0; i < 6; i++) {
            Logger::printf("%02X", mac[i]);
            if (i < 5) Logger::print(":");
        }
        Logger::logf(" ID: %d", receivedState.buoyId);
        
        // Tente d'ajouter la bouée dynamiquement
        if (addBuoyDynamically(mac, receivedState.buoyId)) {
            // Trouve l'index de la bouée nouvellement ajoutée
            index = findBuoyByMac(mac);
        } else {
            Logger::logf("✗ ESP-NOW: Impossible d'ajouter la Bouée #%d", receivedState.buoyId);
            return;
        }
    }
    
    // Déduplication : ignorer si même sequenceNumber que le dernier reçu pour cette bouée
    if (buoys[index].lastUpdateTime > 0 &&
        buoys[index].lastState.sequenceNumber == receivedState.sequenceNumber &&
        receivedState.sequenceNumber != 0) {
        return; // Paquet dupliqué, ignorer
    }
    
    // Copie l'état reçu
    memcpy(&buoys[index].lastState, data, sizeof(BuoyState));
    buoys[index].lastState.timestamp = millis();
    buoys[index].lastUpdateTime = millis();  // Mise à jour du timestamp séparé
    
    newDataAvailable = true;
    
    Logger::logf("← État reçu de Bouée #%d (genMode=%d, navMode=%d, GPS=%s)",
                  buoys[index].lastState.buoyId,
                  buoys[index].lastState.generalMode,
                  buoys[index].lastState.navigationMode,
                  buoys[index].lastState.gpsOk ? "OK" : "NO");
}

int8_t ESPNowCommunication::findBuoyIndex(uint8_t buoyId) {
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].buoyId == buoyId) {
            return i;
        }
    }
    return -1;
}

int8_t ESPNowCommunication::findBuoyByMac(const uint8_t* mac) {
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && 
            memcmp(buoys[i].macAddress, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

// ICommunication interface implementation

BuoyInfo* ESPNowCommunication::getBuoyInfo(uint8_t buoyId) {
    static BuoyInfo convertedInfo[MAX_BUOYS];
    int8_t index = findBuoyIndex(buoyId);
    
    if (index >= 0) {
        // Conversion propre BuoyPeer -> BuoyInfo (comme LoRa)
        convertedInfo[index].registered = buoys[index].registered;
        convertedInfo[index].buoyId = buoys[index].buoyId;
        convertedInfo[index].lastState = buoys[index].lastState;
        convertedInfo[index].lastUpdateTime = buoys[index].lastUpdateTime;
        convertedInfo[index].lastRssi = buoys[index].lastRssi;
        convertedInfo[index].lastSnr = buoys[index].lastSnr;
        return &convertedInfo[index];
    }
    return nullptr;
}

BuoyInfo* ESPNowCommunication::getAllBuoys() {
    static BuoyInfo convertedBuoys[MAX_BUOYS];
    for (int i = 0; i < MAX_BUOYS; i++) {
        convertedBuoys[i].registered = buoys[i].registered;
        convertedBuoys[i].buoyId = buoys[i].buoyId;
        convertedBuoys[i].lastState = buoys[i].lastState;
        convertedBuoys[i].lastUpdateTime = buoys[i].lastUpdateTime;
        convertedBuoys[i].lastRssi = buoys[i].lastRssi;
        convertedBuoys[i].lastSnr = buoys[i].lastSnr;
    }
    return convertedBuoys;
}

int16_t ESPNowCommunication::getLastRssi() const {
    // ESP-NOW doesn't provide RSSI directly
    return 0;
}

float ESPNowCommunication::getLastSnr() const {
    // ESP-NOW doesn't provide SNR
    return 0.0f;
}

const char* ESPNowCommunication::getModeName() const {
    return "ESP-NOW";
}

/**
 * @brief Send command packet via ESP-NOW
 */
bool ESPNowCommunication::sendCommandPacket(const CommandPacket& packet) {
    int8_t index = findBuoyIndex(packet.targetBuoyId);
    if (index < 0) {
        Logger::logf("✗ ESP-NOW: Bouée #%d non trouvée", packet.targetBuoyId);
        return false;
    }
    
    // Envoie via broadcast (permet au Hub de relayer la commande)
    static const uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(
        broadcastAddress,
        (uint8_t*)&packet,
        sizeof(CommandPacket)
    );
    
    if (result == ESP_OK) {
        Logger::logf("→ Commande envoyée à Bouée #%d (type=%d)", packet.targetBuoyId, packet.command);
        return true;
    } else {
        Logger::logf("✗ ESP-NOW: Échec envoi à Bouée #%d (err=%d)", packet.targetBuoyId, result);
        return false;
    }
}

/**
 * @brief Add command to pending queue
 */
bool ESPNowCommunication::addPendingCommand(const CommandPacket& command) {
    // Find a free slot or replace oldest completed command
    int8_t freeSlot = -1;
    
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (pendingCommands[i].ackReceived) {
            freeSlot = i;
            break;
        }
    }
    
    if (freeSlot < 0) {
        Logger::log("⚠️  ESP-NOW: Queue de commandes pleine");
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
 * @brief Process ACK with buoy state packet
 */
void ESPNowCommunication::processAck(const AckWithStatePacket& ack) {
    Logger::logf("✅ ACK+State reçu de Bouée #%d pour commande type=%d (ts=%lu)", 
                 ack.buoyId, ack.commandType, ack.commandTimestamp);
    
    // Find matching pending command
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (!pendingCommands[i].ackReceived &&
            pendingCommands[i].command.targetBuoyId == ack.buoyId &&
            pendingCommands[i].command.timestamp == ack.commandTimestamp &&
            pendingCommands[i].command.command == (BuoyCommand)ack.commandType) {
            
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
    
    // Update BuoyState from ACK data - immediate display refresh
    // Auto-enregistrement si bouée inconnue (comme LoRa)
    int8_t index = findBuoyIndex(ack.buoyId);
    if (index < 0) {
        // Bouée non trouvée par ID - tentative d'enregistrement
        // En ESP-NOW, on a besoin du MAC pour l'ajouter.
        // Si l'ACK vient d'une bouée pas encore indexée par ID, 
        // chercher par MAC via les peers existants
        Logger::logf("   ⚠️  ACK de Bouée #%d - recherche dans les peers...", ack.buoyId);
        
        // Parcourir les bouées pour trouver un slot avec ce buoyId ou en créer un
        for (int i = 0; i < MAX_BUOYS; i++) {
            if (buoys[i].registered && buoys[i].buoyId == ack.buoyId) {
                index = i;
                break;
            }
        }
        
        if (index < 0) {
            Logger::logf("   ⚠️  ACK de Bouée #%d non enregistrée (pas de MAC connue)", ack.buoyId);
            return;
        }
    }
    
    // Copy state data from ACK into stored BuoyState
    BuoyState& state = buoys[index].lastState;
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
    
    buoys[index].lastUpdateTime = millis();  // Mise à jour timestamp séparé
    
    // Signal new data available for immediate display update
    // NOTE: Ne PAS appeler forceRefresh() ni aucune méthode display ici !
    // processAck() est appelé depuis le callback ESP-NOW (tâche WiFi, Core 0)
    // tandis que l'affichage s'exécute sur Core 1. Un accès SPI concurrent
    // corromprait l'écran (pixels multicolores, écran noir/blanc).
    // Le flag newDataAvailable sera traité par update() sur Core 1.
    newDataAvailable = true;
    
    Logger::logf("   ✓ État Bouée #%d mis à jour depuis ACK (genMode=%d, navMode=%d, throttle=%d)",
                 ack.buoyId, ack.generalMode, ack.navigationMode, ack.autoPilotThrottleCmde);
}

/**
 * @brief Process command retries
 */
void ESPNowCommunication::processCommandRetries() {
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
                Logger::logf("❌ ESP-NOW: Commande timeout après %d tentatives (Bouée #%d, type=%d)", 
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
                
                Logger::logf("🔄 ESP-NOW: Renvoi commande (tentative %d/%d) à Bouée #%d", 
                             pendingCommands[i].retryCount + 1,
                             MAX_RETRY_COUNT + 1,
                             pendingCommands[i].command.targetBuoyId);
                
                sendCommandPacket(pendingCommands[i].command);
            }
        }
    }
}

void ESPNowCommunication::setDisplayManager(DisplayManager* display) {
    displayManager = display;
    Logger::log("✓ ESP-NOW: DisplayManager attaché pour feedback visuel");
}
