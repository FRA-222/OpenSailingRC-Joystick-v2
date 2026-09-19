/**
 * @file LoRaCommunication.h
 * @brief LoRa communication management with buoys using M5Stack LoRa 920 module
 * @author Philippe Hubert
 * @date 2025
 * 
 * This module handles bidirectional LoRa communication between the joystick
 * and autonomous GPS buoys using the M5Stack LoRa 920MHz module.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef LORA_COMMUNICATION_H
#define LORA_COMMUNICATION_H

#include <Arduino.h>
#include <M5_LoRa_E220_JP.h>
#include "CommandManager.h"
#include "ICommunication.h"
#include "HardwareConfig.h"   // LORA_RX_PIN / LORA_TX_PIN selon le matériel
#include "LoRaProtocol.h"   // trames partagées bouée / joystick / passerelle (copie littérale)

// Forward declaration
class DisplayManager;

// LoRa E220 module uses UART communication (Serial2), brochage dans HardwareConfig.h.
//
// IMPORTANT: M0/M1 pins sur le module M5Stack LoRa E220-JP
// sont contrôlées par un SWITCH sur le module:
// - Pour CONFIG: Switch sur ON (M0=HIGH, M1=HIGH)
// - Pour NORMAL: Switch sur OFF (M0=LOW, M1=LOW)
//
// CONFIGURATION DU MODE DE DÉMARRAGE
// Décommentez UNE SEULE des deux lignes suivantes:
#define LORA_MODE_NORMAL          // Mode normal (transmission/réception) - À UTILISER EN PRODUCTION
//#define LORA_MODE_CONFIGURATION   // Mode configuration (première fois ou changement de paramètres)
//
// Si votre module n'a pas de switch, décommentez LORA_USE_SOFTWARE_M0M1
// et connectez M0/M1 aux GPIOs indiqués ci-dessous
//
// #define LORA_USE_SOFTWARE_M0M1  // Décommentez si M0/M1 sont connectés aux GPIOs

// Les pins UART (LORA_RX_PIN / LORA_TX_PIN) et M0/M1/AUX sont définies par
// matériel dans HardwareConfig.h (v1 AtomS3 : G1/G2 ; v2 Core2 : Port B G36/G26).

// Maximum number of manageable buoys
#define MAX_BUOYS 8

// ── Instrument de bilan de liaison (essais de portée) ────────────────────────
// Valeur sentinelle : le RSSI de bruit ambiant n'a pas pu être lu.
#define LINK_NOISE_UNKNOWN (-32768)

// Relire le bruit ambiant à chaque bilan, et pas seulement au démarrage.
//
// ⚠️ Laissé à 0 par défaut, volontairement. La lecture se fait par la commande
// C0 C1 C2 C3 sur le MÊME UART que les trames reçues : si une trame arrive
// pendant la fenêtre de lecture, ses premiers octets sont consommés par le
// parseur de la réponse et la trame est perdue. Une trame perdue par le
// dispositif de mesure fausse précisément la grandeur que l'on mesure — le
// taux de réception. Le bruit ambiant d'un site ne dérivant pas en quelques
// minutes, la lecture unique de begin() suffit à un essai de portée.
//
// À passer à 1 seulement pour instrumenter une source d'interférence variable,
// en sachant que le taux de réception affiché devient alors pessimiste.
#define LINK_POLL_NOISE 0

// ── Diagnostic de bruit (§ scanChannelNoise / logNoiseProfile) ───────────────
// Passer à 1, flasher, lire la trace au démarrage, repasser à 0.
//
// Le diagnostic tourne AVANT tout trafic et monopolise l'UART du module : il
// n'a pas sa place dans un firmware d'exploitation. Le déclencher au démarrage
// plutôt que par un bouton est délibéré — on veut relever le bruit dans des
// conditions qu'on fait varier (USB branché/débranché, écran allumé/éteint,
// WiFi actif/coupé), et un redémarrage est justement la façon la plus simple
// de repartir d'un état propre entre deux relevés.
#define LORA_NOISE_DIAG 0

// Plage de canaux balayée par le scan. En 433, f = 410,125 + canal (MHz).
//
// ⚠️ ÉCOUTER n'est pas ÉMETTRE. Le balayage large est légitime en réception —
// il sert à voir la FORME du profil de bruit, ce qui distingue une pollution
// propre (profil plat) d'une interférence externe (profil bosselé). Mais la
// bande ISM 433 utilisable en émission ne va que de 433,05 à 434,79 MHz, soit
// les canaux 23 et 24 UNIQUEMENT. Voir logNoiseProfile() et GATEWAY_DESIGN §5.3.
#define LORA_SCAN_CH_MIN 0
#define LORA_SCAN_CH_MAX 83

// Canaux réellement exploitables en émission dans la bande ISM 433 MHz.
#define LORA_ISM433_CH_MIN 23   // 433,125 MHz — canal en service
#define LORA_ISM433_CH_MAX 24   // 434,125 MHz — le seul canal de repli

// ── Bandes LoRa supportées ───────────────────────────────────────────────────
// Le même module E220 (et la même bibliothèque) est utilisé dans les deux cas :
// seule la configuration radio change. La fréquence n'est PAS détectable par
// logiciel, c'est donc le mode de communication (CommMode::LORA_920 / LORA_433)
// qui détermine la bande, et le switch M0/M1 du module qui gère sa
// configuration. Les deux extrémités (joystick et bouée) doivent être réglées
// sur la même bande, sinon la liaison est silencieusement inexistante.
enum class LoRaBand {
    BAND_920,   ///< Module E220-900T22S(JP) : F = 920.6 + canal * 0.2 MHz
    BAND_433    ///< Module E220-400T22S     : F = 410.125 + canal * 1 MHz
};

/**
 * @brief Débit air du lien LoRa, indépendant de la bande
 *
 * Niveaux ordonnés du plus lent (portée maxi) au plus rapide (portée mini).
 * Chaque cran divise environ par deux le temps d'antenne et coûte environ 3 dB
 * de sensibilité.
 *
 * IMPORTANT : doit être IDENTIQUE côté bouée (constante LORA_AIR_RATE de
 * BuoyConfiguration.cpp). Un désaccord ne produit aucune erreur, seulement un
 * silence total.
 *
 * Correspondance approximative (BW125 sauf mention) :
 *   AIR_2400   SF9    ~1.8 kbps   sensibilité ~ -129 dBm
 *   AIR_4800   SF8    ~3.1 kbps               ~ -126 dBm
 *   AIR_9600   SF7    ~5.5 kbps               ~ -124 dBm
 *   AIR_19200  SF6    ~9.4 kbps               ~ -121 dBm
 *   AIR_38400  SF5   ~15.6 kbps               ~ -118 dBm
 *   AIR_62500  SF5/BW500 ~62.5 kbps           ~ -112 dBm
 *
 * SF5 est le plancher de la famille LoRa : il n'existe ni SF4 ni SF3.
 */
enum class LoRaAirRate {
    AIR_2400,   ///< Le plus lent, portée maximale (réglage historique)
    AIR_4800,
    AIR_9600,   ///< Visé par le protocole passerelle (GATEWAY_DESIGN.md)
    AIR_19200,
    AIR_38400,
    AIR_62500   ///< Le plus rapide, portée minimale
};

// LoRa E220 configuration
// IMPORTANT : le canal doit être IDENTIQUE côté joystick et côté bouée.
#define LORA_CHANNEL_920 0x00       // 920.600 MHz (bande ISM japonaise)
#define LORA_CHANNEL_433 23         // 433.125 MHz (bande ISM européenne)
#define LORA_ADDRESS_H 0x00         // High byte of address
#define LORA_ADDRESS_L 0x07         // Low byte of address (Joystick) - Modifié pour correspondre à la Bouée
#define LORA_NETID 0x00             // Network ID
#define LORA_UART_BAUD 9600         // UART baud rate (default)

/**
 * @brief Buoy state structure (received via LoRa)
 *
 * Stockage interne, en unités physiques, alimenté soit par l'ACK v1 (18 o),
 * soit par le BuoyStatusPacketLora v2 (58 o, décodé par processBuoyStatus).
 */
struct BuoyStateLora {
    uint8_t buoyId;                     ///< Buoy ID (0-7)
    uint32_t timestamp;                 ///< Message timestamp
    uint8_t preparedBuoyId;             ///< v2 — identité en préparation
    double latitude;                    ///< v2 — deg (0 si gpsOk faux)
    double longitude;                   ///< v2 — deg
    float trueHeading;                  ///< v2 — deg (BAM décodé)
    int8_t autoPilotRudderCmde;         ///< v2 — %
    uint8_t monitoringStatus;           ///< v2 — MonitoringStatusBit brut
    uint8_t sensorsValidities;          ///< v2 — SensorsValidityBit brut
    float courseGps;                    ///< v2 (OBSERVABLE) — deg
    float speedGps;                     ///< v2 (OBSERVABLE) — m/s
    uint8_t nbSat;                      ///< v2 (OBSERVABLE)
    uint32_t lastObservableTime;        ///< v2 — millis() de la dernière OBSERVABLE reçue (0 = jamais)
    
    // General state
    tEtatsGeneral generalMode;          ///< General state (INIT, READY, MAINTENANCE, HOME_DEFINITION, NAV)
    tEtatsNav navigationMode;           ///< Current navigation mode
    
    // Sensor status
    bool gpsOk;                         ///< GPS sensor status
    bool headingOk;                     ///< Heading sensor status
    bool yawRateOk;                     ///< Yaw rate sensor status
    
    // Environmental data
    uint8_t temperature;                  ///< Temperature in °C
    
    // Battery data
    uint8_t remainingCapacity;            ///< Remaining battery capacity in %
    
    // Navigation data
    uint8_t distanceToCons;               ///< Distance to consigne/waypoint in meters
    
    // Autopilot commands
    int8_t autoPilotThrottleCmde;       ///< Autopilot throttle command (-100 to +100%)
    int16_t autoPilotTrueHeadingCmde;    ///< Autopilot heading command in degrees (0-359)
};

// LoRaMessageType est défini dans LoRaProtocol.h (copie littérale partagée).

/**
 * @brief Command packet structure (sent via LoRa)
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) CommandPacketLora {
    LoRaMessageType messageType;  ///< Message type (COMMAND)
    uint8_t targetBuoyId;         ///< Target buoy ID
    BuoyCommand command;          ///< Command type
    uint32_t timestamp;           ///< Timestamp
};

/**
 * @brief ACK packet structure (received from Buoy) - LEGACY simple ACK
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) AckPacketLora {
    LoRaMessageType messageType;  ///< Message type (ACK)
    uint8_t buoyId;               ///< Buoy ID sending the ACK
    uint32_t commandTimestamp;    ///< Timestamp of the acknowledged command
    BuoyCommand commandType;      ///< Type of acknowledged command
};

/**
 * @brief ACK with buoy state packet (received from Buoy)
 * Enriched ACK containing full buoy state for immediate display update.
 * IMPORTANT: Packed to avoid padding issues
 */
struct __attribute__((packed)) AckWithStatePacketLora {
    // ACK identification
    LoRaMessageType messageType;        ///< Message type (ACK)
    uint8_t buoyId;                     ///< Buoy ID sending the ACK
    uint32_t commandTimestamp;          ///< Timestamp of the acknowledged command
    BuoyCommand commandType;            ///< Type of acknowledged command
    
    // Buoy state data (same fields as BuoyStateLora)
    uint8_t generalMode;                ///< tEtatsGeneral as uint8_t
    uint8_t navigationMode;             ///< tEtatsNav as uint8_t
    bool gpsOk;                         ///< GPS sensor status
    bool headingOk;                     ///< Heading sensor status
    bool yawRateOk;                     ///< Yaw rate sensor status
    uint8_t temperature;                ///< Temperature in °C
    uint8_t remainingCapacity;          ///< Remaining battery capacity in %
    uint8_t distanceToCons;             ///< Distance to consigne in meters
    int8_t autoPilotThrottleCmde;       ///< Autopilot throttle command
    int16_t autoPilotTrueHeadingCmde;   ///< Autopilot heading command (0-359)
};

static_assert(sizeof(CommandPacketLora) == LORA_COMMAND_PACKET_SIZE, "CommandPacketLora doit faire 7 octets");
static_assert(sizeof(AckWithStatePacketLora) == LORA_ACK_PACKET_SIZE, "AckWithStatePacketLora doit faire 18 octets");

/**
 * @brief Pending command structure for retry mechanism
 */
struct PendingCommand {
    CommandPacketLora command;    ///< Command to send/retry
    uint32_t sentTime;            ///< Time when command was last sent
    uint8_t retryCount;           ///< Number of retries attempted
    bool ackReceived;             ///< Has ACK been received?
};

/**
 * @brief Buoy information structure
 */
struct BuoyInfoLora {
    bool registered;            ///< Is this buoy registered?
    uint8_t buoyId;            ///< Buoy ID (0-5)
    BuoyStateLora lastState;   ///< Last received state
    uint32_t lastUpdateTime;   ///< Last update timestamp
    int16_t lastRssi;          ///< Last RSSI value
    float lastSnr;             ///< Last SNR value
};

/**
 * @brief Class to manage LoRa communication with buoys
 */
class LoRaCommunication : public ICommunication {
public:
    /**
     * @brief Constructor
     * @param band Radio band of the plugged module (default: 920 MHz)
     *
     * The band cannot be probed at runtime: it must match the module actually
     * connected, hence the explicit choice through CommMode in main.cpp.
     */
    explicit LoRaCommunication(LoRaBand band = LoRaBand::BAND_920,
                               LoRaAirRate airRate = LoRaAirRate::AIR_2400);

    /**
     * @brief Set the air data rate before begin()
     * @param airRate Débit air, doit être identique côté bouée
     *
     * Comme la bande, le débit n'est pas négociable à l'exécution : il est
     * écrit dans un registre du module, et les deux extrémités doivent être
     * réglées à l'identique sous peine de silence total.
     */
    void setAirRate(LoRaAirRate airRate);

    /**
     * @brief Libellé du débit air pour les traces ("9.6 kbps (SF7/BW125)")
     */
    const char* getAirRateName() const;

    /**
     * @brief Émet un bilan de liaison sur le port série toutes les 5 s
     *
     * Instrument d'essai de portée : appeler à chaque tour de boucle, il
     * s'auto-cadence. Affiche le débit courant, le nombre de trames reçues,
     * le RSSI moyen/min/max et la marge estimée par rapport à la sensibilité
     * du débit sélectionné. Une ligne « AUCUNE trame » signale une liaison
     * perdue sans ambiguïté.
     */
    /**
     * @brief Bilan de liaison périodique (toutes les 5 s), pour essais de portée
     *
     * @param activeBuoyId Bouée à laquelle les statistiques se rapportent.
     *        **Un changement de valeur clôture le palier en cours et remet les
     *        compteurs à zéro** — sans quoi les statistiques de deux bouées se
     *        mélangeraient dans le même taux de réception, ce qui les rendrait
     *        toutes deux ininterprétables. Passer `buoyState->getSelectedBuoyId()`.
     */
    void logLinkQuality(uint8_t activeBuoyId);

    /**
     * @brief Clôture le palier en cours et remet les compteurs à zéro
     *
     * Appelée automatiquement par logLinkQuality() sur changement de bouée.
     * Publique pour permettre de marquer aussi un changement de **configuration**
     * — palier de distance, antenne, débit air — dont le firmware n'a pas
     * connaissance.
     *
     * @param buoyId Bouée à laquelle se rapporte le NOUVEAU palier
     */
    void resetLinkStats(uint8_t buoyId);

    /** @brief Trace le cumul du palier qui s'achève (fenêtres, taux, pire série) */
    void logLinkSummary();


    /**
     * @brief Set the radio band before begin()
     * @param band Band of the plugged module
     *
     * Has no effect once begin() has run: the configuration is pushed to the
     * module during initialization.
     */
    void setBand(LoRaBand band);

    /**
     * @brief Get the channel number used for the current band
     */
    uint8_t getChannel() const;

    /**
     * @brief Get the center frequency of the current band/channel, in MHz
     */
    float getFrequencyMHz() const;

    /**
     * @brief Get the transmitting power register value for the current band
     *
     * ATTENTION : l'énumération TX_POWER_* de la bibliothèque est étiquetée pour
     * le variant JP. Le champ fait 2 bits dans REG1 et sa table de correspondance
     * dépend de la bande :
     *   - E220-900T22S(JP) : 0b00 = 13 dBm (maxi ARIB), 0b01 = 12, 0b10 = 7, 0b11 = 0
     *   - E220-400T22S     : 0b00 = 22 dBm, 0b01 = 17, 0b10 = 13, 0b11 = 10 dBm
     * Écrire TX_POWER_13dBm (0b00) sur un module 433 le ferait donc émettre à
     * 22 dBm. On sélectionne 0b11 = 10 dBm, compatible avec la limite de 10 mW
     * de la bande ISM 433 européenne.
     * À confirmer contre la datasheet du module 433 avant émission prolongée.
     */
    uint8_t getTxPower() const;

    /**
     * @brief Get the "air data rate" field (REG0) for the current band
     *
     * PIÈGE : le registre REG0 n'a PAS la même structure selon la variante.
     *   - E220-900T22S(JP) : bits[4:0] encodent le couple SF/BW (firmware JP
     *     spécifique). BW125K_SF9 = 0b10000 = 1758 bps.
     *   - E220-400T22S (et toute la famille EBYTE standard) :
     *       bits[4:3] = parité UART (00 = 8N1)
     *       bits[2:0] = débit air (0b010 = 2.4 kbps = SF9/BW125, défaut usine)
     *
     * Écrire BW125K_SF9 (0b10000) sur un module 433 met donc les bits de parité
     * à 0b10, soit 8E1, alors que l'ESP32 continue d'émettre en 8N1 : le module
     * devient sourd dès la fin de la configuration et plus aucune trame ne
     * passe. On écrit 0b010 en 433, qui donne 8N1 + 2.4 kbps, soit la même
     * modulation physique SF9/BW125 que le 920.
     *
     * Note de récupération : en mode configuration (switch M0/M1 sur ON) le
     * module force son UART à 9600 8N1, un module déjà mal configuré reste donc
     * reprogrammable.
     */
    uint8_t getAirDataRate() const;

    // ICommunication interface implementation
    bool begin() override;
    void update() override;  // Obsolète - gardé pour compatibilité
    bool sendCommand(uint8_t buoyId, const Command& cmd) override;
    BuoyState getLastBuoyState() override;
    BuoyState getBuoyState(uint8_t buoyId) override;
    bool hasNewData() override;
    void clearNewData() override;
    uint8_t getBuoyCount() const override;
    bool isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs = 120000) override;
    BuoyInfo* getBuoyInfo(uint8_t buoyId) override;
    BuoyInfo* getAllBuoys() override;
    int16_t getLastRssi() const override;
    float getLastSnr() const override;
    const char* getModeName() const override;
    void removeInactiveBuoys(uint32_t timeoutMs) override;
    
    /**
     * @brief Écoute passive des réponses bouée (non-bloquant)
     * BUOY_STATUS (v2) ou ACK (v1), envoyés par les bouées après COMMAND ou heartbeat
     */
    void listenForResponses();
    
    /**
     * @brief Vérifie et renvoie les commandes en attente d'ACK
     * Appelé régulièrement dans la boucle principale
     */
    void processCommandRetries();
    
    /**
     * @brief Set which buoy to poll in selective mode
     * @param buoyId Buoy ID to select (0-5)
     */
    void setSelectedBuoy(uint8_t buoyId);
    
    /**
     * @brief Set polling mode
     * @param onlySelected true = poll only selected buoy, false = poll all buoys
     */
    void setPollMode(bool onlySelected);
    
    /**
     * @brief Set display manager for visual feedback
     * @param display Pointer to DisplayManager
     */
    void setDisplayManager(DisplayManager* display);

private:
    LoRaBand band;                    ///< Radio band of the plugged module
    LoRaAirRate airRate;              ///< Débit air (identique côté bouée)

    // Bilan de liaison périodique (essais de portée) — voir logLinkQuality()
    //
    // ⚠️ Le E220 n'expose PAS le SNR. Le module encapsule le SX1262 et n'ajoute
    // qu'un octet de RSSI paquet ; ni son jeu de registres ni sa trame de
    // réception ne portent le SNR. Or c'est le SNR qui porte la marge réelle
    // dès que le signal passe sous le plancher de bruit — régime dans lequel
    // LoRa démodule encore, et où le RSSI paquet plafonne. La campagne de
    // portée du 30/08/2026 l'a montré : RSSI plat à ±2 dB de 50 m à 130 m,
    // alors que les trames se perdaient (GATEWAY_DESIGN.md § A.8).
    //
    // Ce que l'on mesure à la place, par ordre de valeur décroissante :
    //   1. le TAUX DE RÉCEPTION (fenêtres non vides / fenêtres) — seul
    //      indicateur qui ait suivi la distance pendant la campagne ;
    //   2. la SÉRIE DE FENÊTRES VIDES, qui décrit la frange intermittente ;
    //   3. le RAPPORT SIGNAL/BRUIT approché = RSSI paquet − RSSI de bruit
    //      ambiant, ce dernier étant lu une fois au démarrage (§ readAmbient…).
    uint32_t linkRxCount;             ///< Trames reçues depuis le dernier bilan
    int32_t  linkRssiSum;             ///< Somme des RSSI, pour la moyenne
    int16_t  linkRssiMin;             ///< RSSI le plus faible (le pire cas)
    int16_t  linkRssiMax;             ///< RSSI le plus fort
    uint32_t lastLinkLogTime;         ///< Horodatage du dernier bilan émis
    uint32_t linkWindows;             ///< Fenêtres de bilan écoulées
    uint32_t linkEmptyWindows;        ///< Fenêtres sans aucune trame
    uint16_t linkEmptyStreak;         ///< Fenêtres vides consécutives, en cours
    uint16_t linkEmptyStreakMax;      ///< Pire série de fenêtres vides
    int16_t  linkNoiseFloor;          ///< RSSI de bruit ambiant (dBm), LINK_NOISE_UNKNOWN si non lu
    uint8_t  linkStatsBuoyId;         ///< Bouée du palier en cours (0xFF = aucun palier ouvert)

    /**
     * @brief La configuration a-t-elle été réellement écrite dans le module ?
     *
     * `InitLoRaSetting()` ne réussit qu'en mode configuration (switch M0/M1 sur
     * ON). Sur un démarrage normal elle échoue, et le module **conserve la
     * configuration qu'il avait en mémoire** — qui peut différer de celle que
     * le firmware vient d'afficher. Un débit air discordant coupe alors la
     * liaison sans le moindre message d'erreur (GATEWAY_DESIGN.md §5.1).
     *
     * Le drapeau ne sert qu'au constat de démarrage : marquer chaque ligne de
     * bilan aurait ajouté une réserve permanente — les campagnes tournant
     * switch OFF — donc illisible à force d'être toujours présente.
     */
    bool configApplied = false;

    /** @brief Accumule un échantillon RSSI pour le bilan périodique */
    void noteLinkSample(int16_t rssi);

    /**
     * @brief Lit le RSSI de bruit ambiant du module (commande C0 C1 C2 C3)
     *
     * Seule grandeur du E220 qui approche le SNR : le bruit ambiant permet de
     * calculer un rapport signal/bruit ≈ RSSI_paquet − RSSI_bruit, là où la
     * « marge » d'une version précédente comparait le RSSI à la sensibilité de
     * datasheet — comparaison sans objet, puisque le RSSI plafonne au bruit.
     *
     * ⚠️ Consomme la réponse sur le MÊME UART que les trames de données. À
     * n'appeler que lorsque le lien est silencieux : cette fonction est appelée
     * une fois en fin de begin(), avant tout trafic. Voir LINK_POLL_NOISE.
     *
     * ⚠️⚠️ **NE JAMAIS APPELER EN MODE CONFIGURATION** (M0=M1=1). En mode
     * normal, `C0 C1 C2 C3` est la commande documentée de lecture du RSSI. En
     * mode configuration, `C0` est au contraire le code d'**écriture de
     * registres**, et les trois octets suivants sont lus comme adresse,
     * longueur et donnée : on enverrait une écriture malformée au module.
     * L'appelant doit garantir le mode normal — voir la garde dans begin().
     *
     * @return RSSI de bruit en dBm, ou LINK_NOISE_UNKNOWN si la lecture échoue
     */
    int16_t readAmbientNoiseRssi();

    /**
     * @brief Profil de bruit sur le canal courant — toujours disponible
     *
     * Lit le bruit ambiant N fois et affiche min / médiane / max / dispersion.
     * C'est le relevé qui répond à la première question du diagnostic : **la
     * valeur bouge-t-elle ?** Un vrai plancher de bruit varie de quelques dB
     * d'un échantillon à l'autre ; un plancher de *lecture* du module reste
     * figé, et il n'y a alors aucun décibel à récupérer.
     *
     * Ne nécessite aucun changement de canal, donc fonctionne switch M0/M1 en
     * position normale — contrairement à scanChannelNoise().
     */
    void logNoiseProfile(uint32_t sampleCount);

    /**
     * @brief Balayage du bruit ambiant canal par canal
     *
     * Le diagnostic qui commande tout le reste : la **forme** du profil dit
     * d'où vient le bruit.
     *   - profil plat sur toute la plage → bruit large bande, donc pollution
     *     propre à la carte (découpage, horloges, écran, câble USB) ; changer
     *     de canal ne servirait à rien ;
     *   - profil bosselé, avec des canaux nettement plus calmes → interférence
     *     externe.
     *
     * ⚠️ Exige LORA_USE_SOFTWARE_M0M1 : changer de canal impose de passer le
     * module en mode configuration (M0=M1=1) puis de revenir en mode normal
     * (M0=M1=0) pour lire le bruit. Avec un switch mécanique, la séquence n'est
     * pas automatisable ; la fonction le signale et se replie sur
     * logNoiseProfile().
     */
    void scanChannelNoise(uint8_t chFrom, uint8_t chTo);

#ifdef LORA_USE_SOFTWARE_M0M1
    /** @brief Bascule le module sur un canal, le temps d'un relevé de bruit */
    bool applyScanChannel(uint8_t ch);
#endif
    LoRa_E220_JP lora;                ///< LoRa E220 module instance
    LoRaConfigItem_t loraConfig;      ///< LoRa configuration structure
    BuoyInfoLora buoys[MAX_BUOYS];    ///< Array of buoy information
    uint8_t buoyCount;                ///< Number of registered buoys
    bool newDataAvailable;            ///< New data flag
    int16_t lastRssi;                 ///< Last received RSSI
    float lastSnr;                    ///< Last received SNR
    SemaphoreHandle_t loraMutex;      ///< Mutex pour protéger l'accès au module LoRa
    
    // Sequential polling state
    uint8_t currentPollIndex;         ///< Current buoy index being polled
    uint32_t lastPollTime;            ///< Last poll timestamp
    uint32_t pollInterval;            ///< Interval between polls (ms)
    uint32_t responseTimeout;         ///< Timeout waiting for response (ms)
    uint8_t selectedBuoyId;           ///< Selected buoy ID for selective polling
    bool pollOnlySelected;            ///< If true, poll only selected buoy
    
    // Command retry mechanism
    static const uint8_t MAX_PENDING_COMMANDS = 10;  ///< Maximum pending commands
    static const uint8_t MAX_RETRY_COUNT = 3;        ///< Maximum retry attempts
    static const uint32_t ACK_TIMEOUT_MS = 2000;     ///< Timeout for ACK (ms)
    static const uint32_t RETRY_INTERVAL_MS = 500;   ///< Interval between retries (ms)
    PendingCommand pendingCommands[MAX_PENDING_COMMANDS]; ///< Queue of pending commands
    uint8_t pendingCommandCount;                     ///< Number of pending commands
    DisplayManager* displayManager;                  ///< Pointer to display manager for visual feedback

    /**
     * @brief Find a buoy by ID
     * @param buoyId Buoy ID
     * @return Index in array or -1 if not found
     */
    int8_t findBuoyById(uint8_t buoyId);

    /**
     * @brief Add or update a buoy
     * @param buoyId Buoy ID
     * @return Index in array
     */
    int8_t addOrUpdateBuoy(uint8_t buoyId);

    /**
     * @brief Process received LoRa message
     * @param data Received data buffer
     * @param len Length of received data
     */
    void processReceivedMessage(const uint8_t* data, size_t len);
    
    /**
     * @brief Process received ACK (enriched with buoy state) — protocole v1 (0x04)
     * @param ack ACK+State packet received
     */
    void processAck(const AckWithStatePacketLora& ack);

    /**
     * @brief Process received BUOY_STATUS — protocole v2 (0x05, 58 o)
     *
     * Décode BAM / quartets / bits (LoRaCodec), met à jour l'état mémorisé et
     * acquitte la commande en attente via lastCmdTimestamp / lastCmdCode.
     */
    void processBuoyStatus(const BuoyStatusPacketLora& status);

    /**
     * @brief Process received OBSERVABLE — protocole v2 (0x06, 11 o)
     *
     * Réponse à CMD_OBSERVABLE : met à jour température, batterie, route et
     * vitesse GPS de la bouée. N'acquitte rien (la trame ne porte pas de
     * lastCmdTimestamp).
     */
    void processObservable(const ObservablePacketLora& obs);

    /**
     * @brief Marque acquittée la commande en attente qui correspond (bouée, timestamp, code)
     * @return true si une commande en attente a été trouvée
     */
    bool acknowledgePending(uint8_t buoyId, uint32_t commandTimestamp, uint8_t commandCode);
    
    /**
     * @brief Add command to pending queue
     * @param command Command packet to add
     * @return true if added successfully
     */
    bool addPendingCommand(const CommandPacketLora& command);
    
    /**
     * @brief Send command packet via LoRa
     * @param packet Command packet to send
     * @return true if sent successfully
     */
    bool sendCommandPacket(const CommandPacketLora& packet);
};

#endif // LORA_COMMUNICATION_H
