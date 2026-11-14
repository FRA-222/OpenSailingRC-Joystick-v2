/**
 * @file JoystickPreferences.cpp
 * @brief Préférences du joystick conservées en NVS
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

#include "JoystickPreferences.h"
#include "JoystickConfiguration.h"
#include "LoRaBandProfiles.h"
#include "Logger.h"
#include <Preferences.h>
#include <stdio.h>

namespace {

const char* NVS_NAMESPACE = "joy_cfg";
const char* KEY_VERSION   = "prefsVer";
const int32_t PREFS_VERSION = 1;

/**
 * @brief Descripteur d'une préférence : clé NVS, défaut, accès au champ
 *
 * Les valeurs transitent en int32_t, ce qui couvre tous les champs actuels.
 */
struct PrefDescriptor {
    const char* key;                                   ///< clé NVS (15 caractères maxi)
    int32_t (*get)(const JoystickSettings&);
    void    (*set)(JoystickSettings&, int32_t);
};

const PrefDescriptor PREFS[] = {
    { "commMode", [](const JoystickSettings& s) -> int32_t { return s.link; },
                  [](JoystickSettings& s, int32_t v) { s.link = (uint8_t)v; } },
    { "loraBand", [](const JoystickSettings& s) -> int32_t { return s.loraBand; },
                  [](JoystickSettings& s, int32_t v) { s.loraBand = (uint8_t)v; } },
    { "loraPwr",  [](const JoystickSettings& s) -> int32_t { return s.loraPowerDbm; },
                  [](JoystickSettings& s, int32_t v) { s.loraPowerDbm = (int8_t)v; } },
    { "loraChan", [](const JoystickSettings& s) -> int32_t { return s.loraChannel; },
                  [](JoystickSettings& s, int32_t v) { s.loraChannel = (uint8_t)v; } },
    { "espnPass", [](const JoystickSettings& s) -> int32_t { return s.espnowPassive ? 1 : 0; },
                  [](JoystickSettings& s, int32_t v) { s.espnowPassive = (v != 0); } },
};

} // namespace

JoystickSettings JoystickPreferences::defaults() {
    JoystickSettings s;
    s.link = CommunicationConfig::isLoRa(DEFAULT_COMM_MODE) ? JOYSTICK_LINK_LORA : JOYSTICK_LINK_ESPNOW;
    s.loraBand = (DEFAULT_COMM_MODE == CommMode::LORA_920) ? LORA_BAND_CODE_920 : LORA_BAND_CODE_433;
    const LoRaBandProfile* p = loRaBandProfile(s.loraBand);
    s.loraChannel = p->defaultChannel;
    s.loraPowerDbm = p->defaultPowerDbm;
    s.espnowPassive = DEFAULT_ESPNOW_PASSIVE;
    return s;
}

void JoystickPreferences::sanitize(JoystickSettings& s) {
    const JoystickSettings d = defaults();
    if (s.link >= JOYSTICK_LINK_COUNT) s.link = d.link;
    if (loRaBandProfile(s.loraBand) == nullptr) s.loraBand = d.loraBand;
    s.loraChannel = loRaSanitizeChannel(s.loraBand, s.loraChannel);
    s.loraPowerDbm = loRaSanitizePowerDbm(s.loraBand, s.loraPowerDbm);
}

JoystickSettings JoystickPreferences::load(bool* fromNvs) {
    JoystickSettings s = defaults();
    Preferences prefs;
    // Lecture seule : begin() échoue si l'espace n'a jamais été créé, ce qui est
    // le cas normal d'un joystick neuf ou fraîchement flashé par M5Burner.
    bool opened = prefs.begin(NVS_NAMESPACE, true);
    bool stored = opened && prefs.isKey(KEY_VERSION);
    if (stored) {
        for (const PrefDescriptor& d : PREFS) {
            if (prefs.isKey(d.key)) {
                d.set(s, prefs.getInt(d.key, d.get(s)));
            }
        }
    }
    if (opened) prefs.end();

    JoystickSettings raw = s;
    sanitize(s);
    if (stored && s != raw) {
        Logger::log("⚠️  Preferences: valeur(s) hors domaine corrigee(s)");
    }
    if (fromNvs) *fromNvs = stored;
    return s;
}

bool JoystickPreferences::save(const JoystickSettings& settings) {
    JoystickSettings s = settings;
    sanitize(s);
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Logger::log("❌ Preferences: ouverture NVS impossible, rien n'est enregistre");
        return false;
    }
    bool ok = true;
    for (const PrefDescriptor& d : PREFS) {
        ok &= (prefs.putInt(d.key, d.get(s)) > 0);
    }
    ok &= (prefs.putInt(KEY_VERSION, PREFS_VERSION) > 0);
    prefs.end();
    Logger::log(ok ? "✓ Preferences enregistrees en NVS" : "❌ Preferences: ecriture NVS incomplete");
    return ok;
}

CommMode JoystickPreferences::toCommMode(const JoystickSettings& s) {
    if (!s.isLoRa()) return CommMode::ESP_NOW;
    return (s.loraBand == LORA_BAND_CODE_920) ? CommMode::LORA_920 : CommMode::LORA_433;
}

void JoystickPreferences::describe(const JoystickSettings& s, char* buf, size_t size) {
    if (!s.isLoRa()) {
        snprintf(buf, size, "ESP-NOW");
        return;
    }
    snprintf(buf, size, "LoRa %s ch%u %ddBm",
             (s.loraBand == LORA_BAND_CODE_920) ? "920" : "433",
             (unsigned)s.loraChannel, (int)s.loraPowerDbm);
}

void JoystickPreferences::log(const JoystickSettings& s, const char* title) {
    Logger::logf("--- %s ---", title);
    Logger::logf("  Liaison       : %s", s.isLoRa() ? "LoRa" : "ESP-NOW");
    if (s.isLoRa()) {
        const LoRaBandProfile* p = loRaBandProfile(s.loraBand);
        Logger::logf("  Bande         : %s", p ? p->name : "?");
        Logger::logf("  Canal         : %u (%.3f MHz)", (unsigned)s.loraChannel,
                     loRaChannelFrequencyMHz(s.loraBand, s.loraChannel));
        Logger::logf("  Puissance     : %d dBm (registre 0b%u%u)", (int)s.loraPowerDbm,
                     (unsigned)((loRaPowerRegister(s.loraBand, s.loraPowerDbm) >> 1) & 1),
                     (unsigned)(loRaPowerRegister(s.loraBand, s.loraPowerDbm) & 1));
        Logger::logf("  ESP-NOW passif: %s", s.espnowPassive ? "actif" : "coupe");
    }
}
