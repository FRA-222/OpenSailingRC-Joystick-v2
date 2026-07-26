/**
 * @file BuoyStateManager.cpp
 * @brief Implémentation de la gestion d'état des bouées
 * @author Philippe Hubert
 * @date 2025
 */

#include "BuoyStateManager.h"
#include "DisplayManager.h"
#include "Logger.h"

BuoyStateManager::BuoyStateManager(ICommunication& comm) 
    : comm(comm), espNowListener(nullptr), displayMgr(nullptr) {
    selectedBuoyId = 0;
    lastUpdateTime = 0;
    lastConnectedBuoyId = 255;
}

void BuoyStateManager::begin() {
    Logger::log("✓ BuoyStateManager: Initialisé");
}

void BuoyStateManager::setDisplayManager(DisplayManager* display) {
    displayMgr = display;
}

void BuoyStateManager::update() {
    uint32_t currentTime = millis();
    
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
        lastUpdateTime = currentTime;
        
        // Vérifie si de nouvelles données sont disponibles (primary)
        if (comm.hasNewData()) {
            comm.clearNewData();
        }
        
        // Vérifie aussi les données ESP-NOW (écoute passive en mode LoRa)
        if (espNowListener && espNowListener->hasNewData()) {
            espNowListener->clearNewData();
        }
        
        // Mise à jour du statut de connexion pour détecter les reconnexions
        // Considère les deux sources (LoRa + ESP-NOW)
        bool connected = isSelectedBuoyConnected();
        if (connected && lastConnectedBuoyId != selectedBuoyId) {
            lastConnectedBuoyId = selectedBuoyId;
            if (displayMgr != nullptr) {
                Logger::log("🔄 Rafraîchissement écran - Bouée reconnectée");
                displayMgr->forceRefresh();
            }
        } else if (!connected && lastConnectedBuoyId == selectedBuoyId) {
            lastConnectedBuoyId = 255;  // Marquer comme déconnectée
        }
    }
}

void BuoyStateManager::selectNextBuoy() {
    // Cycle simple entre bouées 0-5 (pas de vérification de connexion)
    selectedBuoyId = (selectedBuoyId + 1) % MAX_BUOYS;
    Logger::logf("→ Bouée sélectionnée: #%d", selectedBuoyId);
}

void BuoyStateManager::selectPreviousBuoy() {
    // Cycle simple entre bouées 0-5 en arrière (pas de vérification de connexion)
    if (selectedBuoyId == 0) {
        selectedBuoyId = MAX_BUOYS - 1;
    } else {
        selectedBuoyId--;
    }
    Logger::logf("→ Bouée sélectionnée: #%d", selectedBuoyId);
}

void BuoyStateManager::selectBuoy(uint8_t buoyId) {
    if (buoyId < MAX_BUOYS) {
        selectedBuoyId = buoyId;
        Logger::logf("✓ Bouée sélectionnée: #%d", selectedBuoyId);
    }
}

uint8_t BuoyStateManager::getSelectedBuoyId() {
    return selectedBuoyId;
}

BuoyState BuoyStateManager::getSelectedBuoyState() {
    // Si ESP-NOW reçoit activement des données pour cette bouée,
    // utiliser exclusivement ESP-NOW (évite le flip-flop avec les vieux ACK LoRa)
    if (isESPNowActiveForBuoy(selectedBuoyId)) {
        return espNowListener->getBuoyState(selectedBuoyId);
    }
    
    // Sinon, utiliser la source primaire (LoRa ou ESP-NOW selon le mode)
    return comm.getBuoyState(selectedBuoyId);
}

bool BuoyStateManager::isESPNowActiveForBuoy(uint8_t buoyId) {
    if (!espNowListener) return false;
    
    BuoyInfo* info = espNowListener->getBuoyInfo(buoyId);
    if (!info || !info->registered || info->lastUpdateTime == UINT32_MAX) return false;
    
    uint32_t age = millis() - info->lastUpdateTime;
    return age < ESPNOW_ACTIVE_TIMEOUT_MS;
}

bool BuoyStateManager::isUsingESPNowData() {
    return isESPNowActiveForBuoy(selectedBuoyId);
}

uint8_t BuoyStateManager::getConnectedBuoyCount() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_BUOYS; i++) {
        if (comm.isBuoyConnected(i) || 
            (espNowListener && espNowListener->isBuoyConnected(i))) {
            count++;
        }
    }
    return count;
}

bool BuoyStateManager::isSelectedBuoyConnected() {
    if (comm.isBuoyConnected(selectedBuoyId)) return true;
    if (espNowListener && espNowListener->isBuoyConnected(selectedBuoyId)) return true;
    return false;
}

String BuoyStateManager::getBuoyName(uint8_t buoyId) {
    return "Buoy #" + String(buoyId + 1);
}

String BuoyStateManager::getNavModeName(tEtatsNav mode) {
    switch (mode) {
        case NAV_NOTHING:
            return "NOTHING";
        case NAV_HOME:
            return "HOME";
        case NAV_HOLD:
            return "HOLD";
        case NAV_STOP:
            return "STOP";
        case NAV_BASIC:
            return "BASIC";
        case NAV_CAP:
            return "CAP";
        case NAV_TARGET:
            return "TARGET";
        default:
            return "INCONNU";
    }
}

String BuoyStateManager::getGeneralModeName(tEtatsGeneral mode) {
    switch (mode) {
        case INIT:
            return "INIT";
        case READY:
            return "READY";
        case MAINTENANCE:
            return "MAINT";
        case HOME_DEFINITION:
            return "HOME_DEF";
        case NAV:
            return "NAV";
        default:
            return "UNKNOWN";
    }
}

void BuoyStateManager::setESPNowListener(ICommunication* listener) {
    espNowListener = listener;
    if (listener) {
        Logger::log("✓ BuoyStateManager: Écouteur ESP-NOW configuré (données passives)");
    }
}

bool BuoyStateManager::hasNewData() {
    bool hasNew = comm.hasNewData();
    if (espNowListener && espNowListener->hasNewData()) {
        hasNew = true;
    }
    return hasNew;
}

void BuoyStateManager::clearNewData() {
    comm.clearNewData();
    if (espNowListener) {
        espNowListener->clearNewData();
    }
}
