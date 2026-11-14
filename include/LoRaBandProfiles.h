/**
 * @file LoRaBandProfiles.h
 * @brief Profils des bandes LoRa E220 : fréquence, canaux proposés, puissances
 * @author Philippe Hubert
 * @date 2026
 *
 * Tout ce qui dépend du module E220 branché, rassemblé en une table. En-tête
 * pur, sans type du dépôt : il se recopie tel quel dans la bouée et la
 * passerelle (même principe que LoRaProtocol.h). Voir
 * docs/CONFIGURATION_MENU_DESIGN.md §8.2.
 *
 * Codes de bande : même ordre que LoRaBand (joystick) et tLoRaBand (bouée).
 *
 * Puissance : le registre REG1 n'a que 2 bits et sa table dépend du module
 * (l'énumération TX_POWER_* de la bibliothèque M5 n'est juste que pour le JP).
 * La puissance est donc manipulée et stockée en dBm, et le registre n'est
 * déduit qu'au moment d'écrire la configuration.
 *
 * Réglementation : la liste des canaux et des puissances proposées ne garantit
 * pas la conformité. En Europe, la bande 433 est limitée à 10 mW (10 dBm), et
 * seuls les canaux 23 et 24 tombent dans 433,05-434,79 MHz.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef LORA_BAND_PROFILES_H
#define LORA_BAND_PROFILES_H

#include <stdint.h>
#include <stdlib.h>

/// Codes de bande (ordre de LoRaBand / tLoRaBand : ne pas réordonner)
static const uint8_t LORA_BAND_CODE_920 = 0;   ///< E220-900T22S(JP)
static const uint8_t LORA_BAND_CODE_433 = 1;   ///< E220-400T22S
static const uint8_t LORA_BAND_COUNT    = 2;   ///< 868 à ajouter quand son profil sera confirmé

/// Un cran de puissance : valeur en dBm et bits du registre REG1 correspondants
struct LoRaPowerStep {
    int8_t  dBm;
    uint8_t reg;
};

/// Profil d'une bande
struct LoRaBandProfile {
    const char* name;            ///< libellé court ("433 MHz")
    float       baseMHz;         ///< F = baseMHz + canal * stepMHz
    float       stepMHz;
    uint8_t     channelMin;      ///< canaux proposés : channelMin..channelMax
    uint8_t     channelMax;
    uint8_t     defaultChannel;
    const LoRaPowerStep* powers; ///< du plus fort au plus faible
    uint8_t     powerCount;
    int8_t      defaultPowerDbm;
};

static const LoRaPowerStep LORA_POWERS_920[] = { {13, 0b00}, {12, 0b01}, {7, 0b10}, {0, 0b11} };
static const LoRaPowerStep LORA_POWERS_433[] = { {22, 0b00}, {17, 0b01}, {13, 0b10}, {10, 0b11} };

/// Table indexée par code de bande
static const LoRaBandProfile LORA_BAND_PROFILES[LORA_BAND_COUNT] = {
    // 920 MHz JP : 920,6 + 0,2 x canal ; canal 0 en service
    { "920 MHz", 920.6f,   0.2f, 0, 37,  0, LORA_POWERS_920, 4, 13 },
    // 433 MHz : 410,125 + 1 x canal ; ISM europeenne = canaux 23 et 24
    { "433 MHz", 410.125f, 1.0f, 23, 24, 24, LORA_POWERS_433, 4, 22 },
};

/// Profil d'une bande, ou nullptr si le code est inconnu
inline const LoRaBandProfile* loRaBandProfile(uint8_t bandCode) {
    return (bandCode < LORA_BAND_COUNT) ? &LORA_BAND_PROFILES[bandCode] : nullptr;
}

/// Fréquence centrale d'un canal, en MHz
inline float loRaChannelFrequencyMHz(uint8_t bandCode, uint8_t channel) {
    const LoRaBandProfile* p = loRaBandProfile(bandCode);
    return p ? p->baseMHz + channel * p->stepMHz : 0.0f;
}

/// Canal ramené dans la plage proposée par la bande (défaut de la bande si hors plage)
inline uint8_t loRaSanitizeChannel(uint8_t bandCode, int channel) {
    const LoRaBandProfile* p = loRaBandProfile(bandCode);
    if (!p) return 0;
    if (channel < p->channelMin || channel > p->channelMax) return p->defaultChannel;
    return (uint8_t)channel;
}

/// Indice du cran de puissance le plus proche d'une valeur en dBm
inline uint8_t loRaNearestPowerIndex(uint8_t bandCode, int dBm) {
    const LoRaBandProfile* p = loRaBandProfile(bandCode);
    if (!p) return 0;
    uint8_t best = 0;
    int bestGap = 1000;
    for (uint8_t i = 0; i < p->powerCount; i++) {
        int gap = abs(p->powers[i].dBm - dBm);
        // A ecart egal, on garde le cran le plus FAIBLE (prudence reglementaire)
        if (gap < bestGap || (gap == bestGap && p->powers[i].dBm < p->powers[best].dBm)) {
            best = i;
            bestGap = gap;
        }
    }
    return best;
}

/// Puissance ramenée au cran autorisé le plus proche, en dBm
inline int8_t loRaSanitizePowerDbm(uint8_t bandCode, int dBm) {
    const LoRaBandProfile* p = loRaBandProfile(bandCode);
    return p ? p->powers[loRaNearestPowerIndex(bandCode, dBm)].dBm : 0;
}

/// Bits du registre REG1 pour une puissance en dBm (cran le plus proche)
inline uint8_t loRaPowerRegister(uint8_t bandCode, int dBm) {
    const LoRaBandProfile* p = loRaBandProfile(bandCode);
    return p ? p->powers[loRaNearestPowerIndex(bandCode, dBm)].reg : 0b11;
}

#endif // LORA_BAND_PROFILES_H
