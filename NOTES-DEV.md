# Chènevières Tape — notes de développement

## Moteur (`Source/TapeEngine.cpp`, C++ pur, réutilisable par le magnéto 16 pistes)

Par canal, à 2× la fréquence du DAW (44,1 / 48 kHz ; pas de suréchantillonnage à 88,2 kHz et plus) :

1. Gain d'entrée.
2. Préaccentuation d'enregistrement : aigus (50 µs NAB / 35 µs CCIR à 15 ips, 50/70 µs à 7,5 ips, 17,5 µs AES à 30 ips),
   graves en NAB (3180 µs). Les aigus arrivent donc plus fort sur la bande et saturent avant le reste.
3. Hystérésis magnétique de Jiles-Atherton (RK2). La machine fixe saturation, largeur du cycle et drive ;
   le Bias resserre (sur-polarisation) ou élargit (sous-polarisation) le cycle. Normalisation : gain 0 dB
   à 0 VU (-18 dBFS), mesuré à chaque changement de réglage. Asymétrie (harmoniques paires) propre à la machine,
   nulle au repos.
4. Souffle (ajouté sur la bande, donc mis en forme par la désaccentuation).
5. Désaccentuation de lecture (inverse exact : réponse plate à bas niveau).
6. Tête de lecture : head bump (32 / 52 / 88 Hz selon la vitesse) et son creux, coupe-bas, pertes HF
   (fréquence selon vitesse, machine et bias).
7. Pleurage / scintillement : ligne à retard modulée (Hermite), retard fixe 3 ms annoncé au DAW.
   Fréquences proportionnelles à la vitesse ; même modulation sur les deux canaux.
8. Mix parallèle (direct retardé d'autant), gain de sortie, Auto gain (= -Input). Bypass = mix 0 lissé.

Repères mesurés (CH-16, 15 ips, bande +6, NAB) : THD 1 kHz 0,5 % à 0 VU, 3 % vers -6 dBFS ;
-2 dB à 10 kHz à -6 dBFS (saturation des aigus) contre -0,9 dB à bas niveau.

Outils : `ctest` (EngineTest : pas de NaN sur toutes les combinaisons, 0 dB à 0 VU, mix 0 % exact, silence),
`TapeMeasure [machine] [vitesse] [bande] [eq] [bias]` (THD, réponse, souffle ; `FS=96000` pour la fréquence de travail).

## Interface

`Source/PluginEditor.cpp`, tout en dessin vectoriel (bobines, vu-mètres, boutons) ; logo et Pistache dans la barre du haut.
Capture : `cmake -B build -DCHNV_TESTS=ON && cmake --build build --target TapeSnapshot` puis
`xvfb-run -a build/TapeSnapshot docs/apercu.png scale=1.5 level=-20 [machine=2 speed=0 ...]`.

## Publier

Comme Comp et EQ : version dans `CMakeLists.txt`, installeurs dans les artefacts du build GitHub.
AAX : il faut une Wrap Configuration PACE propre à Tape (GUID à mettre dans `tools/package-mac.sh` et
`tools/sign-aax-windows.ps1`, vides pour l'instant).

## Plus tard : magnétophone 16 pistes

Même moteur sur chaque piste, transport façon bande, punch-in, export Broadcast WAV pour Cubase.
