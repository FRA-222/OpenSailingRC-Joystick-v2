# 🖱️ Support du Bouton/Écran Tactile de l'Atom S3

## Vue d'Ensemble

Le M5Stack Atom S3 possède un **écran LCD qui fait également office de bouton tactile**. Cette fonctionnalité a été intégrée au `JoystickManager` pour permettre une interaction utilisateur supplémentaire.

## ✨ Fonctionnalités Ajoutées

### 🎯 Nouveau Bouton : `BTN_ATOM_SCREEN`

Le 5ème bouton est maintenant disponible dans le système :

```cpp
#define BTN_LEFT_STICK  0    // Bouton sur joystick gauche
#define BTN_RIGHT_STICK 1    // Bouton sur joystick droit
#define BTN_LEFT        2    // Bouton jaune gauche
#define BTN_RIGHT       3    // Bouton jaune droit
#define BTN_ATOM_SCREEN 4    // 🆕 Bouton écran Atom S3
```

### 📋 Nouvelles Méthodes

#### Méthodes Générales (tous boutons)
- `isButtonPressed(button)` - Étendu pour supporter le 5ème bouton
- `wasButtonPressed(button)` - Étendu pour supporter le 5ème bouton
- `wasButtonReleased(button)` - 🆕 Nouvelle méthode pour détecter le relâchement

#### Méthodes Spécifiques au Bouton Atom
```cpp
// Vérifier si le bouton est actuellement pressé
bool isAtomScreenPressed();

// Détecter une pression (front montant)
bool wasAtomScreenPressed();

// Détecter un relâchement (front descendant)
bool wasAtomScreenReleased();

// Détecter un maintien prolongé (avec durée configurable)
bool isAtomScreenHeld(uint32_t durationMs = 1000);
```

## 🔧 Implémentation Technique

### Structure Interne

```cpp
class JoystickManager {
private:
    bool buttonState[5];         // États actuels (5 boutons)
    bool buttonPrevState[5];     // États précédents (5 boutons)
    uint32_t atomScreenPressTime; // Timestamp de pression
};
```

### Intégration avec M5Unified

Le bouton Atom est géré via la bibliothèque M5Unified :

```cpp
void JoystickManager::update() {
    // ... lecture des joysticks I2C ...
    
    // Met à jour le bouton de l'écran Atom S3
    M5.update();  // ⚠️ Important : met à jour l'état M5
    buttonPrevState[BTN_ATOM_SCREEN] = buttonState[BTN_ATOM_SCREEN];
    buttonState[BTN_ATOM_SCREEN] = M5.BtnA.isPressed();
    
    // Gère le timestamp pour la détection de maintien
    if (buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = millis();
    } else if (!buttonState[BTN_ATOM_SCREEN]) {
        atomScreenPressTime = 0;
    }
}
```

## 📝 Exemples d'Utilisation

### 1. Détection Simple (Pression)

```cpp
if (joystick.wasAtomScreenPressed()) {
    Serial.println("Bouton écran pressé !");
    // Action : Toggle affichage, ouvrir menu, etc.
}
```

### 2. Détection de Relâchement

```cpp
if (joystick.wasAtomScreenReleased()) {
    Serial.println("Bouton écran relâché !");
    // Action : Confirmer sélection, fermer menu, etc.
}
```

### 3. Maintien du Bouton (Long Press)

```cpp
// Détection de maintien pendant 2 secondes
if (joystick.isAtomScreenHeld(2000)) {
    static bool longPressHandled = false;
    if (!longPressHandled) {
        Serial.println("Bouton maintenu 2 secondes !");
        longPressHandled = true;
        // Action : Reset, calibration, mode expert, etc.
    }
    
    // Reset le flag quand relâché
    if (joystick.wasAtomScreenReleased()) {
        longPressHandled = false;
    }
}
```

### 4. État Continu

```cpp
if (joystick.isAtomScreenPressed()) {
    Serial.println("Bouton maintenu...");
    // Action continue tant que pressé
}
```

### 5. Utilisation avec les Boutons Génériques

```cpp
// Fonctionne avec toutes les méthodes génériques
if (joystick.isButtonPressed(BTN_ATOM_SCREEN)) {
    // ...
}

if (joystick.wasButtonPressed(BTN_ATOM_SCREEN)) {
    // ...
}

if (joystick.wasButtonReleased(BTN_ATOM_SCREEN)) {
    // ...
}
```

## 🎮 Implémentation dans main.cpp

Le code de démonstration a été ajouté dans la boucle principale :

```cpp
void loop() {
    // ... mise à jour joystick ...
    
    // Lecture du bouton écran Atom S3
    if (joystick.wasAtomScreenPressed()) {
        USBSerial.println("\n[ATOM] Bouton ecran Atom S3 presse");
        // TODO: Ajouter fonctionnalité (ex: toggle display, menu)
    }
    
    // Détection de maintien du bouton écran (2 secondes)
    if (joystick.isAtomScreenHeld(2000)) {
        static bool longPressHandled = false;
        if (!longPressHandled) {
            USBSerial.println("\n[ATOM] Bouton ecran MAINTENU (2s)");
            longPressHandled = true;
            // TODO: Ajouter fonctionnalité (ex: reset, calibration)
        }
        if (joystick.wasAtomScreenReleased()) {
            longPressHandled = false;
        }
    }
}
```

## 💡 Cas d'Usage Suggérés

### Pression Simple
- **Toggle affichage** : Activer/désactiver l'écran
- **Navigation menu** : Sélectionner/confirmer
- **Changer de bouée** : Alternative aux boutons jaunes
- **Info rapide** : Afficher temporairement des infos détaillées

### Pression Longue (2-3s)
- **Reset système** : Réinitialiser la configuration
- **Calibration** : Lancer une procédure de calibration
- **Mode expert** : Activer des fonctionnalités avancées
- **Factory reset** : Retour aux paramètres d'usine

### Double Clic (à implémenter si besoin)
- **Screenshot** : Capturer l'écran
- **Changement de mode** : Basculer entre modes rapides
- **Verrouillage** : Verrouiller/déverrouiller l'interface

## 🔍 Détails Techniques

### Détection des Fronts

```cpp
// Front montant (pressed)
return buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN];

// Front descendant (released)
return !buttonState[BTN_ATOM_SCREEN] && buttonPrevState[BTN_ATOM_SCREEN];
```

### Gestion du Timestamp

```cpp
// Démarre le chrono lors de la pression
if (buttonState[BTN_ATOM_SCREEN] && !buttonPrevState[BTN_ATOM_SCREEN]) {
    atomScreenPressTime = millis();
}

// Réinitialise quand relâché
else if (!buttonState[BTN_ATOM_SCREEN]) {
    atomScreenPressTime = 0;
}

// Vérifie la durée
bool isHeld = (millis() - atomScreenPressTime) >= durationMs;
```

### Fréquence de Mise à Jour

Le bouton est mis à jour à chaque appel de `joystick.update()`, typiquement **10 fois par seconde** (100ms) dans la boucle principale.

## ⚠️ Notes Importantes

### 1. M5.update() est Automatique
Le `JoystickManager` appelle automatiquement `M5.update()` - **ne pas l'appeler ailleurs** pour éviter les conflits.

### 2. Debouncing
Le debouncing est géré par la bibliothèque M5Unified, pas besoin d'implémentation supplémentaire.

### 3. Compatibilité
Le code fonctionne uniquement sur **M5Stack Atom S3**. Sur d'autres modèles, `M5.BtnA` peut ne pas exister.

### 4. Performance
L'ajout du bouton Atom a un impact minimal :
- **+8 octets RAM** (atomScreenPressTime)
- **+364 octets Flash** (nouvelles fonctions)

## 📊 Taille Mémoire

### Avant (4 boutons)
```
RAM:   14.4% (47220 bytes)
Flash: 26.1% (873089 bytes)
```

### Après (5 boutons + fonctions Atom)
```
RAM:   14.4% (47228 bytes)  +8 bytes
Flash: 26.1% (873453 bytes) +364 bytes
```

## ✅ Tests de Compilation

```
✅ SUCCESS: Took 5.18 seconds
Platform: ESP32S3 @ 240MHz
RAM:   14.4% (47228 bytes from 327680 bytes)
Flash: 26.1% (873453 bytes from 3342336 bytes)
```

## 🚀 Prochaines Étapes

### Fonctionnalités à Implémenter

1. **Toggle Display** (pression simple)
   ```cpp
   if (joystick.wasAtomScreenPressed()) {
       display.toggle();
   }
   ```

2. **Menu de Configuration** (maintien 1s)
   ```cpp
   if (joystick.isAtomScreenHeld(1000)) {
       displayConfigMenu();
   }
   ```

3. **Reset Système** (maintien 5s)
   ```cpp
   if (joystick.isAtomScreenHeld(5000)) {
       ESP.restart();
   }
   ```

4. **Changement de Luminosité** (pression répétée)
   ```cpp
   if (joystick.wasAtomScreenPressed()) {
       cycleBrightness(); // 25% → 50% → 75% → 100% → 25%
   }
   ```

## 📁 Fichiers Modifiés

- ✅ `include/JoystickManager.h` - Nouvelles déclarations
- ✅ `src/JoystickManager.cpp` - Implémentation
- ✅ `src/main.cpp` - Exemple d'utilisation

---

**Date d'ajout** : 28 octobre 2025  
**Version** : v1.3 - Support Bouton Atom S3  
**Statut** : ✅ Implémenté, compilé et prêt à tester  
**Compatibilité** : M5Stack Atom S3 uniquement  
**Impact** : Minimal (+8 bytes RAM, +364 bytes Flash)
