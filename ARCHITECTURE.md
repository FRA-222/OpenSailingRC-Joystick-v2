# 🎮 OpenSailingRC BuoyJoystick - Architecture

## 📋 Vue d'Ensemble

Système de contrôle à distance pour bouées GPS autonomes utilisant un M5Stack AtomS3 avec Atom JoyStick.

## 🏗️ Architecture Modulaire

```
┌────────────────────────────────────────────────────────────────┐
│                    M5AtomS3 Joystick Controller                │
│                                                                │
│  ┌──────────────┐    ┌──────────────┐    ┌─────────────────┐ │
│  │   Hardware   │    │  Managers    │    │  Communication  │ │
│  ├──────────────┤    ├──────────────┤    ├─────────────────┤ │
│  │ 2 Joysticks  │───▶│  Joystick    │───▶│   Command       │ │
│  │ 4 Buttons    │    │   Manager    │    │   Manager       │ │
│  │ LCD 128x128  │    └──────────────┘    └─────────────────┘ │
│  │ 2 Batteries  │            │                     │          │
│  │ STM32 I2C    │            ▼                     ▼          │
│  └──────────────┘    ┌──────────────┐    ┌─────────────────┐ │
│                      │    Display    │    │    ESP-NOW      │ │
│                      │    Manager    │    │ Communication   │ │
│                      └──────────────┘    └─────────────────┘ │
│                              │                     │          │
│                              ▼                     ▼          │
│                      ┌──────────────────────────────────────┐ │
│                      │     Buoy State Manager              │ │
│                      │  (Gère état de toutes les bouées)   │ │
│                      └──────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
                                    │
                                    │ ESP-NOW (2.4GHz)
                                    ▼
                    ┌───────────────────────────────┐
                    │   Autonomous GPS Buoy #1-5    │
                    │  (Receive commands / Send state) │
                    └───────────────────────────────┘
```

## 📦 Modules

### 1. **JoystickManager** 🕹️
**Fichier**: `JoystickManager.h/cpp`

**Responsabilité**: Communication I2C avec le STM32F030F4P6 pour lire joysticks et boutons

**API Principales**:
- `begin()` - Initialise I2C (GPIO 38/39, 400kHz)
- `update()` - Lit toutes les valeurs (joysticks + boutons)
- `getAxisValue(axis)` - Valeur brute 12-bit (0-4095)
- `getAxisCentered(axis)` - Valeur centrée (-2048 à +2047)
- `isButtonPressed(btn)` - État du bouton
- `wasButtonPressed(btn)` - Détection de front montant
- `getBattery1Voltage()` / `getBattery2Voltage()` - Tensions

**Mapping Joysticks**:
```
Joystick GAUCHE:
├─ Axe X (AXIS_LEFT_X): Non utilisé
└─ Axe Y (AXIS_LEFT_Y): THROTTLE (vitesse bouée)

Joystick DROIT:
├─ Axe X (AXIS_RIGHT_X): CAP +/- 10 degrés
└─ Axe Y (AXIS_RIGHT_Y): Non utilisé
```

**Mapping Boutons**:
```
BTN_LEFT_STICK:  HOLD (maintenir position)
BTN_RIGHT_STICK: INIT HOME (initialise home et démarre CAP mode)
BTN_LEFT:        Bouée précédente
BTN_RIGHT:       Bouée suivante
BTN_M5 (41):     STOP / RETURN HOME (appui long)
```

---

### 2. **CommandManager** 🎯
**Fichier**: `CommandManager.h/cpp`

**Responsabilité**: Traduit les entrées joystick en commandes bouée

**Commandes Disponibles**:
- `CMD_INIT_HOME`: Définit position GPS actuelle comme HOME et démarre mode CAP
- `CMD_SET_HEADING`: Change le cap cible (incréments de 10°)
- `CMD_SET_THROTTLE`: Ajuste la vitesse (-100% à +100%)
- `CMD_HOLD_POSITION`: Maintient position GPS actuelle
- `CMD_RETURN_HOME`: Retourne à la position HOME
- `CMD_STOP`: Arrête tous mouvements

**API Principales**:
- `update(leftX, leftY, rightX, rightY, btns)` - Génère commandes
- `getCommand()` - Récupère dernière commande
- `hasNewCommand()` - Vérifie si nouvelle commande disponible
- `getCurrentHeading()` - Cap actuel
- `getCurrentThrottle()` - Vitesse actuelle

**Logique**:
```cpp
// Zone morte joystick: ±300
// Incrémentation cap: 10° toutes les 200ms
// Throttle: -100% (arrière) à +100% (avant)

if (rightX > 300) cap += 10°;
if (rightX < -300) cap -= 10°;
throttle = map(leftY, -2048, 2047, -100, 100);
```

---

### 3. **ESPNowCommunication** 📡
**Fichier**: `ESPNowCommunication.h/cpp`

**Responsabilité**: Communication bidirectionnelle ESP-NOW avec les bouées

**Structures de Données**:

**CommandPacket** (Envoi vers bouée):
```cpp
struct CommandPacket {
    uint8_t targetBuoyId;     // 0-4
    BuoyCommand command;      
    int16_t heading;          // -180 à +180°
    int8_t throttle;          // -100 à +100%
    NavigationMode mode;
    uint32_t timestamp;
};
```

**BuoyState** (Réception depuis bouée):
```cpp
struct BuoyState {
    uint8_t buoyId;
    double latitude;
    double longitude;
    float heading;            // Cap actuel
    float speed;              // Vitesse m/s
    NavigationMode mode;      // Mode actuel
    uint8_t batteryLevel;     // 0-100%
    int8_t signalQuality;     // Qualité LTE (-1 si pas LTE)
    bool gpsLocked;
    uint32_t timestamp;
};
```

**API Principales**:
- `begin()` - Initialise ESP-NOW et WiFi
- `addBuoy(id, macAddress)` - Ajoute bouée à la liste des peers
- `sendCommand(buoyId, cmd)` - Envoie commande à une bouée
- `getBuoyState(buoyId)` - Récupère état d'une bouée
- `isBuoyConnected(buoyId)` - Vérifie si bouée répond
- `hasNewData()` - Nouvelles données reçues

**Configuration**:
- Canal WiFi: 1 (2.4GHz)
- Portée: 100-200m en champ libre
- Fréquence réception: 1Hz (bouées envoient état toutes les secondes)
- Timeout connexion: 5 secondes

---

### 4. **BuoyStateManager** 🎛️
**Fichier**: `BuoyStateManager.h/cpp`

**Responsabilité**: Gère état de toutes les bouées et sélection bouée active

**API Principales**:
- `selectNextBuoy()` - Sélectionne bouée suivante
- `selectPreviousBuoy()` - Sélectionne bouée précédente
- `getSelectedBuoyId()` - ID bouée sélectionnée
- `getSelectedBuoyState()` - État bouée sélectionnée
- `getConnectedBuoyCount()` - Nombre bouées connectées
- `isSelectedBuoyConnected()` - Vérifie connexion
- `getBuoyName(id)` - Nom bouée ("Buoy #1", "Buoy #2", etc.)
- `getModeName(mode)` - Nom mode ("CAP", "HOLD", "RETURN", etc.)

**Logique**:
- Mise à jour toutes les 100ms
- Sélection cyclique des bouées
- Indicateur visuel de connexion
- Cache des derniers états reçus

---

### 5. **DisplayManager** 🖥️
**Fichier**: `DisplayManager.h/cpp`

**Responsabilité**: Affichage LCD 128x128 pixels

**Écran Principal**:
```
┌────────────────────────┐
│ 🟢 Buoy #2         📶3 │ Header: Nom + Signal
├────────────────────────┤
│                        │
│   CAP MODE             │ Mode navigation
│   ↑ 045°   2.5 m/s    │ Cap + Vitesse
│                        │
│   🔋 85%   📡 LTE ✓   │ Batterie + LTE + GPS
│                        │
│   GPS: LOCKED          │
│                        │
└────────────────────────┘
```

**API Principales**:
- `begin()` - Initialise écran
- `update()` - Rafraîchit affichage (500ms)
- `displayMainScreen()` - Affiche écran principal
- `displayBuoySelection()` - Affiche sélection bouée
- `displayError(msg)` - Affiche erreur
- `setBrightness(0-255)` - Ajuste luminosité

**Couleurs**:
- Vert: Connexion OK, batterie >50%, mode normal
- Orange: Batterie 20-50%, signal faible
- Rouge: Déconnecté, batterie <20%, erreur
- Bleu: Mode spécial (RETURN HOME)

**Icônes**:
- 🟢 Bouée connectée
- 🔴 Bouée déconnectée
- 🔋 Batterie
- 📶 Signal (0-3 barres)
- 📡 LTE actif
- 🛰️ GPS locked

---

## 🔄 Flux de Données

### Loop Principal (10Hz = 100ms)

```cpp
void loop() {
    // 1. Lecture hardware
    joystickManager.update();
    
    // 2. Génération commandes
    commandManager.update(
        joystickManager.getAxisCentered(AXIS_LEFT_X),
        joystickManager.getAxisCentered(AXIS_LEFT_Y),
        joystickManager.getAxisCentered(AXIS_RIGHT_X),
        joystickManager.getAxisCentered(AXIS_RIGHT_Y),
        ...boutons...
    );
    
    // 3. Envoi commandes si nouvelle
    if (commandManager.hasNewCommand()) {
        Command cmd = commandManager.getCommand();
        espNow.sendCommand(buoyState.getSelectedBuoyId(), cmd);
    }
    
    // 4. Mise à jour état bouées
    buoyStateManager.update();
    
    // 5. Mise à jour affichage
    displayManager.update();
    
    delay(100);  // 10Hz
}
```

---

## 🎮 Scénarios d'Utilisation

### 1. Initialisation HOME et démarrage
```
Utilisateur: Presse BTN_RIGHT_STICK sur bouée #1
→ Joystick: Envoie CMD_INIT_HOME
→ Bouée #1: Enregistre position GPS actuelle comme HOME
→ Bouée #1: Passe en MODE_CAP avec cap actuel
→ Bouée #1: Envoie état "MODE: CAP, Heading: 045°"
→ Écran: Affiche "CAP MODE ↑045°"
```

### 2. Ajustement du cap
```
Utilisateur: Pousse joystick DROIT vers la DROITE
→ Joystick: Détecte rightX > 300
→ CommandManager: Incrémente cap de +10° (toutes les 200ms)
→ Joystick: Envoie CMD_SET_HEADING avec nouveau cap
→ Bouée: Ajuste cap cible
→ Écran: Affiche nouveau cap "↑055°"
```

### 3. Contrôle vitesse
```
Utilisateur: Pousse joystick GAUCHE vers l'AVANT
→ Joystick: Détecte leftY > 300
→ CommandManager: Calcule throttle = +75%
→ Joystick: Envoie CMD_SET_THROTTLE
→ Bouée: Ajuste vitesse des moteurs
→ Écran: Affiche vitesse "3.2 m/s"
```

### 4. Maintien position (HOLD)
```
Utilisateur: Presse BTN_LEFT_STICK
→ Joystick: Envoie CMD_HOLD_POSITION
→ Bouée: Enregistre position GPS actuelle
→ Bouée: Passe en MODE_HOLD
→ Bouée: Maintient position avec corrections GPS
→ Écran: Affiche "HOLD MODE"
```

### 5. Retour HOME
```
Utilisateur: Appui LONG sur BTN_M5 (>2 secondes)
→ Joystick: Envoie CMD_RETURN_HOME
→ Bouée: Passe en MODE_RETURN_HOME
→ Bouée: Calcule route vers HOME
→ Bouée: Navigue automatiquement vers HOME
→ Écran: Affiche "RETURN HOME" en bleu
```

### 6. Changement de bouée
```
Utilisateur: Presse BTN_RIGHT
→ BuoyStateManager: selectNextBuoy()
→ Affichage: Change "Buoy #1" → "Buoy #2"
→ Affichage: Montre nouvel état (cap, vitesse, batterie)
→ Commandes suivantes: Envoyées à Buoy #2
```

### 7. STOP d'urgence
```
Utilisateur: Appui COURT sur BTN_M5
→ Joystick: Envoie CMD_STOP
→ Bouée: Arrête tous les moteurs immédiatement
→ Bouée: Passe en MODE_STOPPED
→ Écran: Affiche "STOPPED" en rouge
```

---

## 📊 Configuration ESP-NOW

### Adresses MAC des Bouées (À CONFIGURER)

```cpp
// Dans main.cpp
const uint8_t BUOY1_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t BUOY2_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
const uint8_t BUOY3_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
const uint8_t BUOY4_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
const uint8_t BUOY5_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x05};

void setup() {
    espNow.begin();
    espNow.addBuoy(0, BUOY1_MAC);
    espNow.addBuoy(1, BUOY2_MAC);
    // etc...
}
```

### Obtenir l'adresse MAC d'une bouée:
```cpp
// Sur la bouée, exécuter:
WiFi.mode(WIFI_STA);
Serial.print("MAC Address: ");
Serial.println(WiFi.macAddress());
```

---

## 🔧 Configuration Hardware

### Pins I2C (AtomS3 ↔ STM32)
- **SDA**: GPIO 38
- **SCL**: GPIO 39
- **Fréquence**: 400kHz

### Bouton M5AtomS3
- **GPIO**: 41
- **Pull-up**: Activé
- **Logique**: Active LOW (0 = pressé)

### Écran LCD
- **Résolution**: 128x128 pixels
- **Interface**: SPI (géré par M5Unified)
- **Rafraîchissement**: 500ms

---

## 📈 Performances

| Métrique | Valeur |
|----------|--------|
| Fréquence loop principal | 10 Hz (100ms) |
| Fréquence mise à jour joystick | 10 Hz |
| Fréquence génération commandes | 10 Hz |
| Fréquence réception état bouées | 1 Hz (depuis bouée) |
| Fréquence mise à jour affichage | 2 Hz (500ms) |
| Latence commande → bouée | <50ms |
| Portée ESP-NOW | 100-200m |
| Autonomie batterie | ~3-4 heures (2x 300mAh) |

---

## 🚀 Étapes d'Implémentation

- [x] 1. Architecture définie
- [x] 2. Headers créés
- [ ] 3. Implémentation JoystickManager.cpp
- [ ] 4. Implémentation CommandManager.cpp
- [ ] 5. Implémentation ESPNowCommunication.cpp
- [ ] 6. Implémentation BuoyStateManager.cpp
- [ ] 7. Implémentation DisplayManager.cpp
- [ ] 8. Création main.cpp intégré
- [ ] 9. Tests unitaires
- [ ] 10. Tests en conditions réelles

---

**Date**: 26 octobre 2025  
**Version**: 1.0  
**Auteur**: Philippe Hubert
