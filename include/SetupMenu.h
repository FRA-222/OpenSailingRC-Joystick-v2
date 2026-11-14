/**
 * @file SetupMenu.h
 * @brief Menu de configuration du joystick, ouvert au démarrage (bouton A maintenu)
 * @author Philippe Hubert
 * @date 2026
 *
 * Moteur commun aux deux matériels : entrées, brouillon, navigation, préférences.
 * Seuls le rendu (DisplayManager::drawSetup*, un par matériel) et la
 * correspondance des boutons (BTN_MENU_OK / BTN_MENU_BACK, HardwareConfig.h)
 * dépendent du matériel. Voir docs/CONFIGURATION_MENU_DESIGN.md.
 *
 * Navigation :
 *   stick gauche haut / bas    : entrée précédente / suivante
 *   stick gauche gauche / droite : valeur précédente / suivante
 *   BTN_MENU_OK                : valide une action, ou passe à l'entrée suivante
 *   BTN_MENU_BACK              : abandonne les modifications, curseur sur « Start »
 *
 * Le menu tourne AVANT l'initialisation de la radio : la configuration choisie
 * s'applique donc immédiatement, sans redémarrage.
 */

/*
 * Open Source License Notice
 * SPDX-License-Identifier: GPL-3.0-only
 * This file is part of the OpenSailingRC-Joystick-v2 project and is distributed
 * under the GNU General Public License v3.0.
 * See https://www.gnu.org/licenses/gpl-3.0.html for full license text.
 */

#ifndef SETUP_MENU_H
#define SETUP_MENU_H

#include <stdint.h>
#include "JoystickPreferences.h"

class JoystickManager;

/// Nombre maximal d'entrées affichables (toutes visibles en LoRa)
static const uint8_t SETUP_MENU_MAX_ITEMS = 8;

/**
 * @brief Une ligne du menu, déjà mise en forme pour l'affichage
 */
struct SetupMenuItem {
    char label[18];     ///< "LoRa power"
    char value[16];     ///< "22 dBm" (vide pour une action)
    char detail[30];    ///< précision affichée pour l'entrée sélectionnée
    bool editable;      ///< true : la valeur change avec gauche / droite
    bool warning;       ///< true : détail à afficher en couleur d'alerte
};

/**
 * @brief Ce que l'affichage doit dessiner (indépendant du matériel)
 */
struct SetupMenuView {
    SetupMenuItem items[SETUP_MENU_MAX_ITEMS];
    uint8_t count;            ///< entrées visibles
    uint8_t selected;         ///< indice dans items
    bool dirty;               ///< brouillon différent de la configuration enregistrée
    const char* version;      ///< version du firmware
    const char* okName;       ///< libellé du bouton OK ("BLUE", "Y-R")
    const char* backName;     ///< libellé du bouton Retour
};

/**
 * @class SetupMenu
 * @brief Fenêtre de démarrage et menu de configuration
 */
class SetupMenu {
public:
    explicit SetupMenu(JoystickManager& joystick);

    /**
     * @brief Affiche la configuration en service et guette le bouton A
     *
     * Fenêtre de BOOT_WINDOW_MS. Si le bouton A est (ou devient) maintenu
     * HOLD_MS d'affilée, le menu est demandé.
     *
     * @param current Configuration chargée
     * @param fromNvs true si elle vient de la NVS (false = valeurs par défaut)
     * @return true si l'utilisateur demande le menu
     */
    bool checkBootRequest(const JoystickSettings& current, bool fromNvs);

    /**
     * @brief Menu bloquant : rend la configuration à utiliser
     *
     * @param current Configuration chargée (point de départ du brouillon)
     * @param[out] saved true si « Save & start » a été validé (et l'écriture réussie)
     * @return Brouillon validé (Save & start), ou current (Start, Retour, inactivité)
     */
    JoystickSettings run(const JoystickSettings& current, bool& saved);

    static const uint32_t BOOT_WINDOW_MS = 2000;   ///< affichage de la configuration au démarrage
    static const uint32_t HOLD_MS        = 2000;   ///< maintien du bouton A pour ouvrir le menu
    static const uint32_t IDLE_TIMEOUT_MS = 60000; ///< sans action : démarrage sans enregistrer

private:
    /// Entrées du menu, dans l'ordre d'affichage
    enum EntryId : uint8_t {
        ENTRY_LINK, ENTRY_BAND, ENTRY_POWER, ENTRY_CHANNEL, ENTRY_ESPNOW,
        ENTRY_SAVE, ENTRY_START, ENTRY_COUNT
    };

    /// Directions lues sur le stick gauche (fronts)
    struct StickEvents { int8_t vertical; int8_t horizontal; bool repeat; };

    JoystickManager& joystick;
    JoystickSettings stored;     ///< configuration de départ
    JoystickSettings draft;      ///< brouillon modifié par le menu
    EntryId visible[ENTRY_COUNT];
    uint8_t visibleCount;
    uint8_t selected;            ///< indice dans visible[]

    // Détection de fronts du stick (seuil + hystérésis, auto-répétition horizontale)
    int8_t latchedV;
    int8_t latchedH;
    uint32_t hPressTime;
    uint32_t hLastRepeat;

    void rebuildVisible(EntryId keep);
    void change(EntryId id, int8_t dir);
    void buildView(SetupMenuView& view) const;
    void formatItem(EntryId id, SetupMenuItem& item) const;
    StickEvents readStick();
    void selectEntry(EntryId id);
};

#endif // SETUP_MENU_H
