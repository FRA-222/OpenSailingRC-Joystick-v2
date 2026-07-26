# 🏠 Commande HOME_VALIDATION - Bouton Jaune Droit

**Date** : 29 octobre 2025  
**Statut** : ✅ Implémenté

## 🎯 Objectif

Implémenter la commande `CMD_HOME_VALIDATION` et l'assigner au bouton jaune de droite (BTN_RIGHT_STICK) pour valider la position HOME d'une bouée.

## ✨ Modifications Apportées

### 1. CommandManager.h

Ajout de la nouvelle méthode publique :

```cpp
/**
 * @brief Generate and send HOME_VALIDATION command to a buoy
 * @param targetBuoyId ID of the buoy to send the command to
 * @return true if command was sent successfully
 */
bool generateHomeValidationCommand(uint8_t targetBuoyId);
```

### 2. CommandManager.cpp

Implémentation de la méthode :

```cpp
bool CommandManager::generateHomeValidationCommand(uint8_t targetBuoyId) {
    Logger::logf("\n[CommandManager] Generation commande HOME_VALIDATION pour Bouee #%d", targetBuoyId);
    
    // Création de la commande
    Command validationCmd;
    validationCmd.type = CMD_HOME_VALIDATION;
    validationCmd.heading = 0;
    validationCmd.throttle = 0;
    validationCmd.targetBuoyId = targetBuoyId;
    validationCmd.timestamp = millis();
    
    Logger::log("   -> Type: CMD_HOME_VALIDATION");
    Logger::logf("   -> Target Buoy: #%d", targetBuoyId);
    
    // Envoi via ESP-NOW
    bool success = espNowComm.sendCommand(validationCmd);
    
    if (success) {
        Logger::log("   -> Commande HOME_VALIDATION envoyee avec succes");
    } else {
        Logger::log("   -> ERREUR: Echec envoi commande HOME_VALIDATION");
    }
    
    return success;
}
```

### 3. main.cpp

Modification de l'action du bouton jaune droit :

**Avant** : Sélection de la bouée suivante
```cpp
// Bouton jaune DROIT : Changement de bouée suivante
if (joystick.wasButtonPressed(BTN_RIGHT_STICK)) {
    Logger::log("\n[R] Bouton jaune DROIT presse - Bouee suivante");
    buoyState.selectNextBuoy();
    display.displayBuoySelection();
}
```

**Après** : Envoi de la commande HOME_VALIDATION
```cpp
// Bouton jaune DROIT : Validation HOME
if (joystick.wasButtonPressed(BTN_RIGHT_STICK)) {
    Logger::log("\n[R] Bouton jaune DROIT presse - Validation HOME");
    uint8_t activeBuoy = buoyState.getSelectedBuoyId();
    if (buoyState.isSelectedBuoyConnected()) {
        cmdManager.generateHomeValidationCommand(activeBuoy);
    } else {
        Logger::log("\n[R] Bouton jaune DROIT presse - Aucune bouee connectee");
    }
}
```

## 🎮 Nouvelle Répartition des Boutons

| Bouton | Action | Fonction |
|--------|--------|----------|
| **BTN_LEFT_STICK** (Jaune gauche) | Pression courte | Commande INIT_HOME |
| **BTN_RIGHT_STICK** (Jaune droit) | Pression courte | 🆕 Commande HOME_VALIDATION |
| **BTN_LEFT** (Stick joystick gauche) | Pression courte | (À définir) |
| **BTN_RIGHT** (Stick joystick droit) | Pression courte | (À définir) |
| **BTN_ATOM_SCREEN** (Écran tactile) | Pression courte | Sélectionner bouée suivante |

## 🔄 Flux d'Utilisation

### Scénario : Définition et Validation du Point HOME

1. **Positionner la bouée** physiquement à l'endroit souhaité pour le HOME
2. **Appuyer sur bouton jaune GAUCHE** → Envoie `CMD_INIT_HOME`
   - La bouée enregistre sa position GPS actuelle comme HOME
   - Mode général passe en `HOME_DEFINITION`
3. **Vérifier la position** (optionnel : vérifier coordonnées GPS)
4. **Appuyer sur bouton jaune DROIT** → Envoie `CMD_HOME_VALIDATION`
   - La bouée valide et confirme le point HOME
   - Mode général peut passer en `READY` ou `NAV`

## 📊 Logs Attendus

### Commande Envoyée avec Succès

```
[R] Bouton jaune DROIT presse - Validation HOME

[CommandManager] Generation commande HOME_VALIDATION pour Bouee #0
   -> Type: CMD_HOME_VALIDATION
   -> Target Buoy: #0
   -> Commande HOME_VALIDATION envoyee avec succes
```

### Aucune Bouée Connectée

```
[R] Bouton jaune DROIT presse - Validation HOME
[R] Bouton jaune DROIT presse - Aucune bouee connectee
```

## 🧪 Tests

### Test 1 : Commande HOME_VALIDATION Seule

1. Connecter une bouée
2. S'assurer qu'elle est sélectionnée comme bouée active
3. Appuyer sur le bouton jaune DROIT
4. Vérifier les logs série

**Résultat attendu** :
- ✅ Log "[CommandManager] Generation commande HOME_VALIDATION"
- ✅ Log "Commande HOME_VALIDATION envoyee avec succes"

### Test 2 : Séquence INIT → VALIDATION

1. Connecter une bouée
2. **Étape 1** : Appuyer sur bouton jaune GAUCHE (INIT_HOME)
3. Attendre confirmation de la bouée
4. **Étape 2** : Appuyer sur bouton jaune DROIT (HOME_VALIDATION)
5. Observer le changement de mode général sur la bouée

**Résultat attendu** :
- ✅ Bouée passe en mode `HOME_DEFINITION` après INIT
- ✅ Bouée valide et confirme le HOME après VALIDATION
- ✅ Bouée prête pour la navigation

### Test 3 : Sans Bouée Connectée

1. S'assurer qu'aucune bouée n'est connectée
2. Appuyer sur bouton jaune DROIT
3. Vérifier les logs

**Résultat attendu** :
- ✅ Log "Aucune bouee connectee"
- ✅ Aucune commande envoyée

## 📝 Notes Techniques

### Sécurité

- ✅ Vérification de la connexion de la bouée avant envoi
- ✅ Logs détaillés pour le débogage
- ✅ Gestion des erreurs d'envoi

### Structure de la Commande

```cpp
Command validationCmd;
validationCmd.type = CMD_HOME_VALIDATION;  // Type de commande
validationCmd.heading = 0;                 // Pas utilisé pour cette commande
validationCmd.throttle = 0;                // Pas utilisé pour cette commande
validationCmd.targetBuoyId = activeBuoy;   // ID de la bouée cible
validationCmd.timestamp = millis();        // Horodatage
```

### Protocole ESP-NOW

La commande est envoyée via ESP-NOW en utilisant :
```cpp
espNowComm.sendCommand(validationCmd)
```

## ⚡ Avantages

1. **Workflow Complet** : INIT + VALIDATION = Processus en 2 étapes
2. **Sécurité** : Validation explicite du point HOME
3. **Interface Intuitive** : 2 boutons jaunes côte à côte pour actions liées
4. **Feedback Clair** : Logs détaillés à chaque étape
5. **Robuste** : Vérifications de connexion avant envoi

## 🔄 Sélection de Bouée

La fonctionnalité de sélection de bouée suivante a été **conservée** et reste accessible via :
- **Bouton écran Atom S3** (BTN_ATOM_SCREEN)

Cette réaffectation permet d'avoir les deux commandes HOME (INIT et VALIDATION) sur les boutons jaunes physiques, ce qui est plus logique ergonomiquement.

---

**Version** : 2.0  
**Auteur** : Philippe Hubert  
**Statut** : ✅ Fonctionnel et testé
