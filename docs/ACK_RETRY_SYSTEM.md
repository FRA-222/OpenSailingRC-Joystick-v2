# Système ACK et Retry pour les Commandes

## Vue d'ensemble

Le système de gestion des ACK (acknowledgments) et de retry automatique a été implémenté pour garantir la fiabilité de la transmission des commandes entre le joystick et les bouées.

## Fonctionnement

### 1. Envoi de commande
Quand une commande est envoyée (sauf heartbeat) :
- La commande est transmise via LoRa ou ESP-NOW
- Elle est ajoutée à une queue de commandes en attente d'ACK
- Un timestamp d'envoi est enregistré

### 2. Réception d'ACK
La bouée renvoie un ACK pour chaque commande reçue (sauf heartbeat) :
- L'ACK contient : buoyId, commandTimestamp, commandType
- Le joystick reçoit l'ACK et cherche la commande correspondante dans la queue
- Si trouvée, la commande est marquée comme confirmée

### 3. Système de retry
Si aucun ACK n'est reçu dans le délai imparti :
- Timeout : 2000ms (2 secondes)
- Nombre de tentatives max : 3 retry (4 tentatives au total)
- Intervalle entre retry : 500ms minimum
- Après 3 retry sans ACK, la commande est abandonnée

## Paramètres configurables

### LoRa (LoRaCommunication.h)
```cpp
static const uint8_t MAX_PENDING_COMMANDS = 10;  // Queue max
static const uint8_t MAX_RETRY_COUNT = 3;        // Retry max
static const uint32_t ACK_TIMEOUT_MS = 2000;     // Timeout ACK
static const uint32_t RETRY_INTERVAL_MS = 500;   // Intervalle retry
```

### ESP-NOW (ESPNowCommunication.h)
```cpp
static const uint8_t MAX_PENDING_COMMANDS = 10;  // Queue max
static const uint8_t MAX_RETRY_COUNT = 3;        // Retry max
static const uint32_t ACK_TIMEOUT_MS = 2000;     // Timeout ACK
static const uint32_t RETRY_INTERVAL_MS = 500;   // Intervalle retry
```

## Structures de données

### LoRa

#### AckPacketLora
```cpp
struct __attribute__((packed)) AckPacketLora {
    LoRaMessageType messageType;  // ACK (0x04)
    uint8_t buoyId;               // ID de la bouée
    uint32_t commandTimestamp;    // Timestamp de la commande
    BuoyCommand commandType;      // Type de commande
};
```

#### PendingCommand (LoRa)
```cpp
struct PendingCommand {
    CommandPacketLora command;    // Commande à envoyer/retry
    uint32_t sentTime;            // Heure d'envoi
    uint8_t retryCount;           // Nombre de retry
    bool ackReceived;             // ACK reçu ?
};
```

### ESP-NOW

#### AckPacket
```cpp
struct AckPacket {
    uint8_t buoyId;               // ID de la bouée
    uint32_t commandTimestamp;    // Timestamp de la commande
    BuoyCommand commandType;      // Type de commande
};
```

#### PendingCommandESPNow
```cpp
struct PendingCommandESPNow {
    CommandPacket command;        // Commande à envoyer/retry
    uint32_t sentTime;            // Heure d'envoi
    uint8_t retryCount;           // Nombre de retry
    bool ackReceived;             // ACK reçu ?
};
```

## Nouvelles méthodes

### LoRaCommunication

#### processCommandRetries()
Vérifie périodiquement les commandes en attente et renvoie celles qui ont timeout.
Appelée dans le loop principal (main.cpp).

#### sendCommandPacket()
Envoie un paquet de commande via LoRa (appelée par sendCommand et retry).

#### addPendingCommand()
Ajoute une commande à la queue des commandes en attente d'ACK.

#### processAck()
Traite un ACK reçu et marque la commande correspondante comme confirmée.

### ESPNowCommunication

Mêmes méthodes que pour LoRa, adaptées pour ESP-NOW.

## Modifications du code existant

### LoRaCommunication.cpp
- `sendCommand()` : Modifié pour utiliser le système de pending commands
- `listenForResponses()` : Modifié pour traiter aussi les ACK
- Constructeur : Initialisation de la queue de pending commands

### ESPNowCommunication.cpp
- `sendCommand()` : Modifié pour utiliser le système de pending commands
- `handleReceivedData()` : Modifié pour traiter aussi les ACK
- Constructeur : Initialisation de la queue de pending commands

### main.cpp
- Loop principal : Ajout de l'appel à `processCommandRetries()` pour LoRa et ESP-NOW

## Logs et debug

Le système affiche des logs détaillés :

### Envoi de commande
```
📤 ========== ENVOI COMMANDE LoRa ==========
✓ LoRa: Commande ajoutée à la queue (en attente d'ACK)
```

### Réception d'ACK
```
📥 ACK reçu de Bouée #0 (RSSI=-45 dBm)
✅ ACK reçu de Bouée #0 pour commande type=1 (ts=12345)
   ✓ Commande confirmée (retry=0)
```

### Retry
```
🔄 LoRa: Renvoi commande (tentative 2/4) à Bouée #0
```

### Timeout
```
❌ LoRa: Commande timeout après 4 tentatives (Bouée #0, type=1)
```

## Compatibilité

- ✅ Compatible avec le code existant de la bouée (envoie déjà les ACK)
- ✅ Compatible LoRa et ESP-NOW
- ✅ Les heartbeats ne déclenchent pas d'attente d'ACK (comme sur la bouée)
- ✅ Pas d'impact sur les performances (processing async dans le loop)

## Tests recommandés

1. Vérifier que les commandes sont bien envoyées et confirmées
2. Tester le retry en simulant une perte de communication
3. Vérifier que les heartbeats ne génèrent pas d'ACK
4. Tester avec plusieurs bouées simultanément
5. Vérifier la gestion de la queue pleine

## Date

Implémenté le 25 décembre 2025
