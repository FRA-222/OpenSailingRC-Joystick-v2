/**
 * @file SetupMenu.cpp
 * @brief Menu de configuration du joystick, ouvert au démarrage (bouton A maintenu)
 * @author Philippe Hubert
 * @date 2026
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#include "SetupMenu.h"
#include "JoystickManager.h"
#include "JoystickConfiguration.h"
#include "HardwareConfig.h"
#include "DisplayManager.h"
#include "LoRaBandProfiles.h"
#include "Logger.h"
#include <stdio.h>
#include <string.h>

namespace {

// Seuils du stick, mêmes valeurs que la boucle principale (main.cpp)
const int16_t STICK_THRESHOLD = 1500;            // sur ~2048
const int16_t STICK_RELEASE   = STICK_THRESHOLD / 2;
const uint32_t REPEAT_DELAY_MS    = 500;         // auto-répétition gauche / droite
const uint32_t REPEAT_INTERVAL_MS = 150;
const uint32_t POLL_MS            = 30;

// Puissance au-delà de laquelle la bande 433 sort de la limite européenne
const int8_t EU_433_LIMIT_DBM = 10;

} // namespace

SetupMenu::SetupMenu(JoystickManager& js)
    : joystick(js), visibleCount(0), selected(0),
      latchedV(0), latchedH(0), hPressTime(0), hLastRepeat(0) {
    stored = JoystickPreferences::defaults();
    draft = stored;
}

// ============================================================================
// FENÊTRE DE DÉMARRAGE
// ============================================================================

bool SetupMenu::checkBootRequest(const JoystickSettings& current, bool fromNvs) {
    uint32_t start = millis();
    uint32_t pressStart = 0;
    int lastPercent = -1;
    // La fenêtre se prolonge tant que le bouton est tenu : un maintien commencé
    // à la fin de la fenêtre doit pouvoir aller jusqu'à HOLD_MS.
    while (true) {
        joystick.update();
        bool pressed = joystick.isAtomScreenPressed();
        if (joystick.isAtomScreenHeld(HOLD_MS)) {
            Logger::log("⚙️  Bouton A maintenu : ouverture du menu de configuration");
            return true;
        }

        int percent = 0;
        if (pressed) {
            if (pressStart == 0) pressStart = millis();
            percent = (int)((millis() - pressStart) * 100 / HOLD_MS);
            if (percent > 100) percent = 100;
        } else {
            pressStart = 0;
        }
        if (percent != lastPercent) {
            DisplayManager::drawSetupBoot(current, fromNvs, (uint8_t)percent);
            lastPercent = percent;
        }

        if (!pressed && millis() - start >= BOOT_WINDOW_MS) {
            return false;
        }
        delay(POLL_MS);
    }
}

// ============================================================================
// MENU
// ============================================================================

JoystickSettings SetupMenu::run(const JoystickSettings& current, bool& saved) {
    saved = false;
    stored = current;
    draft = current;
    latchedV = latchedH = 0;
    rebuildVisible(ENTRY_LINK);

    // Attendre le relâchement du bouton A (il vient d'être maintenu) et des
    // boutons du menu, pour qu'aucun appui en cours ne soit pris pour un choix.
    do {
        joystick.update();
        delay(POLL_MS);
    } while (joystick.isAtomScreenPressed() || joystick.isButtonPressed(BTN_MENU_OK)
             || joystick.isButtonPressed(BTN_MENU_BACK));

    SetupMenuView view;
    buildView(view);
    DisplayManager::drawSetupMenu(view);
    uint32_t lastAction = millis();

    while (true) {
        joystick.update();
        bool redraw = false;

        StickEvents ev = readStick();
        if (ev.vertical != 0 && visibleCount > 0) {
            // Haut = vers le début de la liste ; la liste boucle
            selected = (uint8_t)((selected + visibleCount + ev.vertical) % visibleCount);
            redraw = true;
        }
        // L'auto-répétition ne sert qu'au canal (38 valeurs en 920) : sur les
        // autres entrées, garder le stick poussé ferait basculer la valeur en boucle.
        if (ev.horizontal != 0 && (!ev.repeat || visible[selected] == ENTRY_CHANNEL)) {
            EntryId id = visible[selected];
            change(id, ev.horizontal);
            rebuildVisible(id);
            redraw = true;
        }

        if (joystick.wasButtonPressed(BTN_MENU_OK)) {
            EntryId id = visible[selected];
            if (id == ENTRY_SAVE) {
                saved = JoystickPreferences::save(draft);
                DisplayManager::drawSetupMessage(saved ? "Saved" : "Save failed",
                                                 saved ? "Starting..." : "Using previous config");
                delay(1200);
                return saved ? draft : stored;
            }
            if (id == ENTRY_START) {
                if (draft != stored) {
                    Logger::log("⚙️  Menu : modifications abandonnees (Start sans enregistrer)");
                }
                return stored;
            }
            // Sur un réglage, OK passe à l'entrée suivante (pratique sur l'AtomS3)
            selected = (uint8_t)((selected + 1) % visibleCount);
            redraw = true;
        }

        if (joystick.wasButtonPressed(BTN_MENU_BACK)) {
            // Retour : abandon du brouillon, curseur sur « Start » pour sortir
            draft = stored;
            rebuildVisible(ENTRY_START);
            redraw = true;
        }

        if (redraw) {
            lastAction = millis();
            buildView(view);
            DisplayManager::drawSetupMenu(view);
        }

        if (millis() - lastAction >= IDLE_TIMEOUT_MS) {
            Logger::log("⚙️  Menu : inactivite, demarrage sans enregistrer");
            return stored;
        }
        delay(POLL_MS);
    }
}

// ============================================================================
// ENTRÉES
// ============================================================================

void SetupMenu::rebuildVisible(EntryId keep) {
    visibleCount = 0;
    for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
        EntryId id = (EntryId)i;
        bool loraOnly = (id == ENTRY_BAND || id == ENTRY_POWER || id == ENTRY_CHANNEL || id == ENTRY_ESPNOW);
        if (loraOnly && !draft.isLoRa()) continue;
        visible[visibleCount++] = id;
    }
    selectEntry(keep);
}

void SetupMenu::selectEntry(EntryId id) {
    for (uint8_t i = 0; i < visibleCount; i++) {
        if (visible[i] == id) { selected = i; return; }
    }
    if (selected >= visibleCount) selected = 0;
}

void SetupMenu::change(EntryId id, int8_t dir) {
    switch (id) {
        case ENTRY_LINK:
            draft.link = draft.isLoRa() ? JOYSTICK_LINK_ESPNOW : JOYSTICK_LINK_LORA;
            break;
        case ENTRY_BAND:
            draft.loraBand = (uint8_t)((draft.loraBand + LORA_BAND_COUNT + dir) % LORA_BAND_COUNT);
            // Canal et puissance ramenés dans le profil de la nouvelle bande
            JoystickPreferences::sanitize(draft);
            break;
        case ENTRY_POWER: {
            const LoRaBandProfile* p = loRaBandProfile(draft.loraBand);
            if (!p) break;
            // Les crans sont rangés du plus fort au plus faible : droite = plus fort
            int idx = loRaNearestPowerIndex(draft.loraBand, draft.loraPowerDbm) - dir;
            idx = (idx + p->powerCount) % p->powerCount;
            draft.loraPowerDbm = p->powers[idx].dBm;
            break;
        }
        case ENTRY_CHANNEL: {
            const LoRaBandProfile* p = loRaBandProfile(draft.loraBand);
            if (!p) break;
            int span = p->channelMax - p->channelMin + 1;
            int ch = ((draft.loraChannel - p->channelMin + dir) % span + span) % span;
            draft.loraChannel = (uint8_t)(p->channelMin + ch);
            break;
        }
        case ENTRY_ESPNOW:
            draft.espnowPassive = !draft.espnowPassive;
            break;
        default:
            break;   // actions : pas de valeur
    }
}

void SetupMenu::formatItem(EntryId id, SetupMenuItem& item) const {
    item.label[0] = item.value[0] = item.detail[0] = '\0';
    item.editable = true;
    item.warning = false;
    const LoRaBandProfile* p = loRaBandProfile(draft.loraBand);

    switch (id) {
        case ENTRY_LINK:
            snprintf(item.label, sizeof(item.label), "Link");
            snprintf(item.value, sizeof(item.value), "%s", draft.isLoRa() ? "LoRa" : "ESP-NOW");
            snprintf(item.detail, sizeof(item.detail), "%s",
                     draft.isLoRa() ? "Long range radio" : "2.4 GHz, short range");
            break;
        case ENTRY_BAND:
            snprintf(item.label, sizeof(item.label), "LoRa band");
            snprintf(item.value, sizeof(item.value), "%s", p ? p->name : "?");
            snprintf(item.detail, sizeof(item.detail), "Must match the module");
            break;
        case ENTRY_POWER:
            snprintf(item.label, sizeof(item.label), "LoRa power");
            snprintf(item.value, sizeof(item.value), "%d dBm", (int)draft.loraPowerDbm);
            if (draft.loraBand == LORA_BAND_CODE_433 && draft.loraPowerDbm > EU_433_LIMIT_DBM) {
                snprintf(item.detail, sizeof(item.detail), "Over EU 10 dBm limit");
                item.warning = true;
            } else {
                snprintf(item.detail, sizeof(item.detail), "Check local rules");
            }
            break;
        case ENTRY_CHANNEL:
            snprintf(item.label, sizeof(item.label), "LoRa channel");
            snprintf(item.value, sizeof(item.value), "%u", (unsigned)draft.loraChannel);
            snprintf(item.detail, sizeof(item.detail), "%.3f MHz",
                     loRaChannelFrequencyMHz(draft.loraBand, draft.loraChannel));
            break;
        case ENTRY_ESPNOW:
            snprintf(item.label, sizeof(item.label), "ESP-NOW listen");
            snprintf(item.value, sizeof(item.value), "%s", draft.espnowPassive ? "ON" : "OFF");
            snprintf(item.detail, sizeof(item.detail), "Listen to 2.4 GHz");
            break;
        case ENTRY_SAVE:
            item.editable = false;
            snprintf(item.label, sizeof(item.label), "Save & start");
            snprintf(item.detail, sizeof(item.detail), "Band/chan = buoy!");
            item.warning = true;
            break;
        case ENTRY_START:
            item.editable = false;
            snprintf(item.label, sizeof(item.label), "Start");
            snprintf(item.detail, sizeof(item.detail), "%s",
                     (draft != stored) ? "Discard changes" : "Keep current config");
            break;
        default:
            break;
    }
}

void SetupMenu::buildView(SetupMenuView& view) const {
    view.count = visibleCount;
    view.selected = selected;
    view.dirty = (draft != stored);
    view.version = JOYSTICK_FIRMWARE_VERSION;
    view.okName = BTN_MENU_OK_NAME;
    view.backName = BTN_MENU_BACK_NAME;
    for (uint8_t i = 0; i < visibleCount && i < SETUP_MENU_MAX_ITEMS; i++) {
        formatItem(visible[i], view.items[i]);
    }
}

// ============================================================================
// STICK
// ============================================================================

SetupMenu::StickEvents SetupMenu::readStick() {
    StickEvents ev = {0, 0, false};
    int16_t y = joystick.getAxisCentered(AXIS_LEFT_Y);   // négatif = haut (main.cpp)
    int16_t x = (int16_t)(LEFT_STICK_X_SIGN * joystick.getAxisCentered(AXIS_LEFT_X));

    // Vertical : un geste = un cran
    if (latchedV == 0) {
        if (y < -STICK_THRESHOLD)      { latchedV = -1; ev.vertical = -1; }
        else if (y > STICK_THRESHOLD)  { latchedV = +1; ev.vertical = +1; }
    } else if (y > -STICK_RELEASE && y < STICK_RELEASE) {
        latchedV = 0;
    }

    // Horizontal : un cran, puis auto-répétition si le stick reste poussé
    uint32_t now = millis();
    if (latchedH == 0) {
        if (x < -STICK_THRESHOLD)      { latchedH = -1; ev.horizontal = -1; }
        else if (x > STICK_THRESHOLD)  { latchedH = +1; ev.horizontal = +1; }
        if (latchedH != 0) { hPressTime = now; hLastRepeat = now; }
    } else if (x > -STICK_RELEASE && x < STICK_RELEASE) {
        latchedH = 0;
    } else if (now - hPressTime >= REPEAT_DELAY_MS && now - hLastRepeat >= REPEAT_INTERVAL_MS) {
        ev.horizontal = latchedH;
        ev.repeat = true;
        hLastRepeat = now;
    }
    return ev;
}
