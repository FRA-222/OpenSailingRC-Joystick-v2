# 🕹️ Commandes Avancées - Joystick Droit

**Date** : 29 octobre 2025  
**Statut** : ✅ Implémenté

## 🎯 Objectif

Implémenter 5 commandes sur le joystick DROIT pour le contrôle fin du throttle et du cap de la bouée avec des incréments/décréments relatifs.

## ✨ Commandes Implémentées

### 🎮 Mapping Joystick Droit

| Action Joystick | Commande | Paramètre | Incrément | Description |
|----------------|----------|-----------|-----------|-------------|
| **Vers le HAUT** ⬆️ | `CMD_SET_THROTTLE` | throttle | **+33%** | Augmente la puissance |
| **Vers le BAS** ⬇️ | `CMD_SET_THROTTLE` | throttle | **-33%** | Diminue la puissance |
| **Vers la DROITE** ➡️ | `CMD_SET_TRUE_HEADING` | heading | **+30°** | Tourne à droite |
| **Vers la GAUCHE** ⬅️ | `CMD_SET_TRUE_HEADING` | heading | **-30°** | Tourne à gauche |
| **Appui sur stick** 🔘 | `CMD_NAV_STOP` | - | - | Arrêt d'urgence |

## 🔧 Modifications Techniques

### 1. CommandManager.h

Ajout de 3 nouvelles méthodes publiques :

```cpp
/**
 * @brief Generate and send NAV_STOP command to a buoy
 */
bool generateNavStopCommand(uint8_t targetBuoyId);

/**
 * @brief Generate and send SET_THROTTLE command with increment
 * @param currentThrottle Current throttle from buoy state
 * @param increment Throttle increment (positive or negative)
 */
bool generateSetThrottleCommand(uint8_t targetBuoyId, int8_t currentThrottle, int8_t increment);

/**
 * @brief Generate and send SET_TRUE_HEADING command with increment
 * @param currentHeading Current heading from buoy state (degrees)
 * @param increment Heading increment in degrees
 */
bool generateSetHeadingCommand(uint8_t targetBuoyId, float currentHeading, int16_t increment);
```

### 2. CommandManager.cpp

#### Commande NAV_STOP

```cpp
bool CommandManager::generateNavStopCommand(uint8_t targetBuoyId) {
    Command navStopCmd;
    navStopCmd.targetBuoyId = targetBuoyId;
    navStopCmd.type = CMD_NAV_STOP;
    navStopCmd.heading = 0;
    navStopCmd.throttle = 0;
    navStopCmd.timestamp = millis();
    
    return espNowComm.sendCommand(targetBuoyId, navStopCmd);
}
```

#### Commande SET_THROTTLE (Incrémentale)

```cpp
bool CommandManager::generateSetThrottleCommand(uint8_t targetBuoyId, int8_t currentThrottle, int8_t increment) {
    // Calcul du nouveau throttle
    int16_t newThrottle = currentThrottle + increment;
    
    // Limitation entre -100 et +100
    if (newThrottle > 100) newThrottle = 100;
    if (newThrottle < -100) newThrottle = -100;
    
    Command throttleCmd;
    throttleCmd.type = CMD_SET_THROTTLE;
    throttleCmd.throttle = (int8_t)newThrottle;
    // ... envoi
}
```

**Caractéristiques** :
- ✅ Calcul relatif basé sur `forcedThrottleCmde` de la bouée
- ✅ Limitation automatique entre -100% et +100%
- ✅ Logs détaillés (valeur actuelle, incrément, nouvelle valeur)

#### Commande SET_TRUE_HEADING (Incrémentale)

```cpp
bool CommandManager::generateSetHeadingCommand(uint8_t targetBuoyId, float currentHeading, int16_t increment) {
    // Calcul du nouveau cap
    float newHeading = currentHeading + increment;
    
    // Normalisation entre 0 et 360 degrés
    while (newHeading >= 360.0f) newHeading -= 360.0f;
    while (newHeading < 0.0f) newHeading += 360.0f;
    
    Command headingCmd;
    headingCmd.type = CMD_SET_TRUE_HEADING;
    headingCmd.heading = (int16_t)newHeading;
    // ... envoi
}
```

**Caractéristiques** :
- ✅ Calcul relatif basé sur `forcedTrueHeadingCmde` de la bouée
- ✅ Normalisation automatique entre 0° et 360°
- ✅ Logs détaillés avec affichage en degrés

### 3. main.cpp

#### Récupération de l'État de la Bouée

```cpp
uint8_t selectedId = buoyState.getSelectedBuoyId();
BuoyState currentBuoyState = buoyState.getSelectedBuoyState();
```

Cela permet d'accéder aux valeurs actuelles :
- `currentBuoyState.forcedThrottleCmde` - Throttle actuel
- `currentBuoyState.forcedTrueHeadingCmde` - Cap actuel

#### Détection des Mouvements

```cpp
static bool rightUpProcessed = false;
static bool rightDownProcessed = false;
static bool rightRightProcessed = false;
static bool rightLeftProcessed = false;

int16_t rightY = joystick.getAxisCentered(AXIS_RIGHT_Y);
int16_t rightX = joystick.getAxisCentered(AXIS_RIGHT_X);

// HAUT : +33% throttle
if (rightY > JOYSTICK_THRESHOLD && !rightUpProcessed) {
    cmdManager.generateSetThrottleCommand(selectedId, currentBuoyState.forcedThrottleCmde, 33);
    rightUpProcessed = true;
}

// BAS : -33% throttle
if (rightY < -JOYSTICK_THRESHOLD && !rightDownProcessed) {
    cmdManager.generateSetThrottleCommand(selectedId, currentBuoyState.forcedThrottleCmde, -33);
    rightDownProcessed = true;
}

// DROITE : +30° heading
if (rightX > JOYSTICK_THRESHOLD && !rightRightProcessed) {
    cmdManager.generateSetHeadingCommand(selectedId, currentBuoyState.forcedTrueHeadingCmde, 30);
    rightRightProcessed = true;
}

// GAUCHE : -30° heading
if (rightX < -JOYSTICK_THRESHOLD && !rightLeftProcessed) {
    cmdManager.generateSetHeadingCommand(selectedId, currentBuoyState.forcedTrueHeadingCmde, -30);
    rightLeftProcessed = true;
}
```

#### Bouton NAV_STOP

```cpp
if (joystick.wasButtonPressed(BTN_RIGHT)) {
    cmdManager.generateNavStopCommand(selectedId);
}
```

## 🎮 Contrôles Complets des Deux Joysticks

### Vue d'Ensemble

```
┌──────────────────────┐        ┌──────────────────────┐
│   JOYSTICK GAUCHE    │        │   JOYSTICK DROIT     │
│                      │        │                      │
│   ⬆️ NAV_CAP         │        │   ⬆️ THROTTLE +33%   │
│   🔘 NAV_HOLD        │        │   ⬇️ THROTTLE -33%   │
│   ⬇️ NAV_HOME        │        │   ➡️ HEADING +30°    │
│                      │        │   ⬅️ HEADING -30°    │
│                      │        │   🔘 NAV_STOP        │
└──────────────────────┘        └──────────────────────┘
```

### Tableau Récapitulatif

| Joystick | Direction | Commande | Effet |
|----------|-----------|----------|-------|
| **GAUCHE** | ⬆️ Haut | NAV_CAP | Mode navigation par cap |
| **GAUCHE** | ⬇️ Bas | NAV_HOME | Retour au HOME |
| **GAUCHE** | 🔘 Bouton | NAV_HOLD | Maintien position |
| **DROIT** | ⬆️ Haut | SET_THROTTLE +33% | Accélère |
| **DROIT** | ⬇️ Bas | SET_THROTTLE -33% | Ralentit |
| **DROIT** | ➡️ Droite | SET_HEADING +30° | Tourne à droite |
| **DROIT** | ⬅️ Gauche | SET_HEADING -30° | Tourne à gauche |
| **DROIT** | 🔘 Bouton | NAV_STOP | Arrêt d'urgence |

## 📊 Logs Attendus

### SET_THROTTLE (+33%)

```
[JS-R] Joystick DROIT vers le HAUT - SET_THROTTLE +33%

[CommandManager] Generation commande SET_THROTTLE pour Bouee #0
   -> Throttle actuel: 0%
   -> Increment: +33%
   -> Nouveau throttle: 33%
   -> Commande SET_THROTTLE envoyee avec succes
```

### SET_THROTTLE (-33%)

```
[JS-R] Joystick DROIT vers le BAS - SET_THROTTLE -33%

[CommandManager] Generation commande SET_THROTTLE pour Bouee #0
   -> Throttle actuel: 33%
   -> Increment: -33%
   -> Nouveau throttle: 0%
   -> Commande SET_THROTTLE envoyee avec succes
```

### SET_TRUE_HEADING (+30°)

```
[JS-R] Joystick DROIT vers la DROITE - SET_HEADING +30°

[CommandManager] Generation commande SET_TRUE_HEADING pour Bouee #0
   -> Cap actuel: 45.0°
   -> Increment: +30°
   -> Nouveau cap: 75.0°
   -> Commande SET_TRUE_HEADING envoyee avec succes
```

### SET_TRUE_HEADING (-30°)

```
[JS-R] Joystick DROIT vers la GAUCHE - SET_HEADING -30°

[CommandManager] Generation commande SET_TRUE_HEADING pour Bouee #0
   -> Cap actuel: 75.0°
   -> Increment: -30°
   -> Nouveau cap: 45.0°
   -> Commande SET_TRUE_HEADING envoyee avec succes
```

### NAV_STOP (Bouton)

```
[JS-R] Bouton stick joystick DROIT presse - NAV_STOP

[CommandManager] Generation commande NAV_STOP pour Bouee #0
   -> Type: CMD_NAV_STOP
   -> Target Buoy: #0
   -> Commande NAV_STOP envoyee avec succes
```

## 🔍 Détails Techniques

### Incréments Choisis

| Paramètre | Incrément | Justification |
|-----------|-----------|---------------|
| **Throttle** | ±33% | 3 pressions pour passer de 0 à 100% |
| **Heading** | ±30° | 12 pressions pour faire un tour complet (360°) |

### Limitations Automatiques

#### Throttle
```cpp
if (newThrottle > 100) newThrottle = 100;    // Maximum +100%
if (newThrottle < -100) newThrottle = -100;  // Minimum -100%
```

#### Heading
```cpp
while (newHeading >= 360.0f) newHeading -= 360.0f;  // Boucle à 360°
while (newHeading < 0.0f) newHeading += 360.0f;     // Boucle à 0°
```

### Anti-rebond

4 flags distincts pour éviter les envois multiples :
- `rightUpProcessed` - HAUT
- `rightDownProcessed` - BAS
- `rightRightProcessed` - DROITE
- `rightLeftProcessed` - GAUCHE

Reset automatique quand le joystick revient vers le centre (THRESHOLD/2).

### Axes Utilisés

```cpp
AXIS_RIGHT_Y  // Axe vertical (HAUT/BAS pour throttle)
AXIS_RIGHT_X  // Axe horizontal (DROITE/GAUCHE pour heading)
```

## 🧪 Scénarios de Test

### Test 1 : Augmentation Progressive du Throttle

1. Bouée connectée avec throttle = 0%
2. Pousser joystick DROIT HAUT 3 fois
3. Observer les logs

**Résultat attendu** :
- ✅ 1ère pression : 0% → 33%
- ✅ 2ème pression : 33% → 66%
- ✅ 3ème pression : 66% → 99% (limité à 100%)

### Test 2 : Rotation par Incréments

1. Bouée avec heading = 0°
2. Pousser joystick DROIT DROITE 4 fois
3. Observer les logs

**Résultat attendu** :
- ✅ 1ère : 0° → 30°
- ✅ 2ème : 30° → 60°
- ✅ 3ème : 60° → 90°
- ✅ 4ème : 90° → 120°

### Test 3 : Boucle 360° du Heading

1. Bouée avec heading = 350°
2. Pousser joystick DROIT DROITE
3. Observer normalisation

**Résultat attendu** :
- ✅ 350° + 30° = 380° → normalisé à 20°

### Test 4 : Limitation Throttle

1. Bouée avec throttle = 90%
2. Pousser joystick DROIT HAUT
3. Observer limitation

**Résultat attendu** :
- ✅ 90% + 33% = 123% → limité à 100%

### Test 5 : Arrêt d'Urgence

1. Bouée en mouvement
2. Appuyer sur bouton joystick DROIT
3. Observer NAV_STOP

**Résultat attendu** :
- ✅ Commande NAV_STOP envoyée
- ✅ Bouée arrête tous mouvements

### Test 6 : Anti-rebond

1. Maintenir joystick DROIT HAUT pendant 3 secondes
2. Observer logs

**Résultat attendu** :
- ✅ Commande envoyée UNE SEULE fois
- ✅ Pas d'envois répétés

## 📈 Exemples d'Usage

### Navigation Fine

```
1. [Joystick GAUCHE ⬆️] → NAV_CAP (activer navigation)
2. [Joystick DROIT ⬆️] → Throttle +33% (démarrer)
3. [Joystick DROIT ⬆️] → Throttle +33% (accélérer à 66%)
4. [Joystick DROIT ➡️] → Heading +30° (corriger cap)
5. [Joystick GAUCHE 🔘] → NAV_HOLD (maintenir position)
```

### Contrôle Précis du Cap

```
Position initiale : 45°
[Joystick DROIT ➡️] → 75°
[Joystick DROIT ➡️] → 105°
[Joystick DROIT ⬅️] → 75° (correction)
```

### Arrêt Progressif

```
Throttle actuel : 66%
[Joystick DROIT ⬇️] → 33%
[Joystick DROIT ⬇️] → 0%
[Joystick DROIT 🔘] → NAV_STOP
```

## ⚡ Avantages

1. **Contrôle Relatif** : Incréments basés sur l'état actuel de la bouée
2. **Limitations Sûres** : Throttle et heading bornés automatiquement
3. **Logs Complets** : Traçabilité avant/après chaque modification
4. **Anti-rebond** : Pas d'envois multiples involontaires
5. **Ergonomique** : Joystick gauche = modes, Joystick droit = paramètres
6. **Intuitif** : Directions naturelles (haut = accélérer, droite = tourner à droite)

## 🔒 Sécurités

1. **Vérification connexion** avant chaque commande
2. **Récupération état bouée** pour calculs relatifs
3. **Limitations automatiques** (throttle ±100%, heading 0-360°)
4. **Arrêt d'urgence** accessible immédiatement (bouton joystick)
5. **Anti-rebond** pour éviter surcharges

## 🔄 Évolutions Possibles

- [ ] Configuration des incréments via menu
- [ ] Mode turbo (incréments doubles)
- [ ] Affichage en temps réel sur LCD
- [ ] Feedback haptique sur limitations
- [ ] Enregistrement de séquences
- [ ] Mode slow (incréments réduits)

---

**Version** : 2.0  
**Auteur** : Philippe Hubert  
**Statut** : ✅ Fonctionnel et prêt pour tests terrain
