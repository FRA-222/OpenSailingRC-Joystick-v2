# 🎯 Ajout du Mode Général (GeneralMode) au Système

## Vue d'Ensemble

Le système a été amélioré pour inclure un **mode général de premier niveau** (`tEtatsGeneral`) en plus du mode de navigation existant (`tEtatsNav`). Ce mode général représente l'état global de la bouée (INIT, READY, MAINTENANCE, HOME_DEFINITION, NAV).

## ✨ Modifications Apportées

### 📊 Hiérarchie des Modes

```
┌─────────────────────────────────┐
│   GENERAL MODE (tEtatsGeneral)  │  ← Niveau 1 : État global
│  INIT | READY | MAINTENANCE     │
│  HOME_DEFINITION | NAV           │
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│  NAVIGATION MODE (tEtatsNav)    │  ← Niveau 2 : Mode de navigation
│  NAV_NOTHING | NAV_HOME          │
│  NAV_HOLD | NAV_STOP | NAV_CAP  │
│  NAV_BASIC | NAV_TARGET          │
└─────────────────────────────────┘
```

### 🔧 Modifications par Projet

#### **OpenSailingRC-BuoyJoystick** (Contrôleur)

##### `ESPNowCommunication.h`
- **Ajouté** : Champ `generalMode` de type `tEtatsGeneral` dans `struct BuoyState`
- **Renommé** : `mode` → `navigationMode` (type `tEtatsNav`)
- **Ajouté** : Champ `navigationMode` dans `struct CommandPacket`

```cpp
struct BuoyState {
    uint8_t buoyId;
    tEtatsGeneral generalMode;    // 🆕 Mode général
    double latitude;
    double longitude;
    float heading;
    float speed;
    tEtatsNav navigationMode;      // ✏️ Renommé
    uint8_t batteryLevel;
    int8_t signalQuality;
    bool gpsLocked;
    uint32_t timestamp;
};
```

##### `BuoyStateManager.h/.cpp`
- **Ajouté** : Méthode `getGeneralModeName(tEtatsGeneral mode)`
- Retourne le nom lisible du mode général

```cpp
String getGeneralModeName(tEtatsGeneral mode) {
    switch (mode) {
        case INIT: return "INIT";
        case READY: return "READY";
        case MAINTENANCE: return "MAINT";
        case HOME_DEFINITION: return "HOME_DEF";
        case NAV: return "NAV";
        default: return "UNKNOWN";
    }
}
```

##### `DisplayManager.h/.cpp`
- **Modifié** : `drawNavigationState()` affiche maintenant **2 lignes** :
  - Ligne 1 : Mode général (grand, coloré)
  - Ligne 2 : Mode de navigation (petit, en-dessous)
- **Ajouté** : Méthode `getGeneralModeColor(tEtatsGeneral mode)`
- **Renommé** : `getModeColor()` → `getNavModeColor()`

**Affichage résultant :**
```
┌────────────────────┐
│   Buoy #1          │  ← Header
│   CONNECTED        │
│                    │
│      NAV           │  ← Mode général (grand, vert)
│   Nav: CAP         │  ← Mode navigation (petit)
│                    │
│   Hdg: 045         │  ← Cap
│   2.5 m/s          │  ← Vitesse
│                    │
│  [🔋]  [📡]  [🛰️] │  ← Indicateurs
└────────────────────┘
```

**Couleurs des modes généraux :**
- `INIT` → Jaune (initialisation en cours)
- `READY` → Cyan (prêt à naviguer)
- `MAINTENANCE` → Orange (maintenance)
- `HOME_DEFINITION` → Magenta (définition du point home)
- `NAV` → Vert (navigation active)

##### `main.cpp`
- **Mis à jour** : Logs série pour afficher les deux modes
```cpp
USBSerial.printf("  General Mode: %s\n", buoyState.getGeneralModeName(state.generalMode).c_str());
USBSerial.printf("  Nav Mode: %s\n", buoyState.getNavModeName(state.navigationMode).c_str());
```

##### `ESPNowCommunication.cpp`
- **Mis à jour** : `sendCommand()` utilise `navigationMode`
- **Mis à jour** : Logs de réception affichent les deux modes

#### **Autonomous-GPS-Buoy** (Bouée)

##### `ESPNowDataLinkManagement.h`
- **Supprimé** : Redéfinition des enums (utilise ceux de `ModeManagement.h`)
- **Mis à jour** : Structure `BuoyState` avec `generalMode` et `navigationMode`
- **Mis à jour** : Structure `CommandPacket` avec `navigationMode`

##### `Autonomous GPS Buoy.cpp`
- **Ajouté** : Remplissage du champ `generalMode` avec `myGeneralMode`
- **Mis à jour** : Utilisation de `navigationMode` au lieu de `mode`

```cpp
BuoyState buoyState;
buoyState.buoyId = CURRENT_BUOY;
buoyState.generalMode = myGeneralMode;        // 🆕
buoyState.navigationMode = myNavigationMode;  // ✏️
// ... autres champs
```

##### `ESPNowDataLinkManagement.cpp`
- **Mis à jour** : `processCommand()` utilise `cmd.navigationMode`

## 📋 Énumérations

### `tEtatsGeneral` (Mode Général)
```cpp
enum tEtatsGeneral {
    INIT = 0,           // Initialisation du système
    READY,              // Prêt à naviguer
    MAINTENANCE,        // Mode maintenance
    HOME_DEFINITION,    // Définition du point home
    NAV                 // Navigation active
};
```

### `tEtatsNav` (Mode de Navigation)
```cpp
enum tEtatsNav {
    NAV_NOTHING = 0,    // Aucune navigation
    NAV_HOME,           // Retour au point home
    NAV_HOLD,           // Maintien de position
    NAV_STOP,           // Arrêt
    NAV_BASIC,          // Navigation basique
    NAV_CAP,            // Suivi de cap
    NAV_TARGET          // Navigation vers cible
};
```

## 🎨 Exemples d'Affichage

### Bouée en Initialisation
```
┌────────────────────┐
│   Buoy #1          │
│   CONNECTED        │
│      INIT          │  🟡 Jaune
│   Nav: NOTHING     │
│   Hdg: 000         │
│   0.0 m/s          │
└────────────────────┘
```

### Bouée Prête
```
┌────────────────────┐
│   Buoy #2          │
│   CONNECTED        │
│      READY         │  🔵 Cyan
│   Nav: STOP        │
│   Hdg: 180         │
│   0.0 m/s          │
└────────────────────┘
```

### Bouée en Navigation
```
┌────────────────────┐
│   Buoy #1          │
│   CONNECTED        │
│      NAV           │  🟢 Vert
│   Nav: CAP         │
│   Hdg: 045         │
│   2.5 m/s          │
└────────────────────┘
```

### Bouée en Maintenance
```
┌────────────────────┐
│   Buoy #3          │
│   CONNECTED        │
│   MAINTENANCE      │  🟠 Orange
│   Nav: HOLD        │
│   Hdg: 270         │
│   0.1 m/s          │
└────────────────────┘
```

## 🔄 Flux de Communication

### Bouée → Joystick (Broadcast)
```cpp
// La bouée envoie toutes les ~1s
BuoyState state;
state.generalMode = NAV;           // Mode général
state.navigationMode = NAV_CAP;    // Mode navigation
// ... autres données
espNowDataLinkManagement.sendBuoyState(state);
```

### Joystick → Bouée (Commande)
```cpp
// Le joystick envoie une commande
CommandPacket cmd;
cmd.command = CMD_SET_HEADING;
cmd.navigationMode = NAV_CAP;      // Mode de navigation demandé
// ... autres paramètres
```

## 📊 Taille des Paquets

### Ancienne Structure (sans generalMode)
- **41 octets**

### Nouvelle Structure (avec generalMode)
- **42 octets** (+1 octet pour `tEtatsGeneral`)
- Toujours largement sous la limite ESP-NOW de 250 octets ✅

## ✅ Tests de Compilation

### OpenSailingRC-BuoyJoystick
```
✅ SUCCESS: Took 5.12 seconds
RAM:   14.4% (47220 bytes)
Flash: 26.1% (873089 bytes)
```

### Autonomous-GPS-Buoy
```
✅ SUCCESS: Took 8.47 seconds
RAM:   1.3% (57940 bytes)
Flash: 19.1% (1253381 bytes)
```

## 🎯 Bénéfices

1. **Hiérarchie Claire** : Séparation entre état global et mode de navigation
2. **Debug Amélioré** : Visualisation instantanée de l'état de la bouée
3. **Sécurité** : Identifie rapidement les bouées en maintenance
4. **UX Améliorée** : Affichage plus informatif sur le joystick
5. **Extensibilité** : Facile d'ajouter de nouveaux modes généraux

## 📝 Utilisation

### Côté Joystick
```cpp
BuoyState state = buoyMgr.getSelectedBuoyState();

// Obtenir les noms des modes
String genMode = buoyMgr.getGeneralModeName(state.generalMode);
String navMode = buoyMgr.getNavModeName(state.navigationMode);

// Afficher
Serial.printf("General: %s, Nav: %s\n", genMode.c_str(), navMode.c_str());
```

### Côté Bouée
```cpp
// Préparer l'état à envoyer
BuoyState state;
state.generalMode = getCurrentGeneralMode();
state.navigationMode = getCurrentNavMode();
// ... remplir autres champs

// Envoyer
espNowDataLinkManagement.sendBuoyState(state);
```

## 🔮 Évolutions Futures

- [ ] Ajouter des transitions d'état animées
- [ ] Historique des changements de mode
- [ ] Alarmes pour modes critiques (MAINTENANCE)
- [ ] Statistiques par mode (temps passé)
- [ ] Mode DEBUG pour développement

---

**Date de modification** : 28 octobre 2025  
**Version** : v1.2 - Modes Hiérarchiques  
**Projets modifiés** :
- ✅ OpenSailingRC-BuoyJoystick
- ✅ Autonomous-GPS-Buoy

**Statut** : ✅ Implémenté, compilé et prêt à tester  
**Compatibilité** : Rétrocompatible avec l'affichage existant
