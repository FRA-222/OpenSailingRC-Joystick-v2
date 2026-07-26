# 📡 Configuration ESP-NOW Côté Bouée

## Vue d'Ensemble

Pour que la communication bidirectionnelle fonctionne, la bouée doit également implémenter ESP-NOW pour :
1. **Recevoir** les commandes du joystick
2. **Envoyer** son état au joystick (toutes les secondes)

---

## 🔧 Code à Ajouter sur la Bouée

### 1. Includes et Structures

```cpp
#include <esp_now.h>
#include <WiFi.h>

// Structure des commandes reçues (DOIT être identique au joystick)
struct CommandPacket {
    uint8_t targetBuoyId;
    uint8_t command;      // Type BuoyCommand
    int16_t heading;
    int8_t throttle;
    uint8_t mode;         // Type NavigationMode
    uint32_t timestamp;
};

// Structure de l'état envoyé (DOIT être identique au joystick)
struct BuoyState {
    uint8_t buoyId;
    double latitude;
    double longitude;
    float heading;
    float speed;
    uint8_t mode;
    uint8_t batteryLevel;
    int8_t signalQuality;
    bool gpsLocked;
    uint32_t timestamp;
};

// Adresse MAC du joystick (à récupérer depuis le joystick)
uint8_t joystickMAC[] = {0x48, 0xE7, 0x29, 0x9E, 0x2B, 0xAC};

// ID de cette bouée
const uint8_t BUOY_ID = 0;  // 0, 1, 2, 3 ou 4
```

### 2. Callback de Réception

```cpp
void onCommandReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(CommandPacket)) {
        Serial.printf("✗ Taille invalide: %d\n", len);
        return;
    }
    
    CommandPacket cmd;
    memcpy(&cmd, data, sizeof(CommandPacket));
    
    // Vérifie que la commande est pour cette bouée
    if (cmd.targetBuoyId != BUOY_ID) {
        return;  // Commande pour une autre bouée
    }
    
    Serial.printf("← Commande reçue: type=%d, heading=%d, throttle=%d\n",
                  cmd.command, cmd.heading, cmd.throttle);
    
    // Traite la commande
    handleCommand(cmd);
}
```

### 3. Traitement des Commandes

```cpp
void handleCommand(const CommandPacket& cmd) {
    switch (cmd.command) {
        case 0:  // CMD_INIT_HOME
            Serial.println("→ INIT HOME");
            // Enregistrer position GPS actuelle comme HOME
            // Passer en mode CAP avec cap actuel
            currentMode = 1;  // MODE_CAP
            break;
            
        case 1:  // CMD_SET_HEADING
            Serial.printf("→ SET HEADING: %d°\n", cmd.heading);
            targetHeading = cmd.heading;
            currentMode = 1;  // MODE_CAP
            break;
            
        case 2:  // CMD_SET_THROTTLE
            Serial.printf("→ SET THROTTLE: %d%%\n", cmd.throttle);
            targetThrottle = cmd.throttle;
            break;
            
        case 3:  // CMD_HOLD_POSITION
            Serial.println("→ HOLD POSITION");
            currentMode = 3;  // MODE_HOLD
            // Enregistrer position GPS actuelle
            break;
            
        case 4:  // CMD_RETURN_HOME
            Serial.println("→ RETURN HOME");
            currentMode = 4;  // MODE_RETURN_HOME
            break;
            
        case 5:  // CMD_STOP
            Serial.println("→ STOP");
            currentMode = 5;  // MODE_STOPPED
            targetThrottle = 0;
            break;
    }
}
```

### 4. Envoi de l'État (Toutes les Secondes)

```cpp
void sendBuoyState() {
    static uint32_t lastSend = 0;
    uint32_t now = millis();
    
    if (now - lastSend < 1000) {
        return;  // Envoie toutes les secondes seulement
    }
    lastSend = now;
    
    // Prépare le paquet d'état
    BuoyState state;
    state.buoyId = BUOY_ID;
    state.latitude = currentLatitude;    // Depuis GPS
    state.longitude = currentLongitude;  // Depuis GPS
    state.heading = currentHeading;      // Cap actuel
    state.speed = currentSpeed;          // Vitesse m/s
    state.mode = currentMode;            // Mode navigation
    state.batteryLevel = getBatteryLevel();  // 0-100%
    state.signalQuality = getSignalQuality(); // LTE signal
    state.gpsLocked = gpsHasLock();
    state.timestamp = now;
    
    // Envoie via ESP-NOW
    esp_err_t result = esp_now_send(joystickMAC, (uint8_t*)&state, sizeof(BuoyState));
    
    if (result == ESP_OK) {
        // Serial.println("→ État envoyé au joystick");
    } else {
        Serial.printf("✗ Échec envoi état: %d\n", result);
    }
}
```

### 5. Setup ESP-NOW sur la Bouée

```cpp
void setupESPNow() {
    // Configure WiFi en mode Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Affiche l'adresse MAC de cette bouée
    Serial.print("Bouée MAC: ");
    Serial.println(WiFi.macAddress());
    
    // Initialise ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("✗ Échec ESP-NOW init");
        return;
    }
    
    Serial.println("✓ ESP-NOW initialisé");
    
    // Enregistre le callback de réception
    esp_now_register_recv_cb(onCommandReceived);
    
    // Ajoute le joystick comme peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, joystickMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("✗ Échec ajout peer joystick");
        return;
    }
    
    Serial.println("✓ Joystick ajouté comme peer");
}
```

### 6. Intégration dans setup() et loop()

```cpp
void setup() {
    Serial.begin(115200);
    
    // ... autres initialisations (GPS, MQTT, etc.) ...
    
    // Initialise ESP-NOW
    setupESPNow();
}

void loop() {
    // ... code existant ...
    
    // Envoie l'état au joystick
    sendBuoyState();
    
    // ... reste du code ...
}
```

---

## 🔑 Configuration des Adresses MAC

### Étape 1: Obtenir l'Adresse MAC du Joystick

1. Flasher le joystick avec l'ossature
2. Ouvrir le moniteur série
3. Noter l'adresse MAC affichée au démarrage:
   ```
   ✓ ESP-NOW: Adresse MAC locale: 48:E7:29:9E:2B:AC
   ```

4. Copier cette adresse dans le code de la bouée:
   ```cpp
   uint8_t joystickMAC[] = {0x48, 0xE7, 0x29, 0x9E, 0x2B, 0xAC};
   ```

### Étape 2: Obtenir l'Adresse MAC de la Bouée

1. Sur la bouée, dans `setup()`, ajouter:
   ```cpp
   WiFi.mode(WIFI_STA);
   Serial.print("Bouée MAC: ");
   Serial.println(WiFi.macAddress());
   ```

2. Noter l'adresse affichée, par exemple:
   ```
   Bouée MAC: 48:E7:29:9E:2B:AC
   ```

3. Copier cette adresse dans `main.cpp` du joystick:
   ```cpp
   const uint8_t BUOY1_MAC[] = {0x48, 0xE7, 0x29, 0x9E, 0x2B, 0xAC};
   ```

---

## 📊 Variables à Connecter

Sur la bouée, vous devez connecter les variables suivantes au système existant:

### Variables de Navigation
```cpp
// À lire depuis votre système de navigation
double currentLatitude;    // GPS actuel
double currentLongitude;
float currentHeading;      // Cap actuel (0-360°)
float currentSpeed;        // Vitesse en m/s
uint8_t currentMode;       // Mode actuel (0-5)
```

### Variables de Commande
```cpp
// À appliquer à votre système de contrôle
int16_t targetHeading;     // Cap cible (à utiliser dans autopilot)
int8_t targetThrottle;     // Throttle cible (-100 à +100)
```

### Fonctions Utilitaires
```cpp
uint8_t getBatteryLevel() {
    // Retourne niveau batterie 0-100%
    // Exemple: return (batteryVoltage - 3.0) / (4.2 - 3.0) * 100;
}

int8_t getSignalQuality() {
    // Retourne qualité signal LTE (0-31) ou -1 si pas de LTE
    // Exemple: return modem.getSignalQuality();
}

bool gpsHasLock() {
    // Retourne true si GPS a un fix
    // Exemple: return (gps.satellites.value() >= 4);
}
```

---

## 🧪 Test de Communication

### Test 1: Vérifier Adresses MAC

**Joystick:**
```
✓ ESP-NOW: Adresse MAC locale: 48:E7:29:9E:2B:AC
```

**Bouée:**
```
Bouée MAC: 48:E7:29:9E:2B:AC
```

→ Copier ces adresses dans les codes respectifs

### Test 2: Test Ping-Pong

1. Flasher la bouée avec ESP-NOW
2. Flasher le joystick
3. Observer les logs série des deux côtés

**Logs attendus sur Joystick:**
```
→ Heartbeat envoyé à Bouée #0
← État reçu de Bouée #0 (mode=1, bat=87%, GPS=OK)
```

**Logs attendus sur Bouée:**
```
← Commande reçue: type=5, heading=0, throttle=0
→ État envoyé au joystick
```

### Test 3: Vérifier Latence

**Objectif**: Latence < 100ms

1. Envoyer une commande depuis le joystick
2. Mesurer le temps jusqu'à réception sur la bouée
3. Vérifier dans les logs

---

## ⚠️ Points d'Attention

### 1. Synchronisation des Structures

**CRITIQUE**: Les structures `CommandPacket` et `BuoyState` doivent être **EXACTEMENT identiques** sur le joystick et la bouée.

Si vous modifiez une structure, modifiez-la des DEUX côtés !

### 2. IDs des Bouées

Chaque bouée doit avoir un ID unique (0, 1, 2, 3, ou 4):
```cpp
const uint8_t BUOY_ID = 0;  // Unique pour chaque bouée
```

### 3. Canal WiFi

ESP-NOW utilise le même canal que WiFi. Si vous utilisez WiFi sur la bouée:
```cpp
WiFi.begin(ssid, password);
// ESP-NOW utilisera automatiquement le même canal
```

### 4. Portée

- **Portée théorique**: 200-300m en champ libre
- **Portée pratique**: 100-150m avec obstacles
- **Conditions idéales**: Antennes bien orientées, pas d'obstacles métalliques

---

## 📝 Checklist d'Intégration

- [ ] Ajouter `#include <esp_now.h>` et `#include <WiFi.h>`
- [ ] Copier les structures `CommandPacket` et `BuoyState`
- [ ] Définir `BUOY_ID` unique
- [ ] Configurer `joystickMAC` avec la bonne adresse
- [ ] Implémenter `onCommandReceived()` callback
- [ ] Implémenter `handleCommand()` pour traiter les commandes
- [ ] Implémenter `sendBuoyState()` pour envoyer l'état
- [ ] Appeler `setupESPNow()` dans `setup()`
- [ ] Appeler `sendBuoyState()` dans `loop()`
- [ ] Connecter variables de navigation
- [ ] Tester communication bidirectionnelle

---

## 🔍 Exemple Complet Minimal

Voici un exemple minimal pour tester la communication:

```cpp
#include <esp_now.h>
#include <WiFi.h>

// Structures (identiques au joystick)
struct CommandPacket { /* ... */ };
struct BuoyState { /* ... */ };

// Configuration
const uint8_t BUOY_ID = 0;
uint8_t joystickMAC[] = {0x48, 0xE7, 0x29, 0x9E, 0x2B, 0xAC};

// Callback
void onCommandReceived(const uint8_t* mac, const uint8_t* data, int len) {
    CommandPacket cmd;
    memcpy(&cmd, data, sizeof(CommandPacket));
    Serial.printf("← Commande: %d\n", cmd.command);
}

// Setup
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onCommandReceived);
        
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, joystickMAC, 6);
        esp_now_add_peer(&peer);
        
        Serial.println("✓ ESP-NOW OK");
    }
}

// Loop
void loop() {
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        last = millis();
        
        BuoyState state = {};
        state.buoyId = BUOY_ID;
        state.batteryLevel = 85;
        state.mode = 1;
        
        esp_now_send(joystickMAC, (uint8_t*)&state, sizeof(BuoyState));
        Serial.println("→ État envoyé");
    }
}
```

---

**Document**: Configuration ESP-NOW Bouée  
**Version**: 1.0  
**Date**: 26 octobre 2025  
**Auteur**: Philippe Hubert
