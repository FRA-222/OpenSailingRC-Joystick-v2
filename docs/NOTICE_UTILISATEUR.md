# 📘 Notice utilisateur — Buoy-Joystick

**Firmware joystick** : v1.1.0 · **Cible** : bouées OpenSailingRC (Autonomous GPS Buoy)
**Liaison** : ESP-NOW (par défaut) ou LoRa 920 MHz

Cette notice décrit, pour chaque action sur le boîtier, la commande envoyée à la bouée et
l'effet attendu sur l'automate d'états de la bouée (« Mode général » : Init → Ready →
Home Definition → Nav, plus l'état Maintenance).

---

## 1. Description du boîtier

```
┌──────────────────────────────────────────────────────────────────────┐
│                     M5Stack Core2 + 2 Unit Joystick                  │
│                                                                      │
│    🔴 Bouton ROUGE GAUCHE                    🔵 Bouton BLEU DROIT    │
│      Home Validation Cmde                        Memo Home Cmde      │
│                                                                      │
│   ┌────────────────────┐              ┌────────────────────┐        │
│   │  JOYSTICK GAUCHE   │              │  JOYSTICK DROIT    │        │
│   │   (modes de nav)   │              │  (pilotage fin)    │        │
│   │                    │              │                    │        │
│   │      ⬆ NAV_CAP     │              │   ⬆ Throttle +     │        │
│   │  ⬅ Maintenance     │              │ ⬅ Cap −    Cap + ➡ │        │
│   │      Resume Ready ➡│              │   ⬇ Throttle −     │        │
│   │      ⬇ NAV_HOME    │              │                    │        │
│   │   🔘 NAV_HOLD      │              │   🔘 NAV_STOP      │        │
│   └────────────────────┘              └────────────────────┘        │
│                                                                      │
│              ┌────────────────────────────────────┐                  │
│              │   Écran LCD 320x240 (état bouée)   │                  │
│              └────────────────────────────────────┘                  │
│                        [ Bouton A ] = bouée suivante                 │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

**Anti-rebond joysticks** : une commande n'est émise qu'une fois par mouvement. Il faut
ramener le manche au centre avant de pouvoir renvoyer la même commande (seuil de
détection ≈ 73 % de la course, retour au centre à ≈ 37 %).

**Heartbeat** : le joystick envoie automatiquement un « battement de cœur » toutes les
3 s à la bouée sélectionnée. C'est ce qui maintient le Datalink (DL) de la bouée en état
NORMAL. Si le joystick est éteint ou hors de portée, la bouée perd son DL et bascule
d'elle-même (voir § 6).

---

## 2. Mise en route et sélection de la bouée

| Étape | Action | Effet |
|-------|--------|-------|
| 1 | Mettre le joystick sous tension | Écran de démarrage puis **diagnostic matériel** (bus I2C, joysticks, ByteButton, Dual Button) pendant ~4 s |
|
| 2 | Mettre les bouées sous tension | Elles sont **découvertes automatiquement** à la réception de leur broadcast (aucune adresse MAC à saisir) |
|
| 3 | **Bouton A du Core2** | Passe à la bouée suivante (alternative au ByteButton) |

> ⚠️ Toutes les commandes décrites ci-dessous sont envoyées **uniquement à la bouée
> sélectionnée**. Vérifiez le nom affiché en haut de l'écran
> avant toute manœuvre.

L'écran affiche en permanence pour la bouée sélectionnée : voyants GPS / MAG / YAW,
température, batterie, distance à la consigne, cap commandé, throttle commandé, ainsi que
le **mode général** et le **mode de navigation** en cours.

---

## 3. Phase d'initialisation

### 3.1 Ce que fait la bouée toute seule : Init → Ready

Au démarrage, la bouée est en **« Init »**. Elle y reste tant que l'un des éléments
suivants est invalide : Datalink (DL), position GPS (Locate), magnétomètre (Magnet),
vitesse de lacet (yawRate). Aucune commande du joystick ne peut forcer ce passage.

- **Init → Ready** : automatique dès que **DL et Locate et Magnet et yawRate sont OK**.
- Le DL devient OK grâce au heartbeat du joystick : **allumez le joystick avant d'attendre
  le passage en Ready**.
- Sur l'écran : `INIT` puis `READY`.

### 3.2 Les deux boutons du Dual Button

| Action | Commande envoyée | Nom dans l'automate bouée | Effet |
|--------|------------------|---------------------------|-------|
| 🔵 **Bouton bleu DROIT** | `CMD_INIT_HOME` | **Memo Home Cmde** | La bouée mémorise **sa position GPS courante** comme point Home et passe en **« Home Definition »** |
| 🔴 **Bouton rouge GAUCHE** | `CMD_HOME_VALIDATION` | **Home Validation Cmde** | La bouée **verrouille** le Home et passe en **« Nav »** (démarrage en NAV_STOP) |

**Détail des transitions :**

- `Ready` **→** `Home Definition` : bouton bleu DROIT, **si la position GPS est valide
  (Locate OK)**. Si le GPS n'est pas fixé, rien ne se passe.
- `Home Definition` **→** `Home Definition` (boucle) : bouton bleu DROIT de nouveau —
  cela **redéfinit** le Home sur la position courante. Utile pour corriger un placement.
- `Home Definition` **→** `Nav` : bouton rouge GAUCHE, **si un Home valide a été mémorisé**.
  Le Home est alors verrouillé (`lockedHome`) pour toute la session de navigation.

> ℹ️ La transition *« Pos Home Cmde »* du schéma (Home défini par des coordonnées
> transmises) et la définition d'un point de dégagement (*Degt*) ne sont **pas**
> accessibles depuis le joystick : elles proviennent du dashboard/GCS.

### 3.3 Procédure type de mise en œuvre

```
1. Joystick sous tension → attendre "SYSTEME PRET"
2. Sélectionner la bouée (appui sur écran)
3. Attendre l'affichage READY (GPS/MAG/YAW verts)
4. Placer physiquement la bouée à l'emplacement voulu du point Home
5. 🔵 Bouton bleu DROIT    → écran : HOME_DEFINITION
   (répétable autant que nécessaire tant que le placement n'est pas bon)
6. 🔴 Bouton rouge GAUCHE  → écran : NAV / NAV_STOP
   → la bouée est prête à naviguer
```

---

## 4. Mode Maintenance (entrée et sortie)

Le mode **« Maintenance »** sert aux opérations de calibration magnétique et de
configuration (type de batteries). La bouée y entre aussi **d'elle-même** si elle n'est
pas calibrée ou si le type de batteries n'est pas renseigné.

| Action | Commande envoyée | Nom dans l'automate bouée | Effet |
|--------|------------------|---------------------------|-------|
| 🕹️ **Joystick GAUCHE ⬅ vers la GAUCHE** | `CMD_MAINTENANCE_ENTER` | **Maintenance Cmde** | Passage en **« Maintenance »** depuis `Ready` **ou** depuis `Home Definition` |
| 🕹️ **Joystick GAUCHE ➡ vers la DROITE** | `CMD_MAINTENANCE_EXIT` | **Resume Ready Cmde** | Retour en **« Ready »** |

**Points importants :**

- L'entrée en maintenance **efface le Home** (`typeHome = NO_HOME`) : après une sortie de
  maintenance, il faut **refaire toute la phase d'initialisation du Home** (§ 3.2).
- La sortie de maintenance n'est acceptée que si la **calibration magnétique** est faite
  **et** que le **type de batteries** est renseigné. Sinon la bouée reste en Maintenance
  malgré la commande.
- Ces commandes ne sont **pas** disponibles depuis l'état `Nav` : la bouée en navigation
  ne peut pas être mise en maintenance à distance.

```
Ready ──── JS gauche ⬅ ────► Maintenance ──── JS gauche ➡ ────► Ready
Home Definition ── JS gauche ⬅ ──► Maintenance
```

---

## 5. Commandes en Navigation (état « Nav »)

À l'entrée en `Nav`, la bouée démarre systématiquement en **NAV_STOP** (moteurs arrêtés).

### 5.1 Joystick GAUCHE — choix du mode de navigation

| Action | Commande envoyée | Mode de nav résultant | Description |
|--------|------------------|-----------------------|-------------|
| ⬆ **Haut** | `CMD_NAV_CAP` | **NAV_CAP** | Tenue d'un cap consigne + gaz commandés manuellement |
| ⬇ **Bas** | `CMD_NAV_HOME` | **NAV_HOME** | Retour automatique au point Home mémorisé |
| 🔘 **Appui sur le manche** | `CMD_NAV_HOLD` | **NAV_HOLD** | Maintien de la position courante (la bouée « tient » son point) |

### 5.2 Joystick DROIT — pilotage fin

| Action | Commande envoyée | Effet en **NAV_CAP** | Effet en **NAV_HOLD** |
|--------|------------------|----------------------|-----------------------|
| ⬆ **Haut** | `CMD_THROTTLE_INCREASE` | Throttle **+50 %** (borné à +100 %) | Déplace le point tenu de **2 m vers l'avant** |
| ⬇ **Bas** | `CMD_THROTTLE_DECREASE` | Throttle **−50 %** (borné à −100 %) | Déplace le point tenu de **2 m vers l'arrière** |
| ➡ **Droite** | `CMD_HEADING_INCREASE` | Cap consigne **+30°** | Déplace le point tenu de **2 m vers la droite** |
| ⬅ **Gauche** | `CMD_HEADING_DECREASE` | Cap consigne **−30°** | Déplace le point tenu de **2 m vers la gauche** |
| 🔘 **Appui sur le manche** | `CMD_NAV_STOP` | **NAV_STOP** — arrêt immédiat | **NAV_STOP** — arrêt immédiat |

> Les incréments sont décidés **par la bouée**, pas par le joystick : le joystick n'envoie
> qu'un ordre « augmente » / « diminue ». Les déplacements de 2 m en NAV_HOLD sont exprimés
> dans le repère local de la bouée.
>
> En **NAV_HOME** et **NAV_STOP**, les mouvements du joystick droit n'ont pas d'effet sur
> la trajectoire (la consigne est calculée par la bouée).

### 5.3 Transitions autorisées entre modes de navigation

Depuis **NAV_STOP**, on peut aller vers NAV_HOME, NAV_HOLD ou NAV_CAP (à condition que
GPS, cap et vitesse de lacet soient valides).
Depuis **NAV_CAP** ou **NAV_HOLD**, on peut aller vers NAV_STOP (priorité maximale),
NAV_HOME, ou NAV_HOLD.

`CMD_NAV_STOP` (🔘 manche droit) est **toujours prioritaire** : c'est l'arrêt d'urgence.

### 5.4 Séquence type de navigation

```
NAV_STOP (état initial en Nav)
  │
  ├─ 🕹️ gauche ⬆   → NAV_CAP
  │     ├─ 🕹️ droit ⬆ ×1  → gaz 50 %
  │     ├─ 🕹️ droit ➡ ×3  → cap +90° (virage à droite)
  │     └─ 🕹️ droit ⬇ ×1  → gaz 0 %
  │
  ├─ 🕹️ gauche 🔘  → NAV_HOLD (la bouée tient sa position)
  │     └─ 🕹️ droit ⬅/➡/⬆/⬇ → recale le point tenu par pas de 2 m
  │
  ├─ 🕹️ gauche ⬇   → NAV_HOME (retour au point Home)
  │
  └─ 🕹️ droit 🔘   → NAV_STOP (arrêt d'urgence, à tout moment)
```

---

## 6. Comportements automatiques de sécurité

Ces transitions se produisent **sans action du joystick** ; il est important de les
connaître pour ne pas les confondre avec un dysfonctionnement.

| Situation | Comportement de la bouée |
|-----------|--------------------------|
| **Perte totale du Datalink** (joystick éteint / hors portée) en Nav | Passage forcé en **NAV_HOME** si GPS + cap + lacet OK, sinon **NAV_STOP** |
| **Perte partielle du Datalink** en NAV_CAP | Passage en **NAV_HOLD** sur la position courante |
| **Perte de capteur** (GPS, boussole, lacet) | Passage en **NAV_STOP**, ou **NAV_BASIC** sur perte boussole/lacet en NAV_CAP |
| **Durée max d'utilisation atteinte** | Passage forcé en **NAV_HOME** |
| **Perte de Datalink** en Ready / Home Definition / Maintenance | Maintien dans l'état courant, **Home effacé** en Ready |
| Bouée non calibrée ou sans type de batteries en Ready | Passage automatique en **Maintenance** |

---

## 7. Récapitulatif complet des commandes

| Contrôle | Action | Commande | Nom automate bouée | Depuis l'état | Vers l'état |
|----------|--------|----------|--------------------|---------------|-------------|
| 🔵 Bouton bleu droit | Appui | `CMD_INIT_HOME` | Memo Home Cmde | Ready / Home Definition | Home Definition |
| 🔴 Bouton rouge gauche | Appui | `CMD_HOME_VALIDATION` | Home Validation Cmde | Home Definition | Nav |
| 🕹️ Gauche | ⬅ Gauche | `CMD_MAINTENANCE_ENTER` | Maintenance Cmde | Ready / Home Definition | Maintenance |
| 🕹️ Gauche | ➡ Droite | `CMD_MAINTENANCE_EXIT` | Resume Ready Cmde | Maintenance | Ready |
| 🕹️ Gauche | ⬆ Haut | `CMD_NAV_CAP` | — | Nav | NAV_CAP |
| 🕹️ Gauche | ⬇ Bas | `CMD_NAV_HOME` | — | Nav | NAV_HOME |
| 🕹️ Gauche | 🔘 Appui | `CMD_NAV_HOLD` | — | Nav | NAV_HOLD |
| 🕹️ Droit | ⬆ Haut | `CMD_THROTTLE_INCREASE` | — | Nav | gaz +50 % / Hold +2 m |
| 🕹️ Droit | ⬇ Bas | `CMD_THROTTLE_DECREASE` | — | Nav | gaz −50 % / Hold −2 m |
| 🕹️ Droit | ➡ Droite | `CMD_HEADING_INCREASE` | — | Nav | cap +30° / Hold +2 m |
| 🕹️ Droit | ⬅ Gauche | `CMD_HEADING_DECREASE` | — | Nav | cap −30° / Hold −2 m |
| 🕹️ Droit | 🔘 Appui | `CMD_NAV_STOP` | — | Nav | NAV_STOP |
| 🔢 ByteButton | Touche _i_ | — (local) | — | — | Sélection bouée #i |
| 🔘 Bouton A Core2 | Appui | — (local) | — | — | Bouée suivante |
| *(automatique)* | toutes les 3 s | `CMD_HEARTBEAT` | Datalink OK | — | Maintien du DL |

---
---

## 8. Dépannage rapide

| Symptôme | Cause probable | Action |
|----------|----------------|--------|
| La bouée reste en `INIT` | GPS non fixé, magnétomètre ou gyro KO, ou DL absent | Vérifier les voyants GPS/MAG/YAW ; attendre le fix GPS ; vérifier que le joystick est allumé |
| Le bouton bleu droit ne fait rien | GPS non valide (Locate KO), ou bouée en Maintenance | Attendre le fix GPS ; sortir de Maintenance (🕹️ gauche ➡) |
| Le bouton rouge gauche ne fait rien | Aucun Home mémorisé | Appuyer d'abord sur le bouton bleu droit |
| La bouée ne sort pas de Maintenance | Calibration magnétique absente ou type de batteries non renseigné | Effectuer la calibration / la configuration via le dashboard |
| Le Home a disparu après une manœuvre | Passage par Maintenance ou perte de DL en Ready | Refaire la phase § 3.2 |
| La bouée part seule vers le Home | Perte totale du Datalink | Se rapprocher, vérifier l'alimentation du joystick |
| Commande envoyée plusieurs fois | Manche maintenu en butée | Ramener le manche au centre entre deux commandes |
| `VERIFIER MODULES` au démarrage | Un joystick I2C ne répond pas | Vérifier les connecteurs Port A (JS gauche) et Port C (JS droit + ByteButton) |

---

**Auteur** : Philippe Hubert · **Licence** : GPL-3.0-only
**Voir aussi** : [ARCHITECTURE.md](../ARCHITECTURE.md) · [API_DOCUMENTATION.md](../API_DOCUMENTATION.md)
