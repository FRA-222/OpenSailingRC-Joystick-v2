# Guide d'intégration LoRa 920 dans OpenSailingRC-BuoyJoystick

## Vue d'ensemble

Ce guide explique comment intégrer le module LoRa 920 MHz de M5Stack dans le projet BuoyJoystick pour permettre une communication longue portée avec les bouées GPS autonomes.

## Architecture

### 1. Structure des fichiers créés

#### Communication LoRa
- **`include/LoRaCommunication.h`** : Interface de communication LoRa
- **`src/LoRaCommunication.cpp`** : Implémentation LoRa avec M5_LoRa920

#### Configuration
- **`include/CommunicationConfig.h`** : Gestion du mode de communication
- **`src/CommunicationConfig.cpp`** : Implémentation de la configuration

#### Interface commune
- **`include/ICommunication.h`** : Interface abstraite pour ESP-NOW et LoRa
- **`include/CommunicationAdapter.h`** : Adaptateurs ESP-NOW et LoRa

## Configuration LoRa

### Paramètres par défaut
```cpp
Fréquence : 920.0 MHz (bande ISM Japon/Asie)
Bande passante : 125 kHz
Spreading Factor : 7 (compromis portée/vitesse)
Coding Rate : 4/5
Puissance TX : 20 dBm (maximum)
Longueur préambule : 8
```

### Portée estimée
- **Ligne de vue** : 5-15 km
- **Zone urbaine** : 1-3 km
- **Intérieur/obstacles** : 500m-1km

## Choix du mode de communication

### Dans `main.cpp`

Modifier la ligne :
```cpp
#define COMM_MODE CommMode::ESP_NOW  // ou CommMode::LORA
```

Ou changer le mode par défaut dans `CommunicationConfig.cpp` :
```cpp
CommMode CommunicationConfig::currentMode = CommMode::LORA;
```

## Modifications du main.cpp

### Étape 1 : Inclure les nouveaux headers
```cpp
#include "CommunicationConfig.h"
#include "LoRaCommunication.h"
#include "ICommunication.h"
#include "CommunicationAdapter.h"
```

### Étape 2 : Créer les instances
```cpp
// Instances de communication
ESPNowCommunication espNow;
LoRaCommunication lora;
ICommunication* comm = nullptr;  // Pointeur vers le mode actif

// Adaptateurs
ESPNowAdapter* espNowAdapter = nullptr;
LoRaAdapter* loraAdapter = nullptr;

// Managers (créés dynamiquement selon le mode)
BuoyStateManager* buoyState = nullptr;
DisplayManager* display = nullptr;
CommandManager* cmdManager = nullptr;
```

### Étape 3 : Initialiser selon le mode dans setup()
```cpp
void setup() {
    // ... initialisation M5Stack et Logger ...
    
    // Configurer le mode de communication
    CommunicationConfig::setMode(COMM_MODE);
    Logger::logf("Mode de communication : %s", CommunicationConfig::getModeName());
    
    // Initialiser la communication selon le mode
    if (CommunicationConfig::getMode() == CommMode::ESP_NOW) {
        Logger::log("Initialisation ESP-NOW...");
        if (!espNow.begin()) {
            Logger::log("ERREUR: Échec initialisation ESP-NOW");
            while(1) delay(1000);
        }
        espNowAdapter = new ESPNowAdapter(espNow);
        comm = espNowAdapter;
    } else {
        Logger::log("Initialisation LoRa...");
        if (!lora.begin()) {
            Logger::log("ERREUR: Échec initialisation LoRa");
            while(1) delay(1000);
        }
        loraAdapter = new LoRaAdapter(lora);
        comm = loraAdapter;
    }
    
    // Créer les managers avec l'interface commune
    // Note: BuoyStateManager et CommandManager devront être adaptés
    // pour utiliser ICommunication au lieu de ESPNowCommunication
    
    Logger::log("Système prêt");
}
```

### Étape 4 : Appeler update() dans loop()
```cpp
void loop() {
    // Mettre à jour la communication (important pour LoRa)
    if (comm) {
        comm->update();
    }
    
    // ... reste du code ...
}
```

## Dépendances

### platformio.ini
```ini
lib_deps = 
    m5stack/M5GFX@^0.1.16
    m5stack/M5Unified@^0.1.17
    m5stack/M5_LoRa920@^1.0.0  # Nouveau
```

## Structures de données

### BuoyStateLora
Identique à `BuoyState` d'ESP-NOW, permet la compatibilité.

### CommandPacketLora
Identique à `CommandPacket` d'ESP-NOW.

## Avantages et inconvénients

### ESP-NOW
✅ **Avantages**
- Latence très faible (~1-2 ms)
- Bande passante élevée
- Découverte automatique simple
- Pas de licence requise (2.4 GHz ISM)

❌ **Inconvénients**
- Portée limitée (50-200m)
- Sensible aux obstacles
- Partage la bande WiFi (interférences possibles)

### LoRa
✅ **Avantages**
- Portée très longue (plusieurs km)
- Bonne pénétration des obstacles
- Faible consommation
- Bande 920 MHz moins encombrée

❌ **Inconvénients**
- Latence élevée (plusieurs centaines de ms)
- Débit faible
- Configuration plus complexe
- Licence potentiellement requise selon pays

## Prochaines étapes

1. ✅ **Créer les fichiers LoRaCommunication** (fait)
2. ✅ **Ajouter la bibliothèque M5_LoRa920** (fait)
3. ⏳ **Adapter BuoyStateManager pour ICommunication**
4. ⏳ **Adapter CommandManager pour ICommunication**
5. ⏳ **Tester en mode ESP-NOW** (validation baseline)
6. ⏳ **Tester en mode LoRa** (validation nouveau mode)
7. ⏳ **Adapter les bouées pour LoRa** (étape future)

## Notes importantes

### Compatibilité matérielle
Le module M5Stack LoRa 920 utilise le chip SX1262 et communique via I2C. Assurez-vous que :
- Le module est correctement connecté au bus GROVE (I2C)
- L'antenne est bien fixée avant de transmettre
- La fréquence 920 MHz est autorisée dans votre région

### Configuration régionale
La bande 920 MHz est principalement utilisée en :
- 🇯🇵 Japon : 920-928 MHz
- 🇰🇷 Corée : 920-923 MHz
- 🇸🇬 Singapour : 920-925 MHz

Pour l'Europe (868 MHz) ou USA (915 MHz), ajuster `LORA_FREQUENCY`.

## Support et debug

### Logs LoRa
Les logs affichent :
- État d'initialisation du module
- Paramètres configurés (fréquence, SF, BW, etc.)
- Réception de paquets avec RSSI et SNR
- Envoi de commandes
- Découverte de nouvelles bouées

### Indicateurs qualité du signal
- **RSSI** (Received Signal Strength Indicator) : en dBm
  - > -50 dBm : Excellent
  - -50 à -80 dBm : Bon
  - -80 à -100 dBm : Moyen
  - < -100 dBm : Faible
  
- **SNR** (Signal-to-Noise Ratio) : en dB
  - > 10 dB : Excellent
  - 5 à 10 dB : Bon
  - 0 à 5 dB : Moyen
  - < 0 dB : Faible (démodulation LoRa CSS possible jusqu'à -20 dB)

## Auteur
Philippe Hubert - 2025
