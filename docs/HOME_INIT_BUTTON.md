# Initialisation du HOME via Bouton Jaune

## Vue d'ensemble

Cette fonctionnalité permet d'initialiser le point HOME d'une bouée GPS autonome en appuyant sur le bouton jaune de gauche du joystick.

Date de mise en œuvre : 28 octobre 2025

## Fonctionnalité

### Bouton Jaune Gauche (BTN_LEFT_STICK)
**Fonction** : Envoi de la commande `CMD_INIT_HOME` à la bouée sélectionnée

**Comportement** :
1. L'utilisateur appuie sur le bouton jaune de gauche
2. Le système vérifie qu'une bouée est sélectionnée et connectée
3. Une commande `CMD_INIT_HOME` est créée et envoyée via ESP-NOW
4. La bouée reçoit la commande et définit sa position GPS actuelle comme point HOME
5. La bouée démarre en mode navigation HOME (NAV_HOME)

**Validation** :
- Si aucune bouée n'est connectée : message d'erreur dans le log série
- Si l'envoi réussit : confirmation dans le log série
- Si l'envoi échoue : message d'erreur ESP-NOW dans le log série

### Bouton Jaune Droit (BTN_RIGHT_STICK)
**Fonction** : Sélection de la bouée suivante (fonctionnalité existante)

## Structure de Commande

La commande envoyée a la structure suivante :

```cpp
Command homeCmd;
homeCmd.type = CMD_INIT_HOME;  // Type de commande
homeCmd.heading = 0;           // Non utilisé pour cette commande
homeCmd.throttle = 0;          // Non utilisé pour cette commande
homeCmd.mode = NAV_HOME;       // Mode de navigation HOME
homeCmd.timestamp = millis();  // Horodatage de la commande
```

## Protocole ESP-NOW

### Paquet Envoyé

Structure `CommandPacket` transmise via ESP-NOW :
- `targetBuoyId` : ID de la bouée cible (0-5)
- `command` : `CMD_INIT_HOME` (0)
- `heading` : 0 (non utilisé)
- `throttle` : 0 (non utilisé)
- `navigationMode` : `NAV_HOME` (1)
- `timestamp` : Horodatage en millisecondes

Taille du paquet : ~16 octets

### Transmission
- Protocole : ESP-NOW point-à-point
- Adresse MAC cible : Adresse de la bouée sélectionnée (découverte automatiquement)
- Confirmation : Retour de `esp_now_send()` (ESP_OK en cas de succès)

## Flux de Traitement

### Côté Joystick (main.cpp)

```
Appui bouton jaune gauche
         |
         v
Vérification bouée connectée
         |
         +--NO--> Message erreur "Aucune bouée connectée"
         |
        YES
         |
         v
Création commande CMD_INIT_HOME
         |
         v
Envoi via espNow.sendCommand()
         |
         +--FAIL--> Message erreur "Échec envoi"
         |
      SUCCESS
         |
         v
Message confirmation + rafraîchissement affichage
```

### Côté Bouée (ESPNowDataLinkManagement.cpp)

```
Réception paquet ESP-NOW
         |
         v
Décodage CommandPacket
         |
         v
Vérification targetBuoyId == buoyId
         |
         +--NO--> Ignore le paquet
         |
        YES
         |
         v
Traitement selon command.type
         |
    CMD_INIT_HOME
         |
         v
Enregistrement position GPS actuelle comme HOME
         |
         v
Passage en mode NAV_HOME
         |
         v
Démarrage navigation vers HOME (mode HDG)
```

## Messages de Log Série

### Messages de Succès
```
[L] Bouton jaune GAUCHE presse - Envoi CMD_INIT_HOME a Bouee #2
   -> Commande HOME envoyee avec succes
→ Commande envoyée à Bouée #2 (type=0)
```

### Messages d'Erreur

**Aucune bouée connectée** :
```
[L] Bouton jaune GAUCHE presse - Aucune bouee connectee
```

**Bouée non trouvée** :
```
✗ ESP-NOW: Bouée #2 non trouvée
   -> ERREUR: Echec envoi commande HOME
```

**Échec ESP-NOW** :
```
✗ ESP-NOW: Échec envoi à Bouée #2 (err=XX)
   -> ERREUR: Echec envoi commande HOME
```

## Configuration Matérielle

### Joystick M5Stack Atom S3
- **Bouton jaune gauche** : GPIO connecté au module I2C JoyC (BTN_LEFT_STICK)
- **Emplacement physique** : En haut à gauche du module JoyC
- **Type** : Bouton poussoir momentané

### Communication
- **Protocole** : ESP-NOW 2.4GHz
- **Portée** : ~100m en champ libre
- **Latence** : <10ms typique

## Cas d'Usage

### Initialisation avant Navigation

1. **Mise sous tension de la bouée**
   - La bouée démarre en mode INIT
   - Elle acquiert le signal GPS
   - Elle passe en mode READY

2. **Définition du HOME**
   - Positionner physiquement la bouée à l'emplacement désiré comme HOME
   - Sélectionner la bouée sur le joystick (bouton jaune droit si nécessaire)
   - Appuyer sur le bouton jaune gauche
   - La bouée enregistre sa position actuelle comme HOME

3. **Navigation**
   - Déplacer la bouée manuellement à un autre endroit
   - La bouée revient automatiquement vers le point HOME défini
   - Possibilité de redéfinir le HOME à tout moment en répétant l'opération

### Mode Maintenance

Lors de la maintenance ou du repositionnement :
1. Arrêter la bouée (commande STOP)
2. La déplacer physiquement
3. Redéfinir le HOME avec le bouton jaune gauche
4. Relancer la navigation

## Intégration avec CommandManager

La commande `CMD_INIT_HOME` est définie dans `CommandManager.h` :

```cpp
enum BuoyCommand {
    CMD_INIT_HOME = 0,      ///< Initialize Home with current GPS and start in HDG mode
    CMD_SET_HEADING,        ///< Set target heading (HDG mode)
    CMD_SET_THROTTLE,       ///< Set speed
    CMD_HOLD_POSITION,      ///< Hold current position
    CMD_RETURN_HOME,        ///< Return to Home position
    CMD_STOP,               ///< Stop all movements
    CMD_CHANGE_MODE         ///< Change navigation mode
};
```

### Différence avec CMD_RETURN_HOME

- **CMD_INIT_HOME** : Définit le point HOME à la position GPS actuelle de la bouée
- **CMD_RETURN_HOME** : Demande à la bouée de revenir vers un point HOME déjà défini

## Sécurité et Validation

### Vérifications Implémentées

1. **Vérification de connexion** :
   ```cpp
   if (buoyState.isSelectedBuoyConnected())
   ```
   - Empêche l'envoi de commande vers une bouée non connectée

2. **Vérification d'envoi ESP-NOW** :
   ```cpp
   if (espNow.sendCommand(selectedId, homeCmd))
   ```
   - Confirme la transmission effective du paquet

3. **Affichage de feedback** :
   - Messages console pour débogage
   - Rafraîchissement de l'affichage LCD

### Limitations Connues

1. **Pas de confirmation de réception côté bouée** :
   - ESP-NOW ne garantit pas la réception
   - Recommandation : Implémenter un système d'acquittement (ACK)

2. **Pas de validation GPS** :
   - La commande est envoyée même si la bouée n'a pas de fix GPS
   - La bouée doit gérer cette situation de son côté

3. **Pas de protection contre les appuis multiples** :
   - Possibilité d'envoyer plusieurs commandes rapidement
   - Peut être ajouté un debouncing temporel si nécessaire

## Évolutions Futures

### Court Terme
- [ ] Ajouter un feedback visuel sur l'écran LCD lors de l'envoi
- [ ] Implémenter un système d'acquittement (ACK/NACK)
- [ ] Ajouter un délai anti-rebond (cooldown) entre envois

### Moyen Terme
- [ ] Afficher un message de confirmation sur l'écran de la bouée
- [ ] Enregistrer le point HOME en mémoire persistante (EEPROM/Flash)
- [ ] Permettre la sauvegarde de plusieurs points HOME

### Long Terme
- [ ] Interface graphique pour visualiser la position HOME
- [ ] Historique des points HOME définis
- [ ] Synchronisation automatique des points HOME entre bouées

## Tests Recommandés

### Test Unitaire
1. Compiler le firmware sans erreurs ✅
2. Vérifier la structure de la commande
3. Valider l'intégrité du paquet ESP-NOW

### Test d'Intégration
1. Connecter joystick et bouée
2. Sélectionner une bouée
3. Appuyer sur le bouton jaune gauche
4. Observer les logs série des deux côtés
5. Vérifier que la bouée passe en mode NAV_HOME

### Test Fonctionnel
1. Positionner la bouée à un point A
2. Définir le HOME (bouton jaune gauche)
3. Déplacer manuellement la bouée au point B
4. Observer le retour automatique vers le point A
5. Mesurer la précision du retour (distance finale au HOME)

### Test de Robustesse
1. Tester l'envoi sans bouée connectée
2. Tester avec bouée hors de portée
3. Tester avec plusieurs bouées actives
4. Tester les appuis rapides successifs

## Code Source

### Fichiers Modifiés

**OpenSailingRC-BuoyJoystick/src/main.cpp**
- Ligne 151-181 : Gestion du bouton jaune gauche
- Ajout de la création et envoi de commande CMD_INIT_HOME

**Aucune modification nécessaire dans** :
- CommandManager.h (énumération déjà définie)
- ESPNowCommunication.h/cpp (méthode sendCommand() existante)
- Structure BuoyState (déjà compatible)

## Références

### Documentation Associée
- `DYNAMIC_BUOY_DISCOVERY.md` : Découverte automatique des bouées
- `ESPNOW_BROADCAST_MODE.md` : Protocole ESP-NOW en mode broadcast
- `GENERAL_MODE_FEATURE.md` : Gestion des modes généraux et navigation
- `ATOM_SCREEN_BUTTON.md` : Bouton écran Atom S3
- `API_DOCUMENTATION.md` : Documentation API complète
- `ARCHITECTURE.md` : Architecture globale du système

### Code Source
- `src/main.cpp` : Boucle principale et gestion des boutons
- `include/CommandManager.h` : Définitions des commandes
- `src/ESPNowCommunication.cpp` : Envoi ESP-NOW
- `include/ESPNowCommunication.h` : Interface ESP-NOW

### Specifications Matérielles
- M5Stack Atom S3 : https://docs.m5stack.com/en/core/AtomS3
- Module JoyC : https://docs.m5stack.com/en/unit/joyc
- ESP-NOW Protocol : https://www.espressif.com/en/solutions/low-power-solutions/esp-now

## Changelog

### Version 1.0 - 28 octobre 2025
- ✅ Implémentation initiale du bouton HOME
- ✅ Création de la commande CMD_INIT_HOME
- ✅ Envoi ESP-NOW vers bouée sélectionnée
- ✅ Validation avec bouée connectée
- ✅ Messages de log série
- ✅ Compilation réussie (RAM: 14.4%, Flash: 26.2%)

## Notes Techniques

### Consommation Mémoire
- **Impact RAM** : +20 bytes (structure Command temporaire)
- **Impact Flash** : +2,396 bytes (code de gestion du bouton et envoi)
- **Utilisation RAM totale** : 47,248 / 327,680 bytes (14.4%)
- **Utilisation Flash totale** : 875,849 / 3,342,336 bytes (26.2%)

### Performance
- **Temps de traitement** : <1ms (création commande + envoi ESP-NOW)
- **Latence ESP-NOW** : 5-10ms typique
- **Fréquence max d'envoi** : Limitée par la boucle principale (10Hz)

### Compatibilité
- **ESP-IDF** : Compatible avec Arduino framework sur ESP32-S3
- **M5Unified** : Version 0.1.17 requise
- **PlatformIO** : Platform espressif32 v6.12.0+

---

**Auteur** : Philippe Hubert  
**Projet** : OpenSailingRC - BuoyJoystick  
**Licence** : Voir LICENSE.md  
**Date** : 28 octobre 2025
