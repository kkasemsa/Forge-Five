# Clipper 1.1 : l'implémentation MIDI

Clipper est fait pour être joué depuis un clavier MIDI, un contrôleur, un séquenceur ou un DAW autant que depuis ses propres touches. Voici tout ce qu'il reçoit et tout ce qu'il envoie, en USB comme sur le jack TRS. *(In English: [midi.md](midi.md).)*

## Les réglages (GLO > MIDI, GLO > MIDI 2)

Ce sont des réglages de la FM-1 : ils restent après extinction, quel que soit le son chargé.

| Page | Knob | Valeurs | Rôle |
|---|---|---|---|
| MIDI | **IN CH** | OMNI, 1..16 | Le canal que Clipper écoute. OMNI (par défaut) : tous les canaux. Avec le MPE, tous les canaux ; en MULTI, le layer garde son propre canal (EDIT > LAYER > CH). |
| MIDI | **OUT** | 1..16 | Le canal sur lequel partent les touches, l'arpégiateur, le séquenceur et les knobs (CC OUT). |
| MIDI | **BEND** | AUTO, 1..24 | L'amplitude du pitch bend en demi-tons. AUTO : celle du patch HYBRID, ±2 sur les autres moteurs. Le RPN 0 la règle aussi. |
| MIDI | **LOCAL** | ON, OFF | OFF : les touches partent seulement en MIDI, le son joue ce qui arrive (un DAW qui renvoie la piste à Clipper : pas de notes doublées). Le CC 122 le règle aussi. |
| MIDI 2 | **PROG** | ON, OFF | Les program changes chargent les sons (ON, par défaut) ou sont ignorés. |
| MIDI 2 | **SEND** | OFF, CC | CC : tourner un knob envoie le CC qui le règle (les CC ci-dessous), donc un DAW enregistre les mouvements et les rejoue. OFF (par défaut) : rien n'est envoyé. |
| MIDI 2 | **EWI** | OFF, VOL, V+AT | Pour un contrôleur de souffle (un EWI, un TEC, un contrôleur de souffle avec un adaptateur) qui envoie le CC 2. VOL : le souffle est le volume (0 muet, 127 tel que réglé). V+AT : aussi l'aftertouch, donc la destination AT de la page MOD (CUT : le filtre s'ouvre quand on souffle ; VIB, WAH, TREM) et la source AT de la matrice le suivent. OFF (par défaut) : le CC 2 ne fait rien. Mets la VOICE du son en MONO ou LEGATO, avec un peu de GLIDE. |

GLO > SYSTEM garde le reste : **MIDI** (ce qui sort : les touches seules, ou aussi le séquenceur et l'arpège), **SYNC** (l'horloge : interne, USB, TRS), **IN** (NOTES, ou CLOCK : les notes ignorées). MENU > MIDI a **CLOCK OUT**. MENU > SYSTEM a le **MPE**.

## Reçu

| Message | Effet |
|---|---|
| Note on / off | Joue le son, avec la vélocité. Sur la page NOTES, écrit le pas en cours. |
| Pitch bend | Sur l'amplitude BEND (plus haut). |
| Pression par canal, pression polyphonique | L'aftertouch : la destination AT de la page MOD, et la source AT de la matrice. |
| **Program change** | Charge le son à cette place de la liste PRESETS : le programme 0 est le premier son d'usine. Les favoris ne comptent pas ; les sons d'usine viennent dans l'ordre de la liste (celui de SAVE > PRESETS avec le filtre sur ALL), puis tes presets utilisateur. Avec une banque (CC 0), le programme *p* de la banque *b* est la place *b* × 128 + *p*. |
| CC 0 | Sélection de banque (MSB) pour le prochain program change. Le CC 32 (LSB) est ignoré. |
| CC 1 | Molette de modulation : la destination MW de la page MOD, et la source MW de la matrice. |
| CC 2 | Souffle : avec MIDI 2 > EWI, le volume (VOL) et l'aftertouch (V+AT). |
| CC 5 | GLIDE. |
| CC 6 | Data entry : avec le RPN 0 choisi, l'amplitude du pitch bend (1..24 demi-tons). |
| CC 7 | LEVEL. |
| CC 10 | PAN (64 au centre). |
| **CC 11** | Expression : le volume du son (127 tel quel, 0 muet), en plus de LEVEL ; aussi la source EXPR de la matrice. |
| CC 16..19 | Les quatre MACROS (BRITE, TIMBR, MOTN, SPACE) ; HYBRID : M1..M4. |
| CC 20 | MORPH. |
| CC 64 | Pédale de sustain. |
| CC 71 | Résonance (le RES ou le Q du moteur). |
| CC 72, 73, 75 | Release, attaque, decay. |
| CC 74 | La coupure ou la brillance du moteur, sinon le FILTER. |
| CC 76, 77, 78 | La vitesse du LFO, sa profondeur sur la hauteur (vibrato : 64 aucun), son FADE (le délai avant qu'il arrive). |
| CC 91, 93, 94 | Envois réverb, chorus, delay. |
| CC 98, 99 | NRPN : pas utilisés (un data entry après eux ne change rien). |
| CC 100, 101 | Choix du RPN (RPN 0 : l'amplitude du bend). |
| **CC 120** | All sound off. |
| **CC 121** | Reset all controllers : bend au centre, molette et pression à 0, expression au maximum, pédale relevée. |
| **CC 122** | Local control (0 off, 127 on) : comme LOCAL. |
| **CC 123** | All notes off (124..127, les messages de mode, aussi). |
| Horloge, start, continue, stop | Avec GLO > SYSTEM > SYNC = USB ou TRS : le séquenceur, l'arpégiateur et le delay suivent le tempo, le départ et l'arrêt du maître. |
| MPE | MENU > SYSTEM > MPE : le bend, la pression et le CC 74 propres à chaque note sur les canaux 2..16. |
| **Identity request** | `F0 7E 7F 06 01 F7` (n'importe quel device ID) : Clipper répond `F0 7E 7F 06 02 7D 43 4C 46 31 01 01 00 00 F7` (7D, non commercial ; famille « CL », modèle « F1 », version 1.1). |
| SysEx | Les commandes de l'éditeur web et de la mise à jour. |

Un CC règle sa valeur comme le ferait le knob : 0..127 sur toute la plage, 64 au milieu d'une valeur bipolaire.

## Envoyé

| Message | Quand |
|---|---|
| Note on / off | Les touches (toujours, sur OUT), et le séquenceur et l'arpégiateur avec GLO > SYSTEM > MIDI = SEQ. |
| Control change | Avec MIDI 2 > SEND = CC : un knob tourné sur une valeur que règlent les CC ci-dessus (EDIT, ENV, FX..., et les knobs de HOME), quand la valeur qu'il représente change. Un son chargé n'envoie rien, et un CC reçu n'est jamais renvoyé. |
| Horloge, start, stop | MENU > MIDI > CLOCK OUT (USB). |
| Identity reply | En réponse à une identity request. |

## Pas (encore) là

Song position pointer, CC 65 (portamento on / off), CC 66 (sostenuto), les NRPN, le MIDI 2.0.

## À vérifier sur une FM-1

1. GLO appuyé jusqu'à **MIDI** : IN CH, OUT, BEND, LOCAL. GLO encore : **MIDI 2** : PROG, SEND, EWI.
2. Depuis un DAW ou un contrôleur, envoie les program changes 0, 1, 2 : les sons de la liste, dans l'ordre ; CC 0 = 1 puis programme 0 : le 129e son. PROG OFF : rien ne change.
3. Tiens un accord et descends le CC 11 : le son s'éteint en fondu ; remonte : comme avant.
4. Tiens un accord et envoie le CC 123 (ou le bouton panic du DAW, souvent CC 120 / 123) : les notes s'arrêtent.
5. RPN 0 = 12 (CC 101 0, CC 100 0, CC 6 12) : le bend monte d'une octave. GLO > MIDI > BEND affiche 12 ; remets AUTO.
6. LOCAL OFF avec la piste du DAW qui renvoie vers Clipper : chaque touche joue une seule fois, en passant par le DAW.
7. SEND = CC, enregistre les CC sur une piste du DAW, tourne le RELEASE de ENV et les envois de FX : le DAW reçoit les CC 72, 91, 94 ; à la relecture, les mouvements des knobs reviennent.
8. Avec un EWI ou un contrôleur de souffle : MIDI 2 > EWI = VOL, VOICE LEGATO : souffle doucement puis fort : la note enfle ; EWI = V+AT et l'AT de la page MOD = CUT : le son s'éclaircit quand on souffle.
9. Un DAW ou un outil qui demande l'identité (MIDI-OX, Bome SendSX, `amidi`) reçoit la réponse ci-dessus.
