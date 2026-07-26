# Patch de diagnostic LoRa pour la Bouée

> **Portée** : ce document décrit un patch temporaire à appliquer sur le firmware
> **de la bouée**, dépôt séparé `Autonomous GPS Buoy2/Autonomous-GPS-Buoy`.
> Il ne concerne pas le code du joystick présent dans ce dépôt.

## Symptôme visé

- Le Joystick envoie des REQUEST mais la Bouée ne les reçoit pas
- Les switches M0/M1 sont bien en mode normal (OFF) sur les deux modules
- Configuration logicielle identique sur les deux appareils

## Patch de diagnostic à appliquer temporairement sur la Bouée

### Dans `src/LoRaDataLinkManagement.cpp`, fonction `maintainConnection()`

**Insérer ce bloc de debug UART brut au début de la fonction :**

```cpp
void LoRaDataLinkManagement::maintainConnection(GcsManagement *myCurrentDashboard) {
    delay(50);  // Évite saturation watchdog

    static uint32_t lastLogTime = 0;
    uint32_t currentTime = millis();

    // Log toutes les 10 secondes (pas trop souvent)
    if (currentTime - lastLogTime > 10000) {
        Logger::log("📡 LoRa: En attente de données...");
        Logger::logf("   Serial1.available() = %d bytes", Serial1.available());
        lastLogTime = currentTime;
    }

    // **NOUVEAU: Afficher les octets bruts reçus**
    if (Serial1.available() > 0) {
        int bytesAvailable = Serial1.available();
        Logger::logf("🔍 UART DEBUG: %d bytes détectés sur Serial1!", bytesAvailable);

        // Afficher les premiers octets (max 16) en hexadécimal
        String hexData = "   Données brutes: ";
        int bytesToShow = min(bytesAvailable, 16);
        for (int i = 0; i < bytesToShow; i++) {
            if (Serial1.available() > 0) {
                uint8_t byte = Serial1.peek();  // Ne pas consommer pour le moment
                hexData += String(byte, HEX) + " ";
                Serial1.read();  // Consommer maintenant
            }
        }
        Logger::log(hexData.c_str());

        // Vider le reste du buffer si nécessaire
        while (Serial1.available() > 0) {
            Serial1.read();
        }

        Logger::log("   → Données consommées, attendez le prochain paquet");
    }

    // ... suite normale de maintainConnection() ...
}
```

> ⚠️ Ce bloc **consomme** les octets du buffer : le traitement normal des trames
> ne les verra plus. À retirer une fois le diagnostic terminé.

## Ce que ce patch va révéler

### Si vous voyez des logs `🔍 UART DEBUG: X bytes détectés`

✅ **Les données LoRa arrivent physiquement**

- Le module LoRa de la Bouée reçoit bien les transmissions
- Le problème est dans le format des données ou le parsing
- **Solution** : Vérifier que `RecieveFrame()` fonctionne correctement

### Si vous ne voyez AUCUN log `🔍 UART DEBUG`

❌ **Aucune donnée n'arrive sur Serial1**

- Problème de configuration RF des modules
- Les deux modules ne communiquent pas

#### Actions à prendre si aucune donnée

1. **Vérifier l'antenne de la Bouée**
   - Est-elle bien vissée sur le connecteur SMA ?
   - Pas de dommage visible ?

2. **Tester la réception du module de la Bouée**
   - Mettre le switch M0/M1 de la Bouée sur ON (mode configuration)
   - Au démarrage, le firmware effectue déjà un test UART (envoi de `C1 C1 C1`)
     et log `LoRa: Module E220-JP répond correctement` si le module répond

3. **Vérifier la configuration RF stockée dans le module**
   - Les modules E220-JP ont une **mémoire persistante**
   - Même si le code définit le canal `LORA_CHANNEL`, le module peut être resté
     configuré sur un autre canal
   - **Solution** : reprogrammer le module (étape 4)

4. **Reprogrammer le module**
   - Mettre le switch M0/M1 sur **ON** (mode config), puis redémarrer la carte
   - Le firmware applique la configuration automatiquement au démarrage et log
     `# LoRa: Module E220-JP configured successfully!`
   - Remettre le switch sur **OFF** et redémarrer pour repasser en transmission

   > Aucune recompilation n'est nécessaire : depuis l'unification de
   > `begin()` / `initialize()`, le joystick comme la bouée tentent toujours
   > d'appliquer la configuration au démarrage. Elle ne passe que si le switch
   > est sur ON ; sinon le module conserve sa configuration existante.
   > L'ancienne procédure à base de `#define LORA_MODE_CONFIGURATION` à
   > commenter/décommenter n'a plus cours.

## Prochaines étapes

1. Appliquer ce patch sur la Bouée
2. Compiler et uploader
3. Monitorer les logs de la Bouée
4. Lancer le Joystick qui envoie des REQUEST
5. Observer si des données arrivent sur Serial1
6. **Retirer le patch** une fois le diagnostic terminé
