# Diagnostic LoRa - Aucune donnée reçue

## Situation actuelle
- ✅ Joystick configure module avec succès (mode CONFIG)
- ✅ Bouée configure module avec succès (mode CONFIG)  
- ✅ Les deux en mode NORMAL, switches sur OFF
- ✅ Joystick envoie 8 bytes toutes les secondes
- ❌ Bouée reçoit 0 bytes (Serial1.available() = 0)

## Hypothèses restantes

### 1. Les modules n'envoient/reçoivent pas en mode UART transparent
Malgré la configuration, les modules pourraient être en mauvais état.

### 2. Problème de câblage RX/TX
Les fils sont peut-être inversés ou mal connectés.

### 3. Les modules sont sur des canaux différents
La configuration en mémoire pourrait ne pas avoir été écrite correctement.

## Test à faire : Communication brute UART

Au lieu d'utiliser `SendFrame()` et `RecieveFrame()`, testons l'envoi brut sur Serial2/Serial1.

### Sur le Joystick - Test d'envoi brut

Dans `LoRaCommunication.cpp`, fonction `begin()`, après l'initialisation, ajoutez :

```cpp
// TEST: Envoi brut toutes les 2 secondes
Logger::log("🔬 TEST: Envoi brut de 'HELLO' toutes les 2s");
```

Et dans `pollBuoy()`, remplacez temporairement par :

```cpp
bool LoRaCommunication::pollBuoy(uint8_t buoyId, uint32_t timeoutMs) {
    // TEST BRUT: Envoyer directement sur Serial2 sans SendFrame
    Serial2.write("HELLO");
    Serial2.flush();
    Logger::log("📤 TEST: 'HELLO' envoyé sur Serial2");
    delay(500);
    return false;  // Pas de réponse attendue pour ce test
}
```

### Sur la Bouée - Test de réception brute

La fonction `maintainConnection()` lit déjà Serial1.available() et affiche les bytes bruts.
**C'est parfait**, gardez-la telle quelle.

### Résultat attendu

**Si vous voyez "🔍 UART DEBUG: 5 bytes" avec "48 45 4C 4C 4F" (ASCII pour HELLO)**
→ ✅ Les modules LoRa fonctionnent ! Le problème est dans SendFrame/RecieveFrame

**Si toujours 0 bytes**
→ ❌ Les modules ne communiquent pas, même en transparent

## Si le test échoue (0 bytes)

### Vérifier physiquement

1. **Antennes** : Bien vissées sur les deux modules ?
2. **Distance** : Moins de 1 mètre entre les modules ?
3. **Alimentation** : Les LEDs des modules s'allument ?
4. **Câbles** :
   - Joystick : GPIO1 (RX) ← LoRa TX, GPIO2 (TX) → LoRa RX
   - Bouée : GPIO36 (RX) ← LoRa TX, GPIO26 (TX) → LoRa RX

### Test de loopback local

Sur le Joystick, testez si Serial2 fonctionne localement :

```cpp
// Dans loop() ou begin() du Joystick
Serial2.write("TEST");
delay(10);
if (Serial2.available()) {
    Logger::log("✓ Serial2 loopback OK");
} else {
    Logger::log("✗ Serial2 ne loopback pas (normal si pas court-circuité)");
}
```

Si même le loopback ne fonctionne pas → problème de connexion UART.

## Prochaine étape

Voulez-vous implémenter le test d'envoi brut "HELLO" pour voir si les modules LoRa communiquent en transparent ?
