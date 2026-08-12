# Release 2.0.0 — OpenSailingRC Joystick v2

## Contenu

- `OpenSailingRC_Joystick_v2.0.0_Core2_MERGED.bin` — binaire complet (bootloader + table de
  partitions + boot_app0 + application), à flasher à l'adresse **0x0**. Cible M5Stack Core2,
  paramètres flash : DIO, 40 MHz, 16 MB. Taille : 1 041 264 octets.

## ⚠️ Configuration figée dans ce binaire

Ces constantes sont choisies à la compilation (`src/main.cpp`) et **ne sont pas modifiables
après flashage** :

| Constante | Valeur dans ce binaire |
|---|---|
| `COMM_MODE` | `CommMode::LORA_433` |
| `LORA_BAND` | `BAND_433` — module E220-400T22S, canal 23, 433.125 MHz |
| `JOYSTICK_FIRMWARE_VERSION` | `2.0.0` |

**La bouée doit être réglée sur la même bande (433 MHz)**, sinon la liaison est
silencieusement inexistante — aucun message d'erreur, simplement aucune trame reçue. Pour un
joystick équipé du module 920 MHz, recompiler avec `#define COMM_MODE CommMode::LORA_920`
(canal 0x00, 920.600 MHz). Le mode `CommMode::ESP_NOW` reste disponible par la même
constante.

## Nouveautés 2.0.0

- **Support bi-bande LoRa** : le firmware pilote au choix un E220-900T22S(JP) à 920 MHz ou un
  E220-400T22S à 433 MHz, sélectionné par `COMM_MODE`. Le protocole est commun aux deux
  bandes ; seules la configuration radio (canal, débit air, puissance) en dépend. Le module se
  remplace sur le même port, sans changement de brochage ni d'UART.
- **Découverte automatique des bouées** via leurs broadcasts — plus d'adresses MAC à
  configurer manuellement.
- **Affichage LCD sans clignotement** : rafraîchissement optimisé par champs de texte au lieu
  d'un redessin complet, et tailles de texte fractionnaires.
- **En-tête indiquant la provenance de la télémétrie** (source de données) et **niveau de
  batterie du joystick** affiché.
- **Commandes MAINTENANCE_ENTER / MAINTENANCE_EXIT** ajoutées, états de bouée mis à jour.
- **Système de log centralisé** (`Logger`).
- **Correction** : affichage de la température au format flottant.
- Notice utilisateur ajoutée au dépôt (`docs/`).

## Premier démarrage après changement de bande

Le module E220 mémorise sa configuration radio. Après un changement de bande :

1. Placer le switch M0/M1 du module sur **ON** (mode configuration) et démarrer le joystick.
2. Vérifier dans les traces le canal et la fréquence attendus, ainsi que la confirmation de
   configuration du module.
3. Repasser le switch sur **OFF** et redémarrer. Un module laissé en mode configuration
   n'émet pas.

En mode configuration, le module force son UART à 9600 8N1 quelle que soit sa configuration
enregistrée : un module déjà mal configuré reste donc reprogrammable.

## Installation

### M5Burner

M5Burner flashe toujours à l'adresse 0x0 — le binaire MERGED convient tel quel et efface la
zone NVS.

### PlatformIO / esptool

```bash
# Mise à jour application seule (ne touche pas la NVS)
platformio run --environment m5stack-core2 --target upload

# Installation complète équivalente à M5Burner
esptool.py --chip esp32 write_flash 0x0 OpenSailingRC_Joystick_v2.0.0_Core2_MERGED.bin
```

## Reproduire ce binaire

```bash
platformio run --environment m5stack-core2

esptool.py --chip esp32 merge_bin \
  -o OpenSailingRC_Joystick_v2.0.0_Core2_MERGED.bin \
  --flash_mode dio --flash_freq 40m --flash_size 16MB \
  0x1000  .pio/build/m5stack-core2/bootloader.bin \
  0x8000  .pio/build/m5stack-core2/partitions.bin \
  0xe000  ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/m5stack-core2/firmware.bin
```
