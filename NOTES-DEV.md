# Chènevières Tape — notes de développement

## Moteur (`Source/TapeEngine.cpp`, C++ pur, réutilisable par le magnéto 16 pistes)

Par canal, à 2× la fréquence du DAW (44,1 / 48 kHz ; pas de suréchantillonnage à 88,2 kHz et plus) :

1. Gain d'entrée.
2. Préaccentuation d'enregistrement : aigus (50 µs NAB / 35 µs CCIR à 15 ips, 50/70 µs à 7,5 ips, 17,5 µs AES à 30 ips),
   graves en NAB (3180 µs). Accentuation des aigus plafonnée à +4 dB : plus fort, l'aigu saturé se replie en
   intermodulation dans le médium (crêtes à +9 dB sur une caisse claire en 0.1/0.2).
2b. Compression de bande sur les crêtes (genou très doux, ≈1,7:1, attaque 0,3 ms, retour 70 ms) : transitoires arrondies, colle.
3. Hystérésis magnétique de Jiles-Atherton (RK2). La machine fixe saturation, largeur du cycle et drive ;
   le Bias resserre (sur-polarisation) ou élargit (sous-polarisation) le cycle. Normalisation : gain 0 dB
   à 0 VU (-18 dBFS), mesuré à chaque changement de réglage. Asymétrie (harmoniques paires) propre à la machine,
   nulle au repos.
3b. Auto-effacement des aigus : passe-bas qui se ferme quand l'aigu enregistré est fort (attaque 0,2 ms, retour 30 ms),
   plus marqué à 7,5 ips et sur la CH-440 : aigus « soyeux » sans intermodulation.
4. Souffle + bruit de modulation (grain proportionnel au signal), ajoutés sur la bande.
5. Désaccentuation de lecture (inverse exact : réponse plate à bas niveau).
6. Tête de lecture : head bump (32 / 52 / 88 Hz selon la vitesse) et son creux, coupe-bas, pertes HF
   (fréquence selon vitesse, machine et bias).
7. Vie de la bande : fluctuation de contact tête/bande (niveau), pleurage / scintillement / scrape flutter
   (ligne à retard modulée, Hermite, retard fixe 3 ms annoncé au DAW, commune aux canaux) et azimut propre à chaque
   canal (quelques µs, l'image stéréo respire). Bruits filtrés ramenés à un écart-type unitaire.
8. Mix parallèle (direct retardé d'autant), gain de sortie, Auto gain (= -Input). Bypass = mix 0 lissé.

Repères (0.3, CH-16, 15 ips, bande +6, NAB, réglages d'usine) sur une boucle batterie/basse à -4 dBFS crête :
facteur de crête -3,3 dB, 30-80 Hz +3 dB, 10-20 kHz -5 dB. Défauts : Wow 30, Flutter 30, Hiss 15.
Outils : `TapeRender entree.wav sortie.wav [param=valeur]` (plugin complet, latence compensée), `TapeProcTest`.

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
