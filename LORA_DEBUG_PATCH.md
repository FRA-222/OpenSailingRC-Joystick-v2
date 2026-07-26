# Patch de diagnostic LoRa pour la Bouée

## Problème actuel
- Le Joystick envoie des REQUEST mais la Bouée ne les reçoit pas
- Les switches M0/M1 sont bien en mode normal (OFF) sur les deux modules
- Configuration logicielle identique sur les deux appareils

## Patch de diagnostic à appliquer temporairement sur la Bouée

### Dans `src/LoRaDataLinkManagement.cpp`, fonction `maintainConnection()`:

**Remplacer la boucle existante par cette version avec debug UART brut :**

```cpp
void LoRaDataLinkManagement::maintainConnection() {
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
}
```

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

#### Actions à prendre si aucune donnée :

1. **Vérifier l'antenne de la Bouée**
   - Est-elle bien vissée sur le connecteur SMA ?
   - Pas de dommage visible ?

2. **Tester la réception du module de la Bouée**
   - Passer temporairement la Bouée en `LORA_MODE_CONFIGURATION`
   - Envoyer des commandes AT pour vérifier que le module répond
   - Exemple : `Serial1.write("C1 C1 C1")` devrait recevoir une réponse

3. **Vérifier la configuration RF stockée dans le module**
   - Les modules E220-JP ont une **mémoire persistante**
   - Même si votre code définit canal 23, le module pourrait être configuré sur un autre canal
   - **Solution** : Repasser en `LORA_MODE_CONFIGURATION` et reprogrammer

4. **Test en mode configuration forcée**
   - Mettre le switch de la Bouée sur ON (mode config)
   - Décommenter `#define LORA_MODE_CONFIGURATION`
   - Uploader et vérifier que la config passe bien

## Prochaines étapes

1. Appliquer ce patch sur la Bouée
2. Compiler et uploader
3. Monitorer les logs de la Bouée
4. Lancer le Joystick qui envoie des REQUEST
5. Observer si des données arrivent sur Serial1

**Rapporter les résultats ici pour continuer le diagnostic.**
