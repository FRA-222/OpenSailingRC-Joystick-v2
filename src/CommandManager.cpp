/**
 * @file CommandManager.cpp
 * @brief Command management for sending to buoys
 * @author Philippe Hubert
 * @date 2025
 */

#include "CommandManager.h"
#include "ICommunication.h"
#include "Logger.h"

/**
 * @brief Constructor
 * @param comm Reference to communication instance
 */
CommandManager::CommandManager(ICommunication& comm)
    : comm(comm),
      newCommandAvailable(false),
      currentHeading(0),
      currentThrottle(0),
      currentMode(NAV_NOTHING),
      lastHeadingChange(0),
      lastModeChange(0) {
}

/**
 * @brief Update commands based on joystick inputs
 */
void CommandManager::update(int16_t leftX, int16_t leftY, int16_t rightX, int16_t rightY,
                             bool btnLeft, bool btnRight, bool btnLeftStick, bool btnRightStick) {
    // TODO: Implement full update logic
    // For now, this is a placeholder
}

/**
 * @brief Get the last generated command
 */
Command CommandManager::getCommand() {
    return currentCommand;
}

/**
 * @brief Check if a new command is available
 */
bool CommandManager::hasNewCommand() {
    return newCommandAvailable;
}

/**
 * @brief Reset the new command flag
 */
void CommandManager::clearNewCommand() {
    newCommandAvailable = false;
}

/**
 * @brief Get the current calculated heading
 */
int16_t CommandManager::getCurrentHeading() {
    return currentHeading;
}

/**
 * @brief Get the current throttle
 */
int8_t CommandManager::getCurrentThrottle() {
    return currentThrottle;
}

/**
 * @brief Get the current navigation mode
 */
tEtatsNav CommandManager::getCurrentMode() {
    return currentMode;
}

/**
 * @brief Generate and send INIT_HOME command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool CommandManager::generateInitHomeCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande INIT_HOME pour Bouee #%d", targetBuoyId);
    
    // Création de la commande d'initialisation du HOME
    Command homeCmd;
    homeCmd.targetBuoyId = targetBuoyId;
    homeCmd.type = CMD_INIT_HOME;
    homeCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = homeCmd;
    newCommandAvailable = true;
    
    // Envoi de la commande via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, homeCmd);
    
    if (success) {
        Logger::log("   -> Commande HOME envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HOME");
    }
    
    return success;
}

/**
 * @brief Generate and send HOME_VALIDATION command
 */
bool CommandManager::generateHomeValidationCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande HOME_VALIDATION pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command validationCmd;
    validationCmd.targetBuoyId = targetBuoyId;
    validationCmd.type = CMD_HOME_VALIDATION;
    validationCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = validationCmd;
    newCommandAvailable = true;
    
    Logger::log("   -> Type: CMD_HOME_VALIDATION");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, validationCmd);
    
    if (success) {
        Logger::log("   -> Commande HOME_VALIDATION envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HOME_VALIDATION");
    }
    
    return success;
}

/**
 * @brief Generate and send NAV_CAP command
 */
bool CommandManager::generateNavCapCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande NAV_CAP pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command navCapCmd;
    navCapCmd.targetBuoyId = targetBuoyId;
    navCapCmd.type = CMD_NAV_CAP;
    navCapCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = navCapCmd;
    newCommandAvailable = true;
    
    Logger::log("   -> Type: CMD_NAV_CAP");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, navCapCmd);
    
    if (success) {
        Logger::log("   -> Commande NAV_CAP envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande NAV_CAP");
    }
    
    return success;
}

/**
 * @brief Generate and send NAV_HOME command
 */
bool CommandManager::generateNavHomeCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande NAV_HOME pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command navHomeCmd;
    navHomeCmd.targetBuoyId = targetBuoyId;
    navHomeCmd.type = CMD_NAV_HOME;
    navHomeCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = navHomeCmd;
    newCommandAvailable = true;
    
    Logger::log("   -> Type: CMD_NAV_HOME");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, navHomeCmd);
    
    if (success) {
        Logger::log("   -> Commande NAV_HOME envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande NAV_HOME");
    }
    
    return success;
}

/**
 * @brief Generate and send NAV_HOLD command
 */
bool CommandManager::generateNavHoldCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande NAV_HOLD pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command navHoldCmd;
    navHoldCmd.targetBuoyId = targetBuoyId;
    navHoldCmd.type = CMD_NAV_HOLD;
    navHoldCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = navHoldCmd;
    newCommandAvailable = true;
    
    Logger::log("   -> Type: CMD_NAV_HOLD");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, navHoldCmd);
    
    if (success) {
        Logger::log("   -> Commande NAV_HOLD envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande NAV_HOLD");
    }
    
    return success;
}

/**
 * @brief Generate and send NAV_STOP command
 */
bool CommandManager::generateNavStopCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande NAV_STOP pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command navStopCmd;
    navStopCmd.targetBuoyId = targetBuoyId;
    navStopCmd.type = CMD_NAV_STOP;
    navStopCmd.timestamp = millis();
    
    // Sauvegarde de la commande courante
    currentCommand = navStopCmd;
    newCommandAvailable = true;
    
    Logger::log("   -> Type: CMD_NAV_STOP");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = comm.sendCommand(targetBuoyId, navStopCmd);
    
    if (success) {
        Logger::log("   -> Commande NAV_STOP envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande NAV_STOP");
    }
    
    return success;
}

/**
 * @brief Generate and send THROTTLE_INCREASE command
 */
bool CommandManager::generateThrottleIncreaseCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande THROTTLE_INCREASE pour Bouee #%d", targetBuoyId);
    
    Command cmd;
    cmd.targetBuoyId = targetBuoyId;
    cmd.type = CMD_THROTTLE_INCREASE;
    cmd.timestamp = millis();
    
    currentCommand = cmd;
    newCommandAvailable = true;
    
    bool success = comm.sendCommand(targetBuoyId, cmd);
    
    if (success) {
        Logger::log("   -> Commande THROTTLE_INCREASE envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande THROTTLE_INCREASE");
    }
    
    return success;
}

/**
 * @brief Generate and send THROTTLE_DECREASE command
 */
bool CommandManager::generateThrottleDecreaseCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande THROTTLE_DECREASE pour Bouee #%d", targetBuoyId);
    
    Command cmd;
    cmd.targetBuoyId = targetBuoyId;
    cmd.type = CMD_THROTTLE_DECREASE;
    cmd.timestamp = millis();
    
    currentCommand = cmd;
    newCommandAvailable = true;
    
    bool success = comm.sendCommand(targetBuoyId, cmd);
    
    if (success) {
        Logger::log("   -> Commande THROTTLE_DECREASE envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande THROTTLE_DECREASE");
    }
    
    return success;
}

/**
 * @brief Generate and send HEADING_INCREASE command
 */
bool CommandManager::generateHeadingIncreaseCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande HEADING_INCREASE pour Bouee #%d", targetBuoyId);
    
    Command cmd;
    cmd.targetBuoyId = targetBuoyId;
    cmd.type = CMD_HEADING_INCREASE;
    cmd.timestamp = millis();
    
    currentCommand = cmd;
    newCommandAvailable = true;
    
    bool success = comm.sendCommand(targetBuoyId, cmd);
    
    if (success) {
        Logger::log("   -> Commande HEADING_INCREASE envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HEADING_INCREASE");
    }
    
    return success;
}

/**
 * @brief Generate and send HEADING_DECREASE command
 */
bool CommandManager::generateHeadingDecreaseCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande HEADING_DECREASE pour Bouee #%d", targetBuoyId);
    
    Command cmd;
    cmd.targetBuoyId = targetBuoyId;
    cmd.type = CMD_HEADING_DECREASE;
    cmd.timestamp = millis();
    
    currentCommand = cmd;
    newCommandAvailable = true;
    
    bool success = comm.sendCommand(targetBuoyId, cmd);
    
    if (success) {
        Logger::log("   -> Commande HEADING_DECREASE envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HEADING_DECREASE");
    }
    
    return success;
}

/**
 * @brief Send heartbeat to all active buoys
 */
uint8_t CommandManager::sendHeartbeatToAllBuoys() {
    uint8_t sentCount = 0;
    
    // Récupérer le nombre de bouées enregistrées
    uint8_t buoyCount = comm.getBuoyCount();
    
    Logger::log("📡 Envoi heartbeat à " + String(buoyCount) + " bouées");
    
    // Envoyer un heartbeat à chaque bouée
    for (uint8_t buoyId = 0; buoyId < MAX_BUOYS; buoyId++) {
        // Vérifier si la bouée est connectée (données récentes)
        if (comm.isBuoyConnected(buoyId, 30000)) {  // Timeout de 30 secondes
            Command heartbeatCmd;
            heartbeatCmd.targetBuoyId = buoyId;
            heartbeatCmd.type = CMD_HEARTBEAT;
            heartbeatCmd.timestamp = millis();
            
            if (comm.sendCommand(buoyId, heartbeatCmd)) {
                sentCount++;
                Logger::log("  ✓ Heartbeat envoyé à Buoy #" + String(buoyId));
            } else {
                Logger::log("  ✗ Échec heartbeat Buoy #" + String(buoyId));
            }
        }
    }
    
    return sentCount;
}

/**
 * @brief Generate and send HEARTBEAT command to a specific buoy
 */
bool CommandManager::generateHeartbeatCommand(uint8_t targetBuoyId) {
    Command heartbeatCmd;
    heartbeatCmd.targetBuoyId = targetBuoyId;
    heartbeatCmd.type = CMD_HEARTBEAT;
    heartbeatCmd.timestamp = millis();
    
    bool success = comm.sendCommand(targetBuoyId, heartbeatCmd);
    
    if (!success) {
        Logger::logf("✗ Échec heartbeat Bouée #%d", targetBuoyId);
    }
    
    return success;
}

/**
 * @brief Apply deadzone to joystick value
 */
int16_t CommandManager::applyDeadzone(int16_t value) {
    if (abs(value) < STICK_DEADZONE) {
        return 0;
    }
    return value;
}

/**
 * @brief Generate heading change command
 */
void CommandManager::updateHeading(int16_t rightX) {
    // TODO: Implement heading update logic
}

/**
 * @brief Generate throttle command
 */
void CommandManager::updateThrottle(int16_t leftY) {
    // TODO: Implement throttle update logic
}
