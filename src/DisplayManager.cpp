/**
 * @file DisplayManager.cpp
 * @brief Implémentation de la gestion de l'affichage LCD
 * @author Philippe Hubert
 * @date 2025
 */

#include "DisplayManager.h"
#include "Logger.h"
#include "HardwareConfig.h"

/**
 * Convertit RGB565 pour compenser la permutation de l'écran AtomS3
 * L'écran AtomS3 fait: R→G, G→B, B→R
 * Pour afficher la bonne couleur, on doit faire la permutation inverse: R→B, G→R, B→G
 */
uint16_t DisplayManager::swapColorChannels(uint16_t rgb565) {
    // Extraire les canaux RGB
    uint8_t r = (rgb565 >> 11) & 0x1F;  // 5 bits rouge
    uint8_t g = (rgb565 >> 5) & 0x3F;   // 6 bits vert
    uint8_t b = rgb565 & 0x1F;          // 5 bits bleu
    
    // Permutation inverse: R→B, G→R, B→G
    // Pour afficher rouge: mettre valeur dans canal bleu
    // Pour afficher vert: mettre valeur dans canal rouge  
    // Pour afficher bleu: mettre valeur dans canal vert
    uint8_t new_r = (g >> 1);  // Vert(6 bits) → Rouge(5 bits), diviser par 2
    uint8_t new_g = (b << 1);  // Bleu(5 bits) → Vert(6 bits), multiplier par 2
    uint8_t new_b = r;         // Rouge(5 bits) → Bleu(5 bits)
    
    return (new_r << 11) | (new_g << 5) | new_b;
}

DisplayManager::DisplayManager(BuoyStateManager& buoyManager)
    : buoyMgr(buoyManager) {
    displayEnabled = true;
    lastUpdateTime = 0;
    currentBrightness = DEFAULT_BRIGHTNESS;
    commandStatus = CommandStatus::IDLE;
    commandStatusTime = 0;
    showingBuoySelection = false;
    buoySelectionTime = 0;
}

bool DisplayManager::begin() {
    M5.begin();
    M5.Display.setRotation(1);          // Paysage 320x240
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);

    // Écran de démarrage adapté 320x240
    const int16_t cx = DISPLAY_WIDTH / 2;
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("OpenSailingRC", cx, 80);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("Joystick v2  -  Core2", cx, 120);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString("Demarrage...", cx, 150);

    delay(1500);
    M5.Display.fillScreen(TFT_BLACK);

    Logger::log("✓ DisplayManager: Initialisé (Core2 320x240)");
    return true;
}

void DisplayManager::update() {
    if (!displayEnabled) {
        return;
    }
    
    uint32_t currentTime = millis();
    
    // Gérer l'overlay de sélection de bouée (non-bloquant)
    if (showingBuoySelection) {
        if (currentTime - buoySelectionTime >= BUOY_SELECTION_DURATION) {
            showingBuoySelection = false;
            // Forcer un redraw complet après la sélection
            cache.firstUpdate = true;
            lastUpdateTime = 0;
        } else {
            return;  // Ne pas redessiner pendant l'affichage de la sélection
        }
    }
    
    // Vérifier si de nouvelles données sont disponibles
    bool hasNewData = buoyMgr.hasNewData();
    if (hasNewData) {
        buoyMgr.clearNewData();  // Effacer le flag
        lastUpdateTime = 0;      // Forcer mise à jour immédiate
    }
    
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
        lastUpdateTime = currentTime;
        displayMainScreen();
    }
}

void DisplayManager::displayMainScreen() {
    BuoyState state = buoyMgr.getSelectedBuoyState();
    uint8_t buoyId = buoyMgr.getSelectedBuoyId();
    bool connected = buoyMgr.isSelectedBuoyConnected();
    
    // Premier affichage ou changement de bouée : effacer tout l'écran
    bool forceUpdate = false;
    if (cache.firstUpdate || cache.buoyId != buoyId) {
        M5.Display.fillScreen(TFT_BLACK);
        cache.firstUpdate = false;
        cache.buoyId = buoyId;
        cache.connected = !connected;  // Force update
        forceUpdate = true;
    }
    
    // Détection de la source de données active
    bool usingESPNow = buoyMgr.isUsingESPNowData();
    
    // Header avec nom de la bouée (couleur selon connexion et source)
    // Toujours redessiner si un statut de commande est actif ou si la source change
    uint32_t currentTime = millis();
    bool hasActiveCommandStatus = (commandStatus != CommandStatus::IDLE && 
                                     (currentTime - commandStatusTime) < STATUS_DISPLAY_DURATION);
    
    if (cache.connected != connected || cache.usingESPNow != usingESPNow || 
        forceUpdate || hasActiveCommandStatus) {
        drawHeader(connected, usingESPNow);
        if (!hasActiveCommandStatus) {
            cache.connected = connected;
        }
        cache.usingESPNow = usingESPNow;
    }
    
    if (connected) {
        // Calcul batterie pour comparaison
        uint8_t batteryPercent = (state.remainingCapacity / 3000.0) * 100;
        if (batteryPercent > 100) batteryPercent = 100;
        
        // Indicateurs LED des capteurs (ligne 2)
        if (cache.gpsOk != state.gpsOk || cache.headingOk != state.headingOk || 
            cache.yawRateOk != state.yawRateOk || forceUpdate) {
            drawSensorLEDs(state);
            cache.gpsOk = state.gpsOk;
            cache.headingOk = state.headingOk;
            cache.yawRateOk = state.yawRateOk;
        }
        
        // Température et Batterie (ligne 3)
        if (cache.temperature != state.temperature || cache.batteryPercent != batteryPercent || forceUpdate) {
            drawTempBattery(state);
            cache.temperature = state.temperature;
            cache.batteryPercent = batteryPercent;
        }
        
        // Modes général et navigation (ligne 4-5)
        if (cache.generalMode != state.generalMode || cache.navigationMode != state.navigationMode || forceUpdate) {
            drawNavigationState(state);
            cache.generalMode = state.generalMode;
            cache.navigationMode = state.navigationMode;
        }
        
        // Distance to consigne, Heading et Throttle (ligne 6)
        if (cache.distanceToCons != state.distanceToCons || 
            cache.autoPilotTrueHeadingCmde != state.autoPilotTrueHeadingCmde ||
            cache.autoPilotThrottleCmde != state.autoPilotThrottleCmde || forceUpdate) {
            drawDistanceThrottle(state);
            cache.distanceToCons = state.distanceToCons;
            cache.autoPilotTrueHeadingCmde = state.autoPilotTrueHeadingCmde;
            cache.autoPilotThrottleCmde = state.autoPilotThrottleCmde;
        }
    } else {
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.setFont(&fonts::Font4);  // Police plus grande (était Font2)
        M5.Display.drawString("Waiting...", 64, 64);
    }
}

void DisplayManager::drawHeader(bool connected, bool usingESPNow) {
    uint8_t buoyId = buoyMgr.getSelectedBuoyId();
    String buoyName = buoyMgr.getBuoyName(buoyId);
    
    Logger::logf("🎨 drawHeader: connected=%d, usingESPNow=%d, buoyId=%d", connected, usingESPNow, buoyId);
    
    // Effacer la zone du header
    M5.Display.fillRect(0, 0, 128, 20, TFT_BLACK);
    
    M5.Display.setTextDatum(TC_DATUM);
    
    // Déterminer la couleur selon l'état de la commande et la connexion
    uint32_t color;
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - commandStatusTime;
    
    // Vérifier si le statut de commande est récent (moins de 3 secondes)
    bool showCommandStatus = (elapsed < STATUS_DISPLAY_DURATION) && (commandStatus != CommandStatus::IDLE);
    
    if (showCommandStatus) {
        // Afficher l'état de la commande en priorité
        switch (commandStatus) {
            case CommandStatus::SENDING:
                M5.Display.setTextColor(TFT_BLUE, TFT_BLACK);
                break;
            case CommandStatus::ACK_RECEIVED:
                M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                break;
            case CommandStatus::TIMEOUT:
                M5.Display.setTextColor(TFT_RED, TFT_BLACK);
                break;
            default:
                M5.Display.setTextColor(connected ? TFT_GREEN : TFT_RED, TFT_BLACK);
                break;
        }
    } else {
        // Afficher l'état de connexion normal
        // Cyan = données ESP-NOW actives, Vert = LoRa seulement, Rouge = déconnecté
        if (connected) {
            if (usingESPNow) {
                M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            } else {
                M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
            }
        } else {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        }
        
        // Réinitialiser le statut après 3 secondes
        if (commandStatus != CommandStatus::IDLE) {
            Logger::logf("   drawHeader: Réinitialisation status IDLE (elapsed=%lu)", elapsed);
            commandStatus = CommandStatus::IDLE;
        }
    }
    
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString(buoyName, 64, 2);
}

void DisplayManager::drawSensorLEDs(const BuoyState& state) {
    // Ligne 2 : Indicateurs LED des capteurs
    const int16_t y = 32;  // Position Y sous le header (ajusté pour Font4)
    const int16_t ledRadius = 5;  // Rayon de la LED agrandi
    const int16_t spacing = 42;   // Espacement entre les LEDs
    
    // Effacer la zone des LEDs
    M5.Display.fillRect(0, 22, 128, 35, TFT_BLACK);
    
    // Centre de l'écran (128 pixels / 2 = 64)
    // 3 LEDs espacées : GPS, MAG, YAW
    const int16_t startX = 64 - spacing;  // Position de la première LED
    
    M5.Display.setFont(&fonts::Font2);  // Police plus grande (était Font0)
    M5.Display.setTextDatum(TC_DATUM);
    
    // GPS LED
    int16_t gpsX = startX;
    M5.Display.fillCircle(gpsX, y, ledRadius, state.gpsOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("GPS", gpsX, y + 8);
    
    // MAG (Heading) LED
    int16_t magX = startX + spacing;
    M5.Display.fillCircle(magX, y, ledRadius, state.headingOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("MAG", magX, y + 8);
    
    // YAW (YawRate) LED
    int16_t yawX = startX + spacing * 2;
    M5.Display.fillCircle(yawX, y, ledRadius, state.yawRateOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("YAW", yawX, y + 8);
}

void DisplayManager::drawTempBattery(const BuoyState& state) {
    // Ligne 3 : Température et % Batterie
    const int16_t y = 58;  // Ajusté pour nouvelle position
    
    // Effacer la zone température/batterie
    M5.Display.fillRect(0, 57, 128, 17, TFT_BLACK);
    
    M5.Display.setFont(&fonts::Font2);  // Police plus grande (était Font0)
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    
    // Température à gauche
    char tempBuffer[16];
    snprintf(tempBuffer, sizeof(tempBuffer), "%d%C", state.temperature);
    M5.Display.drawString(tempBuffer, 2, y);
    
    // Batterie à droite (conversion de mAh en %)
    uint8_t batteryPercent = (uint8_t)((state.remainingCapacity));
    if (batteryPercent > 100) batteryPercent = 100;
    
    char battBuffer[16];
    snprintf(battBuffer, sizeof(battBuffer), "%d%%", batteryPercent);
    M5.Display.setTextDatum(TR_DATUM);
    M5.Display.drawString(battBuffer, 126, y);
}

void DisplayManager::drawDistanceThrottle(const BuoyState& state) {
    // Ligne 6 : Distance, Forced Heading et Throttle
    const int16_t y = 114;  // Ajusté pour nouvelle position
    
    // Effacer la zone distance/heading/throttle
    M5.Display.fillRect(0, 113, 128, 15, TFT_BLACK);
    
    M5.Display.setFont(&fonts::Font2);  // Police plus grande (était Font0)
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Distance à gauche
    char distBuffer[16];
    M5.Display.setTextDatum(TL_DATUM);
    if (state.distanceToCons < 1000) {
        snprintf(distBuffer, sizeof(distBuffer), "%.0fm", state.distanceToCons);
    } else {
        snprintf(distBuffer, sizeof(distBuffer), "%.1fk", state.distanceToCons / 1000.0);
    }
    M5.Display.drawString(distBuffer, 2, y);
    
    // Autopilot Heading au centre
    char headingBuffer[16];
    snprintf(headingBuffer, sizeof(headingBuffer), "%.0fd", state.autoPilotTrueHeadingCmde);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.drawString(headingBuffer, 64, y);
    
    // Throttle à droite
    char throttleBuffer[16];
    snprintf(throttleBuffer, sizeof(throttleBuffer), "%d%%", state.autoPilotThrottleCmde);
    M5.Display.setTextDatum(TR_DATUM);
    M5.Display.drawString(throttleBuffer, 126, y);
}

void DisplayManager::drawNavigationState(const BuoyState& state) {
    // Effacer la zone des modes
    M5.Display.fillRect(0, 74, 128, 36, TFT_BLACK);
    
    // Mode général (ligne 4)
    String generalModeName = buoyMgr.getGeneralModeName(state.generalMode);
    uint16_t generalColor = getGeneralModeColor(state.generalMode);
    
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(generalColor, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);  // Police moyenne
    M5.Display.drawString(generalModeName, 64, 80);  // Descendu de 2 pixels (76 → 78)
    
    // Mode de navigation (ligne 5, plus grand pour meilleure visibilité)
    String navModeName = buoyMgr.getNavModeName(state.navigationMode);
    uint16_t navModeColor = getNavModeColor(state.navigationMode);
    
    M5.Display.setTextColor(navModeColor, TFT_BLACK);
    M5.Display.setFont(&fonts::Font4);  // Police plus grande pour NavigationMode
    M5.Display.drawString(navModeName, 64, 100);  // Descendu de 2 pixels (96 → 98)
}

void DisplayManager::drawHeadingSpeed(float heading, float speed) {
    char buffer[32];
    
    // Cap
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    sprintf(buffer, "Hdg: %03d", (int)heading);
    M5.Display.drawString(buffer, 64, 65);
    
    // Vitesse
    sprintf(buffer, "%.1f m/s", speed);
    M5.Display.drawString(buffer, 64, 83);
}

void DisplayManager::drawBattery(uint8_t batteryLevel, int16_t x, int16_t y) {
    uint16_t color = getBatteryColor(batteryLevel);
    
    // Dessine icône de batterie
    M5.Display.drawRect(x, y, 30, 15, TFT_WHITE);
    M5.Display.fillRect(x + 30, y + 5, 3, 5, TFT_WHITE);
    
    // Remplit selon le niveau
    int fillWidth = (batteryLevel * 26) / 100;
    if (fillWidth > 26) fillWidth = 26;
    M5.Display.fillRect(x + 2, y + 2, fillWidth, 11, color);
    
    // Affiche pourcentage
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font0);
    char buffer[8];
    sprintf(buffer, "%d%%", batteryLevel);
    M5.Display.drawString(buffer, x, y + 16);
}

void DisplayManager::drawGPS(bool locked, int16_t x, int16_t y) {
    uint16_t color = locked ? TFT_GREEN : TFT_RED;
    
    // Dessine icône GPS
    M5.Display.fillCircle(x + 5, y + 5, 5, color);
    M5.Display.drawCircle(x + 5, y + 5, 8, color);
    M5.Display.drawCircle(x + 5, y + 5, 11, color);
    
    // Texte
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString(locked ? "GPS" : "NO", x + 5, y + 16);
}

void DisplayManager::drawSignal(int8_t signalQuality, int16_t x, int16_t y) {
    if (signalQuality < 0) {
        M5.Display.setTextDatum(TL_DATUM);
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.drawString("--", x, y);
        return;
    }
    
    // Calcule le nombre de barres (0-3)
    uint8_t bars = 0;
    if (signalQuality > 20) bars = 3;
    else if (signalQuality > 10) bars = 2;
    else if (signalQuality > 5) bars = 1;
    
    uint16_t color = (bars >= 2) ? TFT_GREEN : (bars == 1) ? TFT_YELLOW : TFT_RED;
    
    // Dessine les barres
    for (int i = 0; i < 3; i++) {
        int height = (i + 1) * 4;
        uint16_t barColor = (i < bars) ? color : TFT_DARKGREY;
        M5.Display.fillRect(x + i * 5, y + 12 - height, 3, height, barColor);
    }
    
    // Texte
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString("LTE", x + 7, y + 16);
}

void DisplayManager::displayError(const String& message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("ERROR", 64, 40);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString(message, 64, 70);
}

void DisplayManager::displayConnecting(const String& message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("CONNECTING", 64, 40);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString(message, 64, 70);
}

void DisplayManager::displayBuoySelection() {
    uint8_t buoyId = buoyMgr.getSelectedBuoyId();
    String buoyName = buoyMgr.getBuoyName(buoyId);
    
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("SELECTED", 64, 40);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString(buoyName, 64, 70);
    
    // Non-bloquant : l'overlay sera effacé par update() après BUOY_SELECTION_DURATION
    showingBuoySelection = true;
    buoySelectionTime = millis();
}

void DisplayManager::setEnabled(bool enabled) {
    displayEnabled = enabled;
    if (!enabled) {
        M5.Display.fillScreen(TFT_BLACK);
    }
}

void DisplayManager::setBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    M5.Display.setBrightness(brightness);
}

void DisplayManager::setCommandStatus(CommandStatus status) {
    // IMPORTANT: Cette méthode peut être appelée depuis le callback ESP-NOW
    // (tâche WiFi, Core 0). NE JAMAIS faire d'opérations SPI/Display ici
    // car le bus SPI est utilisé par update()/displayMainScreen() sur Core 1.
    // Seuls les flags sont mis à jour, le rendu est fait par update().
    commandStatus = status;
    commandStatusTime = millis();
    
    // Forcer le prochain cycle update() à redessiner
    lastUpdateTime = 0;
}

uint16_t DisplayManager::getBatteryColor(uint8_t batteryLevel) {
    if (batteryLevel > 50) {
        return TFT_GREEN;
    } else if (batteryLevel > 20) {
        return TFT_ORANGE;
    } else {
        return TFT_RED;
    }
}

uint16_t DisplayManager::getNavModeColor(tEtatsNav mode) {
    switch (mode) {
        case NAV_CAP:
            return TFT_GREEN;
        case NAV_TARGET:
            return TFT_CYAN;
        case NAV_HOLD:
            return TFT_YELLOW;
        case NAV_HOME:
            return TFT_BLUE;
        case NAV_STOP:
            return TFT_RED;
        case NAV_BASIC:
            return TFT_ORANGE;
        case NAV_NOTHING:
        default:
            return TFT_WHITE;
    }
}

uint16_t DisplayManager::getGeneralModeColor(tEtatsGeneral mode) {
    switch (mode) {
        case INIT:
            return TFT_YELLOW;
        case READY:
            return TFT_CYAN;
        case MAINTENANCE:
            return TFT_ORANGE;
        case HOME_DEFINITION:
            return TFT_MAGENTA;
        case NAV:
            return TFT_GREEN;
        default:
            return TFT_WHITE;
    }
}

void DisplayManager::forceRefresh() {
    // Réinitialise le cache pour forcer un redraw complet
    cache.buoyId = 255;
    cache.connected = false;
    cache.firstUpdate = true;
    lastUpdateTime = 0;
}

void DisplayManager::displayBootDiagnostic(const HardwareDiagResult& d, uint32_t durationMs) {
    const int16_t W = DISPLAY_WIDTH;   // 320
    const int16_t H = DISPLAY_HEIGHT;  // 240
    const int16_t PAD = 8;
    const int16_t ROW_H = 28;

    M5.Display.fillScreen(TFT_BLACK);

    // ── Titre ────────────────────────────────────────────────────────────────
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("Diagnostic Hardware", W / 2, PAD);

    // ── Ligne I2C bus ─────────────────────────────────────────────────────────
    char buf[64];
    int16_t y = 40;
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "JS  SDA:%d SCL:%d @%lukHz",
             d.sdaPin, d.sclPin, d.freqHz / 1000);
    M5.Display.drawString(buf, PAD, y);
    snprintf(buf, sizeof(buf), "BTN SDA:%d SCL:%d @%lukHz",
             d.byteButtonSdaPin, d.byteButtonSclPin, d.freqHz / 1000);
    M5.Display.drawString(buf, 165, y);

    // ── Séparateur ────────────────────────────────────────────────────────────
    y += 16;
    M5.Display.drawFastHLine(PAD, y, W - PAD * 2, TFT_DARKGREY);
    y += 6;

    // ── Fonction helper : dessine une ligne de diagnostic ────────────────────
    // Colonnes : [●] LABEL      addr     [ OK / KO ]
    auto drawRow = [&](const char* label, bool found, uint8_t addr, uint8_t scanned,
                       bool isGpio, uint8_t gpio1, uint8_t gpio2) {
        uint16_t dotColor = found ? TFT_GREEN : TFT_RED;

        // Pastille colorée
        M5.Display.fillCircle(PAD + 6, y + 10, 6, dotColor);

        // Label
        M5.Display.setFont(&fonts::Font2);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setTextDatum(TL_DATUM);
        M5.Display.drawString(label, PAD + 18, y + 3);

        if (!isGpio) {
            // Adresse attendue
            snprintf(buf, sizeof(buf), "0x%02X", addr);
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.setTextDatum(TL_DATUM);
            M5.Display.drawString(buf, 172, y + 3);

            // Adresse détectée
            if (found) {
                snprintf(buf, sizeof(buf), "->0x%02X", scanned);
                M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            } else {
                snprintf(buf, sizeof(buf), "->---");
                M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
            }
            M5.Display.setFont(&fonts::Font0);
            M5.Display.drawString(buf, 210, y + 6);
            M5.Display.setFont(&fonts::Font2);
        } else {
            snprintf(buf, sizeof(buf), "GPIO %d / %d", gpio1, gpio2);
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.drawString(buf, 172, y + 3);
        }

        // Badge OK / KO
        uint16_t badgeColor = found ? TFT_GREEN : TFT_RED;
        M5.Display.fillRoundRect(W - PAD - 40, y + 2, 38, 18, 4, badgeColor);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.setTextColor(TFT_BLACK, badgeColor);
        M5.Display.drawString(found ? "OK" : "KO", W - PAD - 21, y + 11);

        y += ROW_H;
    };

    drawRow("Joystick G",  d.joystickLeftFound,  d.joystickLeftAddr,  d.joystickLeftAddrScanned,  false, 0, 0);
    drawRow("Joystick D",  d.joystickRightFound, d.joystickRightAddr, d.joystickRightAddrScanned, false, 0, 0);
    drawRow("ByteButton",  d.byteButtonFound,    d.byteButtonAddr,    d.byteButtonAddrScanned,    false, 0, 0);
    drawRow("Dual Button", d.dualBtn1Reachable && d.dualBtn2Reachable,
            0, 0, true, d.dualBtn1Gpio, d.dualBtn2Gpio);

    // Résumé du scan bus I2C (ligne dédiée pour éviter toute confusion par module)
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    if (d.scannedCount == 0) {
        snprintf(buf, sizeof(buf), "Scan bus: aucun module detecte");
    } else if (d.scannedCount == 1) {
        snprintf(buf, sizeof(buf), "Scan bus: %02X", d.scannedAddrs[0]);
    } else {
        snprintf(buf, sizeof(buf), "Scan bus: %02X,%02X", d.scannedAddrs[0], d.scannedAddrs[1]);
    }
    M5.Display.drawString(buf, PAD, y - 8);

    // ── Séparateur ────────────────────────────────────────────────────────────
    M5.Display.drawFastHLine(PAD, y + 2, W - PAD * 2, TFT_DARKGREY);
    y += 10;

    // ── Bannière GO / ATTENTION ───────────────────────────────────────────────
    if (d.allCriticalOk) {
        M5.Display.fillRoundRect(PAD, y, W - PAD * 2, 30, 6, TFT_GREEN);
        M5.Display.setFont(&fonts::Font4);
        M5.Display.setTextColor(TFT_BLACK, TFT_GREEN);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("SYSTEME PRET", W / 2, y + 15);
    } else {
        M5.Display.fillRoundRect(PAD, y, W - PAD * 2, 30, 6, TFT_ORANGE);
        M5.Display.setFont(&fonts::Font4);
        M5.Display.setTextColor(TFT_BLACK, TFT_ORANGE);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("VERIFIER MODULES", W / 2, y + 15);
    }

    // ── Attente durée ou appui BtnA ───────────────────────────────────────────
    if (durationMs > 0) {
        delay(durationMs);
    } else {
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.setTextDatum(BC_DATUM);
        M5.Display.drawString("Appuyer sur A pour continuer", W / 2, H - 4);
        while (true) {
            M5.update();
            if (M5.BtnA.wasPressed()) break;
            delay(10);
        }
    }

    // Efface et prépare l'écran principal
    M5.Display.fillScreen(TFT_BLACK);
    cache.firstUpdate = true;
}
