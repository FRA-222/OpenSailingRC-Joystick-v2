/**
 * @file JoystickConfiguration.h
 * @brief Configuration du joystick : matériel, mode de communication, réglages radio et essais
 * @author Philippe Hubert
 * @date 2026
 *
 * Pendant de BuoyConfiguration côté bouée : TOUS les choix de compilation du
 * joystick sont ici, et nulle part ailleurs.
 *
 * ── Matériel ────────────────────────────────────────────────────────────────
 * Le matériel (v1 AtomS3 ou v2 Core2) est fixé par l'environnement PlatformIO,
 * parce que la carte et ses pins changent :
 *
 *   pio run -e joystick-v1-atoms3     → JOYSTICK_HW = 1  (BuoyJoystick historique)
 *   pio run -e joystick-v2-core2      → JOYSTICK_HW = 2  (Core2 + ExtPort)
 *
 * Le code commun (protocole, gestion des bouées, commandes) ne connaît pas le
 * matériel ; seuls JoystickManagerV*.cpp, DisplayManagerV*.cpp, HardwareConfig.h
 * et le Logger en dépendent. JOYSTICK_HARDWARE ci-dessous en est l'image
 * lisible à l'exécution (traces, écran de démarrage).
 *
 * ── Communication ───────────────────────────────────────────────────────────
 * COMM_MODE, LORA_AIR_RATE et JOYSTICK_ESPNOW_PASSIVE se changent ici. Ils
 * doivent correspondre au réglage de la bouée (BuoyConfiguration.cpp) : un
 * désaccord de bande ou de débit air ne produit aucune erreur, seulement un
 * silence total.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef JOYSTICK_CONFIGURATION_H
#define JOYSTICK_CONFIGURATION_H

#include <stdint.h>
#include "CommunicationConfig.h"
#include "LoRaCommunication.h"   // LoRaBand, LoRaAirRate

// ============================================================================
// MATÉRIEL — fixé par l'environnement PlatformIO (-DJOYSTICK_HW=1 ou 2)
// ============================================================================
#define JOYSTICK_HW_V1_ATOMS3 1
#define JOYSTICK_HW_V2_CORE2  2

#ifndef JOYSTICK_HW
#error "JOYSTICK_HW non défini : compiler avec -e joystick-v1-atoms3 ou -e joystick-v2-core2 (platformio.ini)"
#endif
#if JOYSTICK_HW != JOYSTICK_HW_V1_ATOMS3 && JOYSTICK_HW != JOYSTICK_HW_V2_CORE2
#error "JOYSTICK_HW doit valoir 1 (AtomS3) ou 2 (Core2)"
#endif

/**
 * @brief Matériel du joystick
 */
enum tJoystickHardware : uint8_t {
    JOYSTICK_V1_ATOMS3 = JOYSTICK_HW_V1_ATOMS3,  ///< AtomS3 + carte STM32 (2 sticks, 4 boutons, 2 batteries), écran 128x128
    JOYSTICK_V2_CORE2  = JOYSTICK_HW_V2_CORE2    ///< Core2 + ExtPort : 2 Unit Joystick, ByteButton, Dual Button, écran 320x240
};

constexpr tJoystickHardware JOYSTICK_HARDWARE = (tJoystickHardware)JOYSTICK_HW;

// ============================================================================
// VERSION FIRMWARE
// ============================================================================
constexpr const char* JOYSTICK_FIRMWARE_VERSION = "2.2.0";

// ============================================================================
// COMMUNICATION
// ============================================================================
// - CommMode::ESP_NOW  : ESP-NOW (2,4 GHz, courte portée, rapide)
// - CommMode::LORA_920 : LoRa module E220-900T22S(JP) 920 MHz
// - CommMode::LORA_433 : LoRa module E220-400T22S 433 MHz
//
// Les deux bandes LoRa partagent tout le protocole : le code commun se teste
// avec CommunicationConfig::isLoRa(COMM_MODE), et seule la configuration radio
// (canal, débit air, puissance) dépend de la bande. La fréquence n'étant pas
// détectable par logiciel, elle doit correspondre au module physiquement
// branché — et au réglage de la bouée. Le module se configure via son switch
// M0/M1 (ON = configuration, OFF = normal).
constexpr CommMode COMM_MODE = CommMode::LORA_433;

// Bande radio déduite du mode (utilisée par l'instance LoRa)
constexpr LoRaBand LORA_BAND = (COMM_MODE == CommMode::LORA_433)
                                   ? LoRaBand::BAND_433
                                   : LoRaBand::BAND_920;

// Débit air LoRa — DOIT être identique à LORA_AIR_RATE de la bouée.
// Du plus lent au plus rapide : AIR_2400 / 4800 / 9600 / 19200 / 38400 / 62500.
// Chaque cran divise ~par 2 le temps d'antenne et coûte ~3 dB de sensibilité.
// 9,6 kbps est le débit retenu par les campagnes A.8 / A.9 (GATEWAY_DESIGN.md).
constexpr LoRaAirRate LORA_AIR_RATE = LoRaAirRate::AIR_9600;

// ── Écoute passive ESP-NOW en mode LoRa ──────────────────────────────────────
// 1 = comportement nominal : en mode LoRa, ESP-NOW est aussi initialisé pour
//     recevoir les broadcasts d'état des bouées, plus rapides que le LoRa.
// 0 = radio 2,4 GHz coupée. Le joystick ne reçoit plus que le LoRa.
//
// ⚠️ Ce n'est PAS un simple réglage de confort : c'est l'instrument d'un essai
// de portée. Les deux radios cohabitent sur la même carte, et les bursts WiFi
// remontent le plancher de bruit du récepteur 433 colocalisé (5 à 15 dB) et
// peuvent détruire les trames LoRa qui tombent au mauvais moment.
// Voir GATEWAY_DESIGN.md §5.3, « ESP-NOW et LoRa sur la même carte ».
#define JOYSTICK_ESPNOW_PASSIVE 0

// ============================================================================
// ESSAIS / MISE AU POINT
// ============================================================================
// Trace périodique des valeurs brutes du joystick droit (2 lignes/seconde).
// Utile pour régler le centrage du stick ; laisser à 0 sur le terrain.
#define DEBUG_JOYSTICK_RAW 0

// Bloc "--- Etat systeme ---" toutes les 2 s (~9 lignes à chaque fois).
// Utile en mise au point, mais il noie les bilans de liaison sur le terrain.
#define DEBUG_SYSTEM_STATE 0

/**
 * @class JoystickConfiguration
 * @brief Accès lisible à la configuration (traces, écran de démarrage)
 */
class JoystickConfiguration {
public:
    /** @brief "v1 AtomS3" / "v2 Core2" */
    static const char* hardwareName();

    /** @brief Nom court pour l'écran de démarrage : "Joystick v1" / "Joystick v2" */
    static const char* productName();

    /** @brief Rappel de câblage affiché quand l'initialisation des entrées échoue */
    static const char* wiringHint();

    static constexpr bool isV1() { return JOYSTICK_HARDWARE == JOYSTICK_V1_ATOMS3; }
    static constexpr bool isV2() { return JOYSTICK_HARDWARE == JOYSTICK_V2_CORE2; }

    /** @brief Trace complète de la configuration au démarrage */
    static void logSummary();
};

#endif // JOYSTICK_CONFIGURATION_H
