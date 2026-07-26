# 🕹️ Commandes de Navigation - Joystick Gauche

**Date** : 29 octobre 2025  
**Statut** : ✅ Implémenté

## 🎯 Objectif

Implémenter 3 commandes de navigation sur le joystick GAUCHE pour un contrôle intuitif des modes de navigation de la bouée.

## ✨ Commandes Implémentées

### 🎮 Mapping Joystick Gauche

| Action Joystick | Commande | Mode Navigation | Description |
|----------------|----------|-----------------|-------------|
| **Vers le HAUT** ⬆️ | `CMD_NAV_CAP` | NAV_CAP | Mode navigation par cap |
| **Vers le BAS** ⬇️ | `CMD_NAV_HOME` | NAV_HOME | Retour au point HOME |
| **Appui sur stick** 🔘 | `CMD_NAV_HOLD` | NAV_HOLD | Maintien de position |

## 🔧 Modifications Techniques

### 1. CommandManager.h

Ajout de 3 nouvelles méthodes publiques :

```cpp
/**
 * @brief Generate and send NAV_CAP command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool generateNavCapCommand(uint8_t targetBuoyId);

/**
 * @brief Generate and send NAV_HOME command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool generateNavHomeCommand(uint8_t targetBuoyId);

/**
 * @brief Generate and send NAV_HOLD command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool generateNavHoldCommand(uint8_t targetBuoyId);
```

### 2. CommandManager.cpp

Implémentation des 3 méthodes suivant le même pattern :

```cpp
bool CommandManager::generateNavCapCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande NAV_CAP pour Bouee #%d", targetBuoyId);
    
    Command navCapCmd;
    navCapCmd.targetBuoyId = targetBuoyId;
    navCapCmd.type = CMD_NAV_CAP;
    navCapCmd.heading = 0;
    navCapCmd.throttle = 0;
    navCapCmd.timestamp = millis();
    
    currentCommand = navCapCmd;
    newCommandAvailable = true;
    
    bool success = espNowComm.sendCommand(targetBuoyId, navCapCmd);
    // ... logs et retour
}
```

### 3. main.cpp

#### Détection du bouton joystick gauche (BTN_LEFT)

```cpp
if (joystick.wasButtonPressed(BTN_LEFT)) {
    Logger::log("\n[JS-L] Bouton stick joystick GAUCHE presse - NAV_HOLD");
    uint8_t selectedId = buoyState.getSelectedBuoyId();
    if (buoyState.isSelectedBuoyConnected()) {
        cmdManager.generateNavHoldCommand(selectedId);
    }
}
```

#### Détection des mouvements du joystick (Axe Y)

```cpp
static const int16_t JOYSTICK_THRESHOLD = 1500;
static bool leftUpProcessed = false;
static bool leftDownProcessed = false;

int16_t leftY = joystick.getAxisCentered(AXIS_LEFT_Y);

// Joystick vers le HAUT
if (leftY > JOYSTICK_THRESHOLD && !leftUpProcessed) {
    cmdManager.generateNavCapCommand(selectedId);
    leftUpProcessed = true;
} else if (leftY < JOYSTICK_THRESHOLD / 2) {
    leftUpProcessed = false;  // Reset
}

// Joystick vers le BAS
if (leftY < -JOYSTICK_THRESHOLD && !leftDownProcessed) {
    cmdManager.generateNavHomeCommand(selectedId);
    leftDownProcessed = true;
} else if (leftY > -JOYSTICK_THRESHOLD / 2) {
    leftDownProcessed = false;  // Reset
}
```

## 🎮 Contrôles Complets du Système

### Vue d'Ensemble

| Contrôle | Action | Commande/Fonction |
|----------|--------|-------------------|
| **Bouton jaune GAUCHE** | Pression | CMD_INIT_HOME |
| **Bouton jaune DROIT** | Pression | CMD_HOME_VALIDATION |
| **Joystick GAUCHE ⬆️** | Vers haut | CMD_NAV_CAP |
| **Joystick GAUCHE ⬇️** | Vers bas | CMD_NAV_HOME |
| **Joystick GAUCHE 🔘** | Appui | CMD_NAV_HOLD |
| **Écran Atom** | Pression | Sélection bouée suivante |

### Workflow de Navigation Typique

```
1. [Bouton jaune GAUCHE] → INIT_HOME (définir position HOME)
2. [Bouton jaune DROIT] → HOME_VALIDATION (valider HOME)
3. [Joystick GAUCHE ⬆️] → NAV_CAP (navigation par cap)
   ou
   [Joystick GAUCHE ⬇️] → NAV_HOME (retour au HOME)
4. [Joystick GAUCHE 🔘] → NAV_HOLD (arrêt et maintien position)
```

## 📊 Logs Attendus

### Commande NAV_CAP (Joystick vers le HAUT)

```
[JS-L] Joystick GAUCHE vers le HAUT - NAV_CAP

[CommandManager] Generation commande NAV_CAP pour Bouee #0
   -> Type: CMD_NAV_CAP
   -> Target Buoy: #0
   -> Commande NAV_CAP envoyee avec succes
```

### Commande NAV_HOME (Joystick vers le BAS)

```
[JS-L] Joystick GAUCHE vers le BAS - NAV_HOME

[CommandManager] Generation commande NAV_HOME pour Bouee #0
   -> Type: CMD_NAV_HOME
   -> Target Buoy: #0
   -> Commande NAV_HOME envoyee avec succes
```

### Commande NAV_HOLD (Appui bouton joystick)

```
[JS-L] Bouton stick joystick GAUCHE presse - NAV_HOLD

[CommandManager] Generation commande NAV_HOLD pour Bouee #0
   -> Type: CMD_NAV_HOLD
   -> Target Buoy: #0
   -> Commande NAV_HOLD envoyee avec succes
```

### Sans bouée connectée

```
[JS-L] Joystick GAUCHE vers le HAUT - NAV_CAP
   -> Aucune bouee connectee
```

## 🔍 Détails Techniques

### Seuil de Détection

```cpp
static const int16_t JOYSTICK_THRESHOLD = 1500;
```

- **Valeur centrée** : Le joystick renvoie des valeurs centrées autour de 0 (via `getAxisCentered()`)
- **Plage** : Environ -2048 à +2047
- **Seuil choisi** : 1500 (environ 73% de la course)
- **Hysteresis** : THRESHOLD/2 pour le reset (évite les oscillations)

### Anti-rebond (Debouncing)

```cpp
static bool leftUpProcessed = false;
static bool leftDownProcessed = false;
```

- **Flags statiques** : Évitent l'envoi multiple de la même commande
- **Reset automatique** : Quand le joystick revient vers le centre
- **Hysteresis** : Zone de reset à THRESHOLD/2 pour éviter les rebonds

### Axe Utilisé

```cpp
int16_t leftY = joystick.getAxisCentered(AXIS_LEFT_Y);
```

- **AXIS_LEFT_Y** : Axe vertical du joystick gauche
- **Valeurs** : Positif = HAUT, Négatif = BAS
- **Centré** : 0 au repos

## 🧪 Tests

### Test 1 : NAV_CAP (Haut)

1. Connecter une bouée
2. Pousser le joystick GAUCHE vers le HAUT
3. Maintenir quelques secondes
4. Relâcher

**Résultat attendu** :
- ✅ Log "Joystick GAUCHE vers le HAUT - NAV_CAP"
- ✅ Commande envoyée une seule fois
- ✅ Bouée passe en mode NAV_CAP
- ✅ Pas d'envois multiples même si maintenu

### Test 2 : NAV_HOME (Bas)

1. Avec bouée connectée
2. Pousser le joystick GAUCHE vers le BAS
3. Observer les logs

**Résultat attendu** :
- ✅ Log "Joystick GAUCHE vers le BAS - NAV_HOME"
- ✅ Bouée retourne au point HOME

### Test 3 : NAV_HOLD (Bouton)

1. Avec bouée connectée
2. Appuyer sur le bouton du joystick GAUCHE
3. Observer les logs

**Résultat attendu** :
- ✅ Log "Bouton stick joystick GAUCHE presse - NAV_HOLD"
- ✅ Bouée maintient sa position

### Test 4 : Anti-rebond

1. Pousser joystick GAUCHE vers le HAUT
2. Maintenir 3 secondes
3. Observer logs

**Résultat attendu** :
- ✅ Commande envoyée UNE SEULE fois au début
- ✅ Pas d'envois répétés pendant le maintien

### Test 5 : Reset du flag

1. Pousser joystick HAUT (commande NAV_CAP envoyée)
2. Relâcher au centre
3. Re-pousser vers le HAUT

**Résultat attendu** :
- ✅ Première commande envoyée
- ✅ Deuxième commande envoyée (flag resetté)

### Test 6 : Sans bouée

1. Aucune bouée connectée
2. Tester les 3 actions du joystick gauche

**Résultat attendu** :
- ✅ Logs "Aucune bouee connectee"
- ✅ Aucune commande envoyée

## ⚡ Avantages

1. **Intuitif** : Mouvements naturels du joystick
2. **Ergonomique** : Commandes de navigation sur le même contrôle
3. **Anti-rebond** : Pas d'envois multiples involontaires
4. **Sécurité** : Vérification de connexion avant envoi
5. **Feedback** : Logs détaillés pour débogage
6. **Cohérent** : Architecture similaire aux autres commandes

## 📝 Notes Importantes

### Convention de Direction

- **Positif (HAUT)** : Navigation active (NAV_CAP)
- **Négatif (BAS)** : Retour sécurisé (NAV_HOME)
- **Centre (Bouton)** : Arrêt d'urgence (NAV_HOLD)

Cette convention rend l'usage intuitif et sécurisé.

### Calibration du Seuil

Si le joystick est trop sensible ou pas assez :

```cpp
// Ajuster cette valeur dans main.cpp
static const int16_t JOYSTICK_THRESHOLD = 1500;  // 1000-2000 recommandé
```

### Modes de Navigation

Ces commandes changent le **mode de navigation** de la bouée :
- **NAV_CAP** : Suit un cap défini
- **NAV_HOME** : Retour automatique au point HOME
- **NAV_HOLD** : Maintien de position GPS actuelle

## 🔄 Évolutions Possibles

- [ ] Ajout de LED de feedback sur l'écran Atom
- [ ] Vibration haptique lors de l'envoi de commande
- [ ] Configuration du seuil via menu
- [ ] Joystick DROIT pour contrôle du cap
- [ ] Combinaison de commandes (ex: Haut + Bouton)

---

**Version** : 2.0  
**Auteur** : Philippe Hubert  
**Statut** : ✅ Fonctionnel et prêt pour tests
