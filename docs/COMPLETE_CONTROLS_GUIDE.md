# 🎮 Guide Complet des Contrôles - BuoyJoystick v2.0

**Date** : 29 octobre 2025  
**Statut** : ✅ À jour

## 🗺️ Vue d'Ensemble des Contrôles

```
┌─────────────────────────────────────────────────────────────────┐
│                    M5Stack Atom S3 + JoyStick                   │
│                                                                 │
│         Bouton Jaune                    Bouton Jaune           │
│           GAUCHE                           DROIT                │
│         [INIT_HOME]                  [HOME_VALIDATION]          │
│                                                                 │
│    ┌───────────────┐                  ┌───────────────┐        │
│    │   JOYSTICK    │                  │   JOYSTICK    │        │
│    │    GAUCHE     │                  │     DROIT     │        │
│    │               │                  │               │        │
│    │   ⬆️ NAV_CAP  │                  │  ⬆️ THROTTLE  │        │
│    │  🔘 NAV_HOLD  │                  │     +33%      │        │
│    │   ⬇️ NAV_HOME │                  │  ⬇️ THROTTLE  │        │
│    │               │                  │     -33%      │        │
│    │               │                  │ ➡️ HEADING+30°│        │
│    │               │                  │ ⬅️ HEADING-30°│        │
│    │               │                  │  🔘 NAV_STOP  │        │
│    └───────────────┘                  └───────────────┘        │
│                                                                 │
│                   [Écran LCD 128x128]                           │
│                 (Tactile: Sélection Bouée)                      │
└─────────────────────────────────────────────────────────────────┘
```

## 📋 Tableau Récapitulatif des Commandes

| Contrôle | Type | Action | Commande | Fonction |
|----------|------|--------|----------|----------|
| **🟡 Bouton Jaune Gauche** | Pression | Simple | `CMD_INIT_HOME` | Initialise position HOME |
| **🟡 Bouton Jaune Droit** | Pression | Simple | `CMD_HOME_VALIDATION` | Valide position HOME |
| **🕹️ Joystick Gauche** | Mouvement | ⬆️ Haut | `CMD_NAV_CAP` | Mode navigation par cap |
| **🕹️ Joystick Gauche** | Mouvement | ⬇️ Bas | `CMD_NAV_HOME` | Retour au HOME |
| **🕹️ Joystick Gauche** | Bouton | 🔘 Appui | `CMD_NAV_HOLD` | Maintien position |
| **🕹️ Joystick Droit** | Mouvement | ⬆️ Haut | `CMD_SET_THROTTLE` | Throttle +33% |
| **🕹️ Joystick Droit** | Mouvement | ⬇️ Bas | `CMD_SET_THROTTLE` | Throttle -33% |
| **🕹️ Joystick Droit** | Mouvement | ➡️ Droite | `CMD_SET_TRUE_HEADING` | Heading +30° |
| **🕹️ Joystick Droit** | Mouvement | ⬅️ Gauche | `CMD_SET_TRUE_HEADING` | Heading -30° |
| **🕹️ Joystick Droit** | Bouton | 🔘 Appui | `CMD_NAV_STOP` | Arrêt d'urgence |
| **📱 Écran Atom S3** | Tactile | Pression | - | Sélection bouée suivante |

## 🎯 Scénarios d'Utilisation

### Scénario 1 : Configuration Initiale d'une Bouée

```
1. [Écran Atom] → Sélectionner la bouée
2. Positionner physiquement la bouée au point HOME souhaité
3. [Bouton Jaune GAUCHE] → CMD_INIT_HOME
   └─ La bouée enregistre sa position GPS actuelle
4. [Bouton Jaune DROIT] → CMD_HOME_VALIDATION
   └─ La bouée valide et confirme le point HOME
```

**Modes** : INIT → HOME_DEFINITION → READY

### Scénario 2 : Navigation par Cap

```
1. [Joystick GAUCHE ⬆️] → CMD_NAV_CAP
   └─ La bouée entre en mode navigation par cap
2. [Joystick DROIT ⬆️] → CMD_SET_THROTTLE +33%
   └─ Augmente le throttle pour avancer
3. [Joystick DROIT ➡️] → CMD_SET_TRUE_HEADING +30°
   └─ Ajuste le cap vers la droite
```

**Mode** : NAV (NAV_CAP)

### Scénario 3 : Contrôle Fin de Navigation

```
1. [Joystick DROIT ⬆️] × 2 → Throttle à 66%
2. [Joystick DROIT ➡️] × 3 → Cap +90° (virage à droite)
3. [Joystick DROIT ⬇️] → Réduction throttle à 33%
4. [Joystick DROIT ⬅️] → Correction cap -30°
```

**Mode** : Contrôle manuel précis

### Scénario 3 : Retour au Point HOME

```
1. [Joystick GAUCHE ⬇️] → CMD_NAV_HOME
   └─ La bouée retourne automatiquement au point HOME défini
```

**Mode** : NAV (NAV_HOME)

### Scénario 4 : Arrêt d'Urgence

```
1. [Joystick DROIT 🔘] → CMD_NAV_STOP
   └─ La bouée arrête immédiatement tous les mouvements
```

**Mode** : NAV (NAV_STOP)

### Scénario 5 : Contrôle Multi-Bouées

```
1. [Écran Atom] → Sélection Bouée #0
2. [Joystick GAUCHE ⬆️] → CMD_NAV_CAP (envoyé à Bouée #0)
3. [Écran Atom] → Sélection Bouée #1
4. [Joystick GAUCHE ⬇️] → CMD_NAV_HOME (envoyé à Bouée #1)
```

**Capacité** : Jusqu'à 6 bouées simultanées

## 🎨 Codes Couleur de l'Affichage

| Couleur | État/Mode | Signification |
|---------|-----------|---------------|
| 🟢 **VERT** | Connecté | Bouée connectée, batterie > 50% |
| 🟠 **ORANGE** | Batterie faible | 20% < Batterie < 50% |
| 🔴 **ROUGE** | Déconnecté/Critique | Déconnecté ou batterie < 20% |
| 🔵 **BLEU** | NAV_HOME | Mode retour au HOME actif |
| 🟦 **CYAN** | Header/NAV_TARGET | En-tête et mode waypoint |
| 🟡 **JAUNE** | NAV_HOLD | Mode maintien de position |
| ⚪ **BLANC** | INIT/MAINTENANCE | États de configuration |

## 📡 Commandes Disponibles

### Commandes HOME

| Commande | Description | Paramètres | Mode Résultant |
|----------|-------------|------------|----------------|
| `CMD_INIT_HOME` | Initialise HOME avec position GPS actuelle | - | HOME_DEFINITION |
| `CMD_HOME_VALIDATION` | Valide la position HOME | - | READY ou NAV |

### Commandes Navigation

| Commande | Description | Paramètres | Mode Résultant |
|----------|-------------|------------|----------------|
| `CMD_NAV_CAP` | Navigation par cap | heading (futur) | NAV_CAP |
| `CMD_NAV_HOME` | Retour au point HOME | - | NAV_HOME |
| `CMD_NAV_HOLD` | Maintien de position | - | NAV_HOLD |
| `CMD_NAV_STOP` | Arrêt complet | - | NAV_STOP |
| `CMD_SET_THROTTLE` | Définir vitesse | throttle (±100%) | - |
| `CMD_SET_TRUE_HEADING` | Définir cap cible | heading (0-360°) | - |

### Commandes à Venir

| Commande | Description | Paramètres | Contrôle Prévu |
|----------|-------------|------------|----------------|
| ~~`CMD_SET_TRUE_HEADING`~~ | ~~Définir cap cible~~ | ~~heading~~ | ✅ **IMPLÉMENTÉ** |
| ~~`CMD_SET_THROTTLE`~~ | ~~Définir vitesse~~ | ~~throttle~~ | ✅ **IMPLÉMENTÉ** |

**Toutes les commandes prévues sont maintenant opérationnelles !** 🎉

## ⚙️ Paramètres Techniques

### Joysticks

```cpp
// Axes disponibles
AXIS_LEFT_X   // Axe horizontal gauche
AXIS_LEFT_Y   // Axe vertical gauche (utilisé)
AXIS_RIGHT_X  // Axe horizontal droit
AXIS_RIGHT_Y  // Axe vertical droit

// Plage des valeurs
Raw:      0 à 4095 (12-bit)
Centered: -2048 à +2047 (0 au centre)

// Seuil de détection mouvement
JOYSTICK_THRESHOLD = 1500  // ~73% de la course
```

### Boutons

```cpp
// Boutons disponibles
BTN_LEFT_STICK   (0)  // Bouton sur joystick gauche
BTN_RIGHT_STICK  (1)  // Bouton sur joystick droit
BTN_LEFT         (2)  // Bouton jaune gauche
BTN_RIGHT        (3)  // Bouton jaune droit
BTN_ATOM_SCREEN  (4)  // Écran tactile Atom S3
```

### Communication ESP-NOW

```cpp
// Fréquences
État bouées reçu:     1 Hz (toutes les secondes)
Nettoyage inactives:  30 secondes
Timeout bouée:        15 secondes

// Capacité
Max bouées:           6 simultanées
Portée:               ~100m en champ libre
```

## 🔒 Sécurités Implémentées

1. **Vérification de connexion** : Toutes les commandes vérifient que la bouée est connectée
2. **Anti-rebond** : Flags pour éviter les envois multiples
3. **Feedback visuel** : Codes couleur selon l'état
4. **Logs détaillés** : Traçabilité complète des actions
5. **Timeout automatique** : Bouées inactives retirées après 15s
6. **Validation HOME** : Processus en 2 étapes pour sécurité

## 📊 Logs en Temps Réel

### Exemple de Session

```
===========================================
  OpenSailingRC - Buoy Joystick v2.0
===========================================

[ESP-NOW] Nouvelle bouée détectée !
  MAC: 48:E7:29:9E:2B:AC
  Bouée ID: 0
✓ 1 bouée(s) active(s)

[L] Bouton jaune GAUCHE presse - Envoi CMD_INIT_HOME a Bouee #0
[CommandManager] Generation commande INIT_HOME pour Bouee #0
   -> Commande HOME envoyee avec succes

[R] Bouton jaune DROIT presse - Validation HOME
[CommandManager] Generation commande HOME_VALIDATION pour Bouee #0
   -> Commande HOME_VALIDATION envoyee avec succes

[JS-L] Joystick GAUCHE vers le HAUT - NAV_CAP
[CommandManager] Generation commande NAV_CAP pour Bouee #0
   -> Commande NAV_CAP envoyee avec succes

[JS-L] Bouton stick joystick GAUCHE presse - NAV_HOLD
[CommandManager] Generation commande NAV_HOLD pour Bouee #0
   -> Commande NAV_HOLD envoyee avec succes
```

## 🧪 Checklist de Test

- [ ] **Bouton Jaune Gauche** : CMD_INIT_HOME envoyée
- [ ] **Bouton Jaune Droit** : CMD_HOME_VALIDATION envoyée
- [ ] **Joystick Gauche HAUT** : CMD_NAV_CAP envoyée une seule fois
- [ ] **Joystick Gauche BAS** : CMD_NAV_HOME envoyée une seule fois
- [ ] **Bouton Joystick Gauche** : CMD_NAV_HOLD envoyée
- [ ] **Joystick Droit HAUT** : CMD_SET_THROTTLE +33% envoyée
- [ ] **Joystick Droit BAS** : CMD_SET_THROTTLE -33% envoyée
- [ ] **Joystick Droit DROITE** : CMD_SET_TRUE_HEADING +30° envoyée
- [ ] **Joystick Droit GAUCHE** : CMD_SET_TRUE_HEADING -30° envoyée
- [ ] **Bouton Joystick Droit** : CMD_NAV_STOP envoyée
- [ ] **Écran Atom** : Sélection bouée suivante fonctionne
- [ ] **Anti-rebond** : Pas d'envois multiples quand joystick maintenu
- [ ] **Sans bouée** : Messages "Aucune bouee connectee"
- [ ] **Multi-bouées** : Commandes envoyées à la bonne bouée
- [ ] **Limitation throttle** : Valeurs bornées entre -100% et +100%
- [ ] **Normalisation heading** : Valeurs bouclées entre 0° et 360°

## 📚 Documentation Détaillée

- **[HOME_VALIDATION_BUTTON.md](HOME_VALIDATION_BUTTON.md)** - Commande HOME_VALIDATION (Bouton Jaune Droit)
- **[LEFT_JOYSTICK_NAV_COMMANDS.md](LEFT_JOYSTICK_NAV_COMMANDS.md)** - Commandes navigation (Joystick Gauche)
- **[RIGHT_JOYSTICK_THROTTLE_HEADING.md](RIGHT_JOYSTICK_THROTTLE_HEADING.md)** - 🆕 Commandes throttle/heading (Joystick Droit)
- **[ARCHITECTURE.md](../ARCHITECTURE.md)** - Architecture complète du système
- **[API_DOCUMENTATION.md](../API_DOCUMENTATION.md)** - Documentation des APIs

## 🚀 Prochaines Étapes

### ✅ Complété
- [x] Joystick DROIT → Contrôle du throttle (±33%)
- [x] Joystick DROIT → Contrôle du heading (±30°)
- [x] Bouton Joystick DROIT → NAV_STOP

### En Développement
- [ ] Affichage en temps réel des valeurs throttle/heading sur LCD
- [ ] Configuration des incréments via menu
- [ ] Feedback visuel lors de l'envoi de commandes

### Futures Fonctionnalités
- [ ] Menu de configuration via écran tactile
- [ ] Feedback haptique
- [ ] Calibration joysticks
- [ ] Waypoints multiples
- [ ] Enregistrement de trajectoires

---

**Version** : 2.0  
**Date** : 29 octobre 2025  
**Auteur** : Philippe Hubert  
**Statut** : ✅ Système Complet et Opérationnel
