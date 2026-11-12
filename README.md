# Trautonium (Mixtur) – AU / VST3 instrument za Logic Pro

Softverska emulacija Trautoniuma (Trautwein 1930 → Sala Mixtur-Trautonium 1952). Detalji istorije i šeme: `docs/ISTORIJA_I_SEMA.md`.

## Build (macOS, jednom)

1. Instalirajte Xcode (ili `xcode-select --install`) i CMake (`brew install cmake`).
2. U Terminalu:

```bash
cd TrautoniumAU
cmake -B build -G Xcode
cmake --build build --config Release
```

CMake sam preuzima JUCE 8.0.4. Posle builda se plugin automatski kopira u:
- AU: `~/Library/Audio/Plug-Ins/Components/Trautonium.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/Trautonium.vst3`

3. Proverite AU: `auval -v aumu Trtn Trau` (mora da završi sa `AU VALIDATION SUCCEEDED`).
4. Restartujte Logic Pro. Ako ne vidi plugin: *Logic Pro → Settings → Plug-in Manager → Reset & Rescan Selection*.
5. Na softverskom instrumentu: kliknite polje za instrument → *AU Instruments → TrautoniumLab → Trautonium*.

## Kontrole preko MIDI-ja
| Kontrola | Funkcija |
|---|---|
| Pitch bend | kontinuirano klizanje (opseg u parametru *Bend Range*) |
| Mod wheel (CC1) | dubina vibrata prsta |
| Channel aftertouch | pritisak (jačina) |
| CC11 | nožna papučica (jačina) |
| CC16 | nožni prekidač mixture: Left / Middle / Right |

## Test bez DAW-a
`Tests/render_demo.cpp` renderuje presete u WAV: `g++ -O2 -std=c++17 Tests/render_demo.cpp -o render_demo && ./render_demo out.wav 3`. U `demos/` su gotovi renderi.

## Struktura
- `Source/Params.h` – sve parametre i factory presete (jedan izvor istine)
- `Source/TrautoniumDSP.h` – zvučni motor (bez JUCE zavisnosti)
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` – JUCE omotač (AU/VST3/Standalone)
