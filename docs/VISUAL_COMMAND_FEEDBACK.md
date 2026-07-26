# Feedback Visuel sur l'Écran du Joystick

## Vue d'ensemble

Un système de feedback visuel a été ajouté pour afficher l'état d'envoi des commandes directement sur l'écran du joystick. La ligne "Buoy #X" change de couleur selon l'état de la commande.

## États et couleurs

### 🔵 Bleu - SENDING
**Quand :** Une commande (hors heartbeat) vient d'être envoyée et attend l'ACK de la bouée

**Signification :** La commande est en transit, l'attente de confirmation est en cours

**Durée d'affichage :** Jusqu'à réception de l'ACK ou timeout (max 2 secondes par tentative)

### 🟢 Vert - ACK_RECEIVED  
**Quand :** L'ACK a été reçu de la bouée

**Signification :** La commande a été confirmée, tout s'est bien passé

**Durée d'affichage :** 3 secondes puis retour à la couleur normale

### 🔴 Rouge - TIMEOUT
**Quand :** Aucun ACK reçu après 4 tentatives d'envoi (1 envoi initial + 3 retry)

**Signification :** La commande n'a pas été confirmée, possible problème de communication

**Durée d'affichage :** 3 secondes puis retour à la couleur normale

### Couleur normale (après timeout de 3s)
- **Vert** : Bouée connectée (données reçues récemment)
- **Rouge** : Bouée déconnectée (pas de données)

## Séquence type

```
1. Utilisateur envoie commande → "Buoy #0" passe en BLEU
2a. ACK reçu rapidement → "Buoy #0" passe en VERT (3s)
2b. Pas d'ACK → Retry automatique (reste BLEU)
3. Si toujours pas d'ACK après 4 tentatives → "Buoy #0" passe en ROUGE (3s)
4. Après 3 secondes → Retour à la couleur normale (vert si connecté, rouge si non)
```

## Implémentation technique

### Enum CommandStatus

```cpp
enum class CommandStatus {
    IDLE,          // Aucune commande en cours
    SENDING,       // Commande envoyée, attente ACK (Bleu)
    ACK_RECEIVED,  // ACK reçu (Vert)
    TIMEOUT        // Timeout après max retry (Rouge)
};
```

### DisplayManager

#### Nouvelle méthode
```cpp
void setCommandStatus(CommandStatus status);
```

Appelée par les gestionnaires de communication (LoRa/ESP-NOW) pour notifier l'état.

#### Modification de drawHeader()
- Vérifie si un statut de commande est actif (< 3 secondes)
- Affiche la couleur correspondant au statut
- Après 3 secondes, retour à la couleur normale
- Auto-reset du statut à IDLE après expiration

### LoRaCommunication & ESPNowCommunication

#### Nouvelle méthode
```cpp
void setDisplayManager(DisplayManager* display);
```

Permet de connecter le DisplayManager pour recevoir les notifications.

#### Points de notification

**1. Envoi de commande (sendCommand)**
```cpp
if (addPendingCommand(packet)) {
    // Notifier : commande envoyée
    if (displayManager != nullptr) {
        displayManager->setCommandStatus(CommandStatus::SENDING);
    }
}
```

**2. Réception d'ACK (processAck)**
```cpp
pendingCommands[i].ackReceived = true;
// Notifier : ACK reçu
if (displayManager != nullptr) {
    displayManager->setCommandStatus(CommandStatus::ACK_RECEIVED);
}
```

**3. Timeout après retry (processCommandRetries)**
```cpp
if (pendingCommands[i].retryCount >= MAX_RETRY_COUNT) {
    // Notifier : timeout
    if (displayManager != nullptr) {
        displayManager->setCommandStatus(CommandStatus::TIMEOUT);
    }
}
```

### Configuration dans main.cpp

```cpp
if (COMM_MODE == CommMode::ESP_NOW) {
    espNow.setDisplayManager(display);
} else {
    lora.setDisplayManager(display);
}
```

## Logs de debug

Le système affiche des logs détaillés :

```
🖥️  Display: Statut commande -> SENDING (Bleu)
🖥️  Display: Statut commande -> ACK_RECEIVED (Vert)
🖥️  Display: Statut commande -> TIMEOUT (Rouge)
```

## Comportement spécial

### Heartbeats
Les heartbeats **ne déclenchent pas** de changement de couleur car :
- Ils ne nécessitent pas d'ACK côté bouée
- Ils sont envoyés périodiquement toutes les 3 secondes
- Ils ne doivent pas perturber l'affichage

### Commandes multiples rapides
Si plusieurs commandes sont envoyées rapidement :
- Le statut reflète la dernière commande
- Chaque ACK/timeout met à jour la couleur
- Le timer des 3 secondes est réinitialisé à chaque changement

### Changement de bouée
Lors du changement de bouée sélectionnée :
- Le statut est conservé (reste affiché 3 secondes)
- La nouvelle bouée commence avec sa couleur normale
- Les commandes en attente continuent d'être suivies

## Paramètres ajustables

### DisplayManager.h
```cpp
static const uint32_t STATUS_DISPLAY_DURATION = 3000;  // Durée d'affichage statut (ms)
```

Peut être modifié pour :
- Augmenter/diminuer la durée d'affichage du statut
- Valeur recommandée : 2000-5000ms

### Communication classes
Les paramètres de retry/timeout sont dans :
- `LoRaCommunication.h` : ACK_TIMEOUT_MS, MAX_RETRY_COUNT
- `ESPNowCommunication.h` : ACK_TIMEOUT_MS, MAX_RETRY_COUNT

## Avantages

✅ **Feedback immédiat** : L'utilisateur voit instantanément si sa commande est prise en compte

✅ **Diagnostic rapide** : Le rouge indique clairement un problème de communication

✅ **Confirmation visuelle** : Le vert rassure que tout fonctionne bien

✅ **Non-intrusif** : Retour automatique à la normale après 3 secondes

✅ **Compatible** : Fonctionne en LoRa et ESP-NOW

## Tests recommandés

1. ✅ Envoyer une commande normale et vérifier le passage BLEU → VERT
2. ✅ Simuler une perte de communication et vérifier BLEU → ROUGE
3. ✅ Vérifier que les heartbeats ne changent pas la couleur
4. ✅ Tester avec plusieurs bouées
5. ✅ Vérifier le retour à la normale après 3 secondes
6. ✅ Tester plusieurs commandes rapides successives

## Date

Implémenté le 26 décembre 2025
