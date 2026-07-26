# CommandManager Refactoring

**Date:** 2025-01-XX  
**Auteur:** Philippe Hubert

## Vue d'ensemble

Cette modification refactorise l'architecture de gestion des commandes pour mieux encapsuler la logique de création et d'envoi des commandes vers les bouées. Au lieu de créer manuellement les objets `Command` dans `main.cpp` et d'appeler `espNow.sendCommand()`, cette logique est maintenant encapsulée dans la classe `CommandManager`.

## Motivation

### Problème
- Le code de création et d'envoi des commandes était dispersé dans `main.cpp`
- Violation du principe de responsabilité unique (SRP)
- Duplication potentielle de code pour chaque type de commande
- Difficile de réutiliser la logique de commande dans d'autres contextes

### Solution
- Encapsulation de la logique dans `CommandManager`
- Méthode publique `generateInitHomeCommand()` qui gère tout le processus
- `CommandManager` a maintenant une référence vers `ESPNowCommunication`
- Code plus maintenable et réutilisable

## Modifications détaillées

### 1. Création de `src/CommandManager.cpp`

**Nouveau fichier** contenant l'implémentation de la classe `CommandManager`:

```cpp
// Constructeur avec référence ESPNowCommunication
CommandManager::CommandManager(ESPNowCommunication& espNowComm)
    : espNowComm(espNowComm),
      newCommandAvailable(false),
      currentHeading(0),
      currentThrottle(0),
      currentMode(NAV_NOTHING),
      lastHeadingChange(0),
      lastModeChange(0) {
}

// Méthode publique pour créer et envoyer CMD_INIT_HOME
bool CommandManager::generateInitHomeCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande INIT_HOME pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command homeCmd;
    homeCmd.type = CMD_INIT_HOME;
    homeCmd.heading = 0;
    homeCmd.throttle = 0;
    homeCmd.mode = NAV_HOME;
    homeCmd.timestamp = millis();
    
    // Sauvegarde interne
    currentCommand = homeCmd;
    newCommandAvailable = true;
    
    // Envoi via ESP-NOW
    bool success = espNowComm.sendCommand(targetBuoyId, homeCmd);
    
    if (success) {
        Logger::log("   -> Commande HOME envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HOME");
    }
    
    return success;
}
```

**Méthodes implémentées:**
- `CommandManager(ESPNowCommunication&)` - Constructeur
- `update()` - Placeholder pour future logique joystick
- `getCommand()` - Retourne la dernière commande
- `hasNewCommand()` / `clearNewCommand()` - Gestion du flag
- `getCurrentHeading()` / `getCurrentThrottle()` / `getCurrentMode()` - Getters
- `generateInitHomeCommand(uint8_t)` - **NOUVELLE méthode publique**
- `applyDeadzone()` - Utilitaire pour joystick
- `updateHeading()` / `updateThrottle()` - Placeholders

### 2. Modification de `include/CommandManager.h`

**Changements:**

1. **Forward declaration** pour éviter les dépendances circulaires:
```cpp
// Forward declaration
class ESPNowCommunication;
```

2. **Constructeur modifié** pour accepter une référence ESP-NOW:
```cpp
CommandManager(ESPNowCommunication& espNowComm);
```

3. **Nouvelle méthode publique** (anciennement privée):
```cpp
/**
 * @brief Generate and send INIT_HOME command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool generateInitHomeCommand(uint8_t targetBuoyId);
```

4. **Nouveau membre privé** pour la référence ESP-NOW:
```cpp
private:
    ESPNowCommunication& espNowComm;  ///< Reference to ESP-NOW communication
```

5. **Suppression** de l'ancienne déclaration privée:
```cpp
// SUPPRIMÉ: void generateInitHomeCommand();
```

### 3. Modification de `src/main.cpp`

**Include ajouté:**
```cpp
#include "CommandManager.h"
```

**Instance créée:**
```cpp
CommandManager cmdManager(espNow);
```

**Code simplifié** dans la gestion du bouton gauche:

**Avant:**
```cpp
if (joystick.wasButtonPressed(BTN_LEFT_STICK)) {
    uint8_t selectedId = buoyState.getSelectedBuoyId();
    if (buoyState.isSelectedBuoyConnected()) {
        Logger::logf("\n[L] Bouton jaune GAUCHE presse - Envoi CMD_INIT_HOME a Bouee #%d", selectedId);
        
        // Création manuelle de la commande
        Command homeCmd;
        homeCmd.type = CMD_INIT_HOME;
        homeCmd.heading = 0;
        homeCmd.throttle = 0;
        homeCmd.mode = NAV_HOME;
        homeCmd.timestamp = millis();
        
        // Envoi manuel
        if (espNow.sendCommand(selectedId, homeCmd)) {
            Logger::log("   -> Commande HOME envoyee avec succes");
            display.displayBuoySelection();
        } else {
            Logger::log("   -> ERREUR: Echec envoi commande HOME");
        }
    } else {
        Logger::log("\n[L] Bouton jaune GAUCHE presse - Aucune bouee connectee");
    }
}
```

**Après:**
```cpp
if (joystick.wasButtonPressed(BTN_LEFT_STICK)) {
    uint8_t selectedId = buoyState.getSelectedBuoyId();
    if (buoyState.isSelectedBuoyConnected()) {
        Logger::logf("\n[L] Bouton jaune GAUCHE presse - Envoi CMD_INIT_HOME a Bouee #%d", selectedId);
        
        // Utilisation de CommandManager (encapsule tout)
        if (cmdManager.generateInitHomeCommand(selectedId)) {
            display.displayBuoySelection();
        }
    } else {
        Logger::log("\n[L] Bouton jaune GAUCHE presse - Aucune bouee connectee");
    }
}
```

**Réduction:** -14 lignes de code, logique encapsulée

## Avantages

### 1. Encapsulation
- La logique de commande est maintenant dans `CommandManager`
- `main.cpp` n'a plus besoin de connaître la structure interne de `Command`
- Séparation claire des responsabilités

### 2. Réutilisabilité
- `generateInitHomeCommand()` peut être appelée de n'importe où
- Facile d'ajouter d'autres méthodes similaires (ex: `generateReturnHomeCommand()`)

### 3. Maintenabilité
- Toutes les modifications de logique de commande se font au même endroit
- Logs centralisés dans `CommandManager`
- Tests unitaires plus faciles

### 4. Évolutivité
- Pattern facilement extensible pour d'autres types de commandes
- Structure prête pour des commandes plus complexes

## Prochaines étapes possibles

1. **Implémenter d'autres commandes:**
   - `generateSetHeadingCommand(uint8_t buoyId, int16_t heading)`
   - `generateSetThrottleCommand(uint8_t buoyId, int8_t throttle)`
   - `generateReturnHomeCommand(uint8_t buoyId)`
   - `generateStopCommand(uint8_t buoyId)`

2. **Implémenter la méthode `update()`:**
   - Gérer automatiquement les changements de cap depuis le joystick droit
   - Gérer automatiquement le throttle depuis le joystick gauche
   - Génération automatique de commandes basée sur les inputs

3. **Ajouter validation:**
   - Vérifier que la bouée est connectée avant d'envoyer
   - Ajouter des délais anti-rebond pour les commandes répétées
   - Logger les erreurs de manière plus détaillée

4. **Gestion d'historique:**
   - Garder un historique des N dernières commandes
   - Permettre de rejouer une commande
   - Statistiques d'envoi (succès/échec)

## Compilation

```bash
cd /Users/philippe/Documents/PlatformIO/Projects/OpenSailingRC-BuoyJoystick
~/.platformio/penv/bin/platformio run
```

**Résultat:** ✅ SUCCESS  
- RAM: 47,320 bytes (14.4%)  
- Flash: 879,049 bytes (26.3%)  
- Aucune erreur de compilation

## Tests recommandés

1. **Test bouton gauche:**
   - Presser le bouton jaune gauche
   - Vérifier que `CMD_INIT_HOME` est envoyé
   - Vérifier les logs dans le Serial Monitor
   - Vérifier que la bouée reçoit et traite la commande

2. **Test bouée déconnectée:**
   - Éteindre la bouée
   - Presser le bouton gauche
   - Vérifier le message "Aucune bouee connectee"

3. **Test multi-bouées:**
   - Connecter plusieurs bouées
   - Sélectionner différentes bouées
   - Vérifier que la commande est envoyée à la bonne bouée

## Architecture finale

```
main.cpp
  |
  +-- cmdManager.generateInitHomeCommand(buoyId)
        |
        +-- Crée Command{type, heading, throttle, mode, timestamp}
        +-- Sauvegarde dans currentCommand
        +-- espNowComm.sendCommand(buoyId, cmd)
        +-- Retourne true/false
```

## Notes

- Cette refactorisation ne change pas le comportement fonctionnel
- La bouée reçoit exactement la même commande qu'avant
- Le code est maintenant plus propre et plus maintenable
- Prêt pour extension future vers d'autres types de commandes
