# Menu de configuration du joystick — conception

> Statut : **implémenté** (après 1.2.2), à valider sur matériel. Base : firmware joystick 1.2.2.
> Pendant côté bouée : `docs/MAINTENANCE_MENU_DESIGN.md` du dépôt Autonomous-GPS-Buoy.

## 1. Objectif

Rendre la configuration de communication du joystick modifiable **sans recompiler**, comme sur
la bouée, et la conserver dans les préférences (NVS) :

- mode de communication : **ESP-NOW** ou **LoRa** ;
- en LoRa : **bande** (433 / 920, bientôt 868), **puissance**, pour respecter la réglementation
  de chaque pays, et **canal** ;
- écoute ESP-NOW passive en mode LoRa (`JOYSTICK_ESPNOW_PASSIVE`, aujourd'hui fixée à 0).

Le même firmware doit offrir ce menu sur les deux matériels :

| Matériel | Écran | Entrées |
|---|---|---|
| **Core2** (`joystick-v2-core2`) | 320 × 240 | 2 Unit Joystick, Dual Button rouge / bleu, ByteButton, bouton A |
| **AtomS3** (`joystick-v1-atoms3`) | **128 × 128** | 2 sticks et 4 boutons de la carte STM32, bouton écran |

## 2. Existant

- **Aucune préférence** : le joystick n'utilise pas la NVS.
- **Aucun menu, aucun mode maintenance** : en fonctionnement, chaque entrée envoie une commande à
  la bouée sélectionnée.
- **Configuration figée à la compilation** dans `include/JoystickConfiguration.h` :

  | Constante | Rôle |
  |---|---|
  | `COMM_MODE` | `ESP_NOW`, `LORA_920` ou `LORA_433` ; la bande LoRa est déduite du mode |
  | `LORA_AIR_RATE` | débit air |
  | `JOYSTICK_ESPNOW_PASSIVE` | écoute ESP-NOW en mode LoRa |
  | `LORA_CHANNEL_433` / `_920`, `getTxPower()` | canal et puissance (`LoRaCommunication`) |

- **Choix lus une seule fois dans `setup()`** : l'instance `LoRaCommunication` est construite avec
  la bande et le débit air, puis `main.cpp` teste `COMM_MODE` pour initialiser ESP-NOW ou LoRa.
- **Abstraction matérielle déjà en place** :
  - `JoystickManager` expose les mêmes entrées logiques sur les deux matériels : axes `AXIS_*`,
    boutons `BTN_*`, bouton A avec `wasAtomScreenPressed()` et `isAtomScreenHeld()` ;
  - `HardwareConfig.h` associe déjà boutons physiques et actions (`BTN_ACTION_*`) ;
  - `DisplayManagerV1` et `DisplayManagerV2` se partagent l'interface `DisplayManager.h`.

## 3. Principes

1. **Un seul moteur de menu**, commun aux deux matériels : données, navigation et préférences.
   Seuls l'affichage (un rendu par `DisplayManagerV*`) et la correspondance des boutons
   (`HardwareConfig.h`) dépendent du matériel. C'est le découpage déjà en vigueur.
2. **Mêmes concepts que la bouée** : table descriptive des préférences, profils de bande, page
   générique « choix dans une liste ».
3. **Menu au démarrage, avant l'initialisation de la radio.** La configuration choisie s'applique
   donc tout de suite, sans redémarrage.

## 4. Entrée dans le menu

- **Bouton A maintenu à la mise sous tension**, pendant au moins 2 s. Sur l'AtomS3, c'est un appui
  sur l'écran ; sur le Core2, le bouton A sous l'écran. L'écran de démarrage l'indique :
  « Hold A for setup ».
- Pas d'entrée en cours de fonctionnement : le bouton A sélectionne déjà la bouée suivante à
  chaque appui, et un appui long le déclencherait aussi. Entrer au démarrage évite aussi de
  modifier la radio pendant qu'on pilote une bouée.
- À chaque démarrage normal, la configuration en service s'affiche 2 s, par exemple
  `LoRa 433 · ch 24 · 22 dBm`, pour qu'un réglage inattendu se voie tout de suite.

## 5. Navigation, commune aux deux matériels

| Action | Entrée | Core2 | AtomS3 |
|---|---|---|---|
| Entrée précédente / suivante | stick gauche ▲ / ▼ | Unit Joystick gauche | stick gauche |
| Valeur précédente / suivante | stick gauche ◄ / ► | idem | idem |
| OK : ouvrir, valider | `BTN_MENU_OK` | Dual Button bleu (droit) | bouton jaune droit |
| Retour | `BTN_MENU_BACK` | Dual Button rouge (gauche) | bouton jaune gauche |

- `BTN_MENU_OK` et `BTN_MENU_BACK` sont définis dans `HardwareConfig.h`, sur le modèle des
  `BTN_ACTION_*`.
- Le stick est lu en front, avec le seuil et l'hystérésis déjà utilisés par `main.cpp`
  (`JOYSTICK_THRESHOLD`, retour sous la moitié) : un geste = un cran. Le sens des axes est
  corrigé par `LEFT_STICK_X_SIGN`.
- Le ByteButton n'est pas utilisé dans le menu.
- **OK sur un réglage** passe à l'entrée suivante : avec le seul bouton OK, on parcourt tout le
  menu, ce qui reste utilisable si un stick ne répond pas.
- **Retour** abandonne les modifications et place le curseur sur **Start**.
- **Auto-répétition** : stick maintenu à gauche ou à droite sur le canal seulement (38 valeurs
  en 920). Sur les autres entrées, un geste = un changement.
- **Inactivité** : sans action pendant 60 s, le joystick démarre sans rien enregistrer. Un
  bouton A tenu par erreur ne bloque donc pas le démarrage.

## 6. Arborescence et réglages

```
SETUP
├─ Link          : ESP-NOW / LoRa
├─ LoRa band     : 433 / 920 / 868        (si LoRa)
├─ LoRa power    : valeurs de la bande     (si LoRa)
├─ LoRa channel  : canaux de la bande      (si LoRa)
├─ ESP-NOW listen: ON / OFF                (si LoRa)
├─ Save & start  → enregistre, puis démarre avec la nouvelle configuration
└─ Start         → démarre sans enregistrer les modifications
```

- **Brouillon** : comme sur la bouée, les réglages modifient une copie. Seul **Save & start**
  l'enregistre.
- **Entrées conditionnelles** : les lignes LoRa sont masquées en ESP-NOW.
- **Rappel affiché avant Save & start** : bande, canal et débit air doivent être identiques à
  ceux de la bouée, sinon la liaison est perdue sans aucun message. La puissance peut différer.

## 7. Affichage

### 7.1 Core2 — liste

```
┌──────────────────────────────────────┐
│ SETUP                    v1.2.2      │
│                                      │
│   Link          LoRa                 │
│ ▶ LoRa band     433 MHz      ◄ ►     │
│   LoRa power    22 dBm               │
│   LoRa channel  24  (434.125 MHz)    │
│   ESP-NOW listen OFF                 │
│   Save & start                       │
│                                      │
│ ▲▼ move  ◄► change  BLUE ok  RED back│
└──────────────────────────────────────┘
```

Font2 ou Font4 selon la ligne, l'entrée sélectionnée sur fond clair. Tout tient sur un écran ;
la liste défilera si l'on ajoute des entrées.

### 7.2 AtomS3 — une entrée par écran

Sur 128 × 128, une liste ne laisserait que des libellés tronqués en petite police. L'AtomS3
affiche donc **une seule entrée à la fois**, en gros caractères :

```
┌────────────────┐
│ SETUP     3/6  │
│                │
│   LoRa power   │
│                │
│  ◄  22 dBm  ►  │
│                │
│ ▲▼ next  Y ok  │
└────────────────┘
```

- ▲ / ▼ passe à l'entrée précédente ou suivante ; le compteur `3/6` situe l'entrée dans la liste.
- ◄ / ► change la valeur directement, sans ouvrir de sous-page.
- Les entrées masquées sont sautées, ce qui met le compteur à jour.

Même moteur, même ordre d'entrées, mêmes actions que sur le Core2 : seul le rendu change. Ce
rendu s'appuie sur `drawTextField()`, qui redessine sans clignoter.

## 8. Préférences

### 8.1 Stockage

- Nouvelle classe `JoystickPreferences`, espace NVS `joy_cfg`, organisée autour de la même **table
  de descripteurs** que la bouée (`clé`, `type`, `défaut`, `validation`).
- Clés, alignées sur celles de la bouée quand le sens est le même :

  | Clé | Contenu | Défaut si absente |
  |---|---|---|
  | `commMode` | ESP-NOW / LoRa | déduit de `DEFAULT_COMM_MODE` |
  | `loraBand` | 433 / 920 (868 dès que son profil sera confirmé) | déduit de `DEFAULT_COMM_MODE` |
  | `loraPwr` | puissance en dBm | réglage actuel de la bande |
  | `loraChan` | canal | canal actuel de la bande |
  | `espnPass` | écoute ESP-NOW en LoRa | `DEFAULT_ESPNOW_PASSIVE` |
  | `prefsVer` | version du schéma | `1` |

- Les constantes actuelles deviennent les valeurs par défaut : un joystick qui démarre sans
  préférences se comporte comme en 1.2.2.

### 8.2 Profils de bande partagés

La table `LoRaBandProfile` (fréquence, canaux proposés, correspondance dBm ↔ registre) est la
**même** que sur la bouée. Elle vit dans un en-tête recopié tel quel entre les dépôts, par
exemple `LoRaBandProfiles.h`, sur le modèle de `LoRaProtocol.h`. Les règles
de validation sont celles de la bouée : valeur ramenée à la plus proche autorisée au changement
de bande, défaut si la valeur relue est invalide.

### 8.3 Après un flash M5Burner

M5Burner efface la NVS. Sans carte SD (absente sur l'AtomS3), le joystick repart sur les valeurs
par défaut, celles de la compilation. L'affichage de la configuration au démarrage (§ 4) rend ce
retour visible. Une sauvegarde sur SD pour le Core2, comme sur la bouée, est possible plus tard.

## 9. Application au démarrage

L'ordre de `setup()` devient :

1. `M5.begin()`, Logger.
2. Lecture des préférences (`JoystickPreferences::load()`).
3. Si le bouton A est maintenu : menu de configuration, qui bloque jusqu'à Save & start ou Start.
4. Initialisation de la communication **à partir de la configuration chargée**, et non plus de
   `COMM_MODE` :
   - l'instance globale `LoRaCommunication lora(LORA_BAND, LORA_AIR_RATE)` est construite ou
     configurée avec la bande, le canal et la puissance lus ;
   - `getTxPower()` et le canal passent par le profil de bande ;
   - les tests `COMM_MODE == …` et `CommunicationConfig::isLoRa(COMM_MODE)` de `main.cpp`
     interrogent la configuration chargée ;
   - `JOYSTICK_ESPNOW_PASSIVE` devient une valeur lue au démarrage.
5. Suite du démarrage inchangée.

## 10. Hors périmètre, prévu par la structure

- **Débit air** dans les préférences : une ligne de table et une entrée de menu, comme sur la
  bouée.
- **Commandes de maintenance à distance** : le joystick pourrait envoyer `CMD_MOTORS_TEST` à la
  bouée sélectionnée, déjà décodée par la bouée et acceptée en `MAINTENANCE` seulement. Il faudrait
  lui trouver une entrée en fonctionnement, ce qui sort de ce menu de démarrage.
- **Sauvegarde SD des préférences** sur le Core2.

## 11. Fichiers concernés

| Fichier | Évolution |
|---|---|
| `include/JoystickPreferences.h`, `src/JoystickPreferences.cpp` (nouveaux) | table des préférences, lecture et écriture NVS |
| `include/SetupMenu.h`, `src/SetupMenu.cpp` (nouveaux) | moteur de menu commun : entrées, brouillon, navigation |
| `include/LoRaBandProfiles.h` (nouveau, partagé avec la bouée) | profils de bande |
| `include/DisplayManager.h`, `src/DisplayManagerV1.cpp`, `src/DisplayManagerV2.cpp` | rendu du menu : une entrée par écran (AtomS3), liste (Core2) |
| `include/HardwareConfig.h` | `BTN_MENU_OK`, `BTN_MENU_BACK` par matériel |
| `include/JoystickConfiguration.h` | constantes de communication devenues valeurs par défaut |
| `src/main.cpp`, `src/LoRaCommunication.cpp` | ordre du démarrage, configuration radio lue dans les préférences |

## 12. Plan de validation

1. **Compilation** des deux environnements : `pio run -e joystick-v2-core2` et
   `pio run -e joystick-v1-atoms3`.
2. **Compatibilité** : sans préférences en NVS, le log de démarrage montre la même configuration
   qu'en 1.2.2 (LoRa 433, canal 24, 22 dBm, ESP-NOW coupé).
3. **Menu**, sur chaque matériel :
   - démarrer avec le bouton A maintenu ;
   - parcourir toutes les entrées, vérifier le masquage des lignes LoRa en ESP-NOW ;
   - Start doit démarrer sans rien enregistrer, Save & start avec la nouvelle configuration.
4. **Persistance** : après Save & start puis coupure d'alimentation, le démarrage suivant affiche
   et applique la configuration enregistrée.
5. **Liaison** : joystick et bouée réglés sur le même canal, une commande passe ; sur des canaux
   différents, plus aucune commande ne passe.
