# Release 1.2.2 — Joystick OpenSailingRC (Core2 / AtomS3)

🇫🇷 Version française ci-dessous &nbsp;·&nbsp; 🇬🇧 English version at the end of this document

<br>

---

# 🇫🇷 Version française

Version diffusée le 11 novembre 2026 à la communauté OpenSailingRC, avec la bouée 1.2.2.

Le joystick et le contrôleur sont désormais construits avec le même logiciel et portent le même numéro de version que la bouée.

<br>

## ⚠️ À lire avant de flasher

- **Mettre à jour tout l'écosystème en même temps.** Les trames radio ont changé : un joystick 1.2.2 ne dialogue pas correctement avec une bouée d'une version antérieure, et l'échec est **silencieux**. Flasher ensemble bouée 1.2.2 et joystick 1.2.2.

- **Le module E220 doit être reconfiguré** : voir la section « Mise en service » ci-dessous.

<br>

## Nouveautés depuis 2.1.6

- **Protocole LoRa v2**, aligné sur la bouée 1.2.2 : le protocole est identique à celui de la future Station sol.

- **RSSI descendant** : force du signal du joystick telle que la bouée la reçoit, avec un code couleur : vert ≥ −90 dBm, orange jusqu'à −105 dBm, rouge en dessous.
  - Core2 : dans le badge en haut à droite (`LoRa : -95 dB`).
  - AtomS3 : au centre de la 3e ligne, entre température et batterie.

- **Distance au Home** (Core2) : au centre de la ligne température / batterie, `H 125m`, `H 1.2k`, ou `H --` tant que la bouée n'a pas de GPS ou pas de Home.

- **Correction (Core2) — entrée et sortie de maintenance inversées** : stick gauche **à gauche** = entrée en `MAINTENANCE`, **à droite** = retour en `READY`. L'axe X du stick gauche était lu à l'envers.

- Augmentation de la vitesse de communication entre le module LoRa et le Core2 à **115 200 bauds**.

<br>

## ⚠️ Mise en service — le reflashage ne suffit pas

Le E220 conserve sa configuration **dans sa propre mémoire**. Flasher ce binaire ne change ni le canal, ni la puissance, ni le débit UART : il faut les réécrire une fois, sur **chaque** module (bouée, joystick, passerelle).

1. Flasher le binaire.
2. Switch M0/M1 sur **OFF**, mettre sous tension.
3. **Couper l'alimentation.** Un reset ne suffit pas.
4. Switch M0/M1 sur **ON**, remettre sous tension.
5. **Vérifier qu'une commande passe réellement.** C'est la seule preuve que les équipements sont sur le même canal et au même débit.

<br>
<br>

---

# 🇬🇧 English version

Released on November 11, 2026 to the OpenSailingRC community, together with buoy firmware 1.2.2.

The joystick and the controller are now built from the same software and share the buoy's version number.

<br>

## ⚠️ Read before flashing

- **Update the whole ecosystem at once.** The radio frames have changed: a 1.2.2 joystick does not communicate properly with a buoy running an older version, and the failure is **silent**. Flash buoy 1.2.2 and joystick 1.2.2 together.

- **The E220 module must be reconfigured**: see the "Setup" section below.

<br>

## What's new since 2.1.6

- **LoRa protocol v2**, aligned with buoy 1.2.2: the protocol is identical to the one of the upcoming Ground Station.

- **Downlink RSSI**: strength of the joystick signal as received by the buoy, color-coded: green ≥ −90 dBm, orange down to −105 dBm, red below.
  - Core2: in the badge at the top right (`LoRa : -95 dB`).
  - AtomS3: in the middle of the 3rd line, between temperature and battery.

- **Distance to Home** (Core2): in the middle of the temperature / battery line, `H 125m`, `H 1.2k`, or `H --` as long as the buoy has no GPS fix or no Home.

- **Fix (Core2) — maintenance entry and exit were swapped**: left stick **to the left** = enter `MAINTENANCE`, **to the right** = back to `READY`. The X axis of the left stick was read the wrong way round.

- Communication speed between the LoRa module and the Core2 increased to **115,200 baud**.

<br>

## ⚠️ Setup — reflashing is not enough

The E220 keeps its configuration **in its own memory**. Flashing this binary changes neither the channel, the power nor the UART speed: they must be written once, on **each** module (buoy, joystick, gateway).

1. Flash the binary.
2. Set the M0/M1 switch to **OFF**, power up.
3. **Cut the power.** A reset is not enough.
4. Set the M0/M1 switch to **ON**, power up again.
5. **Check that a command actually gets through.** It is the only proof that the devices are on the same channel and at the same speed.
