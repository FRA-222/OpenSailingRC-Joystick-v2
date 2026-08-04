/**
 * @file DisplayManager.cpp
 * @brief Implémentation de la gestion de l'affichage LCD
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

#include "DisplayManager.h"
#include "Logger.h"
#include "HardwareConfig.h"

/*
 * ── Mise en page de l'écran principal (Core2 320x240) ────────────────────────
 *
 *  y=0    ┌───────────────────────────────────────────────┐
 *         │ JS 87%      NOM DE LA BOUEE      [RX: LoRa]   │  Header
 *  y=34   ├───────────────────────────────────────────────┤
 *         │     (o)GPS         (o)MAG        (o)YAW       │  Capteurs
 *  y=86   ├───────────────────────────────────────────────┤
 *         │  23C                                    87%   │  Temp / Batterie
 *  y=114  ├───────────────────────────────────────────────┤
 *         │                    READY                      │  Mode general
 *         │                  NAV CAP                      │  Mode navigation
 *  y=186  ├───────────────────────────────────────────────┤
 *         │    DIST            CAP            THR         │  Consignes
 *         │    125m            270d           45%         │
 *  y=240  └───────────────────────────────────────────────┘
 */
namespace {
    constexpr int16_t SCR_W = DISPLAY_WIDTH;    // 320
    constexpr int16_t SCR_H = DISPLAY_HEIGHT;   // 240
    constexpr int16_t MARGIN = 8;

    // Origine verticale de chaque bande
    constexpr int16_t HEADER_H = 34;   // Hauteur du header (bande y=0)
    constexpr int16_t LED_Y    = 36;
    constexpr int16_t TEMP_Y   = 88;
    constexpr int16_t MODE_Y   = 116;
    constexpr int16_t CMD_Y    = 188;

    // Séparateurs (dessinés une fois par redraw complet)
    constexpr int16_t SEP1_Y = 34, SEP2_Y = 86, SEP3_Y = 114, SEP4_Y = 186;

    // Trois colonnes réparties sur la largeur (LEDs capteurs et consignes)
    constexpr int16_t COL_1 = SCR_W / 6;         // 53
    constexpr int16_t COL_2 = SCR_W / 2;         // 160
    constexpr int16_t COL_3 = (SCR_W * 5) / 6;   // 266

    // Icône batterie (contour statique, remplissage variable)
    constexpr int16_t BATT_ICON_X = SCR_W - MARGIN - 34;
    constexpr int16_t BATT_ICON_Y = TEMP_Y + 4;
}

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
      firmwareVersion((firmwareVersion != nullptr) ? firmwareVersion : "2.0.0") {
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
    M5.Display.drawString("v" + firmwareVersion, cx, 150);
    M5.Display.drawString("Demarrage...", cx, 175);

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
    
    bool connectionChanged = (cache.connected != connected);

    // Changement d'état de connexion : le contenu sous le header change
    // complètement (bandes de données <-> message d'attente)
    if (connectionChanged && !forceUpdate) {
        M5.Display.fillRect(0, HEADER_H, SCR_W, SCR_H - HEADER_H, TFT_BLACK);
        invalidateCachedFields();
        if (connected) {
            drawStaticLayout();
        }
        forceUpdate = true;
    }

    drawHeader(connected, usingESPNow);
    // Toujours mémoriser l'état : sinon, tant qu'un statut de commande est actif,
    // connectionChanged resterait vrai à chaque cycle et effacerait l'écran.
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
        M5.Display.setFont(&fonts::Font4);
        M5.Display.setTextSize(2);
        M5.Display.drawString("Waiting...", SCR_W / 2, SCR_H / 2);
        M5.Display.setTextSize(1);
    }
}

void DisplayManager::drawStaticLayout() {
    const int16_t x0 = MARGIN;
    const int16_t w = SCR_W - MARGIN * 2;
    M5.Display.drawFastHLine(x0, SEP1_Y, w, TFT_DARKGREY);
    M5.Display.drawFastHLine(x0, SEP2_Y, w, TFT_DARKGREY);
    M5.Display.drawFastHLine(x0, SEP3_Y, w, TFT_DARKGREY);
    M5.Display.drawFastHLine(x0, SEP4_Y, w, TFT_DARKGREY);

    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextDatum(TC_DATUM);

    // Libellés des LEDs capteurs
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("GPS", COL_1, LED_Y + 30);
    M5.Display.drawString("MAG", COL_2, LED_Y + 30);
    M5.Display.drawString("YAW", COL_3, LED_Y + 30);

    // Libellés des consignes
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.drawString("DIST", COL_1, CMD_Y + 2);
    M5.Display.drawString("CAP",  COL_2, CMD_Y + 2);
    M5.Display.drawString("THR",  COL_3, CMD_Y + 2);

    // Contour de l'icône batterie (le remplissage est mis à jour avec la valeur)
    M5.Display.drawRect(BATT_ICON_X, BATT_ICON_Y, 30, 15, TFT_WHITE);
    M5.Display.fillRect(BATT_ICON_X + 30, BATT_ICON_Y + 5, 3, 5, TFT_WHITE);
}

void DisplayManager::invalidateCachedFields() {
    cache.joystickBattery.valid = false;
    cache.buoyName.valid = false;
    cache.sourceTag.valid = false;
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
    cache.batteryIcon = -1;
}

bool DisplayManager::drawTextField(TextField& field, const char* text, uint16_t color,
                                   const m5gfx::IFont* font, uint8_t textSize, m5gfx::textdatum_t datum,
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
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - commandStatusTime;
    uint16_t nameColor;

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

    // Nom de la bouée centré, en grand
    bool nameRedrawn = drawTextField(cache.buoyName, buoyName.c_str(), nameColor,
                                     &fonts::Font4, 1, TC_DATUM, SCR_W / 2, 3, 140);

    // Badge de la source des données affichées (RX), aligné à droite. C'est bien
    // la provenance de la télémétrie et non le canal de commande : en mode LoRa,
    // l'ESP-NOW est écouté passivement et prioritaire s'il est plus frais.
    // Redessiné avec le nom : le padding du nom peut mordre sur le badge.
    const char* tag = !connected ? "RX: --" : (usingESPNow ? "RX: ESP-NOW" : "RX: LoRa");
    uint16_t tagColor = !connected ? TFT_RED : (usingESPNow ? TFT_CYAN : TFT_GREEN);
    drawTextField(cache.sourceTag, tag, tagColor,
                  &fonts::Font2, 1, TR_DATUM, SCR_W - MARGIN, 9, 100, nameRedrawn);

    // Batterie du joystick, en haut à gauche. Redessinée avec le nom pour la même
    // raison que le badge RX.
    drawJoystickBattery(nameRedrawn);
}

void DisplayManager::drawJoystickBattery(bool force) {
    int32_t level = M5.Power.getBatteryLevel();
    bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);

    char buffer[16];
    if (level < 0) {
        // Niveau non disponible sur ce matériel
        snprintf(buffer, sizeof(buffer), "JS --");
    } else if (charging) {
        snprintf(buffer, sizeof(buffer), "JS +%ld%%", (long)level);
    } else {
        snprintf(buffer, sizeof(buffer), "JS %ld%%", (long)level);
    }

    // En charge : cyan, sinon code couleur habituel du niveau
    uint16_t color = charging ? TFT_CYAN
                              : (level < 0 ? TFT_DARKGREY : getBatteryColor((uint8_t)level));

    drawTextField(cache.joystickBattery, buffer, color,
                  &fonts::Font2, 1, TL_DATUM, MARGIN, 9, 80, force);
}

void DisplayManager::drawSensorLEDs(const BuoyState& state, bool force) {
    // Bande capteurs : 3 LEDs réparties sur toute la largeur.
    // Les libellés sont statiques (drawStaticLayout) : seules les pastilles
    // dont l'état a changé sont repeintes.
    const int16_t ledY = LED_Y + 16;      // Centre des pastilles
    const int16_t ledRadius = 10;

    struct { bool ok; int8_t& cached; int16_t x; } leds[] = {
        {state.gpsOk,     cache.gpsOk,     COL_1},
        {state.headingOk, cache.headingOk, COL_2},
        {state.yawRateOk, cache.yawRateOk, COL_3},
    };

    for (auto& led : leds) {
        if (!force && led.cached == (int8_t)led.ok) {
            continue;
        }
        M5.Display.fillCircle(led.x, ledY, ledRadius, led.ok ? TFT_GREEN : TFT_RED);
        M5.Display.drawCircle(led.x, ledY, ledRadius, TFT_DARKGREY);
        led.cached = (int8_t)led.ok;
    }
}

void DisplayManager::drawTempBattery(const BuoyState& state, bool force) {
    // Bande température (gauche) / batterie (droite)
    char tempBuffer[16];
    // state.temperature est un float : "%d" lisait un argument entier inexistant
    snprintf(tempBuffer, sizeof(tempBuffer), "%.0f C", state.temperature);
    drawTextField(cache.temperature, tempBuffer, TFT_CYAN,
                  &fonts::Font4, 1, TL_DATUM, MARGIN, TEMP_Y, 90, force);

    uint8_t batteryPercent = (uint8_t)((state.remainingCapacity));
    if (batteryPercent > 100) batteryPercent = 100;

    char battBuffer[16];
    snprintf(battBuffer, sizeof(battBuffer), "%d%%", batteryPercent);
    drawTextField(cache.battery, battBuffer, getBatteryColor(batteryPercent),
                  &fonts::Font4, 1, TR_DATUM, SCR_W - MARGIN - 40, TEMP_Y, 90, force);

    // Icône batterie tout à droite (contour dessiné une fois par drawStaticLayout)
    if (force || cache.batteryIcon != batteryPercent) {
        drawBattery(batteryPercent, BATT_ICON_X, BATT_ICON_Y);
        cache.batteryIcon = batteryPercent;
    }
}

void DisplayManager::drawDistanceThrottle(const BuoyState& state, bool force) {
    // Bande basse : 3 colonnes de valeurs (libellés statiques DIST / CAP / THR)
    const int16_t valueY = CMD_Y + 20;

    char distBuffer[16];
    if (state.distanceToCons < 1000) {
        snprintf(distBuffer, sizeof(distBuffer), "%.0fm", state.distanceToCons);
    } else {
        snprintf(distBuffer, sizeof(distBuffer), "%.1fk", state.distanceToCons / 1000.0);
    }

    char headingBuffer[16];
    snprintf(headingBuffer, sizeof(headingBuffer), "%.0f", state.autoPilotTrueHeadingCmde);

    char throttleBuffer[16];
    snprintf(throttleBuffer, sizeof(throttleBuffer), "%d%%", state.autoPilotThrottleCmde);

    struct { TextField& field; const char* value; int16_t x; uint16_t color; } cols[] = {
        {cache.distance, distBuffer,     COL_1, TFT_WHITE},
        {cache.heading,  headingBuffer,  COL_2, TFT_YELLOW},
        {cache.throttle, throttleBuffer, COL_3, TFT_WHITE},
    };

    for (auto& col : cols) {
        drawTextField(col.field, col.value, col.color,
                      &fonts::Font4, 1, TC_DATUM, col.x, valueY, 100, force);
    }
}

void DisplayManager::drawNavigationState(const BuoyState& state, bool force) {
    // Bande des modes : mode général (petit) puis mode navigation (très grand)
    String generalModeName = buoyMgr.getGeneralModeName(state.generalMode);
    drawTextField(cache.generalMode, generalModeName.c_str(),
                  getGeneralModeColor(state.generalMode),
                  &fonts::Font2, 1, TC_DATUM, SCR_W / 2, MODE_Y + 1, 200, force);

    // Mode de navigation : élément le plus important de l'écran
    String navModeName = buoyMgr.getNavModeName(state.navigationMode);
    drawTextField(cache.navMode, navModeName.c_str(),
                  getNavModeColor(state.navigationMode),
                  &fonts::Font4, 2, MC_DATUM, SCR_W / 2, MODE_Y + 43, 300, force);
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
    
    // Remplit selon le niveau, puis efface le reste : les deux rectangles sont
    // disjoints, la jauge se met à jour sans clignoter
    int fillWidth = (batteryLevel * 26) / 100;
    if (fillWidth > 26) fillWidth = 26;
    M5.Display.fillRect(x + 2, y + 2, fillWidth, 11, color);
    if (fillWidth < 26) {
        M5.Display.fillRect(x + 2 + fillWidth, y + 2, 26 - fillWidth, 11, TFT_BLACK);
    }
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
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextSize(2);
    M5.Display.drawString("ERROR", SCR_W / 2, 90);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString(message, SCR_W / 2, 150);

    forceRefresh();  // L'écran a été effacé : tout redessiner au prochain update()
}

void DisplayManager::displayConnecting(const String& message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("CONNECTING", SCR_W / 2, 95);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString(message, SCR_W / 2, 145);

    forceRefresh();  // L'écran a été effacé : tout redessiner au prochain update()
}

void DisplayManager::displayBuoySelection() {
    uint8_t buoyId = buoyMgr.getSelectedBuoyId();
    String buoyName = buoyMgr.getBuoyName(buoyId);
    
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("SELECTED", SCR_W / 2, 85);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString(buoyName, SCR_W / 2, 145);
    M5.Display.setTextSize(1);
    
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
        case IDENTIFICATION:
            return TFT_WHITE;
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
