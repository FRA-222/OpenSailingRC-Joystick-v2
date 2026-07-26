# Refonte de l'énuméré BuoyCommand

**Date:** 29 octobre 2025  
**Auteur:** Philippe Hubert

## Vue d'ensemble

Cette modification restructure l'énuméré `BuoyCommand` pour mieux refléter l'architecture de navigation et les modes de la bouée. Les commandes sont maintenant plus explicites et cohérentes avec les modes de navigation correspondants.

## Modifications de l'énuméré

### Tableau comparatif

| Ancien nom | Nouveau nom | Valeur | Changement | Raison |
|-----------|-------------|---------|-----------|---------|
| `CMD_INIT_HOME` | `CMD_INIT_HOME` | 0 | ✅ **Conservé** | Initialise le Home |
| `CMD_SET_HEADING` | `CMD_SET_TRUE_HEADING` | 1 | 🔄 **Renommé** | Clarification : True Heading (cap vrai) |
| `CMD_SET_THROTTLE` | `CMD_SET_THROTTLE` | 2 | ✅ **Conservé** | Définit la vitesse |
| `CMD_HOLD_POSITION` | `CMD_NAV_HOLD` | 3 | 🔄 **Renommé** | Cohérence avec NAV_HOLD |
| *(nouveau)* | `CMD_NAV_CAP` | 4 | ➕ **Ajouté** | Navigation par cap |
| `CMD_RETURN_HOME` | `CMD_NAV_HOME` | 5 | 🔄 **Renommé** | Cohérence avec NAV_HOME |
| `CMD_STOP` | `CMD_NAV_STOP` | 6 | 🔄 **Renommé** | Cohérence avec NAV_STOP |
| `CMD_CHANGE_MODE` | *(supprimé)* | - | ❌ **Supprimé** | Remplacé par commandes NAV_* |
| *(nouveau)* | `CMD_HOME_VALIDATION` | 7 | ➕ **Ajouté** | Valide la position Home |

### Nouvelle définition

```cpp
enum BuoyCommand {
    CMD_INIT_HOME = 0,      ///< Initialize Home with current GPS position
    CMD_SET_TRUE_HEADING,   ///< Set target true heading
    CMD_SET_THROTTLE,       ///< Set speed
    CMD_NAV_HOLD,           ///< Hold current position
    CMD_NAV_CAP,            ///< Navigate by heading (cap mode)
    CMD_NAV_HOME,           ///< Navigate to Home position
    CMD_NAV_STOP,           ///< Stop all movements
    CMD_HOME_VALIDATION     ///< Validate Home position
};
```

## Rationale des changements

### 1. **CMD_SET_HEADING → CMD_SET_TRUE_HEADING**
- **Raison:** Clarification du type de cap (True Heading vs Magnetic Heading)
- **Impact:** Plus explicite pour éviter toute confusion
- **Utilisation:** Définit le cap vrai cible pour la navigation

### 2. **CMD_HOLD_POSITION → CMD_NAV_HOLD**
- **Raison:** Cohérence avec le mode de navigation `NAV_HOLD`
- **Impact:** Mapping 1:1 entre commande et mode
- **Utilisation:** Active le mode NAV_HOLD (maintien de position)

### 3. **CMD_NAV_CAP (nouveau)**
- **Raison:** Ajout d'une commande spécifique pour le mode cap
- **Impact:** Permet de basculer explicitement en mode navigation par cap
- **Utilisation:** Active le mode NAV_CAP
- **Message dashboard:** `#E1=5`

### 4. **CMD_RETURN_HOME → CMD_NAV_HOME**
- **Raison:** Cohérence avec le mode de navigation `NAV_HOME`
- **Impact:** Nom plus explicite sur l'action (navigation, pas juste retour)
- **Utilisation:** Active la navigation vers le Home

### 5. **CMD_STOP → CMD_NAV_STOP**
- **Raison:** Cohérence avec le mode de navigation `NAV_STOP`
- **Impact:** Clarification qu'il s'agit d'un mode de navigation
- **Utilisation:** Active le mode NAV_STOP (arrêt contrôlé)

### 6. **CMD_CHANGE_MODE (supprimé)**
- **Raison:** Redondant avec les commandes NAV_* spécifiques
- **Impact:** Simplification de l'API, modes gérés par commandes explicites
- **Remplacement:** Utiliser `CMD_NAV_HOLD`, `CMD_NAV_CAP`, `CMD_NAV_HOME`, `CMD_NAV_STOP`

### 7. **CMD_HOME_VALIDATION (nouveau)**
- **Raison:** Nécessité de valider/confirmer la position Home
- **Impact:** Permet un workflow de définition de Home en deux étapes
- **Utilisation:** Confirme et valide la position Home après `CMD_INIT_HOME`
- **TODO:** Logique de validation à implémenter côté bouée

## Mapping Commande → Mode de Navigation

| Commande | Mode de Navigation | Code Dashboard | Description |
|----------|-------------------|----------------|-------------|
| `CMD_INIT_HOME` | `HOME_DEFINITION` puis `NAV_HOME` | `0XA` (X=buoyId) | Initialise Home et passe en mode Home |
| `CMD_SET_TRUE_HEADING` | *(inchangé)* | `#E2=XXX` | Définit le cap cible |
| `CMD_SET_THROTTLE` | *(inchangé)* | `#E3=XXX` | Définit la vitesse |
| `CMD_NAV_HOLD` | `NAV_HOLD` | `#E1=3` | Maintien de position |
| `CMD_NAV_CAP` | `NAV_CAP` | `#E1=5` | Navigation par cap |
| `CMD_NAV_HOME` | `NAV_HOME` | `#E1=4` | Navigation vers Home |
| `CMD_NAV_STOP` | `NAV_STOP` | `#E1=0` | Arrêt |
| `CMD_HOME_VALIDATION` | *(validation)* | *(à définir)* | Valide le Home |

## Modifications des fichiers

### 1. Projet Joystick

#### `include/CommandManager.h`
```cpp
// Avant
enum BuoyCommand {
    CMD_INIT_HOME = 0,
    CMD_SET_HEADING,
    CMD_SET_THROTTLE,
    CMD_HOLD_POSITION,
    CMD_RETURN_HOME,
    CMD_STOP,
    CMD_CHANGE_MODE
};

// Après
enum BuoyCommand {
    CMD_INIT_HOME = 0,
    CMD_SET_TRUE_HEADING,
    CMD_SET_THROTTLE,
    CMD_NAV_HOLD,
    CMD_NAV_CAP,
    CMD_NAV_HOME,
    CMD_NAV_STOP,
    CMD_HOME_VALIDATION
};
```

### 2. Projet Bouée

#### `src/ESPNowDataLinkManagement.h`
- Même modification que dans le joystick

#### `src/ESPNowDataLinkManagement.cpp`
Mise à jour du switch case :

```cpp
switch (cmd.command) {
    case CMD_INIT_HOME:
        message = (cmd.targetBuoyId < 10 ? "0" : "") + String(cmd.targetBuoyId) + "A";
        Logger::log("Command: INIT_HOME");
        break;
        
    case CMD_SET_TRUE_HEADING:  // Ancien: CMD_SET_HEADING
        message = "#E2=" + String(cmd.heading);
        Logger::log("Command: SET_TRUE_HEADING " + String(cmd.heading) + "°");
        break;
        
    case CMD_SET_THROTTLE:
        message = "#E3=" + String(cmd.throttle);
        Logger::log("Command: SET_THROTTLE " + String(cmd.throttle) + "%");
        break;
        
    case CMD_NAV_HOLD:  // Ancien: CMD_HOLD_POSITION
        message = "#E1=3";
        Logger::log("Command: NAV_HOLD");
        break;
        
    case CMD_NAV_CAP:  // NOUVEAU
        message = "#E1=5";
        Logger::log("Command: NAV_CAP");
        break;
        
    case CMD_NAV_HOME:  // Ancien: CMD_RETURN_HOME
        message = "#E1=4";
        Logger::log("Command: NAV_HOME");
        break;
        
    case CMD_NAV_STOP:  // Ancien: CMD_STOP
        message = "#E1=0";
        Logger::log("Command: NAV_STOP");
        break;
        
    case CMD_HOME_VALIDATION:  // NOUVEAU
        Logger::log("Command: HOME_VALIDATION");
        // TODO: Add home validation logic
        break;
        
    default:
        Logger::log("Command: UNKNOWN");
        return;
}
```

## Avantages de cette refonte

### 1. **Cohérence architecturale**
- Mapping clair entre commandes `CMD_NAV_*` et modes `NAV_*`
- Nomenclature uniforme et prévisible
- Facilite la compréhension du code

### 2. **Clarté des intentions**
- `CMD_SET_TRUE_HEADING` : Explicite qu'il s'agit d'un cap vrai
- `CMD_NAV_*` : Indique clairement qu'il s'agit d'un changement de mode
- `CMD_HOME_VALIDATION` : Intention claire de validation

### 3. **Extensibilité**
- Facile d'ajouter de nouveaux modes NAV_* avec leur commande correspondante
- Pattern cohérent pour futures commandes
- Prêt pour l'ajout de `CMD_NAV_TARGET` (navigation vers waypoint)

### 4. **Élimination de redondance**
- `CMD_CHANGE_MODE` supprimé car remplacé par commandes spécifiques
- Plus de confusion sur quelle commande utiliser
- Code plus maintenable

### 5. **Workflow Home amélioré**
- `CMD_INIT_HOME` : Initialise le point Home
- `CMD_HOME_VALIDATION` : Valide et confirme le Home
- Processus en deux étapes plus sûr

## Impact sur le code existant

### Code à mettre à jour (si existant)

Si du code utilise les anciennes constantes, il faudra les mettre à jour :

```cpp
// À mettre à jour
if (cmd.type == CMD_SET_HEADING) { ... }
→ if (cmd.type == CMD_SET_TRUE_HEADING) { ... }

if (cmd.type == CMD_HOLD_POSITION) { ... }
→ if (cmd.type == CMD_NAV_HOLD) { ... }

if (cmd.type == CMD_RETURN_HOME) { ... }
→ if (cmd.type == CMD_NAV_HOME) { ... }

if (cmd.type == CMD_STOP) { ... }
→ if (cmd.type == CMD_NAV_STOP) { ... }

if (cmd.type == CMD_CHANGE_MODE) { ... }
→ Supprimer ou remplacer par CMD_NAV_*
```

### Fichiers déjà mis à jour

- ✅ `CommandManager.h` (Joystick)
- ✅ `ESPNowDataLinkManagement.h` (Bouée)
- ✅ `ESPNowDataLinkManagement.cpp` (Bouée)

## Prochaines étapes

### 1. Implémenter CMD_HOME_VALIDATION
```cpp
case CMD_HOME_VALIDATION:
    // Vérifier que Home est défini
    if (homePosition.isValid()) {
        // Marquer comme validé
        homePosition.validate();
        message = "#HOME_VALIDATED";
        Logger::log("Home position validated");
    } else {
        Logger::log("ERROR: Cannot validate - Home not initialized");
    }
    break;
```

### 2. Ajouter méthodes dans CommandManager
```cpp
// Dans CommandManager.h
bool generateSetTrueHeadingCommand(uint8_t targetBuoyId, int16_t heading);
bool generateNavHoldCommand(uint8_t targetBuoyId);
bool generateNavCapCommand(uint8_t targetBuoyId);
bool generateNavHomeCommand(uint8_t targetBuoyId);
bool generateNavStopCommand(uint8_t targetBuoyId);
bool generateHomeValidationCommand(uint8_t targetBuoyId);
```

### 3. Mapper les boutons du joystick
Exemple de mapping :
- **Bouton jaune GAUCHE** : `CMD_INIT_HOME`
- **Bouton jaune DROIT** : `CMD_HOME_VALIDATION`
- **Bouton HAUT** : `CMD_NAV_HOME`
- **Bouton BAS** : `CMD_NAV_HOLD`
- **Bouton A** : `CMD_NAV_CAP`
- **Bouton B** : `CMD_NAV_STOP`

### 4. Ajouter mode NAV_TARGET (futur)
```cpp
enum BuoyCommand {
    // ... existing commands ...
    CMD_NAV_TARGET,         ///< Navigate to target waypoint
    CMD_SET_TARGET_WAYPOINT ///< Set target waypoint coordinates
};
```

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
- Flash: 1,257,093 bytes (19.2%)

## Tests recommandés

### Test 1: CMD_INIT_HOME
1. Presser le bouton d'initialisation Home
2. Vérifier le log "Command: INIT_HOME"
3. Vérifier que la bouée enregistre la position

### Test 2: CMD_SET_TRUE_HEADING
1. Envoyer une commande de cap
2. Vérifier le log "Command: SET_TRUE_HEADING XXX°"
3. Vérifier que la bouée suit le cap

### Test 3: CMD_NAV_CAP (nouveau)
1. Envoyer la commande NAV_CAP
2. Vérifier le log "Command: NAV_CAP"
3. Vérifier message dashboard `#E1=5`
4. Vérifier que la bouée passe en mode NAV_CAP

### Test 4: CMD_NAV_HOLD
1. Envoyer la commande NAV_HOLD
2. Vérifier le log "Command: NAV_HOLD"
3. Vérifier que la bouée maintient sa position

### Test 5: CMD_NAV_HOME
1. Après initialisation du Home
2. Envoyer la commande NAV_HOME
3. Vérifier que la bouée navigue vers le Home

### Test 6: CMD_NAV_STOP
1. Pendant une navigation
2. Envoyer la commande NAV_STOP
3. Vérifier l'arrêt de la bouée

### Test 7: CMD_HOME_VALIDATION (nouveau)
1. Après CMD_INIT_HOME
2. Envoyer CMD_HOME_VALIDATION
3. Vérifier le log "Command: HOME_VALIDATION"
4. TODO: Vérifier la logique de validation

## Notes importantes

1. **Rétrocompatibilité:** Cette modification CASSE la compatibilité avec les anciennes versions. Les deux firmwares (joystick + bouée) doivent être mis à jour ensemble.

2. **Valeurs de l'énuméré:** Les valeurs numériques ont changé. Par exemple, `CMD_STOP` était 5, maintenant `CMD_NAV_STOP` est 6.

3. **CMD_NAV_CAP:** Nouveau mode à bien tester car il n'existait pas avant en tant que commande explicite.

4. **CMD_HOME_VALIDATION:** Logique de validation à implémenter côté bouée (marqué TODO dans le code).

5. **Nomenclature:** Préfixe `CMD_NAV_*` réservé aux commandes de changement de mode de navigation.
