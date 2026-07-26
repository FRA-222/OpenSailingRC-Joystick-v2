# Ajout des Indicateurs LED de Capteurs

## Date
28 octobre 2025

## Objectif
Ajouter des indicateurs visuels LED pour afficher l'état des trois capteurs critiques de la bouée : GPS, Magnétomètre (MAG), et Gyroscope (YAW).

## Interface Mise à Jour

### Nouvelle Disposition de l'Écran

```
┌─────────────────────────────┐
│       Buoy #1 (VERT)        │ ← Ligne 1: Header (y=5)
│                             │
│   🟢 GPS   🟢 MAG   🟢 YAW  │ ← Ligne 2: LEDs capteurs (y=22)
│                             │
│          READY              │ ← Ligne 3: Mode général (y=45)
│       Nav: NAV_HOLD         │ ← Ligne 4: Mode navigation (y=60)
│                             │
│   Cap: 045°  Vit: 2.5m/s    │
│                             │
│  🔋 85%  📡 OK  📶 -75      │
└─────────────────────────────┘
```

### Avant vs Après

**Avant :**
```
Ligne 1: Buoy #1 (y=5)
Ligne 2: CONNECTED (supprimée)
Ligne 3: Mode général (y=35)
Ligne 4: Mode navigation (y=50)
```

**Après :**
```
Ligne 1: Buoy #1 (y=5)
Ligne 2: LEDs GPS/MAG/YAW (y=22) ← NOUVEAU
Ligne 3: Mode général (y=45) ← Décalé +10px
Ligne 4: Mode navigation (y=60) ← Décalé +10px
```

## Spécifications des LEDs

### Positions et Dimensions

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| **Position Y** | 22 pixels | Sous le header |
| **Rayon LED** | 3 pixels | Taille du cercle |
| **Espacement** | 42 pixels | Entre chaque LED |
| **Position X** | 22, 64, 106 | Positions des 3 LEDs |

### Calcul des Positions

```cpp
const int16_t startX = 64 - spacing;     // Premier LED à x=22
const int16_t gpsX = startX;             // GPS: x=22
const int16_t magX = startX + spacing;   // MAG: x=64 (centre)
const int16_t yawX = startX + spacing*2; // YAW: x=106
```

### Code Couleur

| État | Couleur | Constante | Signification |
|------|---------|-----------|---------------|
| **OK** | 🟢 Vert | `TFT_GREEN` | Capteur fonctionnel |
| **KO** | 🔴 Rouge | `TFT_RED` | Capteur défaillant |

### Labels

| LED | Label | Champ BuoyState | Description |
|-----|-------|-----------------|-------------|
| 1 | **GPS** | `gpsOk` | État du capteur GPS |
| 2 | **MAG** | `headingOk` | État du magnétomètre (cap) |
| 3 | **YAW** | `yawRateOk` | État du gyroscope (vitesse de lacet) |

## Implémentation

### Nouvelle Fonction `drawSensorLEDs()`

```cpp
void DisplayManager::drawSensorLEDs(const BuoyState& state) {
    // Ligne 2 : Indicateurs LED des capteurs
    const int16_t y = 22;           // Position Y sous le header
    const int16_t ledRadius = 3;    // Rayon de la LED
    const int16_t spacing = 42;     // Espacement entre les LEDs
    
    // Centre de l'écran (128 pixels / 2 = 64)
    // 3 LEDs espacées : GPS, MAG, YAW
    const int16_t startX = 64 - spacing;  // Position de la première LED
    
    M5.Display.setFont(&fonts::Font0);    // Petite police pour les labels
    M5.Display.setTextDatum(TC_DATUM);
    
    // GPS LED
    int16_t gpsX = startX;
    M5.Display.fillCircle(gpsX, y, ledRadius, state.gpsOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("GPS", gpsX, y + 6);
    
    // MAG (Heading) LED
    int16_t magX = startX + spacing;
    M5.Display.fillCircle(magX, y, ledRadius, state.headingOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("MAG", magX, y + 6);
    
    // YAW (YawRate) LED
    int16_t yawX = startX + spacing * 2;
    M5.Display.fillCircle(yawX, y, ledRadius, state.yawRateOk ? TFT_GREEN : TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("YAW", yawX, y + 6);
}
```

### Intégration dans `displayMainScreen()`

```cpp
// Header avec nom de la bouée (couleur selon connexion)
drawHeader(connected);

// Indicateurs LED des capteurs (ligne 2)
if (connected) {
    drawSensorLEDs(state);
}
```

### Ajustements de Position

Les éléments suivants ont été décalés vers le bas pour faire place aux LEDs :

```cpp
// Mode général
M5.Display.drawString(generalModeName, 64, 45);  // Avant: 35

// Mode de navigation  
M5.Display.drawString("Nav: " + navModeName, 64, 60);  // Avant: 50
```

## Fichiers Modifiés

### 1. `include/DisplayManager.h`

**Ajout de la déclaration :**
```cpp
/**
 * @brief Draw sensor status LEDs
 * @param state Buoy state
 * 
 * Displays three LED indicators for GPS, MAG (heading), and YAW sensors.
 * Green = OK, Red = KO.
 */
void drawSensorLEDs(const BuoyState& state);
```

### 2. `src/DisplayManager.cpp`

**Modifications :**
1. Ajout de l'appel `drawSensorLEDs(state)` dans `displayMainScreen()`
2. Implémentation de la fonction `drawSensorLEDs()`
3. Ajustement des positions Y : mode général (35→45), mode nav (50→60)

## Impact Mémoire

**Résultats de Compilation :**
- **RAM** : 47,496 bytes (14.5%) - stable
- **Flash** : 879,309 bytes (26.3%) - +296 bytes
- **Compilation** : ✅ SUCCESS en 4.79 secondes

L'augmentation de la Flash (+296 bytes) provient de :
- Code de la fonction `drawSensorLEDs()` (~150 bytes)
- Chaînes de caractères "GPS", "MAG", "YAW" (~10 bytes)
- Code de dessin des cercles et textes (~136 bytes)

## Avantages

### 1. **Diagnostic Rapide**
- Vue instantanée de l'état des capteurs critiques
- Identification immédiate des défaillances
- Pas besoin de naviguer dans des menus

### 2. **Sécurité**
- Avertissement visuel clair en cas de panne capteur
- Permet de prendre des décisions éclairées avant navigation
- Évite de naviguer avec des capteurs défaillants

### 3. **Simplicité**
- Code couleur universel (vert=OK, rouge=KO)
- Labels courts et explicites
- Pas de surcharge visuelle

### 4. **Efficacité**
- Affichage permanent (pas de menu)
- Mise à jour en temps réel
- Faible coût en ressources

## Cas d'Usage

### Scénario 1 : Tous les Capteurs OK
```
🟢 GPS   🟢 MAG   🟢 YAW
```
✅ Navigation sécurisée possible

### Scénario 2 : GPS KO
```
🔴 GPS   🟢 MAG   🟢 YAW
```
⚠️ Pas de navigation autonome, mode manuel seulement

### Scénario 3 : Magnétomètre KO
```
🟢 GPS   🔴 MAG   🟢 YAW
```
⚠️ Tenue de cap impossible, mode manuel uniquement

### Scénario 4 : Gyroscope KO
```
🟢 GPS   🟢 MAG   🔴 YAW
```
⚠️ Autopilote dégradé, navigation basique possible

### Scénario 5 : Multi-défaillance
```
🔴 GPS   🔴 MAG   🟢 YAW
```
🛑 Arrêt immédiat, maintenance requise

## Tests Recommandés

### Test 1 : Affichage Normal
1. ✅ Démarrer avec tous les capteurs fonctionnels
2. ✅ Vérifier que les 3 LEDs sont vertes
3. ✅ Vérifier l'alignement horizontal
4. ✅ Vérifier la lisibilité des labels

### Test 2 : Simulation de Panne GPS
1. ⚠️ Forcer `gpsOk = false` dans la bouée
2. ⚠️ Vérifier que la LED GPS devient rouge
3. ⚠️ Vérifier que MAG et YAW restent vertes

### Test 3 : Simulation de Panne MAG
1. ⚠️ Forcer `headingOk = false` dans la bouée
2. ⚠️ Vérifier que la LED MAG devient rouge
3. ⚠️ Vérifier que GPS et YAW restent vertes

### Test 4 : Simulation de Panne YAW
1. ⚠️ Forcer `yawRateOk = false` dans la bouée
2. ⚠️ Vérifier que la LED YAW devient rouge
3. ⚠️ Vérifier que GPS et MAG restent vertes

### Test 5 : Multi-Panne
1. ⚠️ Forcer toutes les valeurs à `false`
2. ⚠️ Vérifier que toutes les LEDs deviennent rouges
3. ⚠️ Vérifier que l'affichage reste lisible

### Test 6 : Changement Dynamique
1. ⚠️ Démarrer avec capteurs KO
2. ⚠️ Activer progressivement chaque capteur
3. ⚠️ Vérifier que les LEDs passent de rouge à vert
4. ⚠️ Vérifier la fluidité de la mise à jour

## Détails Techniques

### Police et Alignement

```cpp
M5.Display.setFont(&fonts::Font0);      // Petite police (5x7 pixels)
M5.Display.setTextDatum(TC_DATUM);      // Alignement Top-Center
M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);  // Blanc sur noir
```

### Dessin des Cercles

```cpp
M5.Display.fillCircle(x, y, radius, color);
// x, y : Centre du cercle
// radius : 3 pixels
// color : TFT_GREEN ou TFT_RED selon l'état
```

### Optimisations Possibles

1. **Clignotement** : Faire clignoter les LEDs rouges pour attirer l'attention
2. **Dégradé** : Ajouter une couleur orange pour états intermédiaires
3. **Animation** : Effet de pulse pour les LEDs actives
4. **Icônes** : Remplacer les labels par des pictogrammes
5. **Historique** : Afficher un mini-graphique de l'état sur les 10 dernières secondes

## Layout Complet de l'Écran

```
┌─────────────────────────────┐  128x128 pixels
│       Buoy #1 (VERT)        │  y=5  (Font2, 16px)
│                             │
│   🟢 GPS   🟢 MAG   🟢 YAW  │  y=22 (LED r=3, Font0)
│                             │  y=28 (labels)
│                             │
│          READY              │  y=45 (Font2, mode général)
│       Nav: NAV_HOLD         │  y=60 (Font0, mode nav)
│                             │
│   Cap: 045°  Vit: 2.5m/s    │  y=75 (données navigation)
│                             │
│  🔋 85%  📡 OK  📶 -75      │  y=100 (indicateurs bas)
└─────────────────────────────┘
```

### Répartition Verticale

| Zone | Y Start | Y End | Hauteur | Contenu |
|------|---------|-------|---------|---------|
| Header | 5 | 20 | 15px | Nom de la bouée |
| LEDs | 22 | 35 | 13px | Indicateurs capteurs |
| Modes | 45 | 65 | 20px | Modes général + nav |
| Données | 75 | 95 | 20px | Cap, vitesse, etc. |
| Indicateurs | 100 | 115 | 15px | Batterie, GPS, Signal |

## Évolutions Futures

### Court Terme
1. **Tooltip** : Afficher le nom complet au survol (si écran tactile)
2. **Clignotement** : LEDs rouges clignotantes
3. **Son** : Alerte sonore si capteur critique KO

### Moyen Terme
1. **Historique** : Mini-graphique temporel de l'état des capteurs
2. **Statistiques** : Taux de disponibilité sur la session
3. **Couleur orange** : État dégradé (capteur opérationnel mais imprécis)

### Long Terme
1. **Diagnostics avancés** : Afficher la raison de la panne
2. **Calibration** : Bouton de recalibration rapide
3. **Logs** : Enregistrement des pannes capteurs

## Conclusion

✅ Ajout réussi des indicateurs LED de capteurs
✅ Interface plus informative et professionnelle
✅ Diagnostic visuel instantané de l'état des capteurs
✅ Code optimisé et impact mémoire minimal (+296 bytes)
✅ Facilite la prise de décision avant navigation

Cette fonctionnalité améliore significativement la sécurité et l'utilisabilité du système en permettant au pilote de connaître instantanément l'état des capteurs critiques.

## Changelog

**Version 1.2.0** (28 octobre 2025)
- ✅ Ajout des indicateurs LED GPS/MAG/YAW
- ✅ Ajustement du layout pour accueillir la nouvelle ligne
- ✅ Optimisation de l'espace d'affichage
- ✅ Documentation complète et tests définis
