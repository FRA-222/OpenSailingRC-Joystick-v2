# 🔧 Correction Inversion Joysticks

**Date** : 29 octobre 2025  
**Statut** : ✅ Corrigé

## 🎯 Problème Identifié

Inversion entre HAUT et BAS pour les joysticks gauche et droit.

## 🔍 Cause

Les axes Y des joysticks utilisent la convention suivante :
- **Valeurs négatives** (`leftY < -THRESHOLD`) = Joystick poussé vers le **HAUT**
- **Valeurs positives** (`leftY > THRESHOLD`) = Joystick poussé vers le **BAS**

Le code initial avait inversé cette logique.

## ✅ Corrections Appliquées

### Joystick GAUCHE (Axe Y)

**AVANT** :
```cpp
// HAUT : leftY > JOYSTICK_THRESHOLD (INCORRECT)
if (leftY > JOYSTICK_THRESHOLD) {
    // NAV_CAP
}

// BAS : leftY < -JOYSTICK_THRESHOLD (INCORRECT)
if (leftY < -JOYSTICK_THRESHOLD) {
    // NAV_HOME
}
```

**APRÈS** :
```cpp
// HAUT : leftY < -JOYSTICK_THRESHOLD (CORRECT)
if (leftY < -JOYSTICK_THRESHOLD) {
    // NAV_CAP
}

// BAS : leftY > JOYSTICK_THRESHOLD (CORRECT)
if (leftY > JOYSTICK_THRESHOLD) {
    // NAV_HOME
}
```

### Joystick DROIT (Axe Y)

**AVANT** :
```cpp
// HAUT : rightY > JOYSTICK_THRESHOLD (INCORRECT)
if (rightY > JOYSTICK_THRESHOLD) {
    // THROTTLE +33%
}

// BAS : rightY < -JOYSTICK_THRESHOLD (INCORRECT)
if (rightY < -JOYSTICK_THRESHOLD) {
    // THROTTLE -33%
}
```

**APRÈS** :
```cpp
// HAUT : rightY < -JOYSTICK_THRESHOLD (CORRECT)
if (rightY < -JOYSTICK_THRESHOLD) {
    // THROTTLE +33%
}

// BAS : rightY > JOYSTICK_THRESHOLD (CORRECT)
if (rightY > JOYSTICK_THRESHOLD) {
    // THROTTLE -33%
}
```

## 📋 Comportement Corrigé

### Joystick GAUCHE

| Action Physique | Détection | Commande |
|----------------|-----------|----------|
| Pousser HAUT ⬆️ | `leftY < -1500` | CMD_NAV_CAP |
| Pousser BAS ⬇️ | `leftY > 1500` | CMD_NAV_HOME |

### Joystick DROIT

| Action Physique | Détection | Commande | Effet |
|----------------|-----------|----------|-------|
| Pousser HAUT ⬆️ | `rightY < -1500` | CMD_SET_THROTTLE | +33% |
| Pousser BAS ⬇️ | `rightY > 1500` | CMD_SET_THROTTLE | -33% |

**Note** : Les axes X (GAUCHE/DROITE) n'ont pas été modifiés car ils étaient déjà corrects.

## 🧪 Test de Validation

### Joystick GAUCHE
1. ⬆️ Pousser vers le HAUT → Doit afficher "NAV_CAP"
2. ⬇️ Pousser vers le BAS → Doit afficher "NAV_HOME"

### Joystick DROIT
1. ⬆️ Pousser vers le HAUT → Doit afficher "THROTTLE +33%"
2. ⬇️ Pousser vers le BAS → Doit afficher "THROTTLE -33%"

## 📝 Fichiers Modifiés

- `src/main.cpp` - Correction des conditions de détection pour les axes Y

## ✅ Résultat

Les mouvements physiques des joysticks correspondent maintenant exactement aux actions logiques attendues :
- HAUT = Actions positives (NAV_CAP, augmenter throttle)
- BAS = Actions négatives/retour (NAV_HOME, diminuer throttle)

---

**Version** : 2.0.1  
**Auteur** : Philippe Hubert  
**Statut** : ✅ Corrigé et testé
