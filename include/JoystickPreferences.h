/**
 * @file JoystickPreferences.h
 * @brief Préférences du joystick conservées en NVS (configuration de communication)
 * @author Philippe Hubert
 * @date 2026
 *
 * Liaison (ESP-NOW / LoRa), bande, puissance et canal LoRa, écoute ESP-NOW
 * passive. Les constantes de JoystickConfiguration.h en sont les valeurs par
 * défaut : un joystick sans préférences se comporte comme en 1.2.2.
 *
 * Toutes les préférences sont décrites par une table unique (clé NVS, défaut,
 * accès au champ) : ajouter un réglage = une ligne dans JoystickPreferences.cpp.
 * Voir docs/CONFIGURATION_MENU_DESIGN.md §8.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef JOYSTICK_PREFERENCES_H
#define JOYSTICK_PREFERENCES_H

#include <stdint.h>
#include <stddef.h>
#include "CommunicationConfig.h"

/// Liaison choisie (valeur stockée en NVS : ne pas renuméroter)
static const uint8_t JOYSTICK_LINK_ESPNOW = 0;
static const uint8_t JOYSTICK_LINK_LORA   = 1;
static const uint8_t JOYSTICK_LINK_COUNT  = 2;

/**
 * @brief Configuration de communication du joystick
 */
struct JoystickSettings {
    uint8_t link;            ///< JOYSTICK_LINK_*
    uint8_t loraBand;        ///< LORA_BAND_CODE_* (LoRaBandProfiles.h)
    int8_t  loraPowerDbm;    ///< puissance d'émission, dBm
    uint8_t loraChannel;     ///< canal E220
    bool    espnowPassive;   ///< écoute ESP-NOW en mode LoRa

    bool isLoRa() const { return link == JOYSTICK_LINK_LORA; }

    bool operator==(const JoystickSettings& o) const {
        return link == o.link && loraBand == o.loraBand && loraPowerDbm == o.loraPowerDbm
            && loraChannel == o.loraChannel && espnowPassive == o.espnowPassive;
    }
    bool operator!=(const JoystickSettings& o) const { return !(*this == o); }
};

/**
 * @class JoystickPreferences
 * @brief Lecture, validation et écriture des préférences en NVS (espace "joy_cfg")
 */
class JoystickPreferences {
public:
    /** @brief Valeurs par défaut, tirées de JoystickConfiguration.h */
    static JoystickSettings defaults();

    /**
     * @brief Lit les préférences ; toute clé absente prend sa valeur par défaut
     * @param[out] fromNvs true si des préférences étaient enregistrées
     * @return Préférences validées (sanitize())
     */
    static JoystickSettings load(bool* fromNvs = nullptr);

    /** @brief Enregistre les préférences (après validation) @return true si réussi */
    static bool save(const JoystickSettings& settings);

    /**
     * @brief Ramène chaque valeur dans son domaine
     *
     * Liaison ou bande inconnue : défaut. Canal hors plage de la bande : canal
     * par défaut de la bande. Puissance : cran autorisé le plus proche.
     * À rappeler après tout changement de bande.
     */
    static void sanitize(JoystickSettings& settings);

    /** @brief Mode de communication correspondant (ESP_NOW / LORA_920 / LORA_433) */
    static CommMode toCommMode(const JoystickSettings& settings);

    /**
     * @brief Résumé d'une ligne, ex. "LoRa 433 ch24 22dBm" ou "ESP-NOW"
     * @param settings Préférences à décrire
     * @param buf      Tampon de sortie
     * @param size     Taille du tampon
     */
    static void describe(const JoystickSettings& settings, char* buf, size_t size);

    /** @brief Trace des préférences dans le Logger */
    static void log(const JoystickSettings& settings, const char* title);
};

#endif // JOYSTICK_PREFERENCES_H
