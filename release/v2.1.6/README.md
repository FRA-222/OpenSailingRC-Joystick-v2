# Release 2.1.6 — Joystick v2

Version **gelée** du 6 septembre 2026. Corrige une anomalie qui rendait le firmware
définitivement muet, aligne le débit air, et remplace l'instrument de bilan de liaison.

## 🔴 Le firmware devenait définitivement muet

`LoggerScope` sauvegarde puis restaure un état **global** du Logger, et il est utilisé
depuis **deux tâches** : `listenForResponses()` dans `loraRxTask` sur le Core 0,
`sendCommand()` dans la boucle principale sur le Core 1. Sans exclusion mutuelle, les
paires s'entrelaçaient :

1. boucle principale : sauve « série = ACTIVE », met INACTIVE
2. tâche RX : sauve « série = **INACTIVE** » — la valeur que l'autre vient de poser
3. boucle principale : restaure ACTIVE
4. tâche RX : restaure **INACTIVE** → plus rien ne s'affiche, définitivement

Le système continuait de fonctionner normalement, il devenait seulement aveugle — donc
impossible à diagnostiquer sur le terrain. La portée prend maintenant un mutex, avec un
ordre de verrouillage constant (`LoggerScope` avant `loraMutex`) et un délai de 5 ms
au-delà duquel elle renonce à modifier la verbosité plutôt que de corrompre l'état.

## ACK concaténés perdus, et mutex relâché avant traitement

Identiques au joystick v1 — voir sa note de version. Le second provoquait des
`Echec envoi commande` sur des commandes de sécurité.

## Instrument de bilan de liaison

- **Taux de réception** et **séries de fenêtres vides** remplacent la colonne « marge »,
  qui indiquait encore 35 à 42 dB au moment où la liaison décrochait. Le RSSI paquet du
  E220 plafonne au niveau de bruit dès que le SNR devient négatif, et le module n'expose
  pas le SNR : la marge affichée était un artefact.
- **Statistiques rattachées à une bouée** : changer de bouée clôture le palier, imprime son
  bilan (fenêtres, taux, pire série) et remet les compteurs à zéro. Indispensable aux
  campagnes multi-bouées.
- Débit air porté à **9,6 kbps**.

## ⚠️ Réglage à vérifier avant diffusion

`JOYSTICK_ESPNOW_PASSIVE` vaut **0** dans ce binaire : la radio 2,4 GHz est **coupée** et
les broadcasts d'état ESP-NOW des bouées ne sont plus reçus — seul le LoRa alimente
l'affichage.

C'est le réglage des essais de portée, et celui sur lequel les tests de non-régression ont
été passés. Pour retrouver l'écoute passive ESP-NOW, repasser cette constante à **1** dans
`src/main.cpp` et reconstruire.

Les deux instruments de diagnostic sont désactivés : `LORA_NOISE_DIAG` = 0 et
`LINK_POLL_NOISE` = 0.

## Réglages radio figés dans ce binaire

| Paramètre | Valeur |
|---|---|
| Bande / canal | 433,125 MHz — canal 23 |
| Débit air | **9,6 kbps** (SF7/BW125), REG0 `0b100` |
| Puissance | registre `0b11` = 10 dBm (limite ISM 433 européenne) |
| UART module | 9600 bauds |

## ⚠️ Mise en service — le reflashage ne suffit pas

Le E220 conserve sa configuration **dans sa propre mémoire**. Flasher ce binaire ne change
ni le débit air, ni le canal, ni la puissance : il faut les réécrire une fois.

1. Flasher le binaire fusionné à l'adresse **0x0**.
2. Switch M0/M1 sur **ON**, mettre sous tension. Vérifier dans la trace :
   `MODULE EN MODE CONFIGURATION` **puis** `Module E220-JP configured successfully!`
3. **Couper l'alimentation.** Un reset ne suffit pas — le firmware ne coupe jamais
   l'alimentation du module.
4. Switch M0/M1 sur **OFF**, remettre sous tension. Vérifier :
   `MODULE EN MODE NORMAL — liaison radio active.`
5. **Vérifier qu'une commande passe réellement.** C'est la seule preuve que les
   équipements sont au même débit.

À l'étape 4, le message `configuration non ecrite a ce demarrage` est **normal** : avec le
switch sur OFF l'écriture échoue toujours, et le module utilise ce qui a été écrit à
l'étape 2.

> Un débit air discordant entre deux équipements coupe la liaison **sans aucun message
> d'erreur**. C'est le mode de panne le plus coûteux de cet écosystème.

## Contenu

| Fichier | Usage |
|---|---|
| `OpenSailingRC_Joystick_v2.1.6_Core2_MERGED.bin` | Binaire complet — bootloader + partitions + application, à flasher à l'adresse **0x0** |

Paramètres flash : DIO, 40 MHz, 16 MB. Bootloader à 0x1000 dans l'image (Core2).

```
sha256  5dfca9419d00ce6a3969edcb32df957b6657808df190808945c0803a61220d42
taille  1046032 octets
```

> **M5Burner flashe toujours à l'adresse 0x0** : c'est ce qui impose le binaire fusionné.
> Les préférences NVS étant écrasées, la sauvegarde SD introduite en 1.1.0 les restaure au
> démarrage suivant.
