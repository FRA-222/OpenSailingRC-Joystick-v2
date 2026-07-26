# Modification de la structure Command

**Date:** 29 octobre 2025  
**Auteur:** Philippe Hubert

## Vue d'ensemble

Cette modification restructure la structure `Command` et `CommandPacket` pour améliorer la cohérence architecturale en ajoutant le champ `targetBuoyId` directement dans la commande et en retirant le champ `mode` (navigation mode).

## Motivation

### Problème
1. **Redondance de paramètre**: L'ID de la bouée cible était passé à la fois comme paramètre de `sendCommand(buoyId, cmd)` et rempli dans `CommandPacket.targetBuoyId`
2. **Champ `mode` ambigu**: Le mode de navigation était dans la commande, mais devrait être géré par le type de commande lui-même (ex: `CMD_INIT_HOME` implique `NAV_HOME`)
3. **Responsabilité peu claire**: La commande ne portait pas toute l'information nécessaire

### Solution
- Ajouter `targetBuoyId` directement dans la structure `Command`
- Retirer le champ `mode` de `Command` et `CommandPacket`
- Les modes de navigation sont maintenant implicites au type de commande

## Modifications détaillées

### 1. Structure `Command` (CommandManager.h)

**Avant:**
```cpp
struct Command {
    BuoyCommand type;       ///< Command type
    int16_t heading;        ///< Target heading (-180 to +180 degrees)
    int8_t throttle;        ///< Speed (-100 to +100%)
    tEtatsNav mode;         ///< Navigation mode
    uint32_t timestamp;     ///< Command timestamp
};
```

**Après:**
```cpp
struct Command {
    uint8_t targetBuoyId;   ///< Target buoy ID
    BuoyCommand type;       ///< Command type
    int16_t heading;        ///< Target heading (-180 to +180 degrees)
    int8_t throttle;        ///< Speed (-100 to +100%)
    uint32_t timestamp;     ///< Command timestamp
};
```

**Changements:**
- ✅ Ajout de `uint8_t targetBuoyId` en premier champ
- ❌ Suppression de `tEtatsNav mode`

### 2. Structure `CommandPacket` (ESPNowCommunication.h - Joystick)

**Avant:**
```cpp
struct CommandPacket {
    uint8_t targetBuoyId;       ///< Target buoy ID
    BuoyCommand command;        ///< Command type
    int16_t heading;            ///< Target heading
    int8_t throttle;            ///< Target speed
    tEtatsNav navigationMode;   ///< Navigation mode
    uint32_t timestamp;         ///< Timestamp
};
```

**Après:**
```cpp
struct CommandPacket {
    uint8_t targetBuoyId;       ///< Target buoy ID
    BuoyCommand command;        ///< Command type
    int16_t heading;            ///< Target heading
    int8_t throttle;            ///< Target speed
    uint32_t timestamp;         ///< Timestamp
};
```

**Changements:**
- ❌ Suppression de `tEtatsNav navigationMode`
- **Taille réduite:** ~1 byte économisé (enum)

### 3. Génération de commande (CommandManager.cpp)

**Avant:**
```cpp
bool CommandManager::generateInitHomeCommand(uint8_t targetBuoyId) {
    Command homeCmd;
    homeCmd.type = CMD_INIT_HOME;
    homeCmd.heading = 0;
    homeCmd.throttle = 0;
    homeCmd.mode = NAV_HOME;    // ❌ Mode explicite
    homeCmd.timestamp = millis();
    
    bool success = espNowComm.sendCommand(targetBuoyId, homeCmd);
    return success;
}
```

**Après:**
```cpp
bool CommandManager::generateInitHomeCommand(uint8_t targetBuoyId) {
    Command homeCmd;
    homeCmd.targetBuoyId = targetBuoyId;  // ✅ ID dans la commande
    homeCmd.type = CMD_INIT_HOME;
    homeCmd.heading = 0;
    homeCmd.throttle = 0;
    // Mode implicite: CMD_INIT_HOME → NAV_HOME
    homeCmd.timestamp = millis();
    
    bool success = espNowComm.sendCommand(targetBuoyId, homeCmd);
    return success;
}
```

### 4. Envoi de commande (ESPNowCommunication.cpp - Joystick)

**Avant:**
```cpp
bool ESPNowCommunication::sendCommand(uint8_t buoyId, const Command& cmd) {
    // ...
    CommandPacket packet;
    packet.targetBuoyId = buoyId;          // ❌ Paramètre fonction
    packet.command = cmd.type;
    packet.heading = cmd.heading;
    packet.throttle = cmd.throttle;
    packet.navigationMode = cmd.mode;       // ❌ Mode explicite
    packet.timestamp = millis();            // ❌ Recalculé
    // ...
}
```

**Après:**
```cpp
bool ESPNowCommunication::sendCommand(uint8_t buoyId, const Command& cmd) {
    // ...
    CommandPacket packet;
    packet.targetBuoyId = cmd.targetBuoyId; // ✅ Depuis Command
    packet.command = cmd.type;
    packet.heading = cmd.heading;
    packet.throttle = cmd.throttle;
    packet.timestamp = cmd.timestamp;        // ✅ Préservé
    // ...
}
```

**Amélioration:** Le timestamp est maintenant préservé depuis la création de la commande, pas recalculé à l'envoi.

### 5. Structure `CommandPacket` (ESPNowDataLinkManagement.h - Bouée)

**Avant:**
```cpp
struct CommandPacket {
    uint8_t targetBuoyId;       ///< Target buoy ID (0-5)
    BuoyCommand command;        ///< Command type
    int16_t heading;            ///< Target heading (-180 to +180°)
    int8_t throttle;            ///< Target speed (-100 to +100%)
    tEtatsNav navigationMode;   ///< Navigation mode
    uint32_t timestamp;         ///< Command timestamp
};
```

**Après:**
```cpp
struct CommandPacket {
    uint8_t targetBuoyId;       ///< Target buoy ID (0-5)
    BuoyCommand command;        ///< Command type
    int16_t heading;            ///< Target heading (-180 to +180°)
    int8_t throttle;            ///< Target speed (-100 to +100%)
    uint32_t timestamp;         ///< Command timestamp
};
```

### 6. Traitement des commandes (ESPNowDataLinkManagement.cpp - Bouée)

**Avant:**
```cpp
case CMD_CHANGE_MODE:
    message = "#E1=" + String(cmd.navigationMode);
    Logger::log("Command: CHANGE_MODE " + String(cmd.navigationMode));
    break;
```

**Après:**
```cpp
case CMD_CHANGE_MODE:
    // Note: navigationMode field removed from CommandPacket
    // Mode changes are now handled by specific commands (INIT_HOME, HOLD_POSITION, etc.)
    Logger::log("Command: CHANGE_MODE (deprecated - no action)");
    break;
```

**Note:** La commande `CMD_CHANGE_MODE` est maintenant dépréciée. Les changements de mode se font via les commandes spécifiques.

## Mapping Type de Commande → Mode de Navigation

Avec cette architecture, chaque type de commande implique un mode de navigation :

| Type de Commande | Mode de Navigation | Description |
|------------------|-------------------|-------------|
| `CMD_INIT_HOME` | `NAV_HOME` | Initialise le HOME avec position GPS actuelle |
| `CMD_HOLD_POSITION` | `NAV_HOLD` | Maintient la position actuelle |
| `CMD_RETURN_HOME` | `NAV_HOME` | Retour au point HOME |
| `CMD_STOP` | `NAV_STOP` | Arrêt de tous les mouvements |
| `CMD_SET_HEADING` | `NAV_CAP` | Navigation par cap |
| `CMD_SET_THROTTLE` | (inchangé) | Modifie seulement le throttle |
| `CMD_CHANGE_MODE` | ⚠️ **DÉPRÉCIÉ** | Utiliser les commandes spécifiques |

## Avantages

### 1. Cohérence
- La structure `Command` contient maintenant TOUTE l'information nécessaire
- Pas de duplication d'information entre paramètres et structure
- Le timestamp est créé une seule fois et préservé

### 2. Clarté
- Le type de commande définit implicitement le mode de navigation
- Moins de paramètres ambigus
- Logique plus simple à comprendre

### 3. Maintenabilité
- Une seule source de vérité pour `targetBuoyId` (dans `Command`)
- Moins de risques d'incohérence entre `buoyId` (paramètre) et `cmd.mode`
- Code plus facile à débugger

### 4. Économie mémoire
- **CommandPacket:** -1 byte (enum `tEtatsNav` supprimé)
- **Command:** +1 byte (`uint8_t targetBuoyId`) mais plus cohérent

### 5. Évolutivité
- Facile d'ajouter de nouvelles commandes avec leur mode implicite
- Structure prête pour des extensions futures

## Taille des structures

### Avant
```
Command: 
  - BuoyCommand type (1 byte)
  - int16_t heading (2 bytes)
  - int8_t throttle (1 byte)
  - tEtatsNav mode (1 byte)
  - uint32_t timestamp (4 bytes)
  Total: ~9 bytes + padding

CommandPacket:
  - uint8_t targetBuoyId (1 byte)
  - BuoyCommand command (1 byte)
  - int16_t heading (2 bytes)
  - int8_t throttle (1 byte)
  - tEtatsNav navigationMode (1 byte)
  - uint32_t timestamp (4 bytes)
  Total: ~10 bytes + padding
```

### Après
```
Command:
  - uint8_t targetBuoyId (1 byte)
  - BuoyCommand type (1 byte)
  - int16_t heading (2 bytes)
  - int8_t throttle (1 byte)
  - uint32_t timestamp (4 bytes)
  Total: ~9 bytes + padding

CommandPacket:
  - uint8_t targetBuoyId (1 byte)
  - BuoyCommand command (1 byte)
  - int16_t heading (2 bytes)
  - int8_t throttle (1 byte)
  - uint32_t timestamp (4 bytes)
  Total: ~9 bytes + padding
```

**Résultat:** Même taille pour `Command`, -1 byte pour `CommandPacket`

## Impact sur le code existant

### Joystick (OpenSailingRC-BuoyJoystick)
- ✅ `CommandManager.h` - Structure `Command` modifiée
- ✅ `CommandManager.cpp` - `generateInitHomeCommand()` mis à jour
- ✅ `ESPNowCommunication.h` - Structure `CommandPacket` modifiée
- ✅ `ESPNowCommunication.cpp` - `sendCommand()` mis à jour
- ✅ Compilation réussie

### Bouée (Autonomous-GPS-Buoy)
- ✅ `ESPNowDataLinkManagement.h` - Structure `CommandPacket` modifiée
- ✅ `ESPNowDataLinkManagement.cpp` - Traitement `CMD_CHANGE_MODE` déprécié
- ✅ Compilation réussie

### Code non modifié (reste compatible)
- `main.cpp` - Aucun changement nécessaire
- `BuoyStateManager` - Aucun changement
- `DisplayManager` - Aucun changement
- Code métier de la bouée - Aucun changement

## Tests à effectuer

### Test 1: Commande INIT_HOME
1. Allumer le joystick et la bouée
2. Sélectionner la bouée
3. Presser le bouton jaune gauche
4. **Vérifier:** Logs montrent `targetBuoyId` correcte
5. **Vérifier:** Bouée reçoit et traite la commande

### Test 2: Vérification timestamp
1. Envoyer une commande
2. **Vérifier:** Le timestamp dans les logs de réception correspond au timestamp de création

### Test 3: Multi-bouées
1. Connecter 2+ bouées
2. Envoyer des commandes à différentes bouées
3. **Vérifier:** Chaque commande arrive à la bonne bouée

### Test 4: CMD_CHANGE_MODE
1. Essayer d'envoyer `CMD_CHANGE_MODE` (si possible)
2. **Vérifier:** Log "deprecated - no action" apparaît
3. **Vérifier:** Aucune erreur système

## Compilation

### Joystick
```bash
cd /Users/philippe/Documents/PlatformIO/Projects/OpenSailingRC-BuoyJoystick
~/.platformio/penv/bin/platformio run
```

**Résultat:** ✅ SUCCESS  
- RAM: 47,320 bytes (14.4%)  
- Flash: 879,049 bytes (26.3%)

### Bouée
```bash
cd "/Users/philippe/Documents/PlatformIO/Projects/Autonomous GPS Buoy2/Autonomous-GPS-Buoy"
~/.platformio/penv/bin/platformio run
```

**Résultat:** ✅ SUCCESS  
- RAM: 57,940 bytes (1.3%)  
- Flash: 1,253,309 bytes (19.1%)

## Migration future

Pour ajouter de nouvelles commandes avec cette architecture :

```cpp
// Exemple: Commande de navigation vers un waypoint
bool CommandManager::generateGoToWaypointCommand(uint8_t targetBuoyId, 
                                                  float lat, float lon) {
    Command cmd;
    cmd.targetBuoyId = targetBuoyId;
    cmd.type = CMD_GO_TO_WAYPOINT;  // Nouveau type
    // Encoder lat/lon dans heading/throttle ou étendre la structure
    cmd.timestamp = millis();
    
    return espNowComm.sendCommand(targetBuoyId, cmd);
}
```

**Mode implicite:** `CMD_GO_TO_WAYPOINT` → `NAV_TARGET`

## Notes importantes

1. **Rétrocompatibilité:** Cette modification CASSE la compatibilité avec les anciennes versions du firmware. Les deux côtés (joystick + bouée) doivent être mis à jour ensemble.

2. **CMD_CHANGE_MODE déprécié:** Cette commande ne fait plus rien. Pour changer de mode, il faut utiliser les commandes spécifiques (`CMD_INIT_HOME`, `CMD_HOLD_POSITION`, etc.)

3. **Timestamp préservé:** Le timestamp est maintenant créé à la génération de la commande et préservé jusqu'à la réception, permettant de mieux mesurer les latences.

4. **Validation:** Le code devrait idéalement vérifier que `cmd.targetBuoyId` correspond au `buoyId` passé en paramètre de `sendCommand()`, mais pour le moment on fait confiance à la cohérence.

## Prochaines étapes possibles

1. **Ajouter validation:** Vérifier que `cmd.targetBuoyId == buoyId` dans `sendCommand()`
2. **Étendre Command:** Ajouter des champs pour waypoints (latitude/longitude)
3. **Statistiques:** Logger les temps de latence depuis création jusqu'à réception
4. **Commandes groupées:** Permettre d'envoyer plusieurs commandes en une seule transmission
