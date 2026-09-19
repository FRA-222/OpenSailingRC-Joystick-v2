/**
 * @file DisplayManagerV1.cpp
 * @brief Affichage du joystick v1 sur l'écran 128x128 de l'AtomS3 (repris de OpenSailingRC-BuoyJoystick)
 * @author Philippe Hubert
 * @date 2025-2026
 *
 * Implémente l'interface commune DisplayManager.h pour le matériel v1.
 * Compilé uniquement dans l'environnement joystick-v1-atoms3 (platformio.ini).
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "DisplayManager.h"
#include "Logger.h"
#include "HardwareConfig.h"
#include "JoystickConfiguration.h"

#if JOYSTICK_HW != 1
#error "DisplayManagerV1.cpp ne se compile que pour le matériel v1 (JOYSTICK_HW=1)"
#endif

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

DisplayManager::DisplayManager(BuoyStateManager& buoyManager, const char* firmwareVersion)
        : buoyMgr(buoyManager),
            firmwareVersion((firmwareVersion != nullptr) ? firmwareVersion : "1.0.0") {
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
    M5.Display.setRotation(0);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);
    
    // Affiche écran de démarrage
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString("OpenSailingRC", 64, 40);
    M5.Display.drawString(JoystickConfiguration::productName(), 64, 60);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.drawString("v" + firmwareVersion, 64, 90);
    
    delay(2000);
    
    M5.Display.fillScreen(TFT_BLACK);
    
    Logger::log("✓ DisplayManager: Initialisé (AtomS3 128x128)");
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
        invalidateCachedFields();
        if (connected) {
            drawStaticLayout();
        }
        cache.firstUpdate = false;
        cache.buoyId = buoyId;
        forceUpdate = true;
    }
    
    // Détection de la source de données active
    bool usingESPNow = buoyMgr.isUsingESPNowData();
    
    // Changement d'état de connexion : le contenu sous le header change
    // complètement (données <-> message d'attente)
    if (cache.connected != connected && !forceUpdate) {
        M5.Display.fillRect(0, 20, 128, 108, TFT_BLACK);
        invalidateCachedFields();
        if (connected) {
            drawStaticLayout();
        }
        forceUpdate = true;
    }

    drawHeader(connected, usingESPNow);
    // Toujours mémoriser l'état : sinon, tant qu'un statut de commande est actif,
    // le header serait redessiné à chaque cycle.
    cache.connected = connected;
    cache.usingESPNow = usingESPNow;

    if (connected) {
        // Chaque zone ne repeint que les champs dont la valeur affichée a changé
        drawSensorLEDs(state, forceUpdate);
        drawTempBattery(state, forceUpdate);
        drawNavigationState(state, forceUpdate);
        drawDistanceThrottle(state, forceUpdate);
    } else if (forceUpdate) {
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.setFont(&fonts::Font4);  // Police plus grande (était Font2)
        M5.Display.drawString("Waiting...", 64, 64);
    }
}

void DisplayManager::drawStaticLayout() {
    // Libellés des LEDs capteurs : dessinés une seule fois par redraw complet
    const int16_t spacing = 42;
    const int16_t startX = 64 - spacing;

    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("GPS", startX, 40);
    M5.Display.drawString("MAG", startX + spacing, 40);
    M5.Display.drawString("YAW", startX + spacing * 2, 40);
}

void DisplayManager::invalidateCachedFields() {
    cache.joystickBattery.valid = false;
    cache.sourceTag.valid = false;
    cache.batteryIcon = -1;
    cache.buoyName.valid = false;
    cache.temperature.valid = false;
    cache.battery.valid = false;
    cache.generalMode.valid = false;
    cache.navMode.valid = false;
    cache.distance.valid = false;
    cache.heading.valid = false;
    cache.throttle.valid = false;
    cache.gpsOk = -1;
    cache.headingOk = -1;
    cache.yawRateOk = -1;
}

bool DisplayManager::drawTextField(TextField& field, const char* text, uint16_t color,
                                   const m5gfx::IFont* font, float textSize, m5gfx::textdatum_t datum,
                                   int16_t x, int16_t y, uint16_t padWidth, bool force) {
    if (!force && field.valid && field.color == color && strcmp(field.text, text) == 0) {
        return false;  // Rien n'a changé : ne pas repeindre (source principale du flickering)
    }

    M5.Display.setFont(font);
    M5.Display.setTextSize(textSize);
    M5.Display.setTextDatum(datum);
    M5.Display.setTextColor(color, TFT_BLACK);
    // Le padding efface l'ancienne valeur en même temps que la nouvelle est
    // écrite : pas de fillRect suivi d'un dessin, donc pas de clignotement.
    M5.Display.setTextPadding(padWidth);
    M5.Display.drawString(text, x, y);
    M5.Display.setTextPadding(0);
    M5.Display.setTextSize(1);

    strncpy(field.text, text, sizeof(field.text) - 1);
    field.text[sizeof(field.text) - 1] = '\0';
    field.color = color;
    field.valid = true;
    return true;
}

void DisplayManager::drawHeader(bool connected, bool usingESPNow) {
    uint8_t buoyId = buoyMgr.getSelectedBuoyId();
    String buoyName = buoyMgr.getBuoyName(buoyId);
    
    // Déterminer la couleur selon l'état de la commande et la connexion
    uint16_t nameColor;
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - commandStatusTime;
    
    // Vérifier si le statut de commande est récent (moins de 3 secondes)
    bool showCommandStatus = (elapsed < STATUS_DISPLAY_DURATION) && (commandStatus != CommandStatus::IDLE);
    
    if (showCommandStatus) {
        // Afficher l'état de la commande en priorité
        switch (commandStatus) {
            case CommandStatus::SENDING:
                nameColor = TFT_BLUE;
                break;
            case CommandStatus::ACK_RECEIVED:
                nameColor = TFT_GREEN;
                break;
            case CommandStatus::TIMEOUT:
                nameColor = TFT_RED;
                break;
            default:
                nameColor = connected ? TFT_GREEN : TFT_RED;
                break;
        }
    } else {
        // Afficher l'état de connexion normal
        // Cyan = données ESP-NOW actives, Vert = LoRa seulement, Rouge = déconnecté
        if (connected) {
            nameColor = usingESPNow ? TFT_CYAN : TFT_GREEN;
        } else {
            nameColor = TFT_RED;
        }
        
        // Réinitialiser le statut après 3 secondes
        if (commandStatus != CommandStatus::IDLE) {
            Logger::logf("   drawHeader: Réinitialisation status IDLE (elapsed=%lu)", elapsed);
            commandStatus = CommandStatus::IDLE;
        }
    }
    
    // y=1 (au lieu de 2) : la zone effacée par le padding fait 26 px de haut et
    // viendrait sinon mordre sur le haut des pastilles LED (y=27).
    drawTextField(cache.buoyName, buoyName.c_str(), nameColor,
                  &fonts::Font4, 1.0f, TC_DATUM, 64, 1, 128);
}

void DisplayManager::drawSensorLEDs(const BuoyState& state, bool force) {
    // Ligne 2 : Indicateurs LED des capteurs. Les libellés sont statiques
    // (drawStaticLabels) : seules les pastilles dont l'état change sont repeintes.
    const int16_t y = 32;         // Position Y sous le header (ajusté pour Font4)
    const int16_t ledRadius = 5;  // Rayon de la LED agrandi
    const int16_t spacing = 42;   // Espacement entre les LEDs
    const int16_t startX = 64 - spacing;

    struct { bool ok; int8_t& cached; int16_t x; } leds[] = {
        {state.gpsOk,     cache.gpsOk,     startX},
        {state.headingOk, cache.headingOk, (int16_t)(startX + spacing)},
        {state.yawRateOk, cache.yawRateOk, (int16_t)(startX + spacing * 2)},
    };

    for (auto& led : leds) {
        if (!force && led.cached == (int8_t)led.ok) {
            continue;
        }
        M5.Display.fillCircle(led.x, y, ledRadius, led.ok ? TFT_GREEN : TFT_RED);
        led.cached = (int8_t)led.ok;
    }
}

void DisplayManager::drawTempBattery(const BuoyState& state, bool force) {
    // Ligne 3 : Température et % Batterie
    const int16_t y = 58;  // Ajusté pour nouvelle position

    // Température à gauche.
    // state.temperature est un float : "%d" lisait un argument entier inexistant
    // (et "%C" un second argument absent), d'où une valeur jamais affichée.
    char tempBuffer[16];
    snprintf(tempBuffer, sizeof(tempBuffer), "%.0f C", state.temperature);
    drawTextField(cache.temperature, tempBuffer, TFT_CYAN,
                  &fonts::Font2, 1.0f, TL_DATUM, 2, y, 60, force);

    // Batterie à droite (conversion de mAh en %)
    uint8_t batteryPercent = (uint8_t)((state.remainingCapacity));
    if (batteryPercent > 100) batteryPercent = 100;

    char battBuffer[16];
    snprintf(battBuffer, sizeof(battBuffer), "%d%%", batteryPercent);
    drawTextField(cache.battery, battBuffer, TFT_CYAN,
                  &fonts::Font2, 1.0f, TR_DATUM, 126, y, 60, force);
}

void DisplayManager::drawDistanceThrottle(const BuoyState& state, bool force) {
    // Ligne 6 : Distance, Forced Heading et Throttle
    const int16_t y = 114;  // Ajusté pour nouvelle position

    // Distance à gauche
    char distBuffer[16];
    if (state.distanceToCons < 1000) {
        snprintf(distBuffer, sizeof(distBuffer), "%.0fm", state.distanceToCons);
    } else {
        snprintf(distBuffer, sizeof(distBuffer), "%.1fk", state.distanceToCons / 1000.0);
    }
    drawTextField(cache.distance, distBuffer, TFT_WHITE,
                  &fonts::Font2, 1.0f, TL_DATUM, 2, y, 40, force);

    // Autopilot Heading au centre
    char headingBuffer[16];
    snprintf(headingBuffer, sizeof(headingBuffer), "%.0fd", state.autoPilotTrueHeadingCmde);
    drawTextField(cache.heading, headingBuffer, TFT_WHITE,
                  &fonts::Font2, 1.0f, TC_DATUM, 64, y, 40, force);

    // Throttle à droite
    char throttleBuffer[16];
    snprintf(throttleBuffer, sizeof(throttleBuffer), "%d%%", state.autoPilotThrottleCmde);
    drawTextField(cache.throttle, throttleBuffer, TFT_WHITE,
                  &fonts::Font2, 1.0f, TR_DATUM, 126, y, 40, force);
}

void DisplayManager::drawNavigationState(const BuoyState& state, bool force) {
    // Mode général (ligne 4)
    String generalModeName = buoyMgr.getGeneralModeName(state.generalMode);
    drawTextField(cache.generalMode, generalModeName.c_str(),
                  getGeneralModeColor(state.generalMode),
                  &fonts::Font2, 1.0f, MC_DATUM, 64, 80, 128, force);

    // Mode de navigation (ligne 5, plus grand pour meilleure visibilité)
    String navModeName = buoyMgr.getNavModeName(state.navigationMode);
    drawTextField(cache.navMode, navModeName.c_str(),
                  getNavModeColor(state.navigationMode),
                  &fonts::Font4, 1.0f, MC_DATUM, 64, 100, 128, force);
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

    forceRefresh();  // L'écran a été effacé : tout redessiner au prochain update()
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

    forceRefresh();  // L'écran a été effacé : tout redessiner au prochain update()
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
        forceRefresh();  // Repartir d'un écran complet à la réactivation
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
    invalidateCachedFields();
    lastUpdateTime = 0;
}

// ── Méthodes de l'interface commune sans équivalent v1 ──────────────────────

void DisplayManager::drawJoystickBattery(bool force) {
    (void)force;   // Les tensions batterie v1 sont tracées sur la console, pas affichées
}

void DisplayManager::displayBootDiagnostic(const HardwareDiagResult& d, uint32_t durationMs) {
    // Version compacte pour 128x128 : un seul esclave I2C (le STM32) porte
    // sticks, boutons et batteries.
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("Diagnostic", 64, 4);

    char buf[32];
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "I2C SDA:%d SCL:%d", d.sdaPin, d.sclPin);
    M5.Display.drawString(buf, 64, 26);

    uint16_t color = d.joystickLeftFound ? TFT_GREEN : TFT_RED;
    M5.Display.fillCircle(20, 52, 6, color);
    M5.Display.setTextDatum(ML_DATUM);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(buf, sizeof(buf), "STM32 0x%02X", d.joystickLeftAddr);
    M5.Display.drawString(buf, 34, 52);
    M5.Display.setTextDatum(MR_DATUM);
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.drawString(d.joystickLeftFound ? "OK" : "KO", 124, 70);

    M5.Display.setTextDatum(MC_DATUM);
    if (d.allCriticalOk) {
        M5.Display.fillRoundRect(6, 92, 116, 26, 5, TFT_GREEN);
        M5.Display.setTextColor(TFT_BLACK, TFT_GREEN);
        M5.Display.drawString("PRET", 64, 105);
    } else {
        M5.Display.fillRoundRect(6, 92, 116, 26, 5, TFT_ORANGE);
        M5.Display.setTextColor(TFT_BLACK, TFT_ORANGE);
        M5.Display.drawString("VERIFIER I2C", 64, 105);
    }

    if (durationMs > 0) {
        delay(durationMs);
    } else {
        while (true) {
            M5.update();
            if (M5.BtnA.wasPressed()) break;
            delay(10);
        }
    }

    M5.Display.fillScreen(TFT_BLACK);
    cache.firstUpdate = true;
}
