# Trautonium – istorija, šema i kako je to preslikano u plugin

## 1. Istorija (kratko)

| Godina | Događaj |
|---|---|
| 1929–30 | Friedrich Trautwein razvija instrument u Rundfunkversuchsstelle pri Berlinskoj muzičkoj visokoj školi. Prvi javni nastup: 20. jun 1930, „Neue Musik Berlin 1930“ (Paul Hindemith i Oskar Sala). |
| 1930–31 | Hindemith piše *Des kleinen Elektromusikers Lieblinge* i *Konzertstück für Trautonium und Streichorchester*; Sala je solista. |
| 1933–35 | Telefunken pravi serijski „Volkstrautonium“ (Ela T 42): jedan manual, jedna papučica, ~100–200 komada. |
| ~1935 | Trautwein odlazi na druge projekte, Sala nastavlja razvoj do kraja života. |
| 1948–52 | **Mixtur-Trautonium**: dva manuala, *subharmonijski generator*, formantni filteri, Schlagwerk, šum. |
| 1963 | Zvuk ptica u Hičkokovim „Pticama“ (Sala i Remi Gassmann). |
| 1995 / 2002 | Sala poklanja originalni instrument muzeju u Bonu; umire 27. februara 2002. |

## 2. Fizička šema originala

**Saitenmanual (kontroler).** Nevodljiva žica omotana otpornom žicom (konstantan) razapeta nekoliko centimetara iznad metalne ploče. Pritisak prsta spaja žicu i ploču; napon na ploči zavisi od pozicije prsta, pa se visina tona menja **kontinuirano** (portamento, vibrato, četvrttonovi). Ploča je pokretna oko ose: **vertikalni pritisak kontroliše jačinu** (kod Sale preko tečnog otpornika sa glicerinom – fina rezolucija u pianissimu). Opseg ≈ 3½ oktave, dužina ≈ 70 cm. Iznad žice su opružni „jezičci“ (Sala ih koristi za C, D, G, A) kao vizuelna orijentacija.

**Generator tona.**
- 1930: relaksacioni oscilator sa **neonskom cevi** (kasnije tiratron RK1, pa tranzistori) → testerast talas sa eksponencijalnim (RC) punjenjem.
- Mixtur (Sala): **master oscilator** (≈ 392 Hz … 4186 Hz) + **4 delitelja frekvencije** podešljiva 1…24. Izlazi su pravougaoni talasi pretvoreni u testere – „subharmonici“ f/n (nisu sinusi!). Kombinacija 4 delitelja = *mixtura*; nožni prekidači biraju 1 od **3 mixture** (12 delitelja po manualu). Najniža frekvencija ≈ 392/24 ≈ 16 Hz.
- **Nebenoszillator** (pomoćni oscilator, slobodno štimovan → udari/fazing), **generator šuma**.

**Kanalski pojačavač + Schlagwerk.** Pojačavač kontroliše vertikalni pritisak manuala *ili* jedan od dva A/R generatora omotača („Schlagwerk“): *triggered* (okida ga pokret manuala) ili *free-running* (radi kao LFO).

**Formantni filteri.** 8 filtera (4 propusnika opsega + 4 niskopropusna, u parovima – aktivna su najviše 4). Frekvencija, rezonanca i nivo se podešavaju ručno; filteri su **fiksni** (nisu naponski kontrolisani). Pri većim nivoima dolazi do klipovanja – „Trompeten-Effekt“.

**Sumirajući pojačavač.** Kontroliše ga **nožna papučica**; izlaz ide u efekte (Sala: frekventni pomerač + dva digitalna delay-a) i PA.

## 3. Signal flow u pluginu

```
 MIDI nota ──► [glide u log-domenu = žica] ──┐  pitch bend (opseg), mod wheel (vibrato prsta), tube drift
                                             ▼
                       MASTER OSC (testera, RC-krivulja "Neon Curve", PolyBLEP)
                         │ brojač/delitelj 1..24 (stvarno celobrojno deljenje – fazno zaključano)
                         ├─► SUB 1 (f/n1) ─┐
                         ├─► SUB 2 (f/n2) ─┤  4 nivoa po mixturi
                         ├─► SUB 3 (f/n3) ─┤  3 mixture: Left / Middle / Right  (parametar ili CC16)
                         └─► SUB 4 (f/n4) ─┘
        SIDE OSC (slobodan, detune u centima) ───┤
        NOISE ───────────────────────────────────┤
                                                 ▼
          KANALSKI POJAČAVAČ  ◄─ Normal (pritisak: velocity + channel aftertouch, glicerinska krivulja)
                                ◄─ Schlagwerk 1 (AR, trigger)  ◄─ Schlagwerk 2 (AR kao LFO)
                                                 ▼
              4 × FORMANT FILTER (paralelno; Off / Band / Low; freq, reso, level) + ulazni drive (klip)
                                                 ▼
          SUMIRAJUĆI POJAČAVAČ (Volume / CC11 = nožna papučica) → amp stage + Bandwidth (ograničen opseg 1930-ih)
                                                 ▼
          FREQUENCY SHIFTER (Hilbert) → 2 DELAY-A (L/R, ping-pong) → REVERB → izlaz
```

## 4. Šta je verno, a šta aproksimacija

**Verno (po dokumentovanim podacima):** testerasti talas relaksacionog oscilatora (RC-krivulja); celobrojni delitelji 1–24 sa fazno zaključanim izlazima (pravi „subharmonici“, ne sinusi); 4 delitelja × 3 mixture; opseg masteru ≈ 400 Hz–4 kHz (podesivo oktavom); nebenoszillator; šum; Schlagwerk (triggered AR / free-running); 4 paralelna formantna filtera Off/Band/Low sa klipovanjem na ulazu; glicerinska krivulja jačine; kontinuirani portamento; papučica i frekventni pomerač + delay.

**Aproksimacija / nije originalno:** tačna kola neonske cevi i tiratrona (modelovana je njihova talasna forma + blagi drift, ne diskretna kola); tačne rezonantne krivulje Salinih filtera (korišćen je SVF band/low – podesite uz uho i uporedite sa snimcima); plugin je **jednoglasan, jedan manual** (original ima dva – drugi manual je moguć kao druga instanca plugina); pomerač frekvencije je Hilbert-FIR (127 tapova) i slabije prati ispod ~200 Hz; „Key Tracking“ filtera nije u originalu.

## 5. Preporuke za sviranje u Logic Pro

- Za osećaj žice: **Glide 60–150 ms**, pitch bend opseg 2–12 st, **mod wheel = vibrato prsta**.
- **CC16** = nožni prekidač mixture (0–42 Left, 43–85 Middle, 86–127 Right). **CC11** = papučica jačine. **Channel aftertouch** = pritisak.
- Poluton-ljestvica: u Logicu isključite *Smart Quantize*; za mikrotonove koristite pitch bend automaciju.
- Za „Ptice“ koristite preset *Voegel*: master +2 okt, Schlagwerk 2, šum + side-osc, uski formanti oko 2–4 kHz, glide 200+ ms.
