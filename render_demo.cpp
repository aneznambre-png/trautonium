// Offline renderer: renders a preset to a 16-bit WAV so the DSP can be checked/listened to without a DAW.
//   usage: render_demo <out.wav> <preset index> [mode]      mode: phrase (default) | note | shift
#include "../Source/TrautoniumDSP.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace traut;

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, rate = (uint32_t) sr, br = rate * 4;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f); std::fwrite (&rate, 4, 1, f);
    std::fwrite (&br, 4, 1, f); std::fwrite (&ba, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t a = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, L[i])) * 32767.0f);
        const int16_t b = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, R[i])) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: %s out.wav presetIndex [phrase|note|shift]\n", argv[0]); return 1; }
    const int sr = 48000;
    const int idx = std::atoi (argv[2]);
    const std::string mode = argc > 3 ? argv[3] : "phrase";
    const auto& pr = presets();
    if (idx < 0 || idx >= (int) pr.size()) { std::printf ("preset index 0..%d\n", (int) pr.size() - 1); return 1; }

    Params p;
    for (auto& kv : pr[(size_t) idx].values) p.set (kv.first, kv.second);
    if (mode == "note")  { p.set ("revMix", 0); p.set ("dlyMix", 0); p.set ("drift", 0); p.set ("vibDepth", 0); p.set ("shiftMix", 0); }
    if (mode == "shift") { p.set ("shiftHz", 100); p.set ("shiftMix", 1); p.set ("revMix", 0); p.set ("dlyMix", 0);
                           p.set ("lvl_1_1", 0); p.set ("lvl_1_2", 0); p.set ("lvl_1_3", 0); p.set ("f0_mode", 0); p.set ("f1_mode", 0); p.set ("f2_mode", 0);
                           p.set ("f3_mode", 0); p.set ("neon", 0); p.set ("drift", 0); p.set ("vibDepth", 0); p.set ("masterOct", 0); p.set ("ampMode", 0); p.set ("ampDrive", 0); }
    std::printf ("preset %d: %s (mode %s)\n", idx, pr[(size_t) idx].name, mode.c_str());

    Engine e; e.prepare (sr); e.setParams (p);
    std::vector<float> L, R;
    auto run = [&] (double sec) {
        const int n = (int) (sec * sr); std::vector<float> a ((size_t) n), b ((size_t) n);
        for (int o = 0; o < n; o += 256) { const int c = std::min (256, n - o); e.setParams (p); e.render (a.data() + o, b.data() + o, c); }
        L.insert (L.end(), a.begin(), a.end()); R.insert (R.end(), b.begin(), b.end());
    };

    if (mode == "note" || mode == "shift")
    {
        e.noteOn (60, 1.0f); run (2.0); e.noteOff (60); run (0.3);   // C4 (master = C4 * 2^masterOct)
    }
    else
    {   // phrase: legato glide line, a gap, a Schlagwerk-style repeat, then a held chord-like drone note
        const int line[] = { 57, 60, 64, 62, 59, 57 };
        e.noteOn (line[0], 0.9f); run (0.7);
        for (int i = 1; i < 6; ++i) { e.noteOn (line[i], 0.85f); run (0.55); e.noteOff (line[i - 1]); }
        e.noteOff (line[5]); run (1.2);
        e.noteOn (45, 1.0f); run (2.5); e.noteOff (45); run (2.5);
    }

    double peak = 0, sq = 0; bool bad = false;
    for (size_t i = 0; i < L.size(); ++i) { peak = std::max (peak, (double) std::fabs (L[i])); sq += L[i] * L[i]; if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true; }
    std::printf ("samples %zu  peak %.3f  rms %.4f  %s\n", L.size(), peak, std::sqrt (sq / (double) L.size()), bad ? "NaN/Inf FOUND" : "finite OK");
    writeWav (argv[1], L, R, sr);
    return bad ? 2 : 0;
}
