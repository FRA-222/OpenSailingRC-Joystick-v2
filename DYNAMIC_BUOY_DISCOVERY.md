# 📡 Système de Découverte Dynamique des Bouées

## Vue d'Ensemble

Le système de gestion des adresses MAC des bouées a été modifié pour passer d'un modèle **statique** (adresses MAC codées en dur) à un modèle **dynamique** (découverte automatique via broadcasts ESP-NOW).

## ✨ Nouveautés

### 🔄 Découverte Automatique
- Les bouées sont automatiquement détectées lors de leur premier broadcast
- Plus besoin de configurer manuellement les adresses MAC dans le code
- Support dynamique jusqu'à 6 bouées simultanées (constante `MAX_BUOYS`)

### 🧹 Nettoyage Automatique
- Suppression automatique des bouées inactives après 15 secondes
- Nettoyage périodique toutes les 30 secondes dans la boucle principale
- Libération automatique des slots pour de nouvelles bouées

### 📊 Monitoring Amélioré
- Messages de log détaillés lors de la découverte de nouvelles bouées
- Compteur du nombre total de bouées actives
- Indication visuelle des bouées supprimées pour inactivité

## 🛠️ Modifications Techniques

### Fichiers Modifiés

#### `include/ESPNowCommunication.h`
- **Ajouté** : `addBuoyDynamically()` - Fonction pour ajout automatique
- **Ajouté** : `removeInactiveBuoys()` - Fonction de nettoyage

#### `src/ESPNowCommunication.cpp`
- **Modifié** : `addBuoy()` - Marqué comme obsolète, redirige vers la version dynamique
- **Ajouté** : `addBuoyDynamically()` - Implémentation de la découverte automatique
- **Ajouté** : `removeInactiveBuoys()` - Implémentation du nettoyage
- **Modifié** : `handleReceivedData()` - Détection et ajout automatique des nouvelles bouées

#### `src/main.cpp`
- **Supprimé** : Constantes `BUOY1_MAC`, `BUOY2_MAC`, `BUOY3_MAC`
- **Supprimé** : Appels manuels à `espNow.addBuoy()`
- **Ajouté** : Nettoyage périodique des bouées inactives dans la boucle principale

## 🔄 Fonctionnement

### Découverte d'une Nouvelle Bouée
1. La bouée envoie son état via ESP-NOW (toutes les secondes)
2. Le joystick reçoit le broadcast dans `handleReceivedData()`
3. Si l'adresse MAC est inconnue → appel automatique à `addBuoyDynamically()`
4. La bouée est ajoutée comme peer ESP-NOW
5. Log de confirmation avec MAC et ID de la bouée

### Suppression d'une Bouée Inactive
1. Vérification périodique (toutes les 30s) via `removeInactiveBuoys()`
2. Si aucun message reçu depuis > 15s → marquage comme inactive
3. Suppression du peer ESP-NOW
4. Libération du slot pour une future bouée
5. Log de confirmation de suppression

## 📋 Messages de Debug

### Découverte
```
📡 ESP-NOW: Nouvelle bouée détectée - MAC: 48:E7:29:9E:2B:AC ID: 1
🆕 ESP-NOW: Bouée #1 découverte - MAC: 48:E7:29:9E:2B:AC (total: 1 bouées)
```

### Réception d'État
```
← État reçu de Bouée #1 (mode=1, bat=95%, GPS=OK)
```

### Nettoyage
```
🗑️  ESP-NOW: Bouée #1 supprimée (inactive)
🧹 ESP-NOW: 1 bouée(s) inactive(s) supprimée(s) (total restant: 0)
```

## ⚡ Avantages

1. **Simplicité** : Plus de configuration manuelle des adresses MAC
2. **Flexibilité** : Support de bouées avec des MAC changeantes
3. **Robustesse** : Gestion automatique des déconnexions/reconnexions
4. **Maintenance** : Nettoyage automatique des peers obsolètes
5. **Évolutivité** : Facile d'ajouter/retirer des bouées sur le terrain

## 🔧 Configuration Requise Côté Bouée

La bouée doit envoyer périodiquement (≤ 15s) son état via ESP-NOW avec :
- Structure `BuoyState` correctement formatée
- Champ `buoyId` renseigné (0-5)
- Timestamp mis à jour

## 🔬 Test et Validation

### Scénarios de Test
1. **Démarrage** : Vérifier que le joystick démarre sans bouées pré-configurées
2. **Découverte** : Allumer une bouée et vérifier la détection automatique
3. **Multi-bouées** : Tester avec plusieurs bouées simultanées
4. **Déconnexion** : Éteindre une bouée et vérifier la suppression automatique
5. **Reconnexion** : Rallumer une bouée et vérifier la redécouverte

### Commandes de Test
```cpp
// Vérifier le nombre de bouées détectées
Serial.printf("Bouées actives: %d\n", espNow.getBuoyCount());

// Forcer un nettoyage manuel
espNow.removeInactiveBuoys(5000);  // Supprime les bouées inactives > 5s
```

## 📝 Notes de Compatibilité

- **Rétrocompatible** : L'ancienne fonction `addBuoy()` est toujours disponible mais obsolète
- **Migration transparente** : Aucune modification requise pour les autres modules
- **Performance** : Overhead minimal pour la découverte automatique

---

**Date de modification** : 27 octobre 2025  
**Version** : v1.1 - Découverte Dynamique  
**Statut** : ✅ Implémenté et testé syntaxiquement