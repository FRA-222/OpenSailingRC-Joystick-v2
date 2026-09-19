/**
 * @file LoRaCommunication.cpp
 * @brief Implementation of LoRa communication with buoys
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

#include "LoRaCommunication.h"
#include "Logger.h"
#include "DisplayManager.h"

#ifndef LORA_USE_SOFTWARE_M0M1
#include <M5Unified.h>  // Needed for button press detection in jumper mode
#endif

static bool ecrLORACommunication = false;

/**
 * @brief Garde RAII qui restaure l'etat du Logger a la sortie de portee
 *
 * PIEGE CORRIGE : les drapeaux du Logger sont des statiques GLOBALES.
 * listenForResponses() les positionnait a l'entree sans jamais les restaurer ;
 * appelee 1000 fois par seconde par loraRxTask (Core 0), elle reduisait au
 * silence TOUT le firmware — traces ESP-NOW, boucle principale, bilans de
 * liaison compris. Le systeme tournait normalement, mais sans plus rien
 * afficher des que la tache RX demarrait.
 *
 * Toute fonction qui veut moduler la verbosite doit donc passer par cette
 * garde, qui restaure l'etat anterieur sur TOUS les chemins de sortie.
 */
/**
 * @brief Verrou des portees de verbosite — voir le piege ci-dessous
 *
 * Cree a la premiere utilisation (static local : initialisation thread-safe
 * garantie par le compilateur).
 */
static SemaphoreHandle_t loggerScopeMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

class LoggerScope {
public:
    /**
     * ⚠️ PIEGE CORRIGE LE 06/09/2026 — le firmware devenait definitivement muet.
     *
     * Cette classe sauvegarde puis restaure un etat GLOBAL du Logger, et elle
     * est utilisee depuis DEUX taches : listenForResponses() tourne dans
     * loraRxTask sur le Core 0, sendCommand() dans la boucle principale sur le
     * Core 1. Sans exclusion mutuelle, les paires sauvegarde/restauration
     * s'entrelacent :
     *
     *   1. boucle principale : sauve « serie=ACTIVE », met serie=INACTIVE
     *   2. tache RX          : sauve « serie=INACTIVE » (!), met serie=INACTIVE
     *   3. boucle principale : restaure serie=ACTIVE
     *   4. tache RX          : restaure serie=INACTIVE  ← plus rien ne s'affiche
     *
     * L'etat final depend de l'ordre d'entrelacement, donc le symptome est
     * intermittent : le joystick cesse de logger sans rien d'autre d'anormal,
     * et le systeme continue de fonctionner. Le mutex garantit qu'une seule
     * portee est active a la fois, donc que les paires s'imbriquent.
     *
     * Ordre de verrouillage : LoggerScope AVANT loraMutex, partout, sans quoi
     * les deux taches se bloqueraient mutuellement.
     *
     * En cas de contention (5 ms), on renonce simplement a modifier la
     * verbosite : mieux vaut une trace trop bavarde qu'un etat corrompu.
     */
    LoggerScope(bool serial, bool lcd) : owned(false), prevSerial(false), prevLcd(false) {
        if (xSemaphoreTake(loggerScopeMutex(), pdMS_TO_TICKS(5)) == pdTRUE) {
            owned = true;
            prevSerial = Logger::getSerialOutput();
            prevLcd = Logger::getLcdOutput();
            Logger::setSerialOutput(serial);
            Logger::setLcdOutput(lcd);
        }
    }
    ~LoggerScope() {
        if (owned) {
            Logger::setSerialOutput(prevSerial);
            Logger::setLcdOutput(prevLcd);
            xSemaphoreGive(loggerScopeMutex());
        }
    }
private:
    bool owned;
    bool prevSerial;
    bool prevLcd;
};

// Forward declarations for conversion functions
static BuoyState convertLoraToState(const BuoyStateLora& loraState);
static BuoyInfo convertLoraInfoToInfo(const BuoyInfoLora& loraInfo);

LoRaCommunication::LoRaCommunication(LoRaBand radioBand, LoRaAirRate radioAirRate) {
    band = radioBand;
    airRate = radioAirRate;
    buoyCount = 0;
    newDataAvailable = false;
    lastRssi = 0;
    lastSnr = 0.0f;
    linkRxCount = 0;
    linkRssiSum = 0;
    linkRssiMin = 0;
    linkRssiMax = -200;
    lastLinkLogTime = 0;
    linkWindows = 0;
    linkEmptyWindows = 0;
    linkEmptyStreak = 0;
    linkEmptyStreakMax = 0;
    linkNoiseFloor = LINK_NOISE_UNKNOWN;
    linkStatsBuoyId = 0xFF;   // aucun palier ouvert
    
    // Initialize sequential polling state
    currentPollIndex = 0;
    lastPollTime = 0;
    pollInterval = 1000;        // Poll every 1 second
    responseTimeout = 1000;     // Wait 1000ms for response (WOR_500MS + marge)
    selectedBuoyId = 0;         // Bouée sélectionnée par défaut (0)
    pollOnlySelected = true;    // Mode simple: toujours la bouée sélectionnée uniquement
    
    // Initialize buoy array
    for (int i = 0; i < MAX_BUOYS; i++) {
        buoys[i].registered = false;
        buoys[i].buoyId = i;
        buoys[i].lastUpdateTime = UINT32_MAX;  // Force disconnected au démarrage
        buoys[i].lastRssi = 0;
        buoys[i].lastSnr = 0.0f;
    }
    
    // Initialize command retry mechanism
    pendingCommandCount = 0;
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        pendingCommands[i].ackReceived = true;  // Mark as completed initially
        pendingCommands[i].retryCount = 0;
    }
    
    // Initialize display manager pointer
    displayManager = nullptr;
    
    // Créer le mutex pour protéger l'accès au module LoRa
    loraMutex = xSemaphoreCreateMutex();
    if (loraMutex == nullptr) {
        Logger::log("ERROR: Failed to create LoRa mutex!");
    }
}

void LoRaCommunication::setBand(LoRaBand radioBand) {
    band = radioBand;
}

void LoRaCommunication::setAirRate(LoRaAirRate radioAirRate) {
    airRate = radioAirRate;
}

const char* LoRaCommunication::getAirRateName() const {
    switch (airRate) {
        case LoRaAirRate::AIR_2400:  return "2.4 kbps (SF9/BW125)";
        case LoRaAirRate::AIR_4800:  return "4.8 kbps (SF8/BW125)";
        case LoRaAirRate::AIR_9600:  return "9.6 kbps (SF7/BW125)";
        case LoRaAirRate::AIR_19200: return "19.2 kbps (SF6/BW125)";
        case LoRaAirRate::AIR_38400: return "38.4 kbps (SF5/BW125)";
        case LoRaAirRate::AIR_62500: return "62.5 kbps (SF5/BW500)";
    }
    return "?";
}

uint8_t LoRaCommunication::getChannel() const {
    return (band == LoRaBand::BAND_433) ? LORA_CHANNEL_433 : LORA_CHANNEL_920;
}

float LoRaCommunication::getFrequencyMHz() const {
    // Formules de la datasheet E220 : pas de 1 MHz en 433, 0.2 MHz en 920 (JP)
    if (band == LoRaBand::BAND_433) {
        return 410.125f + getChannel() * 1.0f;
    }
    return 920.6f + getChannel() * 0.2f;
}

uint8_t LoRaCommunication::getTxPower() const {
    // Voir le commentaire détaillé dans LoRaCommunication.h : la table de
    // puissance du registre dépend de la bande, l'énumération de la
    // bibliothèque n'est valable que pour le variant JP.
    return (band == LoRaBand::BAND_433) ? 0b11 : (uint8_t)TX_POWER_13dBm;
}

void LoRaCommunication::noteLinkSample(int16_t rssi) {
    linkRxCount++;
    linkRssiSum += rssi;
    if (rssi < linkRssiMin) linkRssiMin = rssi;
    if (rssi > linkRssiMax) linkRssiMax = rssi;
}

int16_t LoRaCommunication::readAmbientNoiseRssi() {
    // Commande de lecture du RSSI du E220 : C0 C1 C2 C3. Le module repond
    // C1 <adr> <len> <bruit_ambiant> <rssi_derniere_trame>. Les deux octets de
    // RSSI sont en complement : valeur_dBm = octet - 256.
    //
    // rssi_ambient_noise_flag doit etre a RSSI_AMBIENT_NOISE_ENABLE dans la
    // config (c'est le cas, voir begin()) sans quoi le module ne repond pas.
    //
    // ⚠️ Cette commande n'est pas confirmee sur la datasheet du 400T22S — c'est
    // le meme registre que le piege documente dans LORA_DUAL_BAND_PLAN.md. La
    // reponse brute est donc tracee au premier appel : si le format differe,
    // il est lisible dans le log sans avoir a instrumenter davantage. En cas
    // d'echec la fonction renvoie LINK_NOISE_UNKNOWN et l'instrument continue
    // de fonctionner sans le rapport signal/bruit — c'est un bonus, pas un
    // prerequis.
    while (Serial2.available()) Serial2.read();  // partir d'un tampon propre

    Serial2.write(0xC0);
    Serial2.write(0xC1);
    Serial2.write(0xC2);
    Serial2.write(0xC3);
    Serial2.flush();

    uint8_t resp[8];
    uint8_t len = 0;
    uint32_t deadline = millis() + 150;
    while (millis() < deadline && len < sizeof(resp)) {
        if (Serial2.available()) resp[len++] = (uint8_t)Serial2.read();
    }

    if (len == 0) {
        Logger::log("   ⚠️  Bruit ambiant : aucune reponse du module (commande C0C1C2C3)");
        return LINK_NOISE_UNKNOWN;
    }

    String hex = "";
    for (uint8_t i = 0; i < len; i++) {
        if (resp[i] < 0x10) hex += "0";
        hex += String(resp[i], HEX);
        hex += " ";
    }
    Logger::logf("   📻 Bruit ambiant, reponse brute (%u o) : %s", len, hex.c_str());

    // L'avant-dernier octet utile est le bruit ambiant. On exige l'en-tete C1
    // pour ne pas interpreter une trame de donnees comme une reponse.
    if (resp[0] != 0xC1 || len < 4) {
        Logger::log("   ⚠️  Bruit ambiant : format de reponse inattendu, valeur ignoree");
        return LINK_NOISE_UNKNOWN;
    }

    int16_t noise = (int16_t)resp[len - 2] - 256;
    if (noise > -20 || noise < -140) {
        Logger::logf("   ⚠️  Bruit ambiant hors plage (%d dBm), valeur ignoree", (int)noise);
        return LINK_NOISE_UNKNOWN;
    }
    return noise;
}

// ─────────────────────────────────────────────────────────────────────────────
// Diagnostic de bruit — voir LoRaCommunication.h et GATEWAY_DESIGN.md §5.3
// ─────────────────────────────────────────────────────────────────────────────

void LoRaCommunication::logNoiseProfile(uint32_t sampleCount) {
    // Repond a la PREMIERE question du diagnostic : la valeur bouge-t-elle ?
    // Un plancher de bruit reel varie de quelques dB d'un echantillon a l'autre.
    // Un plancher de LECTURE du module reste fige — et il n'y a alors aucun
    // decibel a recuperer, quel que soit le travail fait sur le materiel.
    const uint32_t MAX_SAMPLES = 32;
    if (sampleCount > MAX_SAMPLES) sampleCount = MAX_SAMPLES;
    if (sampleCount == 0) return;

    int16_t v[MAX_SAMPLES];
    uint32_t n = 0;

    Logger::logf("🔬 Profil de bruit — canal %d (%.3f MHz), %lu echantillons",
                 getChannel(), getFrequencyMHz(), (unsigned long)sampleCount);

    uint32_t echecs = 0;
    for (uint32_t i = 0; i < sampleCount; i++) {
        int16_t r = readAmbientNoiseRssi();
        if (r != LINK_NOISE_UNKNOWN) {
            v[n++] = r;
            echecs = 0;
        } else if (++echecs >= 3) {
            // Inutile d'insister : si le module n'a pas repondu trois fois de
            // suite, il ne repondra pas davantage a la vingtieme. Repeter
            // l'echec ne fait que noyer le log de l'essai.
            Logger::logf("   ↳ abandon apres %lu echecs consecutifs", (unsigned long)echecs);
            break;
        }
        delay(50);
    }

    if (n == 0) {
        Logger::log("   ⚠️  Aucun echantillon valide — le module ne repond pas a C0C1C2C3.");
        Logger::log("   CAUSE STRUCTURELLE, verifiee le 05/09/2026 : le module est en");
        Logger::log("   UART_P2P_MODE, ou tout octet entrant par l'UART est EMIS PAR RADIO");
        Logger::log("   au lieu d'etre interprete comme une commande. Il n'existe donc aucun");
        Logger::log("   chemin de lecture du bruit ambiant en fonctionnement normal, et le");
        Logger::log("   bit RSSI_AMBIENT_NOISE_ENABLE n'y change rien (il a ete ecrit).");
        Logger::log("   ➜ Ne pas reessayer. Le plancher de bruit s'estime autrement : voir");
        Logger::log("     GATEWAY_DESIGN.md §5.3, par le PLATEAU du RSSI paquet.");
        return;
    }

    // Tri par insertion : n <= 32, inutile de faire mieux.
    for (uint32_t i = 1; i < n; i++) {
        int16_t k = v[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
        v[j + 1] = k;
    }

    int16_t vmin = v[0], vmax = v[n - 1], vmed = v[n / 2];
    int16_t spread = vmax - vmin;

    Logger::logf("   min %d | mediane %d | max %d dBm | dispersion %d dB (%lu/%lu valides)",
                 (int)vmin, (int)vmed, (int)vmax, (int)spread,
                 (unsigned long)n, (unsigned long)sampleCount);

    if (spread <= 1) {
        Logger::log("   ➜ Valeur FIGEE : c'est probablement un plancher de LECTURE du E220,");
        Logger::log("     pas un plancher de bruit. Aucun dB a recuperer de ce cote — ne pas");
        Logger::log("     engager de travail materiel sur cette base.");
    } else {
        Logger::logf("   ➜ Valeur VARIABLE (%d dB) : le relevé mesure bien du bruit reel.", (int)spread);
        if (vmed > -100) {
            Logger::logf("     Bruit thermique attendu en 125 kHz : ~-117 dBm. Ici %+d dB au-dessus.",
                         (int)(vmed + 117));
            Logger::log("     Refaire ce releve en changeant UNE condition a la fois, dans cet ordre :");
            Logger::log("       1. USB debranche, sur batterie   (suspect n.1 : bruit de mode commun)");
            Logger::log("       2. ESP-NOW coupe des DEUX cotes  (suspect n.2 : la bouee emet a 1 s,");
            Logger::log("          a quelques cm de son propre recepteur LoRa — GATEWAY_DESIGN §5.3)");
            Logger::log("       3. retroeclairage et ecran eteints");
            Logger::log("       4. antenne deportee de 20-30 cm du boitier");
        }
    }
}

void LoRaCommunication::scanChannelNoise(uint8_t chFrom, uint8_t chTo) {
#ifndef LORA_USE_SOFTWARE_M0M1
    (void)chFrom; (void)chTo;
    Logger::log("🔬 Scan de canaux : INDISPONIBLE sur ce montage.");
    Logger::log("   Changer de canal impose de passer le module en mode configuration");
    Logger::log("   (M0=M1=1) puis de revenir en mode normal (M0=M1=0) pour lire le bruit.");
    Logger::log("   Avec un switch mecanique, la sequence n'est pas automatisable : il faut");
    Logger::log("   cabler M0/M1 sur deux GPIO et definir LORA_USE_SOFTWARE_M0M1.");
    Logger::log("   Repli : profil de bruit sur le canal courant, qui suffit a dire si le");
    Logger::log("   plancher est reel (voir ci-dessous).");
    logNoiseProfile(20);
#else
    if (chFrom > chTo) { uint8_t t = chFrom; chFrom = chTo; chTo = t; }
    if (chTo > 83) chTo = 83;

    Logger::logf("🔬 Scan de bruit, canaux %u a %u — %u relevés", chFrom, chTo,
                 (unsigned)(chTo - chFrom + 1));
    Logger::log("   ⚠️  Ecoute seule. La bande ISM 433 utilisable en EMISSION se limite aux");
    Logger::logf("      canaux %u et %u (433,05-434,79 MHz) : un canal calme hors de cette",
                 LORA_ISM433_CH_MIN, LORA_ISM433_CH_MAX);
    Logger::log("      plage se diagnostique mais ne s'exploite pas.");
    Logger::log("   ⚠️  Chaque changement de canal ecrit les parametres en flash du module.");
    Logger::log("      Reserver ce scan au diagnostic, pas a une boucle permanente.");

    uint8_t saved = getChannel();
    int16_t best = 0, worst = -200;
    uint8_t bestCh = saved;
    uint32_t valid = 0;

    for (uint8_t ch = chFrom; ch <= chTo; ch++) {
        if (!applyScanChannel(ch)) {
            Logger::logf("   CH%02u : changement de canal refuse", ch);
            continue;
        }
        int16_t r = readAmbientNoiseRssi();
        float f = 410.125f + (float)ch;
        if (r == LINK_NOISE_UNKNOWN) {
            Logger::logf("   CH%02u %.3f MHz : lecture indisponible", ch, f);
            continue;
        }
        bool ism = (ch >= LORA_ISM433_CH_MIN && ch <= LORA_ISM433_CH_MAX);
        Logger::logf("   CH%02u %.3f MHz : %d dBm%s", ch, f, (int)r, ism ? "   [ISM, exploitable]" : "");
        if (valid == 0 || r < best) { best = r; bestCh = ch; }
        if (r > worst) worst = r;
        valid++;
    }

    applyScanChannel(saved);

    if (valid < 2) {
        Logger::log("   ➜ Trop peu de relevés valides pour conclure.");
        return;
    }

    int16_t shape = worst - best;
    Logger::logf("   ➜ Plus calme : CH%02u a %d dBm | plus bruyant : %d dBm | amplitude %d dB",
                 bestCh, (int)best, (int)worst, (int)shape);

    if (shape <= 6) {
        Logger::log("   ➜ Profil PLAT : bruit large bande, donc AUTO-POLLUTION de la carte.");
        Logger::log("     Changer de canal ne servira a rien. Traiter le materiel :");
        Logger::log("       1. USB debranche, sur batterie   2. ESP-NOW coupe des deux cotes");
        Logger::log("       3. ecran eteint                  4. antenne deportee du boitier");
    } else {
        Logger::log("   ➜ Profil BOSSELE : interference EXTERNE.");
        Logger::logf("     Mais seuls les canaux %u et %u sont exploitables en emission.",
                     LORA_ISM433_CH_MIN, LORA_ISM433_CH_MAX);
        Logger::log("     Si les deux sont bruyants, le changement de canal ne sauve rien et");
        Logger::log("     le vrai repli est la bande 868/920 (voir LORA_DUAL_BAND_PLAN.md).");
    }
#endif
}

#ifdef LORA_USE_SOFTWARE_M0M1
bool LoRaCommunication::applyScanChannel(uint8_t ch) {
    // Mode configuration : seul mode ou les registres sont accessibles.
    digitalWrite(LORA_M0_PIN, HIGH);
    digitalWrite(LORA_M1_PIN, HIGH);
    delay(60);

    loraConfig.own_channel = ch;
    loraConfig.target_channel = ch;
    int r = lora.InitLoRaSetting(loraConfig);

    // Retour en mode normal : la lecture du bruit ambiant (C0C1C2C3) s'y fait.
    digitalWrite(LORA_M0_PIN, LOW);
    digitalWrite(LORA_M1_PIN, LOW);
    delay(60);

    return (r == 0);
}
#endif

void LoRaCommunication::logLinkSummary() {
    if (linkStatsBuoyId == 0xFF || linkWindows == 0) return;

    uint32_t ok = linkWindows - linkEmptyWindows;
    Logger::logf("═══ BILAN Bouee #%u | %s | %lu fen. de 5 s | %lu recues (%lu%%) | "
                 "pire serie %u fen. (%u s)",
                 (unsigned)linkStatsBuoyId,
                 getAirRateName(),
                 (unsigned long)linkWindows,
                 (unsigned long)ok,
                 (unsigned long)(100UL * ok / linkWindows),
                 (unsigned)linkEmptyStreakMax,
                 (unsigned)(linkEmptyStreakMax * 5));
}

void LoRaCommunication::resetLinkStats(uint8_t buoyId) {
    // Cloturer le palier precedent AVANT de remettre a zero : sur une campagne
    // multi-bouees, c'est cette ligne qui porte le resultat du palier. Melanger
    // les statistiques de deux bouees dans un meme taux de reception les rendrait
    // toutes deux ininterpretables.
    logLinkSummary();

    linkRxCount = 0;
    linkRssiSum = 0;
    linkRssiMin = 0;
    linkRssiMax = -200;
    linkWindows = 0;
    linkEmptyWindows = 0;
    linkEmptyStreak = 0;
    linkEmptyStreakMax = 0;
    linkStatsBuoyId = buoyId;

    Logger::logf("📊 Stats de liaison remises a zero — palier Bouee #%u", (unsigned)buoyId);
}

void LoRaCommunication::logLinkQuality(uint8_t activeBuoyId) {
    // Bilan de liaison periodique — instrument d'essai de portee.
    //
    // Ce bilan affichait une colonne « marge ~N dB » calculee comme
    // RSSI - sensibilite_datasheet. Elle a ete RETIREE : elle indiquait encore
    // 35 a 42 dB au moment ou la liaison decrochait pendant la campagne du
    // 30/08/2026 (GATEWAY_DESIGN.md § A.8). La raison est que le RSSI paquet
    // du E220 plafonne au niveau de bruit des que le SNR devient negatif — il
    // est reste plat a ±2 dB de 50 m a 130 m, sur les six debits testes. La
    // marge reelle vit alors dans le SNR, que le module n'expose pas.
    //
    // La grandeur qui a effectivement suivi la distance est le TAUX DE
    // RECEPTION. C'est elle qui est mise en avant ici.
    const uint32_t LINK_LOG_INTERVAL = 5000;

    // Changement de bouee = nouveau palier. On cloture et on repart de zero,
    // avant meme de tester la cadence : les trames deja accumulees appartiennent
    // a la bouee precedente.
    if (activeBuoyId != linkStatsBuoyId) {
        resetLinkStats(activeBuoyId);
        lastLinkLogTime = millis();   // repartir sur une fenetre pleine
        return;
    }

    uint32_t now = millis();
    if (now - lastLinkLogTime < LINK_LOG_INTERVAL) return;
    lastLinkLogTime = now;

#if LINK_POLL_NOISE
    linkNoiseFloor = readAmbientNoiseRssi();
#endif

    linkWindows++;

    if (linkRxCount == 0) {
        linkEmptyWindows++;
        linkEmptyStreak++;
        if (linkEmptyStreak > linkEmptyStreakMax) linkEmptyStreakMax = linkEmptyStreak;

        Logger::logf("📶 LIAISON B#%u %s | AUCUNE trame sur %lu s | %u fen. vides d'affilee | recu %lu/%lu fen. (%lu%%)",
                     (unsigned)linkStatsBuoyId,
                     getAirRateName(),
                     (unsigned long)(LINK_LOG_INTERVAL / 1000),
                     (unsigned)linkEmptyStreak,
                     (unsigned long)(linkWindows - linkEmptyWindows),
                     (unsigned long)linkWindows,
                     (unsigned long)(100UL * (linkWindows - linkEmptyWindows) / linkWindows));
    } else {
        linkEmptyStreak = 0;

        // Rapport signal/bruit approche — le plus proche d'un SNR que le E220
        // permette. Non affiche si le bruit n'a pas pu etre lu.
        char sb[24] = "";
        if (linkNoiseFloor != LINK_NOISE_UNKNOWN) {
            snprintf(sb, sizeof(sb), " | S/B ~%+d dB", (int)(linkRssiMin - linkNoiseFloor));
        }

        Logger::logf("📶 LIAISON B#%u %s | %lu trames | RSSI moy %d | min %d | max %d dBm%s | recu %lu/%lu fen. (%lu%%)",
                     (unsigned)linkStatsBuoyId,
                     getAirRateName(),
                     (unsigned long)linkRxCount,
                     (int)(linkRssiSum / (int32_t)linkRxCount),
                     (int)linkRssiMin, (int)linkRssiMax,
                     sb,
                     (unsigned long)(linkWindows - linkEmptyWindows),
                     (unsigned long)linkWindows,
                     (unsigned long)(100UL * (linkWindows - linkEmptyWindows) / linkWindows));
    }

    // Rappel de la pire serie, qui decrit la frange intermittente : c'est elle
    // qui dimensionne DELAY_PARTIAL_REFRESH_DL cote bouee (GATEWAY_DESIGN §4.8).
    if (linkEmptyStreakMax >= 2 && linkEmptyStreak == 0) {
        Logger::logf("   ↳ pire serie depuis le demarrage : %u fen. vides (%u s sans liaison)",
                     (unsigned)linkEmptyStreakMax,
                     (unsigned)(linkEmptyStreakMax * (LINK_LOG_INTERVAL / 1000)));
    }

    linkRxCount = 0;
    linkRssiSum = 0;
    linkRssiMin = 0;
    linkRssiMax = -200;
}

uint8_t LoRaCommunication::getAirDataRate() const {
    // Voir le commentaire détaillé dans LoRaCommunication.h : la structure de
    // REG0 diffère entre le variant JP et le E220-400T22S, et une mauvaise
    // valeur casse la parité UART du module.
    if (band == LoRaBand::BAND_433) {
        // E220-400T22S : bits[4:3] = 00 (8N1) pour toutes les valeurs
        // ci-dessous, bits[2:0] = débit air. Valeurs nominales de la datasheet
        // EBYTE ; la correspondance exacte vers SF/BW n'est pas vérifiée.
        switch (airRate) {
            case LoRaAirRate::AIR_2400:  return 0b010;  // défaut usine
            case LoRaAirRate::AIR_4800:  return 0b011;
            case LoRaAirRate::AIR_9600:  return 0b100;
            case LoRaAirRate::AIR_19200: return 0b101;
            case LoRaAirRate::AIR_38400: return 0b110;
            case LoRaAirRate::AIR_62500: return 0b111;
        }
        return 0b010;
    }
    // E220-900T22S(JP) : bits[4:0] encodent directement le couple SF/BW.
    switch (airRate) {
        case LoRaAirRate::AIR_2400:  return (uint8_t)BW125K_SF9;   // 1758 bps
        case LoRaAirRate::AIR_4800:  return (uint8_t)BW125K_SF8;   // 3125 bps
        case LoRaAirRate::AIR_9600:  return (uint8_t)BW125K_SF7;   // 5469 bps
        case LoRaAirRate::AIR_19200: return (uint8_t)BW125K_SF6;   // 9375 bps
        case LoRaAirRate::AIR_38400: return (uint8_t)BW125K_SF5;   // 15625 bps
        case LoRaAirRate::AIR_62500: return (uint8_t)BW500K_SF5;   // 62500 bps
    }
    return (uint8_t)BW125K_SF9;
}

bool LoRaCommunication::begin() {
    Logger::logf("✓ LoRa: Initialisation E220 %s...",
                 (band == LoRaBand::BAND_433) ? "433 MHz" : "920 MHz (JP)");
    
    Logger::log("⚙️  LoRa mode will be determined by the M0/M1 switch position.");
    Logger::log("");
    Logger::log("INFO: M0/M1 controlled by HARDWARE SWITCH on module");
    Logger::log("  -> Switch ON = configuration mode");
    Logger::log("  -> Switch OFF = normal mode");
    Logger::log("");

#ifdef LORA_USE_SOFTWARE_M0M1
    // Configure M0 and M1 pins as outputs (si contrôle logiciel activé)
    pinMode(LORA_M0_PIN, OUTPUT);
    pinMode(LORA_M1_PIN, OUTPUT);
    delay(500);  // Wait for mode change
#else
    Logger::log("ℹ️  LoRa: M0/M1 contrôlés par SWITCH sur le module");
#ifdef LORA_MODE_CONFIGURATION
    Logger::log("   → Assurez-vous que le switch est sur ON (config)");
#else
    Logger::log("   → Assurez-vous que le switch est sur OFF (normal)");
#endif
    delay(500);
#endif
    
    // Initialize UART for E220-JP module
    lora.Init(&Serial2, CONFIG_MODE_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    
    Logger::logf("✓ LoRa: UART2 initialisé (RX=GPIO%d, TX=GPIO%d, baud=%d)", 
                 LORA_RX_PIN, LORA_TX_PIN, CONFIG_MODE_BAUD);
    
    // Test if Serial2 is available
    if (!Serial2) {
        Logger::log("✗ LoRa: Serial2 non disponible!");
        return false;
    }
    
    // Clear any pending data in buffer
    while (Serial2.available()) {
        Serial2.read();
    }
    delay(100);
    
    Logger::log("✓ LoRa: Port série vérifié et nettoyé");
    Logger::log("");
    
    // Unified runtime behavior: test UART, prepare config and attempt to apply it.
    // If the module is in CONFIG mode (M0/M1=HIGH) the configuration will be applied.
    // If the module is in NORMAL mode (M0/M1=LOW) the InitLoRaSetting() call will
    // typically fail, and we fall back to using the existing configuration.

    // === UART COMMUNICATION TEST ===
    Logger::log("🔍 Test de communication UART...");
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.write(0xC1);
    Serial2.flush();
    delay(300);

    if (Serial2.available()) {
        Logger::log("✓ Module LoRa E220-JP répond correctement (UART test).");
        while (Serial2.available()) Serial2.read();
        // Le test UART est le SEUL indicateur fiable du mode reel. En mode
        // normal (P2P transparent) tout octet ecrit sur l'UART est EMIS PAR
        // RADIO et rien ne revient ; une reponse ne peut donc venir que du
        // mode configuration. Le dire explicitement : le mode ne se deduisait
        // jusqu'ici que de deux lignes eloignees, ce qui est une source
        // d'erreur garantie sur une flottille a configurer.
        Logger::log("");
        Logger::log("🔎 LoRa: MODULE EN MODE CONFIGURATION — la liaison radio est INACTIVE.");
        Logger::log("   Etat attendu UNIQUEMENT pour ecrire la configuration.");
        Logger::log("   ➜ Pour exploiter : basculer le switch M0/M1, COUPER L'ALIMENTATION,");
        Logger::log("     puis redemarrer. Un simple reset ne suffit pas — le firmware ne");
        Logger::log("     coupe jamais l'alimentation du module.");
    } else {
        Logger::log("ℹ️  Pas de réponse immédiate au test UART (possible si switch OFF)");
        // Aucune reponse = mode normal : en P2P transparent, les octets ecrits
        // sur l'UART partent par radio et rien ne revient. C'est l'etat
        // d'exploitation.
        Logger::log("");
        Logger::log("🔎 LoRa: MODULE EN MODE NORMAL — liaison radio active.");
    }

    Logger::log("");

    // Set default configuration values
    lora.SetDefaultConfigValue(loraConfig);

    // Configure LoRa E220-JP parameters
    loraConfig.own_address = 0x0000;  // Adresse joystick / broadcast
    loraConfig.baud_rate = BAUD_9600;
    loraConfig.air_data_rate = getAirDataRate();
    loraConfig.subpacket_size = SUBPACKET_200_BYTE;
    loraConfig.rssi_ambient_noise_flag = RSSI_AMBIENT_NOISE_ENABLE;
    loraConfig.transmitting_power = getTxPower();
    loraConfig.own_channel = getChannel();
    loraConfig.rssi_byte_flag = RSSI_BYTE_ENABLE;
    loraConfig.transmission_method_type = UART_P2P_MODE;
    loraConfig.lbt_flag = LBT_DISABLE;
    loraConfig.wor_cycle = WOR_500MS;
    loraConfig.encryption_key = 0x1234;
    loraConfig.target_address = 0x0000;
    loraConfig.target_channel = getChannel();

    Logger::log("✓ LoRa: Configuration prepared");
    Logger::logf("   - Canal: %d (%.3f MHz)", getChannel(), getFrequencyMHz());
    // String(v, BIN) supprime les zeros de tete : 0b010 s'afficherait "10", ce
    // qui se lit comme bits[4:3]=10, c'est-a-dire 8E1 — exactement le piege de
    // parite que cette trace sert a detecter. On pad sur 5 bits, largeur reelle
    // du champ REG0[4:0].
    String rateBits = String(getAirDataRate(), BIN);
    while (rateBits.length() < 5) rateBits = "0" + rateBits;
    // Debit SOUHAITE : la ligne s'intitule « Configuration prepared », elle
    // decrit ce qu'on s'apprete a ecrire. Le constat de ce qui a reellement
    // ete applique vient juste apres la tentative d'ecriture.
    Logger::logf("   - Air data rate: %s - REG0 0b%s%s",
                 getAirRateName(),
                 rateBits.c_str(),
                 (band == LoRaBand::BAND_433) ? " (8N1)" : "");
    Logger::logf("   - Puissance: registre 0b%s (%s)",
                 (band == LoRaBand::BAND_433) ? "11" : "00",
                 (band == LoRaBand::BAND_433) ? "10 dBm" : "13 dBm");
    Logger::log("");

    Logger::log("⏳ LoRa: Sending configuration to module (will succeed only if switch ON)...");
    int result = lora.InitLoRaSetting(loraConfig);

    if (result == 0) {
        Logger::log("# LoRa: Module E220-JP configured successfully!");
        Logger::log("");
        Logger::log("ℹ️  IMPORTANT: Pour les prochains démarrages, mettez le switch M0/M1 sur OFF.");
        Logger::log("");

#ifdef LORA_USE_SOFTWARE_M0M1
        // If we control M0/M1 via GPIO, set them LOW for normal mode
        digitalWrite(LORA_M0_PIN, LOW);
        digitalWrite(LORA_M1_PIN, LOW);
        Logger::log("✓ LoRa: Mode normal activé (M0=LOW, M1=LOW via GPIO)");
        delay(200);
#endif

    } else {
        Logger::log("📡 LoRa: Configuration command failed (assume switch OFF - normal mode).");
        Logger::log(String("   Result code: ") + String(result));
        Logger::log("");
        Logger::log("   Using existing module configuration for normal operation.");
        Logger::log("");
        // Still call InitLoRaSetting to initialize internal mutex/state of library
        // (some libraries require this even if module rejects config)
        lora.InitLoRaSetting(loraConfig);
    }

    Logger::log("✓ LoRa: Ready to operate");
    Logger::log("");

    // Plancher de bruit du site, lu UNE fois, ici, parce que c'est le seul
    // moment ou l'UART est garanti silencieux : aucune trame n'a encore ete
    // recue. Le lire plus tard couterait des trames (voir LINK_POLL_NOISE).
    //
    // C'est ce qui rend exploitable le bilan de liaison : sans plancher de
    // bruit, un RSSI de -85 dBm ne dit pas si l'on est a 3 dB ou a 30 dB du
    // decrochage. La campagne du 30/08/2026 a bute exactement la-dessus
    // (GATEWAY_DESIGN.md § A.8).
    // Le module n'est en mode NORMAL que si l'ecriture de configuration a
    // ECHOUE : InitLoRaSetting() ne reussit qu'en mode configuration. Un succes
    // signifie donc switch sur ON, et il ne faut alors surtout pas envoyer la
    // sequence de lecture du bruit — voir readAmbientNoiseRssi().
    const bool moduleEnModeConfig = (result == 0);
    configApplied = moduleEnModeConfig;

    if (!configApplied) {
        // Constat factuel, pas une alerte : avec le switch sur OFF l'ecriture
        // echoue TOUJOURS, c'est le cas nominal. Un avertissement qui se
        // declenche a chaque demarrage devient invisible ; on se contente donc
        // de dire ce qui est, et ou regarder si la liaison ne passe pas.
        Logger::log("ℹ️  LoRa: configuration non ecrite a ce demarrage (switch M0/M1 sur OFF).");
        Logger::log("   Le module utilise celle memorisee lors du dernier demarrage switch ON.");
        Logger::log("   Les valeurs ci-dessus sont celles DEMANDEES par le firmware.");
        Logger::log("   Liaison muette ? verifier ce point en premier (GATEWAY_DESIGN §5.1).");
        Logger::log("");
    }

    if (moduleEnModeConfig) {
        Logger::log("ℹ️  LoRa: module en mode CONFIGURATION — mesure de bruit reportee.");
        Logger::log("   La configuration vient d'etre ecrite, dont le bit de bruit ambiant.");
        Logger::log("   ➜ Mettre le switch M0/M1 sur OFF et REDEMARRER : c'est au demarrage");
        Logger::log("     suivant que le plancher de bruit sera mesure.");
        linkNoiseFloor = LINK_NOISE_UNKNOWN;
    } else {
        linkNoiseFloor = readAmbientNoiseRssi();
    }

    if (linkNoiseFloor != LINK_NOISE_UNKNOWN) {
        Logger::logf("✓ LoRa: plancher de bruit mesure : %d dBm", (int)linkNoiseFloor);
        if (linkNoiseFloor > -100) {
            Logger::logf("   ⚠️  Bruit eleve — le bruit thermique attendu en 125 kHz est"
                         " d'environ -117 dBm. %d dB au-dessus : interference locale ou"
                         " rayonnement propre a suspecter, la portee en depend directement.",
                         (int)(linkNoiseFloor + 117));
        }
    } else {
        Logger::log("ℹ️  LoRa: plancher de bruit non disponible — le bilan de liaison"
                    " affichera le RSSI sans rapport signal/bruit.");
    }
    Logger::log("");

#if LORA_NOISE_DIAG
    // Diagnostic de bruit — voir LORA_NOISE_DIAG dans LoRaCommunication.h.
    // Placé ici, avant tout trafic : l'UART du module est encore silencieux.
    // Sauté en mode configuration, pour la meme raison que la mesure de bruit.
    if (!moduleEnModeConfig) {
        Logger::log("");
        scanChannelNoise(LORA_SCAN_CH_MIN, LORA_SCAN_CH_MAX);
        Logger::log("");
    } else {
        Logger::log("ℹ️  LoRa: diagnostic de bruit saute (module en mode configuration).");
    }
#endif

    Logger::log("✓ LoRa: Prêt à recevoir");
    Logger::log("");

    return true;
}

void LoRaCommunication::update() {
    // Méthode obsolète - le polling REQUEST/RESPONSE n'est plus utilisé
    // On garde la méthode pour compatibilité interface mais elle ne fait rien
    // Utilisez listenForResponses() à la place
}

void LoRaCommunication::listenForResponses()
{
    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    // Sans cette garde, la fonction rendait tout le firmware muet.
    LoggerScope logScope(ecrLORACommunication, false);

    // ⚠️ Le mutex ne protege QUE l'acces au module (Serial2 + RecieveFrame).
    // Le decodage et le traitement des ACK se font APRES l'avoir rendu.
    //
    // Auparavant il etait tenu pendant tout le traitement, processAck()
    // compris. Depuis que les buffers concatenes sont decoupes correctement,
    // processAck() peut etre appele DEUX fois par lecture (la bouee emet
    // chaque ACK deux fois), ce qui allongeait d'autant la section critique.
    // sendCommandPacket() n'attend le mutex que 50 ms : au-dela il abandonne
    // et l'operateur voit « Echec envoi commande ». Une commande de securite
    // comme NAV_STOP ne doit pas echouer parce qu'un ACK etait en cours de
    // decodage.
    RecvFrame_t recvFrame;
    bool frameRecue = false;

    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        // Mutex non disponible, quelqu'un d'autre utilise le LoRa
        return;
    }
    if (Serial2.available() > 0) {
        frameRecue = (lora.RecieveFrame(&recvFrame) == 0);
    }
    xSemaphoreGive(loraMutex);

    if (frameRecue)
    {
        if (recvFrame.recv_data_len > 0)
        {
            // Frame reçue
            lastRssi = recvFrame.rssi;
            // Le E220 n'expose pas le SNR (voir LoRaCommunication.h). On publie
            // le rapport signal/bruit approche a partir du plancher de bruit,
            // qui est la meilleure approximation disponible — et NAN quand ce
            // plancher n'a pas pu etre lu, pour ne pas faire passer une absence
            // de mesure pour un SNR de 0 dB.
            lastSnr = (linkNoiseFloor != LINK_NOISE_UNKNOWN)
                          ? (float)(lastRssi - linkNoiseFloor)
                          : NAN;
            noteLinkSample(lastRssi);

            // Vérifier le type de message
            if (recvFrame.recv_data_len >= sizeof(LoRaMessageType))
            {
                LoRaMessageType *msgType = (LoRaMessageType *)recvFrame.recv_data;
                
                Logger::logf("📥 LoRa: Paquet reçu - type=%d, taille=%d bytes", *msgType, recvFrame.recv_data_len);

                // ⚠️ NE PAS exiger une longueur EXACTE sur tout le buffer.
                //
                // Une lecture UART peut contenir PLUSIEURS trames. La bouee
                // emet chaque ACK DEUX fois (ACK_REPEAT_COUNT), et le E220 les
                // livre regulierement concatenes : on observe couramment des
                // buffers de 37 octets = 18 + 18 + 1. L'ancien test
                // « recv_data_len == sizeof(AckWithStatePacketLora) » echouait
                // alors, et les DEUX copies etaient perdues — donc une
                // reemission de commande, et un acquittement jamais vu par
                // l'operateur.
                //
                // NOTE : cela n'a PAS fausse le taux de reception des campagnes
                // A.8 / A.9. noteLinkSample() est appele AVANT ce parsing, donc
                // un buffer concatene comptait deja comme une trame recue. Le
                // defaut coutait des acquittements, pas des fenetres vides.
                //
                // On parcourt donc le buffer trame par trame, en relisant le
                // messageType a chaque position. Cf. LORA_PROTOCOL.md §8.3 et
                // le meme correctif cote bouee (maintainConnection).
                size_t offset = 0;
                size_t ackCount = 0;

                while (offset < recvFrame.recv_data_len)
                {
                    uint8_t frameType = recvFrame.recv_data[offset];
                    size_t remaining = recvFrame.recv_data_len - offset;
                    size_t frameSize = loRaFrameSize(frameType);   // LoRaProtocol.h

                    if (frameType == (uint8_t)LoRaMessageType::ACK)
                    {
                        // Ambiguite assumee : le type ACK couvre deux tailles,
                        // AckWithStatePacketLora (18 o) et le legacy
                        // AckPacketLora (7 o), sans rien pour les distinguer.
                        // On privilegie 18 des qu'il y a la place : c'est ce que
                        // toutes les bouees v1 emettent. Le legacy n'est
                        // reconnu que s'il ne reste que 7 octets.
                        frameSize = (remaining >= sizeof(AckWithStatePacketLora))
                                        ? sizeof(AckWithStatePacketLora)
                                        : sizeof(AckPacketLora);
                    }

                    if (frameSize == 0)
                    {
                        Logger::logf("⚠️  LoRa: type inconnu 0x%02X a l'offset %u — reste du buffer ignore (%u octets)",
                                     (int)frameType, (unsigned)offset, (unsigned)remaining);
                        break;
                    }

                    if (frameSize > remaining)
                    {
                        Logger::logf("⚠️  LoRa: trame tronquee a l'offset %u (%u octets restants, %u attendus)",
                                     (unsigned)offset, (unsigned)remaining, (unsigned)frameSize);
                        break;
                    }

                    switch ((LoRaMessageType)frameType)
                    {
                        case LoRaMessageType::BUOY_STATUS:
                        {
                            // Protocole v2 : reponse a nos COMMAND_BUOY_STATUS (0x08)
                            BuoyStatusPacketLora *status =
                                (BuoyStatusPacketLora *)(recvFrame.recv_data + offset);
                            Logger::logf("📥 BUOY_STATUS reçu de Bouée #%d (RSSI=%d dBm)",
                                         LoRaCodec::nibbleLo(status->buoyIds), lastRssi);
                            processBuoyStatus(*status);
                            break;
                        }

                        case LoRaMessageType::OBSERVABLE:
                        {
                            // Protocole v2 : reponse a notre CMD_OBSERVABLE periodique
                            ObservablePacketLora *obs =
                                (ObservablePacketLora *)(recvFrame.recv_data + offset);
                            Logger::logf("📥 OBSERVABLE reçu de Bouée #%d (RSSI=%d dBm)", obs->buoyId, lastRssi);
                            processObservable(*obs);
                            break;
                        }

                        case LoRaMessageType::ACK:
                        {
                            // Protocole v1 : conserve pour une bouee qui repondrait encore en 0x04
                            if (frameSize == sizeof(AckWithStatePacketLora))
                            {
                                AckWithStatePacketLora *ack =
                                    (AckWithStatePacketLora *)(recvFrame.recv_data + offset);

                                if (ackCount == 0)
                                {
                                    Logger::logf("📥 ACK+State reçu de Bouée #%d (RSSI=%d dBm)",
                                                 ack->buoyId, lastRssi);
                                }
                                processAck(*ack);
                            }
                            else
                            {
                                AckPacketLora *legacyAck =
                                    (AckPacketLora *)(recvFrame.recv_data + offset);

                                Logger::logf("📥 ACK simple (legacy) reçu de Bouée #%d (RSSI=%d dBm)",
                                             legacyAck->buoyId, lastRssi);

                                AckWithStatePacketLora enrichedAck;
                                memset(&enrichedAck, 0, sizeof(enrichedAck));
                                enrichedAck.messageType = legacyAck->messageType;
                                enrichedAck.buoyId = legacyAck->buoyId;
                                enrichedAck.commandTimestamp = legacyAck->commandTimestamp;
                                enrichedAck.commandType = legacyAck->commandType;
                                processAck(enrichedAck);
                            }
                            ackCount++;
                            break;
                        }

                        default:
                            // COMMAND* d'un autre maitre, OBSERVABLE* destinees a la
                            // passerelle : enjambees sans traitement.
                            break;
                    }

                    offset += frameSize;
                }
            }
        }
    }
}

// pollBuoy() (REQUEST/RESPONSE) retiré : le protocole est COMMAND → réponse depuis 2025.

bool LoRaCommunication::sendCommand(uint8_t buoyId, const Command& cmd) {

    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    LoggerScope logScope(ecrLORACommunication, false);

    if (buoyId >= MAX_BUOYS) {
        Logger::logf("✗ LoRa: ID bouée invalide %d", buoyId);
        return false;
    }
    
    // Create LoRa COMMAND packet
    // 0x08 COMMAND_BUOY_STATUS : meme charge utile que 0x03, mais la bouee
    // repond par un BuoyStatusPacketLora (58 o, x1) au lieu de l'ACK v1
    // (18 o, x2). Le type de la requete fixe le format de la reponse
    // (LORA_PROTOCOL.md §3.1) ; la bouee n'a aucun mode a configurer.
    // Exige un firmware bouee >= 1.2.0 : une bouee plus ancienne ignore 0x08.
    CommandPacketLora packet;
    packet.messageType = LoRaMessageType::COMMAND_BUOY_STATUS;
    packet.targetBuoyId = buoyId;
    packet.command = cmd.type;
    packet.timestamp = millis();
    
    // LOG DÉTAILLÉ DU PAQUET ENVOYÉ
    Logger::log("📤 ========== ENVOI COMMANDE LoRa ==========");
    Logger::logf("   Taille paquet : %d bytes", sizeof(packet));
    Logger::logf("   messageType   : %d (0x%02X)", packet.messageType, packet.messageType);
    Logger::logf("   targetBuoyId  : %d", packet.targetBuoyId);
    Logger::logf("   command       : %d (0x%02X)", packet.command, packet.command);
    Logger::logf("   timestamp     : %lu", packet.timestamp);
    
    // Affichage hexadécimal du paquet complet
    Logger::log("   Données brutes (hex):");
    uint8_t* data = (uint8_t*)&packet;
    char hexStr[100];
    for (size_t i = 0; i < sizeof(packet); i++) {
        sprintf(hexStr + (i*3), "%02X ", data[i]);
    }
    Logger::logf("   %s", hexStr);
    Logger::log("==========================================");
    
    // Send command via LoRa
    bool sent = sendCommandPacket(packet);
    
    if (sent) {
        // Add to pending commands queue — sauf heartbeat et demandes de trame
        // OBSERVABLE*, dont la réponse ne porte pas d'acquittement (pas de
        // lastCmdTimestamp) : les mettre en file produirait 3 réémissions et un
        // « timeout » rouge sur une demande pourtant servie.
        if (cmd.type != CMD_HEARTBEAT && cmd.type != CMD_POLL &&
            cmd.type != CMD_OBSERVABLE && cmd.type != CMD_OBSERVABLE2 && cmd.type != CMD_OBSERVABLE_GPS) {
            if (addPendingCommand(packet)) {
                Logger::logf("✓ LoRa: Commande ajoutée à la queue (en attente d'ACK)");
                // Notifier le display : commande envoyée (Bleu)
                if (displayManager != nullptr) {
                    displayManager->setCommandStatus(CommandStatus::SENDING);
                }
            } else {
                Logger::log("⚠️  LoRa: Queue pleine, commande envoyée sans attente d'ACK");
            }
        }
        return true;
    }
    
    return false;
}

BuoyState LoRaCommunication::getLastBuoyState() {
    // Return the most recently updated buoy state
    uint32_t mostRecent = 0;
    int8_t mostRecentIndex = -1;
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].lastUpdateTime > mostRecent) {
            mostRecent = buoys[i].lastUpdateTime;
            mostRecentIndex = i;
        }
    }
    
    if (mostRecentIndex >= 0) {
        return convertLoraToState(buoys[mostRecentIndex].lastState);
    }
    
    // Return empty state if no buoy found
    BuoyState emptyState = {};
    return emptyState;
}

BuoyState LoRaCommunication::getBuoyState(uint8_t buoyId) {
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        return convertLoraToState(buoys[index].lastState);
    }
    
    // Return empty state if buoy not found
    BuoyState emptyState = {};
    return emptyState;
}

bool LoRaCommunication::hasNewData() {
    return newDataAvailable;
}

void LoRaCommunication::clearNewData() {
    newDataAvailable = false;
}

uint8_t LoRaCommunication::getBuoyCount() const {
    return buoyCount;
}

bool LoRaCommunication::isBuoyConnected(uint8_t buoyId, uint32_t timeoutMs) {
    int8_t index = findBuoyById(buoyId);
    if (index < 0) {
        return false;
    }
    
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - buoys[index].lastUpdateTime;
    return elapsed < timeoutMs;
}

BuoyInfo* LoRaCommunication::getBuoyInfo(uint8_t buoyId) {
    static BuoyInfo convertedInfo[MAX_BUOYS];
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        convertedInfo[index] = convertLoraInfoToInfo(buoys[index]);
        return &convertedInfo[index];
    }
    
    return nullptr;
}

BuoyInfo* LoRaCommunication::getAllBuoys() {
    static BuoyInfo convertedBuoys[MAX_BUOYS];
    for (int i = 0; i < MAX_BUOYS; i++) {
        convertedBuoys[i] = convertLoraInfoToInfo(buoys[i]);
    }
    return convertedBuoys;
}

int16_t LoRaCommunication::getLastRssi() const {
    return lastRssi;
}

float LoRaCommunication::getLastSnr() const {
    return lastSnr;
}

int8_t LoRaCommunication::findBuoyById(uint8_t buoyId) {
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered && buoys[i].buoyId == buoyId) {
            return i;
        }
    }
    return -1;
}

int8_t LoRaCommunication::addOrUpdateBuoy(uint8_t buoyId) {
    // Check if buoy already exists
    int8_t index = findBuoyById(buoyId);
    
    if (index >= 0) {
        return index; // Buoy already registered
    }
    
    // Find free slot
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (!buoys[i].registered) {
            buoys[i].registered = true;
            buoys[i].buoyId = buoyId;
            buoyCount++;
            
            Logger::logf("✓ LoRa: Nouvelle bouée découverte - ID #%d (total: %d)", 
                         buoyId, buoyCount);
            
            return i;
        }
    }
    
    Logger::logf("✗ LoRa: Impossible d'ajouter Bouée #%d - Slots pleins", buoyId);
    return -1;
}

void LoRaCommunication::processReceivedMessage(const uint8_t* data, size_t len) {

    // Verbosite locale, restauree a la sortie de portee (voir LoggerScope).
    LoggerScope logScope(ecrLORACommunication, false);
    
    // Check if this is a buoy state message
    if (len == sizeof(BuoyStateLora)) {
        BuoyStateLora* state = (BuoyStateLora*)data;
        
        // Validate buoy ID
        if (state->buoyId >= MAX_BUOYS) {
            Logger::logf("✗ LoRa: ID bouée invalide %d", state->buoyId);
            return;
        }
        
        // Add or update buoy
        int8_t index = addOrUpdateBuoy(state->buoyId);
        
        if (index >= 0) {
            // Update buoy state
            buoys[index].lastState = *state;
            buoys[index].lastUpdateTime = millis();
            buoys[index].lastRssi = lastRssi;
            buoys[index].lastSnr = lastSnr;
            
            // Set new data flag
            newDataAvailable = true;
            
            Logger::logf("📊 Bouée #%d: Mode=%d, Nav=%d, GPS=%s, Batt=%.0f mAh", 
                         state->buoyId,
                         state->generalMode,
                         state->navigationMode,
                         state->gpsOk ? "OK" : "NOK",
                         state->remainingCapacity);
        }
    } else {
        Logger::logf("⚠️  LoRa: Taille message invalide (%d bytes, attendu %d)", 
                     len, sizeof(BuoyStateLora));
    }
}

// Helper function to convert BuoyStateLora to BuoyState
static BuoyState convertLoraToState(const BuoyStateLora& loraState) {
    BuoyState state;
    state.buoyId = loraState.buoyId;
    state.timestamp = loraState.timestamp;
    state.generalMode = loraState.generalMode;
    state.navigationMode = loraState.navigationMode;
    state.gpsOk = loraState.gpsOk;
    state.headingOk = loraState.headingOk;
    state.yawRateOk = loraState.yawRateOk;
    state.temperature = loraState.temperature;
    state.remainingCapacity = loraState.remainingCapacity;
    state.distanceToCons = loraState.distanceToCons;
    state.autoPilotThrottleCmde = loraState.autoPilotThrottleCmde;
    state.autoPilotTrueHeadingCmde = loraState.autoPilotTrueHeadingCmde;
    state.latitude = loraState.latitude;
    state.longitude = loraState.longitude;
    state.sequenceNumber = 0;
    state.ttl = 0;
    return state;
}

// Helper function to convert BuoyInfoLora to BuoyInfo
static BuoyInfo convertLoraInfoToInfo(const BuoyInfoLora& loraInfo) {
    BuoyInfo info;
    info.registered = loraInfo.registered;
    info.buoyId = loraInfo.buoyId;
    info.lastState = convertLoraToState(loraInfo.lastState);
    info.lastUpdateTime = loraInfo.lastUpdateTime;
    info.lastRssi = loraInfo.lastRssi;
    info.lastSnr = loraInfo.lastSnr;
    return info;
}

const char* LoRaCommunication::getModeName() const {
    return (band == LoRaBand::BAND_433) ? "LoRa 433" : "LoRa 920";
}

void LoRaCommunication::setSelectedBuoy(uint8_t buoyId) {
    if (buoyId < MAX_BUOYS) {
        selectedBuoyId = buoyId;
        Logger::logf("📡 LoRa: Bouée sélectionnée #%d", buoyId);
    }
}

void LoRaCommunication::setPollMode(bool onlySelected) {
    pollOnlySelected = onlySelected;
    if (onlySelected) {
        Logger::logf("📡 LoRa: Mode sélectif activé (bouée #%d)", selectedBuoyId);
    } else {
        Logger::log("📡 LoRa: Mode découverte activé (toutes les bouées)");
    }
}

void LoRaCommunication::removeInactiveBuoys(uint32_t timeoutMs) {
    uint32_t currentTime = millis();
    uint8_t removedCount = 0;
    
    for (int i = 0; i < MAX_BUOYS; i++) {
        if (buoys[i].registered) {
            // Check if buoy has timed out
            if (currentTime - buoys[i].lastUpdateTime > timeoutMs) {
                Logger::logf("🗑️  LoRa: Suppression Bouée #%d (inactive depuis %d ms)", 
                             buoys[i].buoyId, currentTime - buoys[i].lastUpdateTime);
                
                buoys[i].registered = false;
                buoyCount--;
                removedCount++;
            }
        }
    }
    
    if (removedCount > 0) {
        Logger::logf("✓ LoRa: %d bouée(s) inactive(s) supprimée(s)", removedCount);
    }
}

/**
 * @brief Send command packet via LoRa
 */
bool LoRaCommunication::sendCommandPacket(const CommandPacketLora& packet) {
    // Prendre le mutex (attente max 50ms)
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Logger::log("✗ LoRa: Timeout acquisition mutex pour envoi");
        return false;
    }
    
    // Broadcast COMMAND to all buoys at address 0x0000
    loraConfig.target_address = 0x0000;  // Broadcast
    loraConfig.target_channel = getChannel();
    
    // Send packet using E220-JP SendFrame()
    int result = lora.SendFrame(loraConfig, (uint8_t*)&packet, sizeof(packet));
    
    // Libérer le mutex
    xSemaphoreGive(loraMutex);
    
    if (result == 0) {
        Logger::logf("✓ LoRa: Commande envoyée à Bouée #%d", packet.targetBuoyId);
        return true;
    } else {
        Logger::logf("✗ LoRa: Échec envoi commande (err=%d)", result);
        return false;
    }
}

/**
 * @brief Add command to pending queue
 */
bool LoRaCommunication::addPendingCommand(const CommandPacketLora& command) {
    // Find a free slot or replace oldest completed command
    int8_t freeSlot = -1;
    
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (pendingCommands[i].ackReceived) {
            freeSlot = i;
            break;
        }
    }
    
    if (freeSlot < 0) {
        Logger::log("⚠️  LoRa: Queue de commandes pleine");
        return false;
    }
    
    // Add command to queue
    pendingCommands[freeSlot].command = command;
    pendingCommands[freeSlot].sentTime = millis();
    pendingCommands[freeSlot].retryCount = 0;
    pendingCommands[freeSlot].ackReceived = false;
    
    pendingCommandCount++;
    
    return true;
}

/**
 * @brief Acquitte la commande en attente correspondant a (bouee, timestamp, code)
 *
 * Commun aux deux protocoles : l'ACK v1 porte (commandTimestamp, commandType),
 * le BUOY_STATUS v2 porte (lastCmdTimestamp, lastCmdCode).
 */
bool LoRaCommunication::acknowledgePending(uint8_t buoyId, uint32_t commandTimestamp, uint8_t commandCode) {
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (!pendingCommands[i].ackReceived &&
            pendingCommands[i].command.targetBuoyId == buoyId &&
            pendingCommands[i].command.timestamp == commandTimestamp &&
            (uint8_t)pendingCommands[i].command.command == commandCode) {

            // Mark as acknowledged
            pendingCommands[i].ackReceived = true;
            pendingCommandCount--;

            Logger::logf("   ✓ Commande confirmée (retry=%d)", pendingCommands[i].retryCount);

            // Notifier le display : ACK reçu (Vert)
            if (displayManager != nullptr) {
                displayManager->setCommandStatus(CommandStatus::ACK_RECEIVED);
            }
            return true;
        }
    }
    return false;
}

/**
 * @brief Process BUOY_STATUS (protocole v2) — GATEWAY_DESIGN.md §3.3
 */
void LoRaCommunication::processBuoyStatus(const BuoyStatusPacketLora& status) {
    using namespace LoRaCodec;

    uint8_t buoyId = nibbleLo(status.buoyIds);   // filtrer sur le quartet BAS, jamais l'octet entier

    Logger::logf("✅ BUOY_STATUS reçu de Bouée #%d, acquitte commande type=%d (ts=%lu)",
                 buoyId, status.lastCmdCode, status.lastCmdTimestamp);

    // Acquittement implicite : la trame porte la derniere commande acceptee.
    // Un poll / heartbeat ne correspond a aucune commande en attente — normal.
    acknowledgePending(buoyId, status.lastCmdTimestamp, status.lastCmdCode);

    if (buoyId >= MAX_BUOYS) {
        Logger::logf("   ⚠️  BUOY_STATUS de Bouée #%d hors limites", buoyId);
        return;
    }

    int8_t index = addOrUpdateBuoy(buoyId);
    if (index < 0) {
        Logger::logf("   ⚠️  Impossible d'enregistrer Bouée #%d", buoyId);
        return;
    }

    BuoyStateLora& state = buoys[index].lastState;
    state.buoyId = buoyId;
    state.preparedBuoyId = nibbleHi(status.buoyIds);
    state.timestamp = millis();
    state.generalMode = (tEtatsGeneral)nibbleLo(status.modes);
    state.navigationMode = (tEtatsNav)nibbleHi(status.modes);
    state.monitoringStatus = status.monitoringStatus;
    state.sensorsValidities = status.sensorsValidities;
    state.gpsOk = hasBit(status.sensorsValidities, SensorsValidityBit::GPS_OK);
    state.headingOk = hasBit(status.sensorsValidities, SensorsValidityBit::HEADING_OK);
    state.yawRateOk = hasBit(status.sensorsValidities, SensorsValidityBit::YAWRATE_OK);
    state.latitude = state.gpsOk ? decodeDeg1e7(status.latGps) : 0.0;
    state.longitude = state.gpsOk ? decodeDeg1e7(status.lonGps) : 0.0;
    state.trueHeading = fromBam(status.trueHeading);
    state.distanceToCons = (uint8_t)((status.distanceToCons / 10 > 255) ? 255 : status.distanceToCons / 10);  // m, sature (champ v1)
    state.autoPilotThrottleCmde = status.autoPilotThrottleCmde;
    state.autoPilotRudderCmde = status.autoPilotRudderCmde;
    state.autoPilotTrueHeadingCmde = (int16_t)lroundf(fromBam(status.autoPilotTrueHeadingCmde));
    // Temperature et batterie ne sont pas dans la trame rapide : elles viennent
    // de la trame OBSERVABLE, demandee toutes les 30 s (main.cpp), et gardent
    // ici leur derniere valeur connue.

    buoys[index].lastUpdateTime = millis();
    buoys[index].lastRssi = lastRssi;
    newDataAvailable = true;

    Logger::logf("   ✓ État Bouée #%d mis à jour depuis BUOY_STATUS (genMode=%d, navMode=%d, hdg=%.0f, throttle=%d, dist=%u dm)",
                 buoyId, state.generalMode, state.navigationMode, state.trueHeading,
                 state.autoPilotThrottleCmde, status.distanceToCons);
}

/**
 * @brief Process OBSERVABLE (protocole v2) — GATEWAY_DESIGN.md §3.4
 */
void LoRaCommunication::processObservable(const ObservablePacketLora& obs) {
    using namespace LoRaCodec;

    if (obs.buoyId >= MAX_BUOYS) {
        Logger::logf("   ⚠️  OBSERVABLE de Bouée #%d hors limites", obs.buoyId);
        return;
    }
    int8_t index = addOrUpdateBuoy(obs.buoyId);
    if (index < 0) {
        return;
    }

    BuoyStateLora& state = buoys[index].lastState;
    state.buoyId = obs.buoyId;
    state.temperature = (uint8_t)((obs.temperature < 0) ? 0 : obs.temperature);  // champ v1 non signé
    state.remainingCapacity = obs.remainingCapacity;
    state.courseGps = fromBam(obs.courseGps);
    state.speedGps = obs.speedGps / 100.0f;
    state.nbSat = obs.nbSat;
    state.lastObservableTime = millis();

    buoys[index].lastUpdateTime = millis();
    buoys[index].lastRssi = lastRssi;
    newDataAvailable = true;

    Logger::logf("   ✓ Bouée #%d : temp=%d °C, batterie=%u %%, sat=%u, SOG=%.2f m/s, COG=%.0f°, pertes DL %u/%u",
                 obs.buoyId, obs.temperature, obs.remainingCapacity, obs.nbSat,
                 state.speedGps, state.courseGps,
                 obs.partialDlToBuoyLossNumber, obs.totalDlToBuoyLossNumber);
}

/**
 * @brief Process ACK packet enriched with buoy state (protocole v1)
 */
void LoRaCommunication::processAck(const AckWithStatePacketLora& ack) {
    Logger::logf("✅ ACK+State reçu de Bouée #%d pour commande type=%d (ts=%lu)", 
                 ack.buoyId, ack.commandType, ack.commandTimestamp);
    
    acknowledgePending(ack.buoyId, ack.commandTimestamp, (uint8_t)ack.commandType);
    
    // Update BuoyStateLora from ACK data - immediate display refresh
    if (ack.buoyId >= MAX_BUOYS) {
        Logger::logf("   ⚠️  ACK de Bouée #%d hors limites", ack.buoyId);
        return;
    }
    
    int8_t index = addOrUpdateBuoy(ack.buoyId);
    if (index < 0) {
        Logger::logf("   ⚠️  Impossible d'enregistrer Bouée #%d", ack.buoyId);
        return;
    }
    
    // Copy state data from ACK into stored BuoyStateLora
    BuoyStateLora& state = buoys[index].lastState;
    state.buoyId = ack.buoyId;
    state.timestamp = millis();  // Use current time as update time
    state.generalMode = (tEtatsGeneral)ack.generalMode;
    state.navigationMode = (tEtatsNav)ack.navigationMode;
    state.gpsOk = ack.gpsOk;
    state.headingOk = ack.headingOk;
    state.yawRateOk = ack.yawRateOk;
    state.temperature = ack.temperature;
    state.remainingCapacity = ack.remainingCapacity;
    state.distanceToCons = ack.distanceToCons;
    state.autoPilotThrottleCmde = ack.autoPilotThrottleCmde;
    state.autoPilotTrueHeadingCmde = ack.autoPilotTrueHeadingCmde;
    
    buoys[index].lastUpdateTime = millis();
    buoys[index].lastRssi = lastRssi;
    
    // Signal new data available: DisplayManager::update() picks this flag up and
    // redraws immediately. Do NOT call forceRefresh() here — it clears the whole
    // screen and would repaint everything on every incoming frame (flickering).
    newDataAvailable = true;

    Logger::logf("   ✓ État Bouée #%d mis à jour depuis ACK (genMode=%d, navMode=%d, throttle=%d)",
                 ack.buoyId, ack.generalMode, ack.navigationMode, ack.autoPilotThrottleCmde);
}

/**
 * @brief Process command retries
 */
void LoRaCommunication::processCommandRetries() {
    uint32_t currentTime = millis();
    
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (pendingCommands[i].ackReceived) {
            continue;  // Already acknowledged
        }
        
        uint32_t elapsedTime = currentTime - pendingCommands[i].sentTime;
        
        // Check if ACK timeout
        if (elapsedTime >= ACK_TIMEOUT_MS) {
            if (pendingCommands[i].retryCount >= MAX_RETRY_COUNT) {
                // Max retries reached, give up
                Logger::logf("❌ LoRa: Commande timeout après %d tentatives (Bouée #%d, type=%d)", 
                             MAX_RETRY_COUNT + 1,
                             pendingCommands[i].command.targetBuoyId,
                             pendingCommands[i].command.command);
                
                // Notifier le display : timeout (Rouge)
                if (displayManager != nullptr) {
                    displayManager->setCommandStatus(CommandStatus::TIMEOUT);
                }
                
                // Mark as completed (failed)
                pendingCommands[i].ackReceived = true;
                pendingCommandCount--;
            } else {
                // Retry command
                pendingCommands[i].retryCount++;
                pendingCommands[i].sentTime = currentTime;
                
                Logger::logf("🔄 LoRa: Renvoi commande (tentative %d/%d) à Bouée #%d", 
                             pendingCommands[i].retryCount + 1,
                             MAX_RETRY_COUNT + 1,
                             pendingCommands[i].command.targetBuoyId);
                
                sendCommandPacket(pendingCommands[i].command);
            }
        }
    }
}


void LoRaCommunication::setDisplayManager(DisplayManager* display) {
    displayManager = display;
    Logger::log("✓ LoRa: DisplayManager attaché pour feedback visuel");
}