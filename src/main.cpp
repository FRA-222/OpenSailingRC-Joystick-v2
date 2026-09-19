/**
 * @file main.cpp
 * @brief Programme principal du joystick de contrôle de bouées
 * @author Philippe Hubert
 * @date 2025
 * 
 * Ossature avec communication ESP-NOW ou LoRa bidirectionnelle et affichage LCD.
 * Un seul code pour les deux matériels (v1 AtomS3, v2 Core2) : voir
 * JoystickConfiguration.h et HardwareConfig.h.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include <Arduino.h>
#include <M5Unified.h>
#include "Logger.h"
#include "HardwareConfig.h"
#include "JoystickConfiguration.h"
#include "JoystickManager.h"
#include "CommunicationConfig.h"
#include "ESPNowCommunication.h"
#include "LoRaCommunication.h"
#include "BuoyStateManager.h"
#include "DisplayManager.h"
#include "CommandManager.h"

// ============================================================================
// CONFIGURATION
// ============================================================================
// Matériel (v1 AtomS3 / v2 Core2), mode de communication, bande et débit air
// LoRa, écoute passive ESP-NOW, traces de mise au point : TOUT est dans
// include/JoystickConfiguration.h (le matériel est fixé par l'environnement
// PlatformIO, le reste s'y édite). Rien à régler dans ce fichier.
//
// Les bouées sont découvertes automatiquement via leurs broadcasts.

// ============================================================================
// INSTANCES DES MANAGERS
// ============================================================================
JoystickManager joystick;
ESPNowCommunication espNow;
LoRaCommunication lora(LORA_BAND, LORA_AIR_RATE);

// Instances statiques pour chaque mode
BuoyStateManager buoyStateESPNow(espNow);
CommandManager cmdManagerESPNow(espNow);
DisplayManager displayESPNow(buoyStateESPNow, JOYSTICK_FIRMWARE_VERSION);

BuoyStateManager buoyStateLora(lora);
CommandManager cmdManagerLora(lora);
DisplayManager displayLora(buoyStateLora, JOYSTICK_FIRMWARE_VERSION);

// Pointeurs vers les instances actives selon le mode
BuoyStateManager* buoyState = nullptr;
DisplayManager* display = nullptr;
CommandManager* cmdManager = nullptr;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================
uint32_t lastLoopTime = 0;
const uint32_t LOOP_INTERVAL = 100;  // 10Hz

uint32_t lastHeartbeatTime = 0;
const uint32_t HEARTBEAT_INTERVAL = 3000;  // 3 secondes

// Demande périodique de la trame OBSERVABLE (LoRa v2) : température, batterie,
// route et vitesse GPS ne sont plus dans la trame rapide BUOY_STATUS
// (GATEWAY_DESIGN.md §3.4). Cadence lente : ces grandeurs bougent peu, et
// chaque demande coûte un aller-retour radio (41 + 46 ms de temps d'antenne).
uint32_t lastObservableTime = 0;
const uint32_t OBSERVABLE_INTERVAL = 30000;  // 30 secondes

// ============================================================================
// TÂCHE FREERTOS POUR RÉCEPTION LORA
// ============================================================================
TaskHandle_t loraRxTaskHandle = NULL;

/**
 * @brief Tâche dédiée à la réception LoRa (Core 0)
 * Cette tâche s'exécute en parallèle du loop principal pour optimiser
 * la réception des réponses bouée sans ralentir l'envoi des commandes
 */
void loraRxTask(void* parameter) {
    Logger::log("# Tâche LoRa RX démarrée sur Core 0");
    
    while (true) {        
        lora.listenForResponses();

        // Courte pause avant la prochaine série d'écoutes
        vTaskDelay(1 / portTICK_PERIOD_MS);  // 1ms
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // Initialisation M5 (AtomS3 ou Core2, doit être faite en premier)
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // Initialisation du Logger (sortie série uniquement par défaut)
    Logger::init(true, false);  // Serial activé, LCD désactivé

    // Enregistre le mode choisi à la compilation : getModeName() est utilisé
    // dans les logs et resterait sinon sur sa valeur par défaut.
    CommunicationConfig::setMode(COMM_MODE);

    // Initialisation Serial pour debug (non bloquant)
    //USBSerial.begin(115200);
    //delay(100);  // Court délai pour stabilisation (non bloquant)
    
    Logger::log();
    Logger::log("*** DEMARRAGE ***");
    Logger::log("*** TEST SERIAL ***");
    Logger::log();
    Logger::log("===========================================");
    Logger::logf("  OpenSailingRC - %s (%s) v%s", JoystickConfiguration::productName(),
                 JoystickConfiguration::hardwareName(), JOYSTICK_FIRMWARE_VERSION);
    Logger::log("===========================================");
    Logger::log();
    JoystickConfiguration::logSummary();
    Logger::log();

    // 0. Sélection du mode de communication et des managers associés
    Logger::log("0. Configuration du mode de communication...");
    if (COMM_MODE == CommMode::ESP_NOW) {
        Logger::log("   -> Mode: ESP-NOW (2.4 GHz)");
        buoyState = &buoyStateESPNow;
        cmdManager = &cmdManagerESPNow;
        display = &displayESPNow;
        espNow.setDisplayManager(display);
    } else {
        Logger::logf("   -> Mode: %s", CommunicationConfig::getModeName());
        buoyState = &buoyStateLora;
        cmdManager = &cmdManagerLora;
        display = &displayLora;
        lora.setDisplayManager(display);
    }
    
    // 1. Initialisation du joystick (I2C)
    Logger::log();
    Logger::log("1. Initialisation Joystick...");
    if (!joystick.begin()) {
        Logger::log("   -> ERREUR: Echec initialisation joystick");
        Logger::logf("   -> Verifiez: %s", JoystickConfiguration::wiringHint());
        // On continue quand même pour tester ESP-NOW
    } else {
        Logger::log("   -> Joystick: OK");
    }
    
    // 2. Initialisation de la communication
    Logger::log();
    if (COMM_MODE == CommMode::ESP_NOW) {
        Logger::log("2. Initialisation ESP-NOW...");
        if (!espNow.begin()) {
            Logger::log("   -> ERREUR CRITIQUE: Echec initialisation ESP-NOW");
            while (1) {
                delay(1000);
            }
        } else {
            Logger::log("   -> ESP-NOW: OK");
        }
    } else {
        Logger::logf("2. Initialisation %s...", CommunicationConfig::getModeName());
        if (!lora.begin()) {
            Logger::log("   -> ERREUR CRITIQUE: Echec initialisation LoRa");
            while (1) {
                delay(1000);
            }
        } else {
            Logger::logf("   -> %s: OK (canal %d, %.3f MHz)",
                         CommunicationConfig::getModeName(),
                         lora.getChannel(), lora.getFrequencyMHz());
        }
        
#if JOYSTICK_ESPNOW_PASSIVE
        // En mode LoRa, initialiser aussi ESP-NOW en écoute passive
        // pour recevoir les broadcasts de statut des bouées (plus rapide que LoRa)
        Logger::log("2b. Initialisation ESP-NOW (écoute passive)...");
        if (!espNow.begin()) {
            Logger::log("   -> WARNING: Echec init ESP-NOW passif (optionnel)");
        } else {
            Logger::log("   -> ESP-NOW passif: OK (réception broadcasts bouées)");
        }
#else
        // Radio 2,4 GHz coupée — voir JOYSTICK_ESPNOW_PASSIVE dans JoystickConfiguration.h.
        // WiFi.mode(WIFI_OFF) est explicite plutôt qu'implicite : ne pas appeler
        // espNow.begin() suffirait à ne pas allumer la radio, mais le dire
        // fermement garantit l'etat mesure, et c'est ce qui compte pour un essai.
        WiFi.mode(WIFI_OFF);
        Logger::log("2b. ESP-NOW passif: DESACTIVE (JOYSTICK_ESPNOW_PASSIVE = 0)");
        Logger::log("   -> Radio 2.4 GHz coupee. Essai de portee : comparer le TAUX");
        Logger::log("      DE RECEPTION du bilan de liaison avec et sans (§5.3).");
        Logger::log("   -> Les broadcasts d'etat ESP-NOW des bouees ne sont plus recus ;");
        Logger::log("      seul le LoRa alimente l'affichage.");
#endif
    }
    
    // 3. Préparation pour découverte automatique des bouées
    Logger::log();
    Logger::log("3. Attente découverte automatique des bouées...");
    Logger::log("   -> Les bouées seront ajoutées automatiquement");
    Logger::log("   -> lors de la réception de leurs broadcasts");
    
    // Initialiser BuoyStateManager
    Logger::log();
    Logger::log("4. Initialisation BuoyStateManager...");
    buoyState->begin();
    buoyState->setDisplayManager(display);
    
    // En mode LoRa, configurer l'écoute passive ESP-NOW
    if (CommunicationConfig::isLoRa(COMM_MODE)) {
        buoyState->setESPNowListener(&espNow);
    }

    // 5. Initialisation de l'affichage (display valide après choix du mode)
    Logger::log();
    Logger::log("5. Initialisation Display...");
    if (!display->begin()) {
        Logger::log("   -> ERREUR: Echec initialisation display");
    } else {
        Logger::log("   -> Display: OK");
    }

    // 5b. Diagnostic hardware détaillé affiché sur l'écran de boot
    Logger::log();
    // Allume la LED verte du ByteButton correspondant à la bouée sélectionnée (v2 ; neutre en v1)
    joystick.setBuoySelectionLed(buoyState->getSelectedBuoyId());

    Logger::log("5b. Diagnostic hardware...");
    HardwareDiagResult diagResult = joystick.performDiagnostic();
    display->displayBootDiagnostic(diagResult, 4000);
    Logger::log("   -> Diagnostic termine");

    Logger::log();
    Logger::log("===========================================");
    Logger::log("  SYSTEM READY");
    Logger::log("  Mode: COMMAND_BUOY_STATUS → BUOY_STATUS");
    Logger::log("===========================================");
    Logger::log();
    
    // Affiche écran de connexion
    display->displayConnecting("Ready");
    
    // En mode LoRa, créer la tâche de réception sur Core 0
    if (CommunicationConfig::isLoRa(COMM_MODE)) {
        Logger::log();
        Logger::log("6. Création tâche LoRa RX sur Core 0...");
        xTaskCreatePinnedToCore(
            loraRxTask,         // Fonction de la tâche
            "LoRaRxTask",       // Nom de la tâche
            4096,               // Taille de pile (4KB)
            NULL,               // Paramètre
            1,                  // Priorité (1 = normale)
            &loraRxTaskHandle,  // Handle de la tâche
            0                   // Core 0 (le loop() s'exécute sur Core 1)
        );
        Logger::log("   -> Tâche LoRa RX créée sur Core 0");
    }
    
    delay(2000);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    uint32_t currentTime = millis();
    
    // Maintient une fréquence de loop stable (10Hz)
    if (currentTime - lastLoopTime < LOOP_INTERVAL) {
        vTaskDelay(1);
        return;
    }
    lastLoopTime = currentTime;
    
    // ========================================================================
    // 1. LECTURE DES JOYSTICKS ET BOUTONS
    // ========================================================================
    joystick.update();
    
    // ByteButton (v2 uniquement ; renvoie toujours -1 en v1) : sélection directe
    // de la bouée, de gauche à droite (touche la plus à gauche -> bouée #1)
    int8_t byteBtnIdx = joystick.getByteButtonPressedIndex();
    if (byteBtnIdx >= 0) {
        Logger::logf("\n[BYTE-BTN] Touche %d pressee - Selection Bouee #%d", byteBtnIdx + 1, byteBtnIdx);
        buoyState->selectBuoy(byteBtnIdx);
        joystick.setBuoySelectionLed(buoyState->getSelectedBuoyId());
        display->displayBuoySelection();
    }

    // Les boutons physiques différent entre v1 et v2 : la correspondance
    // bouton -> action (BTN_ACTION_*) et les libellés sont dans HardwareConfig.h.

    // Validation HOME -> passage en NAV
    if (joystick.wasButtonPressed(BTN_ACTION_HOME_VALIDATION)) {
        uint8_t activeBuoy = buoyState->getSelectedBuoyId();
        Logger::logf("\n%s presse - Envoi HOME_VALIDATION a Bouee #%d", BTN_LABEL_HOME_VALIDATION, activeBuoy);
        cmdManager->generateHomeValidationCommand(activeBuoy);
    }

    // Initialisation du HOME à la position courante
    if (joystick.wasButtonPressed(BTN_ACTION_INIT_HOME)) {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n%s presse - Envoi CMD_INIT_HOME a Bouee #%d", BTN_LABEL_INIT_HOME, selectedId);
        cmdManager->generateInitHomeCommand(selectedId);
    }

    // CMD_NAV_HOLD
    if (joystick.wasButtonPressed(BTN_ACTION_NAV_HOLD)) {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n%s presse - NAV_HOLD (Bouee #%d)", BTN_LABEL_NAV_HOLD, selectedId);
        cmdManager->generateNavHoldCommand(selectedId);
    }

    
    // Détection mouvement joystick GAUCHE (Y axis)
    // Seuil pour détecter un mouvement significatif (valeur centrée autour de 0)
    static const int16_t JOYSTICK_THRESHOLD = 1500;  // Seuil de détection (sur ~2048)
    static bool leftUpProcessed = false;
    static bool leftDownProcessed = false;
    
    int16_t leftY = joystick.getAxisCentered(AXIS_LEFT_Y);

    // Joystick GAUCHE vers le HAUT : CMD_NAV_CAP
    if (leftY < -JOYSTICK_THRESHOLD && !leftUpProcessed)
    {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n[JS-L] Joystick GAUCHE vers le HAUT - NAV_CAP (Bouee #%d)", selectedId);
        cmdManager->generateNavCapCommand(selectedId);
        leftUpProcessed = true;
    }
    else if (leftY > -JOYSTICK_THRESHOLD / 2)
    {
        leftUpProcessed = false; // Reset quand joystick revient au centre
    }

    // Joystick GAUCHE vers le BAS : CMD_NAV_HOME
    if (leftY > JOYSTICK_THRESHOLD && !leftDownProcessed)
    {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n[JS-L] Joystick GAUCHE vers le BAS - NAV_HOME (Bouee #%d)", selectedId);
        cmdManager->generateNavHomeCommand(selectedId);
        leftDownProcessed = true;
    }
    else if (leftY < JOYSTICK_THRESHOLD / 2)
    {
        leftDownProcessed = false; // Reset quand joystick revient au centre
    }

    // Détection mouvement joystick GAUCHE (X axis) - Mode MAINTENANCE
    static bool leftLeftProcessed = false;
    static bool leftRightProcessed = false;

    int16_t leftX = joystick.getAxisCentered(AXIS_LEFT_X);

    // Joystick GAUCHE vers la GAUCHE : CMD_MAINTENANCE_ENTER
    if (leftX < -JOYSTICK_THRESHOLD && !leftLeftProcessed)
    {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n[JS-L] Joystick GAUCHE vers la GAUCHE - MAINTENANCE_ENTER (Bouee #%d)", selectedId);
        cmdManager->generateMaintenanceEnterCommand(selectedId);
        leftLeftProcessed = true;
    }
    else if (leftX > -JOYSTICK_THRESHOLD / 2)
    {
        leftLeftProcessed = false; // Reset quand joystick revient au centre
    }

    // Joystick GAUCHE vers la DROITE : CMD_MAINTENANCE_EXIT
    if (leftX > JOYSTICK_THRESHOLD && !leftRightProcessed)
    {
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("\n[JS-L] Joystick GAUCHE vers la DROITE - MAINTENANCE_EXIT (Bouee #%d)", selectedId);
        cmdManager->generateMaintenanceExitCommand(selectedId);
        leftRightProcessed = true;
    }
    else if (leftX < JOYSTICK_THRESHOLD / 2)
    {
        leftRightProcessed = false; // Reset quand joystick revient au centre
    }

    // ========================================================================
    // JOYSTICK DROIT - Contrôle Throttle et Heading
    // ========================================================================

    // --- DEBUG : log périodique des valeurs brutes du joystick droit ---
    // Activer avec DEBUG_JOYSTICK_RAW (JoystickConfiguration.h) pour regler le
    // centrage du stick ; a laisser desactive pendant les essais de portee.
#if DEBUG_JOYSTICK_RAW
    static uint32_t lastJsDebugTime = 0;
    if (currentTime - lastJsDebugTime >= 500) {
        lastJsDebugTime = currentTime;
        uint16_t rawRX = joystick.getAxisValue(AXIS_RIGHT_X);
        uint16_t rawRY = joystick.getAxisValue(AXIS_RIGHT_Y);
        Logger::logf("[JS-R DEBUG] raw X=%4u  Y=%4u  (centré X=%+5d  Y=%+5d)",
                     rawRX, rawRY,
                     joystick.getAxisCentered(AXIS_RIGHT_X),
                     joystick.getAxisCentered(AXIS_RIGHT_Y));
    }
#endif
    // -------------------------------------------------------------------

    // Détection mouvement joystick DROIT
    static bool rightUpProcessed = false;
    static bool rightDownProcessed = false;
    static bool rightRightProcessed = false;
    static bool rightLeftProcessed = false;
    
    int16_t rightY = joystick.getAxisCentered(AXIS_RIGHT_Y);
    // Sens de l'axe X normalisé par matériel (RIGHT_STICK_X_SIGN, HardwareConfig.h) :
    // après correction, pousser à DROITE donne toujours une valeur POSITIVE.
    int16_t rightX = (int16_t)(RIGHT_STICK_X_SIGN * joystick.getAxisCentered(AXIS_RIGHT_X));
    
    // Récupération de l'état actuel de la bouée pour incrémenter les valeurs
    uint8_t selectedId = buoyState->getSelectedBuoyId();
    BuoyState currentBuoyState = buoyState->getSelectedBuoyState();

    // Joystick DROIT vers le HAUT : CMD_THROTTLE_INCREASE
    if (rightY < -JOYSTICK_THRESHOLD && !rightUpProcessed)
    {
        Logger::logf("\n[JS-R] Joystick DROIT vers le HAUT - THROTTLE_INCREASE (Bouee #%d)", selectedId);
        cmdManager->generateThrottleIncreaseCommand(selectedId);
        rightUpProcessed = true;
    }
    else if (rightY > -JOYSTICK_THRESHOLD / 2)
    {
        rightUpProcessed = false; // Reset
    }

    // Joystick DROIT vers le BAS : CMD_THROTTLE_DECREASE
    if (rightY > JOYSTICK_THRESHOLD && !rightDownProcessed)
    {
        Logger::logf("\n[JS-R] Joystick DROIT vers le BAS - THROTTLE_DECREASE (Bouee #%d)", selectedId);
        cmdManager->generateThrottleDecreaseCommand(selectedId);
        rightDownProcessed = true;
    }
    else if (rightY < JOYSTICK_THRESHOLD / 2)
    {
        rightDownProcessed = false; // Reset
    }

    // Joystick DROIT vers la DROITE : CMD_HEADING_INCREASE
    if (rightX > JOYSTICK_THRESHOLD && !rightRightProcessed)
    {
        Logger::logf("\n[JS-R] Joystick DROIT vers la DROITE - HEADING_INCREASE (Bouee #%d)", selectedId);
        cmdManager->generateHeadingIncreaseCommand(selectedId);
        rightRightProcessed = true;
    }
    else if (rightX < JOYSTICK_THRESHOLD / 2)
    {
        rightRightProcessed = false; // Reset
    }

    // Joystick DROIT vers la GAUCHE : CMD_HEADING_DECREASE
    if (rightX < -JOYSTICK_THRESHOLD && !rightLeftProcessed)
    {
        Logger::logf("\n[JS-R] Joystick DROIT vers la GAUCHE - HEADING_DECREASE (Bouee #%d)", selectedId);
        cmdManager->generateHeadingDecreaseCommand(selectedId);
        rightLeftProcessed = true;
    }
    else if (rightX > -JOYSTICK_THRESHOLD / 2)
    {
        rightLeftProcessed = false; // Reset
    }

    // CMD_NAV_STOP
    if (joystick.wasButtonPressed(BTN_ACTION_NAV_STOP))
    {
        Logger::logf("\n%s presse - NAV_STOP (Bouee #%d)", BTN_LABEL_NAV_STOP, selectedId);
        cmdManager->generateNavStopCommand(selectedId);
    }

    // Bouton A de l'appareil (écran AtomS3 en v1, BtnA Core2 en v2) : bouée suivante
    if (joystick.wasAtomScreenPressed())
    {
        Logger::log("\n[BTN-A] Bouton A presse - bouee suivante");
        buoyState->selectNextBuoy();
        joystick.setBuoySelectionLed(buoyState->getSelectedBuoyId());
        display->displayBuoySelection();
    }

    // ========================================================================
    // 2. MISE À JOUR COMMUNICATION ET ÉTAT DES BOUÉES
    // ========================================================================
    
    // Mode ESP-NOW : Réception via callbacks (pas besoin d'appel explicite)
    // Mode LoRa : La réception est gérée par loraRxTask() sur Core 0 (non-bloquant)
    
    // Traiter les retry de commandes en attente d'ACK
    if (CommunicationConfig::isLoRa(COMM_MODE)) {
        lora.processCommandRetries();
        // Bilan de liaison toutes les 5 s — instrument d'essai de portee.
        // S'auto-cadence, sans effet hors LoRa.
        // La bouee active est passee au bilan : un changement cloture le palier
        // en cours et remet les compteurs a zero (campagne multi-bouees).
        lora.logLinkQuality(buoyState->getSelectedBuoyId());
    } else if (COMM_MODE == CommMode::ESP_NOW) {
        espNow.processCommandRetries();
    }
    
    // Mise à jour de l'état des bouées
    buoyState->update();
    
    // ========================================================================
    // 3. ENVOI HEARTBEAT PÉRIODIQUE (toutes les 5 secondes)
    // ========================================================================
    if (currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
        lastHeartbeatTime = currentTime;
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        // Envoyer heartbeat uniquement à la bouée sélectionnée
        cmdManager->generateHeartbeatCommand(selectedId);
        Logger::logf("💓 Heartbeat envoyé à Bouée #%d", selectedId);
    } 

    // ========================================================================
    // 3 bis. DEMANDE LENTE DE LA TRAME OBSERVABLE (LoRa uniquement)
    // ========================================================================
    // Décalée d'une demi-période par rapport au heartbeat pour ne pas enchaîner
    // deux émissions dans la même seconde.
    if (CommunicationConfig::isLoRa(COMM_MODE) &&
        currentTime - lastObservableTime >= OBSERVABLE_INTERVAL &&
        currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL / 2) {
        lastObservableTime = currentTime;
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        cmdManager->generateObservableCommand(selectedId);
        Logger::logf("🌡️  Demande OBSERVABLE envoyée à Bouée #%d", selectedId);
    }
    
    // ========================================================================
    // 4. MISE À JOUR AFFICHAGE
    // ========================================================================
    display->update();
    
    // ========================================================================
    // 5. DEBUG SÉRIE (toutes les 2 secondes)
    // ========================================================================
#if DEBUG_SYSTEM_STATE
    static uint32_t lastDebug = 0;
    if (currentTime - lastDebug > 2000) {
        lastDebug = currentTime;
        
        Logger::log("\n--- Etat systeme ---");
        
        // Joystick
        Logger::logf("Joystick L: X=%d Y=%d",
                     joystick.getAxisCentered(AXIS_LEFT_X),
                     joystick.getAxisCentered(AXIS_LEFT_Y));
        Logger::logf("Joystick R: X=%d Y=%d",
                     joystick.getAxisCentered(AXIS_RIGHT_X),
                     joystick.getAxisCentered(AXIS_RIGHT_Y));
        Logger::logf("Batteries: %.2fV / %.2fV",
                     joystick.getBattery1Voltage(),
                     joystick.getBattery2Voltage());
#if JOYSTICK_HW == 2
        Logger::logf("DualBtn: GPIO%d=%d GPIO%d=%d | ByteBtn mask=0x%02X",
                     DUAL_BUTTON_1_GPIO, digitalRead(DUAL_BUTTON_1_GPIO),
                     DUAL_BUTTON_2_GPIO, digitalRead(DUAL_BUTTON_2_GPIO),
                     joystick.getByteButtonMask());
#endif
        
        // Bouées - Affichage simplifié
        uint8_t selectedId = buoyState->getSelectedBuoyId();
        Logger::logf("Bouee selectionnee: #%d", selectedId);
        
        // Afficher les données si la bouée répond (mode-dépendant)
        BuoyInfo* buoyInfo = nullptr;
        if (COMM_MODE == CommMode::ESP_NOW) {
            buoyInfo = espNow.getBuoyInfo(selectedId);
        } else {
            buoyInfo = lora.getBuoyInfo(selectedId);
        }
        
        // En mode LoRa, vérifier aussi les données ESP-NOW (plus récentes ?)
        // Sans écoute passive, cette source n'existe pas : ne pas l'interroger,
        // sinon l'affichage retomberait sur des données jamais rafraîchies.
        BuoyInfo* espNowInfo = nullptr;
#if JOYSTICK_ESPNOW_PASSIVE
        if (CommunicationConfig::isLoRa(COMM_MODE)) {
            espNowInfo = espNow.getBuoyInfo(selectedId);
        }
#endif
        
        // Choisir la source la plus récente
        if (espNowInfo != nullptr && espNowInfo->registered && 
            espNowInfo->lastUpdateTime != UINT32_MAX) {
            bool useEspNow = false;
            if (buoyInfo == nullptr || !buoyInfo->registered || 
                buoyInfo->lastUpdateTime == UINT32_MAX) {
                useEspNow = true;
            } else if (espNowInfo->lastUpdateTime > buoyInfo->lastUpdateTime) {
                useEspNow = true;
            }
            if (useEspNow) {
                buoyInfo = espNowInfo;
            }
        }
        
        if (buoyInfo != nullptr && buoyInfo->lastUpdateTime > 0) {  
            BuoyState state = buoyInfo->lastState;
            uint32_t age = millis() - buoyInfo->lastUpdateTime;  // Utiliser timestamp LOCAL
            Logger::logf("  Donnees recues il y a %lu ms", age);
            Logger::logf("  General Mode: %s", buoyState->getGeneralModeName(state.generalMode).c_str());
            Logger::logf("  Nav Mode: %s", buoyState->getNavModeName(state.navigationMode).c_str());
            Logger::logf("  Heading: %.0f deg Throttle: %d%%", 
                         state.autoPilotTrueHeadingCmde, 
                         state.autoPilotThrottleCmde);
            uint8_t batteryPercent = (uint8_t)((state.remainingCapacity / 10000.0) * 100);
            Logger::logf("  GPS: %s Battery: %d%% Temp: %.1fC",
                         state.gpsOk ? "OK" : "NO",
                         batteryPercent,
                         state.temperature);
        } else {
            Logger::log("  Aucune donnee recue (bouee inactive ou hors portee)");
        }
        
        Logger::log("-------------------\n");
    }
#endif  // DEBUG_SYSTEM_STATE
}
