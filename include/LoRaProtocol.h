/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC ecosystem and is distributed
 * under the GNU General Public License v3.0.
 * See LICENSE for full license text.
 */

/**
 * @file LoRaProtocol.h
 * @brief Trames LoRa partagées bouée / joystick / passerelle — copie LITTÉRALE
 *
 * Ce fichier est identique dans les trois dépôts :
 *   - Autonomous-GPS-Buoy            src/LoRaProtocol.h
 *   - OpenSailingRC-Joystick-v2      include/LoRaProtocol.h
 *   - OpenSailingRC-GroundStation    include/LoRaProtocol.h
 *
 * Toute modification se fait ici PUIS se recopie à l'identique. Les
 * static_assert de taille en fin de fichier gardent les copies en phase.
 *
 * Référence : GATEWAY_DESIGN.md §3 (dépôt Ground Station) et
 * LORA_PROTOCOL.md §3, §4, §5.2 (dépôt bouée). Le contenu des données est
 * fixé par le tableur « Interfaces DashBoard - Buoy.xlsx ».
 *
 * Ce fichier ne dépend d'aucun type propre à un dépôt : tous les champs sont
 * des entiers. Les énumérés métier (tBuoys, tEtatsGeneral, tEtatsNav,
 * tEtatsTypeCons, tEtatsPhaseCons, BuoyCommand) sont transportés en uint8_t
 * et convertis par l'appelant.
 */

#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <stdint.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Types de messages (octet 0 de toute trame) — allocation LORA_PROTOCOL.md §5.2
// ─────────────────────────────────────────────────────────────────────────────
enum class LoRaMessageType : uint8_t {
    // 0x01 REQUEST et 0x02 RESPONSE : retirés (ancien polling). Valeurs non réattribuées.
    COMMAND                 = 0x03,  ///< Descendant, 7 o — réponse attendue : ACK (joystick v1)
    ACK                     = 0x04,  ///< Montant, 18 o — AckWithStatePacketLora (joystick v1)
    BUOY_STATUS             = 0x05,  ///< Montant, 58 o — BuoyStatusPacketLora
    OBSERVABLE              = 0x06,  ///< Montant, 11 o — ObservablePacketLora
    COMMAND_POS             = 0x07,  ///< Descendant, 15 o — réponse attendue : ACK
    COMMAND_BUOY_STATUS     = 0x08,  ///< Descendant, 7 o — même charge que 0x03, réponse : BUOY_STATUS
    COMMAND_POS_BUOY_STATUS = 0x09,  ///< Descendant, 15 o — même charge que 0x07, réponse : BUOY_STATUS
    OBSERVABLE2             = 0x0A,  ///< Montant, 11 o — Observable2PacketLora (à la demande)
    OBSERVABLE_GPS          = 0x0B   ///< Montant, 8 o  — ObservableGPSPacketLora (à la demande)
};

/// Identifiant de diffusion à toute la flottille (targetBuoyId)
static const uint8_t LORA_BROADCAST_ID = 0xFF;

// ─────────────────────────────────────────────────────────────────────────────
// Codage des données — GATEWAY_DESIGN.md §3.0
// ─────────────────────────────────────────────────────────────────────────────
namespace LoRaCodec {

/// Positions : degrés × 1e7 sur int32_t (1,1 cm)
inline int32_t encodeDeg1e7(double deg) { return (int32_t)lround(deg * 1e7); }
inline double  decodeDeg1e7(int32_t v)  { return (double)v / 1e7; }

/// Angle BAM (Binary Angular Measurement) : 0..360° sur un octet, LSB = 1,40625°
inline uint8_t toBam(float deg) {
    float d = fmodf(deg, 360.0f);
    if (d < 0.0f) d += 360.0f;
    int v = (int)lroundf(d * 256.0f / 360.0f);
    return (uint8_t)(v & 0xFF);              // 256 (359,3°..360°) → 0
}
inline float fromBam(uint8_t v) { return (float)v * (360.0f / 256.0f); }

/// Angle BAM signé (écart de cap) : ±180° — même octet, décodage recentré
inline uint8_t toBamSigned(float deg) { return toBam(deg); }
inline float fromBamSigned(uint8_t v) {
    float d = fromBam(v);
    return (d >= 180.0f) ? d - 360.0f : d;
}

/// Deux valeurs 0..15 sur un octet : PREMIER champ du tableur en quartet bas
inline uint8_t packNibbles(uint8_t lo, uint8_t hi) {
    return (uint8_t)((lo & 0x0F) | ((hi & 0x0F) << 4));
}
inline uint8_t nibbleLo(uint8_t v) { return (uint8_t)(v & 0x0F); }
inline uint8_t nibbleHi(uint8_t v) { return (uint8_t)(v >> 4); }

/// Drapeau n (0..7) d'un octet de booléens (nommé flag : bit() est une macro Arduino)
inline uint8_t flag(uint8_t n, bool set) { return set ? (uint8_t)(1u << n) : 0; }
inline bool    hasBit(uint8_t v, uint8_t n) { return ((v >> n) & 1u) != 0; }

/// Grandeurs mises à l'échelle, saturées à la borne du type
inline uint8_t  satU8(float x)  { if (x < 0.0f) return 0; if (x > 255.0f) return 255; return (uint8_t)lroundf(x); }
inline uint16_t satU16(float x) { if (x < 0.0f) return 0; if (x > 65535.0f) return 65535; return (uint16_t)lroundf(x); }
inline int8_t   satI8(float x)  { if (x < -127.0f) return -127; if (x > 127.0f) return 127; return (int8_t)lroundf(x); }

} // namespace LoRaCodec

// ─────────────────────────────────────────────────────────────────────────────
// Drapeaux de BuoyStatusPacketLora — GATEWAY_DESIGN.md §3.3
// ─────────────────────────────────────────────────────────────────────────────
namespace MonitoringStatusBit {
    static const uint8_t DL_PARTIALLY_KO      = 0;  ///< tEtatsDatalink == PARTIALLY_KO
    static const uint8_t DL_KO                = 1;  ///< tEtatsDatalink == KO
    static const uint8_t SERVICE_LIFE_EXCEEDED = 2; ///< tEtatsDatalink == SERVICE_LIFE_EXCEEDED
    static const uint8_t TEMPERATURE_KO       = 3;
    static const uint8_t BATTERY_KO           = 4;
    static const uint8_t LIPO                 = 5;  ///< tTypeBatteries == LIPO
    static const uint8_t LIFE                 = 6;  ///< tTypeBatteries == LIFE
    // b7 réservé
}

namespace SensorsValidityBit {
    static const uint8_t HEADING_OK     = 0;
    static const uint8_t YAWRATE_OK     = 1;
    static const uint8_t ACCEL_LONGI_OK = 2;
    static const uint8_t GPS_OK         = 3;  ///< latGps / lonGps valides
    static const uint8_t HOME_OK        = 4;  ///< latHome / lonHome valides
    static const uint8_t DEGT_OK        = 5;  ///< latDegt / lonDegt valides
    static const uint8_t TARGET_OK      = 6;  ///< latTarget / lonTarget valides
    static const uint8_t CONS_OK        = 7;  ///< latCons / lonCons valides
}

// ─────────────────────────────────────────────────────────────────────────────
// Trames montantes (bouée → maître)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief État bouée — réponse à tout COMMAND_BUOY_STATUS / COMMAND_POS_BUOY_STATUS
 *
 * 58 octets. Envoyé UNE fois (pas de répétition ×2 comme l'ACK v1).
 */
struct __attribute__((packed)) BuoyStatusPacketLora {
    uint8_t  messageType;               ///< LoRaMessageType::BUOY_STATUS (0x05)
    uint8_t  buoyIds;                   ///< b0..3 buoyId (tBuoys 0..8) | b4..7 preparedBuoyId
    uint8_t  halfHourServiceLife;       ///< 0..15 demi-heures
    uint8_t  modes;                     ///< b0..3 generalMode (tEtatsGeneral) | b4..7 navigationMode (tEtatsNav)
    uint8_t  monitoringStatus;          ///< MonitoringStatusBit
    uint8_t  sensorsValidities;         ///< SensorsValidityBit
    uint8_t  trueHeading;               ///< BAM
    int32_t  latGps,    lonGps;         ///< deg × 1e7
    int32_t  latCons,   lonCons;        ///< deg × 1e7
    uint8_t  cons;                      ///< b0..3 typeCons (tEtatsTypeCons) | b4..7 phaseCons (tEtatsPhaseCons)
    int32_t  latHome,   lonHome;        ///< deg × 1e7
    int32_t  latDegt,   lonDegt;        ///< deg × 1e7
    int32_t  latTarget, lonTarget;      ///< deg × 1e7
    uint16_t distanceToCons;            ///< décimètres, saturé 65535
    int8_t   autoPilotThrottleCmde;     ///< %
    int8_t   autoPilotRudderCmde;       ///< %
    uint8_t  autoPilotTrueHeadingCmde;  ///< BAM
    uint32_t lastCmdTimestamp;          ///< timestamp de la dernière commande acceptée (acquittement implicite)
    uint8_t  lastCmdCode;               ///< BuoyCommand acquittée
};

/**
 * @brief Observables lents d'exploitation — réponse à CMD_OBSERVABLE (11 octets)
 */
struct __attribute__((packed)) ObservablePacketLora {
    uint8_t  messageType;               ///< LoRaMessageType::OBSERVABLE (0x06)
    uint8_t  buoyId;                    ///< tBuoys 0..8 (octet entier)
    uint8_t  courseGps;                 ///< BAM
    uint8_t  speedGps;                  ///< m/s × 100, saturé 255
    uint8_t  nbSat;
    int8_t   temperature;               ///< °C signé
    uint8_t  remainingCapacity;         ///< %
    uint8_t  partialDlToBuoyLossNumber; ///< saturé 255
    uint8_t  totalDlToBuoyLossNumber;   ///< saturé 255
    uint8_t  courseToCons;              ///< BAM
    uint8_t  headingError;              ///< BAM signé
};

/**
 * @brief Observables de mise au point : gains PID et moteurs — réponse à CMD_OBSERVABLE2 (11 octets)
 */
struct __attribute__((packed)) Observable2PacketLora {
    uint8_t  messageType;               ///< LoRaMessageType::OBSERVABLE2 (0x0A)
    uint8_t  buoyId;
    int8_t   autoPilotThrottleKpCmde;   ///< %
    int8_t   autoPilotThrottleKiCmde;
    int8_t   autoPilotThrottleKdCmde;
    int8_t   autoPilotRudderKpCmde;
    int8_t   autoPilotRudderKiCmde;
    int8_t   autoPilotRudderKdCmde;
    int8_t   commandeMoteurD;           ///< %
    int8_t   commandeMoteurG;           ///< %
    uint8_t  lateralHeadingCmdeNumber;  ///< saturé 255
};

/**
 * @brief Observables de mise au point : dérive GPS locale — réponse à CMD_OBSERVABLE_GPS (8 octets)
 *
 * ⚠️ Les quatre écarts dx/dy sont codés 0..51 m SANS signe (tableur). Voir
 * GATEWAY_DESIGN.md §3.4 bis : point ouvert, à trancher avant exploitation.
 */
struct __attribute__((packed)) ObservableGPSPacketLora {
    uint8_t  messageType;               ///< LoRaMessageType::OBSERVABLE_GPS (0x0B)
    uint8_t  buoyId;
    uint8_t  distanceToInitial;         ///< 0,2 m, saturé 255 (51 m)
    uint8_t  courseToInitial;           ///< BAM
    uint8_t  dxLatToInitial;            ///< 0,2 m
    uint8_t  dyLonToInitial;            ///< 0,2 m
    uint8_t  dxLat;                     ///< 0,2 m
    uint8_t  dyLon;                     ///< 0,2 m
};

// ─────────────────────────────────────────────────────────────────────────────
// Trames descendantes (maître → bouée)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Commande avec position — CMD_SET_HOME / CMD_SET_TARGET / CMD_SET_DEGT (15 octets)
 *
 * Les 7 premiers octets sont identiques champ pour champ à CommandPacketLora
 * (messageType, targetBuoyId, command, timestamp).
 */
struct __attribute__((packed)) CommandPosPacketLora {
    uint8_t  messageType;               ///< COMMAND_POS (0x07) ou COMMAND_POS_BUOY_STATUS (0x09)
    uint8_t  targetBuoyId;              ///< 0..7 ou LORA_BROADCAST_ID
    uint8_t  command;                   ///< BuoyCommand
    uint32_t timestamp;                 ///< identifiant unique, anti-doublon
    int32_t  latitude;                  ///< deg × 1e7
    int32_t  longitude;                 ///< deg × 1e7
};

// ─────────────────────────────────────────────────────────────────────────────
// Tailles — les gardiens de la copie littérale
// ─────────────────────────────────────────────────────────────────────────────
static const size_t LORA_COMMAND_PACKET_SIZE = 7;   ///< CommandPacketLora (défini par chaque dépôt)
static const size_t LORA_ACK_PACKET_SIZE     = 18;  ///< AckWithStatePacketLora (joystick v1)

static_assert(sizeof(BuoyStatusPacketLora)    == 58, "BuoyStatusPacketLora doit faire 58 octets");
static_assert(sizeof(ObservablePacketLora)    == 11, "ObservablePacketLora doit faire 11 octets");
static_assert(sizeof(Observable2PacketLora)   == 11, "Observable2PacketLora doit faire 11 octets");
static_assert(sizeof(ObservableGPSPacketLora) ==  8, "ObservableGPSPacketLora doit faire 8 octets");
static_assert(sizeof(CommandPosPacketLora)    == 15, "CommandPosPacketLora doit faire 15 octets");

/**
 * @brief Taille de la trame dont l'octet de type est donné, 0 si type inconnu
 *
 * Sert au découpage d'une lecture UART contenant plusieurs trames
 * (LORA_PROTOCOL.md §8.3). Un type inconnu doit interrompre le parcours,
 * jamais être enjambé « au hasard ».
 */
inline size_t loRaFrameSize(uint8_t type) {
    switch ((LoRaMessageType)type) {
        case LoRaMessageType::COMMAND:
        case LoRaMessageType::COMMAND_BUOY_STATUS:     return LORA_COMMAND_PACKET_SIZE;
        case LoRaMessageType::COMMAND_POS:
        case LoRaMessageType::COMMAND_POS_BUOY_STATUS: return sizeof(CommandPosPacketLora);
        case LoRaMessageType::ACK:                     return LORA_ACK_PACKET_SIZE;
        case LoRaMessageType::BUOY_STATUS:             return sizeof(BuoyStatusPacketLora);
        case LoRaMessageType::OBSERVABLE:              return sizeof(ObservablePacketLora);
        case LoRaMessageType::OBSERVABLE2:             return sizeof(Observable2PacketLora);
        case LoRaMessageType::OBSERVABLE_GPS:          return sizeof(ObservableGPSPacketLora);
        default:                                       return 0;
    }
}

/// La requête attend-elle un BUOY_STATUS (vrai) ou un ACK v1 (faux) ?
inline bool loRaRequestWantsBuoyStatus(uint8_t type) {
    return type == (uint8_t)LoRaMessageType::COMMAND_BUOY_STATUS ||
           type == (uint8_t)LoRaMessageType::COMMAND_POS_BUOY_STATUS;
}

#endif // LORA_PROTOCOL_H
