# 🎮 OpenSailingRC BuoyJoystick - v2.0

## ✅ État d'Avancement

### Modules Implémentés

- [x] **JoystickManager** - Lecture I2C des joysticks et boutons (5 boutons incluant écran Atom S3)
- [x] **ESPNowCommunication** - Communication bidirectionnelle ESP-NOW avec découverte dynamique
- [x] **BuoyStateManager** - Gestion état des bouées avec modes général et navigation
- [x] **DisplayManager** - Affichage LCD 128x128 avec informations complètes
- [x] **Logger** - Système de logging centralisé (série + LCD) - 100% intégré
- [x] **CommandManager** - Génération et envoi de commandes aux bouées
- [x] **main.cpp** - Loop principal avec intégration complète

### Fonctionnalités Opérationnelles

✅ **Contrôles**
- Lecture des 2 joysticks (4 axes avec calibration)
- Lecture des 5 boutons (4 JoyC + 1 écran tactile Atom S3)
- Détection pression simple, maintien prolongé et relâchement
- Lecture batteries (2x)

✅ **Communication ESP-NOW**
- Découverte automatique des bouées via broadcast
- Nettoyage automatique des bouées inactives (15s timeout)
- Support jusqu'à 6 bouées simultanées
- Réception état des bouées (1Hz)
- Envoi commandes bidirectionnel

✅ **Commandes Disponibles**
- INIT_HOME (bouton jaune gauche)
- Sélection bouée active (bouton jaune droit / écran Atom)
- Modes général et navigation

✅ **Affichage**
- LCD temps réel (128x128)
- Header avec ID bouée et indicateur LED bouée
- Affichage modes général et navigation hiérarchiques
- Indicateurs batterie/GPS/Signal avec icônes
- Informations de navigation (cap, vitesse)
- Codes couleur selon état (vert/orange/rouge/bleu/cyan/jaune)

✅ **Logging**
- Système centralisé Logger dans toutes les classes
- 90 remplacements Serial → Logger
- Impact mémoire négligeable (<3KB Flash)

### Prochaines Évolutions

- [ ] Mapping complet Joystick → Commandes
- [ ] Commande SET_HEADING (Joystick droit X)
- [ ] Commande SET_THROTTLE (Joystick gauche Y)
- [ ] Commande HOLD (BTN_LEFT_STICK)
- [ ] Commande STOP/RETURN
- [ ] Menu de configuration via écran

---

## 🚀 Démarrage Rapide

### 1. Configuration Automatique

**Plus besoin de configurer les adresses MAC !** 

Le système utilise maintenant la **découverte automatique des bouées** :
- Les bouées sont détectées automatiquement lors de leur premier broadcast
- Support jusqu'à 6 bouées simultanées
- Nettoyage automatique des bouées inactives après 15 secondes

### 2. Compilation

```bash
cd "/Users/philippe/Documents/PlatformIO/Projects/OpenSailingRC-BuoyJoystick"
platformio run
```

### 3. Upload

```bash
platformio run --target upload
```

### 4. Moniteur Série

```bash
platformio device monitor
```

## 📡 Communication ESP-NOW

### Découverte Dynamique des Bouées

Le système détecte automatiquement les bouées :
- **Découverte** : Première réception d'un broadcast → bouée ajoutée automatiquement
- **Monitoring** : État reçu toutes les secondes (1Hz)
- **Nettoyage** : Bouées inactives retirées après 15 secondes
- **Capacité** : Jusqu'à 6 bouées simultanées (constante `MAX_BUOYS`)

### Structure CommandPacket (Joystick → Bouée)

```cpp
struct CommandPacket {
    uint8_t targetBuoyId;      // 0-5
    BuoyCommand command;       // Type de commande
    int16_t heading;           // Cap cible (-180 à +180°)
    int8_t throttle;           // Vitesse (-100 à +100%)
    tEtatsNav navigationMode;  // Mode de navigation
    uint32_t timestamp;        // Horodatage
};
```

### Structure BuoyState (Bouée → Joystick)

```cpp
struct BuoyState {
    // Identification
    uint8_t buoyId;                     // ID de la bouée (0-5)
    uint32_t timestamp;                 // Horodatage du message
    
    // État général
    tEtatsGeneral generalMode;          // Mode général (INIT/READY/MAINTENANCE/HOME_DEFINITION/NAV)
    tEtatsNav navigationMode;           // Mode navigation (NAV_NOTHING/HOME/HOLD/STOP/CAP/BASIC/TARGET)
    
    // État des capteurs
    bool gpsOk;                         // État capteur GPS
    bool headingOk;                     // État capteur cap
    bool yawRateOk;                     // État capteur vitesse de lacet
    
    // Données environnementales
    float temperature;                  // Température en °C
    
    // Données batterie
    float remainingCapacity;            // Capacité restante en mAh
    
    // Données de navigation
    float distanceToCons;               // Distance à la consigne/waypoint en mètres
    
    // Commandes autopilote
    int8_t autoPilotThrottleCmde;       // Commande throttle autopilote (-100 à +100%)
    float autoPilotTrueHeadingCmde;     // Commande cap autopilote en degrés
    int8_t autoPilotRudderCmde;         // Commande safran autopilote (-100 à +100%)
    
    // Commandes forcées
    int8_t forcedThrottleCmde;          // Commande throttle forcée (-100 à +100%)
    bool forcedThrottleCmdeOk;          // Flag commande throttle forcée active
    float forcedTrueHeadingCmde;        // Commande cap forcée en degrés
    bool forcedTrueHeadingCmdeOk;       // Flag commande cap forcée active
    int8_t forcedRudderCmde;            // Commande safran forcée (-100 à +100%)
    bool forcedRudderCmdeOk;            // Flag commande safran forcée active
};
```

### Hiérarchie des Modes

```
Mode Général (tEtatsGeneral)     ← Niveau 1 : État global
├─ INIT
├─ READY
├─ MAINTENANCE
├─ HOME_DEFINITION
└─ NAV
    │
    └─ Mode Navigation (tEtatsNav)  ← Niveau 2 : Mode de navigation
       ├─ NAV_NOTHING
       ├─ NAV_HOME
       ├─ NAV_HOLD
       ├─ NAV_STOP
       ├─ NAV_CAP
       ├─ NAV_BASIC
       └─ NAV_TARGET
```
    double longitude;
    float heading;           // Cap actuel
    float speed;             // Vitesse m/s
    NavigationMode mode;     // Mode actuel
    uint8_t batteryLevel;    // Batterie 0-100%
    int8_t signalQuality;    // Signal LTE (-1 si pas LTE)
    bool gpsLocked;          // GPS verrouillé
    uint32_t timestamp;      // Horodatage
};
```

### Fréquences

- **Réception état bouées**: 1 Hz (toutes les secondes)
- **Nettoyage bouées inactives**: 30 secondes
- **Timeout bouée**: 15 secondes
- **Loop principal**: 10 Hz (100ms)
- **Mise à jour affichage**: 2 Hz (500ms)

---

## 🖥️ Affichage LCD

### Écran Principal

```
┌────────────────────────┐
│  Bouée #1  ●           │ Header (cyan) + LED état bouée
├────────────────────────┤
│   CONNECTE             │ État connexion (vert/rouge)
│                        │
│   MODE_GENERAL         │ Mode général (INIT/READY/NAV...)
│   MODE_NAVIGATION      │ Mode navigation (CAP/HOLD...)
│                        │
│   Cap: 045°            │ Informations navigation
│   2.5 m/s              │
│                        │
│ 🔋85%  🛰️GPS  📶LTE   │ Indicateurs
└────────────────────────┘
```

### Nouveautés v2.0

- **LED d'état** : Indicateur visuel en haut à droite du header
- **Double affichage modes** : Mode général ET mode navigation
- **Codes couleur enrichis** : 
  - Vert : Connecté, batterie >50%
  - Orange : Batterie 20-50%
  - Rouge : Déconnecté, batterie <20%
  - Bleu : Mode NAV_HOME
  - Cyan : Header, mode NAV_TARGET
  - Jaune : Mode NAV_HOLD
  - Blanc : Mode INIT/MAINTENANCE

---

## 🎮 Contrôles

### Boutons Disponibles

Le système reconnaît **5 boutons** :

```cpp
BTN_LEFT_STICK  (0)  // Bouton sur joystick gauche
BTN_RIGHT_STICK (1)  // Bouton sur joystick droit
BTN_LEFT        (2)  // Bouton jaune gauche
BTN_RIGHT       (3)  // Bouton jaune droit
BTN_ATOM_SCREEN (4)  // 🆕 Écran tactile Atom S3
```

### Fonctionnalités Actuelles

| Bouton | Action | Fonction |
|--------|--------|----------|
| **BTN_LEFT** | Pression courte | Commande INIT_HOME à la bouée active |
| **BTN_RIGHT** | Pression courte | Sélectionner bouée suivante |
| **BTN_ATOM_SCREEN** | Pression courte | Sélectionner bouée suivante (alternative) |

### Détections Disponibles

- **Pression simple** : `wasButtonPressed()`
- **Relâchement** : `wasButtonReleased()`
- **Maintien prolongé** : `isAtomScreenHeld(durationMs)`

### À Implémenter

- **Joystick GAUCHE Y** → Throttle (vitesse)
- **Joystick DROIT X** → Cap (+/- 10°)
- **BTN_RIGHT_STICK** → Fonction à définir
- **BTN_LEFT_STICK** → HOLD
- **Maintien BTN_LEFT** → RETURN HOME

---

## 🔧 Architecture Logicielle

```
main.cpp (Loop 10Hz)
    │
    ├─→ JoystickManager.update()
    │   └─→ Lit I2C (STM32 @ 0x59)
    │       ├─→ 4 axes joystick (calibrés)
    │       ├─→ 5 boutons (incluant écran Atom)
    │       └─→ 2 batteries
    │
    ├─→ BuoyStateManager.update()
    │   └─→ ESPNowCommunication
    │       ├─→ Découverte automatique des bouées
    │       ├─→ Reçoit BuoyState (1Hz)
    │       ├─→ Nettoyage bouées inactives (15s)
    │       └─→ Envoie CommandPacket
    │
    ├─→ CommandManager.generateInitHomeCommand()
    │   └─→ Création et envoi de commandes
    │       └─→ ESPNowCommunication.sendCommand()
    │
    └─→ DisplayManager.update()
        └─→ Affiche sur LCD 128x128
            ├─→ Header avec LED état
            ├─→ Mode général + navigation
            ├─→ Cap / Vitesse
            └─→ Batterie / GPS / Signal
```

### Modules et Responsabilités

| Module | Responsabilité | Fichiers |
|--------|----------------|----------|
| **JoystickManager** | Interface I2C avec STM32, lecture joysticks/boutons | `JoystickManager.h/cpp` |
| **ESPNowCommunication** | Protocole ESP-NOW, découverte dynamique | `ESPNowCommunication.h/cpp` |
| **BuoyStateManager** | Gestion états des bouées, sélection active | `BuoyStateManager.h/cpp` |
| **DisplayManager** | Affichage LCD, interface utilisateur visuelle | `DisplayManager.h/cpp` |
| **CommandManager** | Génération et envoi de commandes aux bouées | `CommandManager.h/cpp` |
| **Logger** | Logging centralisé série + LCD | `Logger.h/cpp` |

---

## 📊 Logs Série

Le système utilise le **Logger centralisé** pour tous les messages.

### Au Démarrage

```
===========================================
  OpenSailingRC - Buoy Joystick v2.0
===========================================

1. Initialisation Joystick...
✓ JoystickManager: STM32 détecté sur I2C

2. Initialisation ESP-NOW...
✓ ESP-NOW: Adresse MAC locale: 48:E7:29:9E:2B:AC
✓ ESP-NOW: Initialisé
✓ Découverte automatique des bouées activée

3. Initialisation BuoyStateManager...
✓ BuoyStateManager: Initialisé
✓ Support jusqu'à 6 bouées simultanées

4. Initialisation CommandManager...
✓ CommandManager: Initialisé

5. Initialisation Display...
✓ DisplayManager: Initialisé

===========================================
  SYSTÈME PRÊT - Découverte automatique
===========================================
```

### Découverte d'une Bouée

```
[ESP-NOW] Nouvelle bouée détectée !
  MAC: 48:E7:29:9E:2B:AC
  Bouée ID: 0
✓ Bouée #0 ajoutée dynamiquement
✓ 1 bouée(s) active(s)
```

### En Fonctionnement

```
--- État système ---
Joystick L: X=-12 Y=234
Joystick R: X=0 Y=-5
Batteries: 4.15V / 4.18V
Bouée sélectionnée: #0
Bouées actives: 1/6
  Mode général: NAV
  Mode navigation: NAV_CAP
  Cap: 45° Vitesse: 2.3 m/s
  GPS: OK Batterie: 87%
-------------------

[ESP-NOW] État reçu de Bouée #0
  Mode: NAV → NAV_CAP
  Position: 48.8566°N, 2.3522°E
  Batterie: 87%, GPS: Locked

[CommandManager] Commande INIT_HOME envoyée à Bouée #0
```

### Nettoyage Automatique

```
[ESP-NOW] Nettoyage bouées inactives...
⚠️ Bouée #2 inactive (dernier contact: il y a 16s)
✓ Bouée #2 retirée
✓ 1 bouée(s) active(s)
```

---

## 🧪 Tests

### Test 1: Joystick et Boutons

1. Compiler et flasher le contrôleur
2. Ouvrir le moniteur série
3. Vérifier que les 4 axes et 5 boutons sont détectés
4. Bouger les joysticks → Valeurs changent
5. Presser les boutons (incluant écran Atom) → Détection OK

**Résultat attendu**: ✅ Valeurs affichées avec calibration

### Test 2: Découverte Automatique

1. Flasher la bouée avec firmware ESP-NOW compatible
2. Flasher le contrôleur (pas de configuration MAC nécessaire)
3. Allumer la bouée
4. Observer les logs série du contrôleur

**Résultat attendu**:
- ✅ Détection automatique de la bouée
- ✅ État reçu toutes les secondes
- ✅ Log "Nouvelle bouée détectée" avec MAC et ID

### Test 3: Modes Hiérarchiques

1. Avec bouée connectée
2. Observer l'affichage LCD
3. Vérifier affichage simultané du mode général ET navigation
4. Tester différents modes sur la bouée

**Résultat attendu**: ✅ Double affichage des modes

### Test 4: Commande INIT_HOME

1. Sélectionner une bouée active
2. Presser BTN_LEFT (bouton jaune gauche)
3. Observer logs série

**Résultat attendu**: 
- ✅ Log "[CommandManager] Commande INIT_HOME envoyée"
- ✅ Bouée reçoit et exécute la commande

### Test 5: Nettoyage Automatique

1. Connecter une bouée
2. Éteindre la bouée
3. Attendre 15 secondes
4. Observer logs série après 30s (cycle de nettoyage)

**Résultat attendu**: 
- ✅ Log "Bouée #X inactive"
- ✅ Bouée retirée automatiquement
- ✅ Mise à jour du compteur de bouées actives

### Test 6: Multi-Bouées

1. Allumer plusieurs bouées (jusqu'à 6)
2. Observer leur découverte automatique
3. Utiliser BTN_RIGHT ou écran Atom pour naviguer
4. Vérifier l'affichage pour chaque bouée

**Résultat attendu**: 
- ✅ Toutes les bouées détectées
- ✅ Navigation fluide entre bouées
- ✅ Affichage correct de chaque état

---

## ⚠️ Dépannage

### Problème: "Échec initialisation joystick"

**Cause**: STM32 ne répond pas sur I2C

**Solution**:
1. Vérifier connexion I2C (SDA=38, SCL=39)
2. Vérifier alimentation du STM32
3. Scanner I2C (devrait trouver 0x59)

### Problème: "Échec initialisation ESP-NOW"

**Cause**: WiFi non configuré correctement

**Solution**:
1. Vérifier que `WiFi.mode(WIFI_STA)` est appelé
2. Redémarrer l'ESP32
3. Vérifier version du firmware ESP32

### Problème: Bouées non détectées automatiquement

**Cause**: Bouée ne broadcast pas ou hors de portée

**Solution**:
1. Vérifier que la bouée envoie des paquets `BuoyState`
2. Vérifier que la bouée est sous tension et initialisée
3. Vérifier portée ESP-NOW (<100m en champ libre)
4. Observer les logs série pour voir si des paquets arrivent

### Problème: Bouées disparaissent après 15 secondes

**Cause**: Nettoyage automatique actif (normal si bouée inactive)

**Solution**:
1. Vérifier que la bouée envoie bien des états toutes les secondes
2. Si c'est normal, la bouée sera redétectée au prochain broadcast
3. Ajuster le timeout `BUOY_TIMEOUT_MS` si nécessaire

### Problème: Écran noir

**Cause**: LCD non initialisé

**Solution**:
1. Vérifier que `M5.begin(true, ...)` est appelé
2. Vérifier alimentation
3. Essayer `display.setBrightness(255)`

### Problème: Bouton écran Atom ne répond pas

**Cause**: Bouton tactile non détecté

**Solution**:
1. Vérifier calibration tactile de l'Atom S3
2. Utiliser `wasAtomScreenPressed()` au lieu de `isButtonPressed(4)`
3. Tester avec maintien prolongé `isAtomScreenHeld(1000)`

---

## 📈 Évolutions et Améliorations

### Version 2.0 - Actuelle (Octobre 2025)

✅ **Communication Améliorée**
- Découverte automatique des bouées (plus de configuration MAC)
- Nettoyage automatique des bouées inactives
- Support jusqu'à 6 bouées simultanées

✅ **Interface Enrichie**
- Support du bouton tactile écran Atom S3 (5ème bouton)
- Détection pression, relâchement et maintien prolongé
- LED d'état dans le header de l'affichage

✅ **Architecture de Modes**
- Hiérarchie à deux niveaux (Général + Navigation)
- Mode général : INIT/READY/MAINTENANCE/HOME_DEFINITION/NAV
- Mode navigation : NOTHING/HOME/HOLD/STOP/CAP/BASIC/TARGET

✅ **Système de Logging**
- Logger centralisé dans toutes les classes
- 90 remplacements Serial → Logger
- Support série + LCD simultané

✅ **CommandManager Opérationnel**
- Classe dédiée pour génération de commandes
- Encapsulation de la logique d'envoi
- Commande INIT_HOME fonctionnelle

### Prochaines Étapes

🔜 **Contrôle Complet**
- Mapping joystick → commandes de navigation
- Throttle (joystick gauche Y)
- Heading (joystick droit X)
- Commandes HOLD, STOP, RETURN

🔜 **Interface Avancée**
- Menu de configuration
- Affichage trajectoire/historique
- Calibration joystick interactive

🔜 **Fonctionnalités**
- Waypoints multiples
- Missions programmées
- Enregistrement sessions

---

## 📚 Documentation Complémentaire

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Architecture détaillée du système
- **[API_DOCUMENTATION.md](API_DOCUMENTATION.md)** - Documentation complète des APIs
- **[GENERAL_MODE_FEATURE.md](GENERAL_MODE_FEATURE.md)** - Système de modes hiérarchiques
- **[DYNAMIC_BUOY_DISCOVERY.md](DYNAMIC_BUOY_DISCOVERY.md)** - Découverte automatique
- **[ATOM_SCREEN_BUTTON.md](ATOM_SCREEN_BUTTON.md)** - Support bouton écran tactile
- **[COMMAND_MANAGER_REFACTORING.md](COMMAND_MANAGER_REFACTORING.md)** - Architecture CommandManager
- **[docs/LOGGER_FINAL_SUMMARY.md](docs/LOGGER_FINAL_SUMMARY.md)** - Intégration complète du Logger

---

## 💾 Spécifications Techniques

### Matériel
- **Contrôleur** : M5Stack Atom S3
- **Écran** : LCD 128x128 pixels (tactile)
- **Joysticks** : Dual Atom JoyStick (STM32F030F4P6)
- **Communication** : ESP-NOW 2.4GHz
- **Portée** : ~100m en champ libre
- **Batteries** : 2x mesure de tension

### Logiciel
- **Plateforme** : PlatformIO
- **Framework** : Arduino ESP32
- **RAM utilisée** : ~47KB (14.4%)
- **Flash utilisée** : ~879KB (26.3%)
- **Fréquence loop** : 10Hz
- **Fréquence affichage** : 2Hz

---

**Version**: 2.0  
**Date**: 29 octobre 2025  
**Auteur**: Philippe Hubert  
**Status**: ✅ Système Opérationnel - Prêt pour extensions
