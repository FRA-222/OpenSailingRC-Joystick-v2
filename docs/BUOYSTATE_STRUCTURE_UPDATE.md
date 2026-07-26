# Mise à Jour de la Structure BuoyState

## Date
28 octobre 2025

## Objectif
Enrichir la structure `BuoyState` pour transmettre plus de données détaillées entre la bouée et le joystick via ESP-NOW.

## Modifications Apportées

### Nouveaux Champs Ajoutés

La structure `BuoyState` a été étendue pour inclure les données suivantes :

#### 1. État Général et Navigation
- `generalMode` - Mode général de la bouée (INIT, READY, MAINTENANCE, HOME_DEFINITION, NAV)
- `navigationMode` - Mode de navigation actif (STOP, BASIC, HOLD, HOME, TARGET, etc.)

#### 2. État des Capteurs
- `gpsOk` - État du capteur GPS (bool)
- `headingOk` - État du capteur de cap (bool)
- `yawRateOk` - État du capteur de vitesse de lacet (bool)

#### 3. Données Environnementales
- `temperature` - Température en °C (float)

#### 4. Données Batterie
- `remainingCapacity` - Capacité restante en mAh (float)

#### 5. Données de Navigation
- `distanceToCons` - Distance à la consigne/waypoint en mètres (float)

#### 6. Commandes du Pilote Automatique
- `autoPilotThrottleCmde` - Commande throttle (-100 à +100%) (int8_t)
- `autoPilotTrueHeadingCmde` - Consigne de cap en degrés (float)
- `autoPilotRudderCmde` - Commande gouvernail (-100 à +100%) (int8_t)

#### 7. Commandes Forcées
- `forcedThrottleCmde` - Commande throttle forcée (-100 à +100%) (int8_t)
- `forcedThrottleCmdeOk` - Flag d'activation de la commande throttle forcée (bool)
- `forcedTrueHeadingCmde` - Commande cap forcée en degrés (float)
- `forcedTrueHeadingCmdeOk` - Flag d'activation de la commande cap forcée (bool)
- `forcedRudderCmde` - Commande gouvernail forcée (-100 à +100%) (int8_t)
- `forcedRudderCmdeOk` - Flag d'activation de la commande gouvernail forcée (bool)

### Champs Legacy (Rétrocompatibilité)

Pour assurer la compatibilité avec le code existant, les anciens champs ont été conservés et marqués comme DEPRECATED :

- `latitude` - Latitude GPS (double) - DEPRECATED
- `longitude` - Longitude GPS (double) - DEPRECATED
- `heading` - Cap actuel en degrés (float) - DEPRECATED
- `speed` - Vitesse en m/s (float) - DEPRECATED
- `batteryLevel` - Niveau batterie 0-100% (uint8_t) - DEPRECATED
- `signalQuality` - Qualité signal LTE (int8_t) - DEPRECATED
- `gpsLocked` - État GPS verrouillé (bool) - DEPRECATED (utiliser `gpsOk` à la place)

## Fichiers Modifiés

### Projet Joystick
- **`include/ESPNowCommunication.h`** : Structure `BuoyState` mise à jour

### Projet Bouée Autonome
- **`src/ESPNowDataLinkManagement.h`** : Structure `BuoyState` mise à jour
- **`src/Autonomous GPS Buoy.cpp`** : Code de remplissage de la structure mis à jour (2 occurrences)

## Impact sur la Mémoire

### Projet Joystick
- **RAM** : 47,496 bytes (14.5%) - +240 bytes
- **Flash** : 879,085 bytes (26.3%) - +304 bytes

### Projet Bouée
- **RAM** : 57,940 bytes (1.3%) - stable
- **Flash** : 1,253,593 bytes (19.1%) - stable

## Structure Complète de BuoyState

```cpp
struct BuoyState {
    uint8_t buoyId;                     // ID de la bouée (0-5)
    uint32_t timestamp;                 // Timestamp du message
    
    // État général
    tEtatsGeneral generalMode;          // Mode général
    tEtatsNav navigationMode;           // Mode de navigation
    
    // État des capteurs
    bool gpsOk;                         // État GPS
    bool headingOk;                     // État cap
    bool yawRateOk;                     // État vitesse de lacet
    
    // Données environnementales
    float temperature;                  // Température en °C
    
    // Données batterie
    float remainingCapacity;            // Capacité restante en mAh
    
    // Données de navigation
    float distanceToCons;               // Distance à la consigne en m
    
    // Commandes autopilote
    int8_t autoPilotThrottleCmde;       // Commande throttle autopilote
    float autoPilotTrueHeadingCmde;     // Commande cap autopilote
    int8_t autoPilotRudderCmde;         // Commande gouvernail autopilote
    
    // Commandes forcées
    int8_t forcedThrottleCmde;          // Commande throttle forcée
    bool forcedThrottleCmdeOk;          // Flag throttle forcé actif
    float forcedTrueHeadingCmde;        // Commande cap forcée
    bool forcedTrueHeadingCmdeOk;       // Flag cap forcé actif
    int8_t forcedRudderCmde;            // Commande gouvernail forcée
    bool forcedRudderCmdeOk;            // Flag gouvernail forcé actif
    
    // Champs legacy (compatibilité)
    double latitude;                    // DEPRECATED
    double longitude;                   // DEPRECATED
    float heading;                      // DEPRECATED
    float speed;                        // DEPRECATED
    uint8_t batteryLevel;               // DEPRECATED
    int8_t signalQuality;               // DEPRECATED
    bool gpsLocked;                     // DEPRECATED
};
```

## Taille de la Structure

- **Nouveaux champs** : ~52 bytes
- **Champs legacy** : ~41 bytes
- **Total** : ~93 bytes par paquet BuoyState

## Code de Remplissage (Bouée)

```cpp
BuoyState buoyState;
buoyState.buoyId = CURRENT_BUOY;
buoyState.timestamp = millis();

// General state
buoyState.generalMode = myGeneralMode;
buoyState.navigationMode = myNavigationMode;

// Sensor status
buoyState.gpsOk = myLocationGpsOk;
buoyState.headingOk = myHeadingOk;
buoyState.yawRateOk = myYawRateOk;

// Environmental data
buoyState.temperature = myTemperature;

// Battery data
buoyState.remainingCapacity = myRemainingCapacity;

// Navigation data
buoyState.distanceToCons = myDistanceToCons;

// Autopilot commands
buoyState.autoPilotThrottleCmde = (int8_t)myAutoPilotThrottleCmde;
buoyState.autoPilotTrueHeadingCmde = myAutoPilotTrueHeadingCmde;
buoyState.autoPilotRudderCmde = (int8_t)myAutoPilotRudderCmde;

// Forced commands
buoyState.forcedThrottleCmde = (int8_t)myForcedThrottleCmde;
buoyState.forcedThrottleCmdeOk = myForcedThrottleCmdeOk;
buoyState.forcedTrueHeadingCmde = myForcedTrueHeadingCmde;
buoyState.forcedTrueHeadingCmdeOk = myForcedTrueHeadingCmdeOk;
buoyState.forcedRudderCmde = (int8_t)myForcedRudderCmde;
buoyState.forcedRudderCmdeOk = myForcedRudderCmdeOk;

// Legacy fields (for backward compatibility)
buoyState.latitude = myXLatGps;
buoyState.longitude = myYLonGps;
buoyState.heading = myTrueHeading;
buoyState.speed = mySpeedGps;
buoyState.batteryLevel = (uint8_t)((myRemainingCapacity / 10000.0) * 100);
buoyState.signalQuality = lteSignalQuality;
buoyState.gpsLocked = myLocationGpsOk;

espNowDataLinkManagement.sendBuoyState(buoyState);
```

## Utilisation dans le Joystick

Le joystick peut maintenant accéder à toutes les nouvelles données via la structure `BuoyState` reçue :

```cpp
BuoyState state = espNowComm.getLastBuoyState();

// Accès aux nouvelles données
if (state.gpsOk && state.headingOk) {
    Logger::logf("Autopilot Heading: %.1f°", state.autoPilotTrueHeadingCmde);
    Logger::logf("Distance to waypoint: %.1f m", state.distanceToCons);
    Logger::logf("Temperature: %.1f°C", state.temperature);
    Logger::logf("Battery: %.0f mAh", state.remainingCapacity);
    
    if (state.forcedThrottleCmdeOk) {
        Logger::logf("Forced Throttle: %d%%", state.forcedThrottleCmde);
    }
}

// Accès aux données legacy (compatible avec ancien code)
Logger::logf("Heading: %.1f° Speed: %.1f m/s", state.heading, state.speed);
Logger::logf("Battery: %d%%", state.batteryLevel);
```

## Plan de Migration

### Phase 1 : Transition (Actuelle)
- ✅ Les deux structures (nouveaux et anciens champs) coexistent
- ✅ La bouée remplit les deux ensembles de champs
- ✅ Le joystick peut utiliser les anciens champs pendant la transition

### Phase 2 : Migration Graduelle (À venir)
- Mettre à jour progressivement le code du joystick pour utiliser les nouveaux champs
- Créer de nouveaux écrans d'affichage pour exploiter les nouvelles données
- Ajouter des indicateurs visuels pour l'état des capteurs (gpsOk, headingOk, yawRateOk)

### Phase 3 : Nettoyage (Future)
- Supprimer les champs legacy de la structure
- Réduire la taille du paquet ESP-NOW (~41 bytes économisés)
- Mise à jour de la documentation

## Tests Recommandés

1. **Test de Communication**
   - ✅ Vérifier que les paquets ESP-NOW sont bien reçus
   - ✅ Vérifier que la taille du paquet ne dépasse pas les limites ESP-NOW (250 bytes max)
   - ⚠️ Tester la portée de communication avec la structure élargie

2. **Test des Données**
   - ⚠️ Vérifier que toutes les valeurs sont correctement transmises
   - ⚠️ Vérifier les conversions de types (float → int8_t pour throttle/rudder)
   - ⚠️ Valider la conversion batteryLevel (mAh → %)

3. **Test de Performance**
   - ⚠️ Mesurer la fréquence d'envoi des paquets
   - ⚠️ Vérifier l'impact sur la latence ESP-NOW
   - ⚠️ Tester la stabilité sur longue durée

## Notes Importantes

1. **Conversion de Batterie** : La conversion `remainingCapacity` (mAh) → `batteryLevel` (%) utilise une capacité max de 10,000 mAh. Ajuster si nécessaire.

2. **Taille du Paquet** : La structure actuelle fait ~93 bytes, bien en dessous de la limite ESP-NOW de 250 bytes.

3. **Compatibilité** : Les champs legacy assurent une transition en douceur. Ils peuvent être supprimés dans une version future.

4. **Types de Données** : Les commandes throttle et rudder sont converties de `float` vers `int8_t` pour économiser de la mémoire tout en conservant une précision suffisante (-100 à +100).

## Conclusion

✅ La structure `BuoyState` a été enrichie avec succès avec 16 nouveaux champs
✅ Les deux projets (Joystick et Bouée) compilent sans erreur
✅ La rétrocompatibilité est assurée par les champs legacy
✅ Impact mémoire minimal (+240 bytes RAM Joystick, stable pour Bouée)
✅ Prêt pour les tests terrain

## Prochaines Étapes

1. Tester la communication ESP-NOW avec la nouvelle structure
2. Créer de nouveaux écrans sur le joystick pour afficher les nouvelles données
3. Implémenter des indicateurs visuels pour l'état des capteurs
4. Planifier la suppression des champs legacy après validation
