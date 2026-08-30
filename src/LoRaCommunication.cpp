/**
 * @file LoRaCommunication.cpp
 * @brief Implementation of LoRa communication with buoys
 * @author Philippe Hubert
 * @date 2025
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "LoRaCommunication.h"
#include "Logger.h"
#include "DisplayManager.h"

#ifndef LORA_USE_SOFTWARE_M0M1
#include <M5Unified.h>  // Needed for button press detection in jumper mode
#endif

static bool ecrLORACommunication = false;

/**
 * @brief Garde RAII qui restaure l'etat du Logger a la sortie de portee
 *
 * PIEGE CORRIGE : les drapeaux du Logger sont des statiques GLOBALES.
 * listenForResponses() les positionnait a l'entree sans jamais les restaurer ;
 * appelee 1000 fois par seconde par loraRxTask (Core 0), elle reduisait au
 * silence TOUT le firmware — traces ESP-NOW, boucle principale, bilans de
 * liaison compris. Le systeme tournait normalement, mais sans plus rien
 * afficher des que la tache RX demarrait.
 *
 * Toute fonction qui veut moduler la verbosite doit donc passer par cette
 * garde, qui restaure l'etat anterieur sur TOUS les chemins de sortie.
 */
class LoggerScope {
public:
    LoggerScope(bool serial, bool lcd)
        : prevSerial(Logger::getSerialOutput())
        , prevLcd(Logger::getLcdOutput()) {
        Logger::setSerialOutput(serial);
        Logger::setLcdOutput(lcd);
    }
    ~LoggerScope() {
        Logger::setSerialOutput(prevSerial);
        Logger::setLcdOutput(prevLcd);
    }
private:
    bool prevSerial;
    bool prevLcd;
};

// Forward declarations for conversion functions
static BuoyState convertLoraToState(const BuoyStateLora& loraState);
static BuoyInfo convertLoraInfoToInfo(const BuoyInfoLora& loraInfo);

LoRaCommunication::LoRaCommunication(LoRaBand radioBand, LoRaAirRate radioAirRate) {
    band = radioBand;
    airRate = radioAirRate;
    buoyCount = 0;
    newDataAvailable = false;
    lastRssi = 0;
    lastSnr = 0.0f;
    linkRxCount = 0;
    linkRssiSum = 0;
    linkRssiMin = 0;
    linkRssiMax = -200;
    lastLinkLogTime = 0;
    
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

void LoRaCommunication::setBand(LoRaBand radioBand) {
    band = radioBand;
}

void LoRaCommunication::setAirRate(LoRaAirRate radioAirRate) {
    airRate = radioAirRate;
}

const char* LoRaCommunication::getAirRateName() const {
    switch (airRate) {
        case LoRaAirRate::AIR_2400:  return "2.4 kbps (SF9/BW125)";
        case LoRaAirRate::AIR_4800:  return "4.8 kbps (SF8/BW125)";
        case LoRaAirRate::AIR_9600:  return "9.6 kbps (SF7/BW125)";
        case LoRaAirRate::AIR_19200: return "19.2 kbps (SF6/BW125)";
        case LoRaAirRate::AIR_38400: return "38.4 kbps (SF5/BW125)";
        case LoRaAirRate::AIR_62500: return "62.5 kbps (SF5/BW500)";
    }
    return "?";
}

uint8_t LoRaCommunication::getChannel() const {
    return (band == LoRaBand::BAND_433) ? LORA_CHANNEL_433 : LORA_CHANNEL_920;
}

float LoRaCommunication::getFrequencyMHz() const {
    // Formules de la datasheet E220 : pas de 1 MHz en 433, 0.2 MHz en 920 (JP)
    if (band == LoRaBand::BAND_433) {
        return 410.125f + getChannel() * 1.0f;
    }
    return 920.6f + getChannel() * 0.2f;
}

uint8_t LoRaCommunication::getTxPower() const {
    // Voir le commentaire détaillé dans LoRaCommunication.h : la table de
    // puissance du registre dépend de la bande, l'énumération de la
    // bibliothèque n'est valable que pour le variant JP.
    return (band == LoRaBand::BAND_433) ? 0b11 : (uint8_t)TX_POWER_13dBm;
}

void LoRaCommunication::noteLinkSample(int16_t rssi) {
    linkRxCount++;
    linkRssiSum += rssi;
    if (rssi < linkRssiMin) linkRssiMin = rssi;
    if (rssi > linkRssiMax) linkRssiMax = rssi;
}

void LoRaCommunication::logLinkQuality() {
    // Bilan de liaison periodique — instrument d'essai de portee.
    // Le RSSI seul ne dit rien : c'est la MARGE qui compte. Le module 433
    // decroche vers -118 dBm a 15.6 kbps ; un RSSI de -95 laisse donc environ
    // 23 dB, un RSSI de -112 seulement 6 dB (a la limite).
    const uint32_t LINK_LOG_INTERVAL = 5000;
    uint32_t now = millis();
    if (now - lastLinkLogTime < LINK_LOG_INTERVAL) return;
    lastLinkLogTime = now;

    if (linkRxCount == 0) {
        Logger::logf("📶 LIAISON %s | AUCUNE trame recue sur %lu s",
                     getAirRateName(), LINK_LOG_INTERVAL / 1000);
    } else {
        Logger::logf("📶 LIAISON %s | %lu trames | RSSI moy %d | min %d | max %d dBm | marge ~%d dB",
                     getAirRateName(),
                     (unsigned long)linkRxCount,
                     (int)(linkRssiSum / (int32_t)linkRxCount),
                     (int)linkRssiMin, (int)linkRssiMax,
                     (int)(getAirRateSensitivity() - linkRssiMin) * -1);
    }
    linkRxCount = 0;
    linkRssiSum = 0;
    linkRssiMin = 0;
    linkRssiMax = -200;
}

int16_t LoRaCommunication::getAirRateSensitivity() const {
    // Sensibilite indicative du SX1262 par debit, en dBm. Sert uniquement a
    // afficher une marge approximative pendant les essais de portee.
    switch (airRate) {
        case LoRaAirRate::AIR_2400:  return -129;
        case LoRaAirRate::AIR_4800:  return -126;
        case LoRaAirRate::AIR_9600:  return -124;
        case LoRaAirRate::AIR_19200: return -121;
        case LoRaAirRate::AIR_38400: return -118;
        case LoRaAirRate::AIR_62500: return -112;
    }
    return -129;
}

uint8_t LoRaCommunication::getAirDataRate() const {
    // Voir le commentaire détaillé dans LoRaCommunication.h : la structure de
    // REG0 diffère entre le variant JP et le E220-400T22S, et une mauvaise
    // valeur casse la parité UART du module.
    if (band == LoRaBand::BAND_433) {
        // E220-400T22S : bits[4:3] = 00 (8N1) pour toutes les valeurs
        // ci-dessous, bits[2:0] = débit air. Valeurs nominales de la datasheet
        // EBYTE ; la correspondance exacte vers SF/BW n'est pas vérifiée.
        switch (airRate) {
            case LoRaAirRate::AIR_2400:  return 0b010;  // défaut usine
            case LoRaAirRate::AIR_4800:  return 0b011;
            case LoRaAirRate::AIR_9600:  return 0b100;
            case LoRaAirRate::AIR_19200: return 0b101;
            case LoRaAirRate::AIR_38400: return 0b110;
            case LoRaAirRate::AIR_62500: return 0b111;
        }
        return 0b010;
    }
    // E220-900T22S(JP) : bits[4:0] encodent directement le couple SF/BW.
    switch (airRate) {
        case LoRaAirRate::AIR_2400:  return (uint8_t)BW125K_SF9;   // 1758 bps
        case LoRaAirRate::AIR_4800:  return (uint8_t)BW125K_SF8;   // 3125 bps
        case LoRaAirRate::AIR_9600:  return (uint8_t)BW125K_SF7;   // 5469 bps
        case LoRaAirRate::AIR_19200: return (uint8_t)BW125K_SF6;   // 9375 bps
        case LoRaAirRate::AIR_38400: return (uint8_t)BW125K_SF5;   // 15625 bps
        case LoRaAirRate::AIR_62500: return (uint8_t)BW500K_SF5;   // 62500 bps
    }
    return (uint8_t)BW125K_SF9;
}

bool LoRaCommunication::begin() {
    Logger::logf("✓ LoRa: Initialisation E220 %s...",
                 (band == LoRaBand::BAND_433) ? "433 MHz" : "920 MHz (JP)");
    
    Logger::log("⚙️  LoRa mode will be determined by the M0/M1 switch position.");
    Logger::log("");
    Logger::log("INFO: M0/M1 controlled by HARDWARE SWITCH on module");
    Logger::log("  -> Switch ON = configuration mode");
    Logger::log("  -> Switch OFF = normal mode");
    Logger::log("");

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
    
    // Unified runtime behavior: test UART, prepare config and attempt to apply it.
    // If the module is in CONFIG mode (M0/M1=HIGH) the configuration will be applied.
    // If the module is in NORMAL mode (M0/M1=LOW) the InitLoRaSetting() call will
    // typically fail, and we fall back to using the existing configuration.

    // === UART COMMUNICATION TEST ===
    Logger::log("🔍 Test de communication UART...");
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.flush();
    delay(300);

    if (Serial2.available()) {
        Logger::log("✓ Module LoRa E220-JP répond correctement (UART test).");
        while (Serial2.available()) Serial2.read();
    } else {
        Logger::log("ℹ️  Pas de réponse immédiate au test UART (possible si switch OFF)");
    }

    Logger::log("");

    // Set default configuration values
    lora.SetDefaultConfigValue(loraConfig);

    // Configure LoRa E220-JP parameters
    loraConfig.own_address = 0x0000;  // Adresse joystick / broadcast
    loraConfig.baud_rate = BAUD_9600;
    loraConfig.air_data_rate = getAirDataRate();
    loraConfig.subpacket_size = SUBPACKET_200_BYTE;
    loraConfig.rssi_ambient_noise_flag = RSSI_AMBIENT_NOISE_ENABLE;
    loraConfig.transmitting_power = getTxPower();
    loraConfig.own_channel = getChannel();
    loraConfig.rssi_byte_flag = RSSI_BYTE_ENABLE;
    loraConfig.transmission_method_type = UART_P2P_MODE;
    loraConfig.lbt_flag = LBT_DISABLE;
    loraConfig.wor_cycle = WOR_500MS;
    loraConfig.encryption_key = 0x1234;
    loraConfig.target_address = 0x0000;
    loraConfig.target_channel = getChannel();

    Logger::log("✓ LoRa: Configuration prepared");
    Logger::logf("   - Canal: %d (%.3f MHz)", getChannel(), getFrequencyMHz());
    // String(v, BIN) supprime les zeros de tete : 0b010 s'afficherait "10", ce
    // qui se lit comme bits[4:3]=10, c'est-a-dire 8E1 — exactement le piege de
    // parite que cette trace sert a detecter. On pad sur 5 bits, largeur reelle
    // du champ REG0[4:0].
    String rateBits = String(getAirDataRate(), BIN);
    while (rateBits.length() < 5) rateBits = "0" + rateBits;
    Logger::logf("   - Air data rate: %s - REG0 0b%s%s",
                 getAirRateName(),
                 rateBits.c_str(),
                 (band == LoRaBand::BAND_433) ? " (8N1)" : "");
    Logger::logf("   - Puissance: registre 0b%s (%s)",
                 (band == LoRaBand::BAND_433) ? "11" : "00",
                 (band == LoRaBand::BAND_433) ? "10 dBm" : "13 dBm");
    Logger::log("");

    Logger::log("⏳ LoRa: Sending configuration to module (will succeed only if switch ON)...");
    int result = lora.InitLoRaSetting(loraConfig);

    if (result == 0) {
        Logger::log("# LoRa: Module E220-JP configured successfully!");
        Logger::log("");
        Logger::log("ℹ️  IMPORTANT: Pour les prochains démarrages, mettez le switch M0/M1 sur OFF.");
        Logger::log("");

#ifdef LORA_USE_SOFTWARE_M0M1
        // If we control M0/M1 via GPIO, set them LOW for normal mode
        digitalWrite(LORA_M0_PIN, LOW);
        digitalWrite(LORA_M1_PIN, LOW);
        Logger::log("✓ LoRa: Mode normal activé (M0=LOW, M1=LOW via GPIO)");
        delay(200);
#endif

    } else {
        Logger::log("📡 LoRa: Configuration command failed (assume switch OFF - normal mode).");
        Logger::log(String("   Result code: ") + String(result));
        Logger::log("");
        Logger::log("   Using existing module configuration for normal operation.");
        Logger::log("");
        // Still call InitLoRaSetting to initialize internal mutex/state of library
        // (some libraries require this even if module rejects config)
        lora.InitLoRaSetting(loraConfig);
    }

    Logger::log("✓ LoRa: Ready to operate");
    Logger::log("");
    
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
    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    // Sans cette garde, la fonction rendait tout le firmware muet.
    LoggerScope logScope(ecrLORACommunication, false);

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
            noteLinkSample(lastRssi);

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
    loraConfig.target_channel = getChannel();
    
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

    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    LoggerScope logScope(ecrLORACommunication, false);

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

    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    LoggerScope logScope(ecrLORACommunication, false);
    
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
    return (band == LoRaBand::BAND_433) ? "LoRa 433" : "LoRa 920";
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
    loraConfig.target_channel = getChannel();
    
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
    
    // Signal new data available: DisplayManager::update() picks this flag up and
    // redraws immediately. Do NOT call forceRefresh() here — it clears the whole
    // screen and would repaint everything on every incoming frame (flickering).
    newDataAvailable = true;

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