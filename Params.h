// Trautonium AU/VST3 - parameter table (single source of truth) and factory presets.
// No JUCE dependency: used by the plugin, the editor and the offline test renderer.
#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace traut
{
enum ParamIndex : int
{
    // String manual / master oscillator
    kMasterOct = 0, kGlide, kBend, kVibDepth, kVibRate, kDrift,
    // Subharmonic generator
    kMixture, kNeon, kSideLevel, kSideCents, kNoise,
    kDivBase,                 // 12 divisors  (mixture*4 + oscillator)   11..22
    kLvlBase = kDivBase + 12, // 12 levels                               23..34
    // Channel amplifier / Schlagwerk
    kAmpMode = kLvlBase + 12, kAttack, kRelease, kSchlagRate,
    // Formant filters: 4 x (mode, freq, reso, level)
    kFiltBase,                // 16 entries                              39..54
    kFiltDrive = kFiltBase + 16, kFiltKey,
    // Rack (Sala used a frequency shifter + two digital delays on nearly every production)
    kShiftHz, kShiftMix, kDlyL, kDlyR, kDlyFb, kDlyMix, kRevMix, kRevSize,
    // Output stage
    kBandwidth, kAmpDrive, kVolume,
    kNumParams
};

enum class ParamType { Float, Int, Choice };

struct ParamSpec
{
    std::string id, name, group, unit;
    ParamType type = ParamType::Float;
    float lo = 0, hi = 1, def = 0;
    float skewMid = 0;                 // value at the slider centre (0 = linear)
    std::vector<std::string> choices;
};

inline const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = []
    {
        std::vector<ParamSpec> s (kNumParams);

        auto F = [&] (int i, const std::string& id, const std::string& name, const std::string& grp, float lo, float hi,
                      float def, const std::string& unit = "", float mid = 0.0f)
        { s[(size_t) i] = ParamSpec { id, name, grp, unit, ParamType::Float, lo, hi, def, mid, {} }; };
        auto I = [&] (int i, const std::string& id, const std::string& name, const char* grp, int lo, int hi,
                      int def, const char* unit = "")
        { s[(size_t) i] = ParamSpec { id, name, grp, unit, ParamType::Int, (float) lo, (float) hi, (float) def, 0.0f, {} }; };
        auto C = [&] (int i, const std::string& id, const std::string& name, const char* grp,
                      std::vector<std::string> ch, int def)
        { s[(size_t) i] = ParamSpec { id, name, grp, "", ParamType::Choice, 0.0f, (float) ch.size() - 1.0f, (float) def, 0.0f, std::move (ch) }; };

        // --- Manual (Saitenmanual) ---
        I (kMasterOct, "masterOct", "Master Octave", "Manual", -2, 3, 1, "oct");
        F (kGlide,     "glide",     "Glide",          "Manual", 0.0f, 1000.0f, 70.0f, "ms", 120.0f);
        I (kBend,      "bend",      "Bend Range",     "Manual", 0, 24, 2, "st");
        F (kVibDepth,  "vibDepth",  "Finger Vibrato", "Manual", 0.0f, 100.0f, 25.0f, "ct");
        F (kVibRate,   "vibRate",   "Vibrato Rate",   "Manual", 0.5f, 12.0f, 5.5f, "Hz");
        F (kDrift,     "drift",     "Tube Drift",     "Manual", 0.0f, 1.0f, 0.35f);

        // --- Oscillators ---
        C (kMixture,   "mixture",   "Mixture (Foot Switch)", "Oscillators", { "Left", "Middle", "Right" }, 1);
        F (kNeon,      "neon",      "Neon Curve",     "Oscillators", 0.0f, 1.0f, 0.5f);
        F (kSideLevel, "sideLevel", "Side Osc Level", "Oscillators", 0.0f, 1.0f, 0.0f);
        F (kSideCents, "sideCents", "Side Osc Detune","Oscillators", -1200.0f, 1200.0f, 9.0f, "ct");
        F (kNoise,     "noise",     "Noise",          "Oscillators", 0.0f, 1.0f, 0.0f);

        static const int   defDiv[3][4] = { { 1, 2, 4, 8 }, { 1, 2, 3, 4 }, { 1, 3, 5, 7 } };
        static const float defLvl[3][4] = { { 1.0f, 0.7f, 0.5f, 0.35f }, { 1.0f, 0.8f, 0.6f, 0.45f }, { 1.0f, 0.7f, 0.55f, 0.4f } };
        static const char* mixGroup[3] = { "Mixture Left", "Mixture Middle", "Mixture Right" };
        for (int m = 0; m < 3; ++m)
            for (int k = 0; k < 4; ++k)
            {
                const std::string tag = std::to_string (m) + "_" + std::to_string (k);
                I (kDivBase + m * 4 + k, "div_" + tag, "Divisor " + std::to_string (k + 1), mixGroup[m], 1, 24, defDiv[m][k]);
                F (kLvlBase + m * 4 + k, "lvl_" + tag, "Level " + std::to_string (k + 1),   mixGroup[m], 0.0f, 1.0f, defLvl[m][k]);
            }

        // --- Channel amp / Schlagwerk ---
        C (kAmpMode,    "ampMode",    "Amp Control", "Schlagwerk",
           { "Normal (pressure)", "Schlagwerk 1 (AR trigger)", "Schlagwerk 2 (free run)" }, 0);
        F (kAttack,     "attack",     "Attack",  "Schlagwerk", 1.0f, 2000.0f, 12.0f, "ms", 100.0f);
        F (kRelease,    "release",    "Release", "Schlagwerk", 5.0f, 4000.0f, 140.0f, "ms", 250.0f);
        F (kSchlagRate, "schlagRate", "Free-run Rate", "Schlagwerk", 0.1f, 30.0f, 5.0f, "Hz", 3.0f);

        // --- Formant filters (4 parallel, each Off / Band / Low) ---
        static const int   fMode[4] = { 1, 1, 1, 0 };
        static const float fFreq[4] = { 700.0f, 1500.0f, 3000.0f, 5000.0f };
        static const float fReso[4] = { 5.0f, 6.0f, 6.0f, 1.0f };
        static const float fLvl[4]  = { 1.0f, 0.8f, 0.5f, 0.5f };
        for (int j = 0; j < 4; ++j)
        {
            const std::string t = "f" + std::to_string (j);
            const std::string g = "Formant " + std::to_string (j + 1);
            C (kFiltBase + j * 4 + 0, t + "_mode",  "Mode",      g.c_str(), { "Off", "Band", "Low" }, fMode[j]);
            F (kFiltBase + j * 4 + 1, t + "_freq",  "Frequency", g.c_str(), 60.0f, 8000.0f, fFreq[j], "Hz", 800.0f);
            F (kFiltBase + j * 4 + 2, t + "_reso",  "Resonance", g.c_str(), 0.5f, 40.0f, fReso[j], "Q", 4.0f);
            F (kFiltBase + j * 4 + 3, t + "_level", "Level",     g.c_str(), 0.0f, 1.5f, fLvl[j]);
        }
        F (kFiltDrive, "filtDrive", "Input Drive (Trompeteneffekt)", "Formant Global", 0.25f, 8.0f, 1.0f, "", 1.5f);
        F (kFiltKey,   "filtKey",   "Key Tracking (not original)",   "Formant Global", 0.0f, 1.0f, 0.0f);

        // --- Rack ---
        F (kShiftHz,  "shiftHz",  "Freq Shift",  "Rack", -300.0f, 300.0f, 0.0f, "Hz");
        F (kShiftMix, "shiftMix", "Shift Mix",   "Rack", 0.0f, 1.0f, 0.0f);
        F (kDlyL,     "dlyL",     "Delay L",     "Rack", 20.0f, 1500.0f, 310.0f, "ms", 300.0f);
        F (kDlyR,     "dlyR",     "Delay R",     "Rack", 20.0f, 1500.0f, 470.0f, "ms", 300.0f);
        F (kDlyFb,    "dlyFb",    "Delay Feedback", "Rack", 0.0f, 0.9f, 0.3f);
        F (kDlyMix,   "dlyMix",   "Delay Mix",   "Rack", 0.0f, 1.0f, 0.0f);
        F (kRevMix,   "revMix",   "Reverb Mix",  "Rack", 0.0f, 1.0f, 0.15f);
        F (kRevSize,  "revSize",  "Reverb Size", "Rack", 0.0f, 1.0f, 0.6f);

        // --- Output ---
        F (kBandwidth, "bandwidth", "Bandwidth",   "Output", 1500.0f, 16000.0f, 9000.0f, "Hz", 6000.0f);
        F (kAmpDrive,  "ampDrive",  "Amp Drive",   "Output", 0.0f, 1.0f, 0.3f);
        F (kVolume,    "volume",    "Foot Pedal / Volume", "Output", 0.0f, 1.0f, 0.8f);
        return s;
    }();
    return specs;
}

inline int indexOfID (const std::string& id)
{
    const auto& s = paramSpecs();
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i].id == id) return (int) i;
    return -1;
}

struct Params
{
    std::array<float, kNumParams> v {};
    Params() { for (size_t i = 0; i < v.size(); ++i) v[i] = paramSpecs()[i].def; }
    float operator[] (int i) const { return v[(size_t) i]; }
    void set (const std::string& id, float value) { const int i = indexOfID (id); if (i >= 0) v[(size_t) i] = value; }
};

// ---------------------------------------------------------------- factory presets
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // everything else = defaults
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p =
    {
        { "Mixtur - Init (Sala)", {} },

        { "Telefunken 1933 - Volkstrautonium",
          { {"masterOct", 0}, {"glide", 90}, {"neon", 0.8f}, {"lvl_1_1", 0}, {"lvl_1_2", 0}, {"lvl_1_3", 0},
            {"attack", 25}, {"release", 200}, {"f0_freq", 520}, {"f0_reso", 6}, {"f1_freq", 1800}, {"f1_reso", 5}, {"f1_level", 1.2f}, {"f0_level", 1.5f}, {"volume", 1.0f},
            {"f2_mode", 0}, {"bandwidth", 5000}, {"ampDrive", 0.5f}, {"revMix", 0.1f}, {"vibDepth", 30} } },

        { "Hindemith - Langsames Stueck",
          { {"masterOct", 0}, {"glide", 140}, {"neon", 0.9f}, {"lvl_1_1", 0}, {"lvl_1_2", 0}, {"lvl_1_3", 0},
            {"f0_freq", 380}, {"f0_reso", 8}, {"f1_freq", 1100}, {"f1_reso", 6}, {"f1_level", 0.9f},
            {"f2_mode", 2}, {"f2_freq", 2500}, {"f2_reso", 1}, {"f2_level", 0.4f}, {"bandwidth", 5500}, {"ampDrive", 0.45f},
            {"revMix", 0.12f}, {"vibDepth", 35}, {"attack", 40}, {"release", 260} } },

        { "Sala - Subharmonic Choir",
          { {"lvl_1_0", 1}, {"lvl_1_1", 0.9f}, {"lvl_1_2", 0.8f}, {"lvl_1_3", 0.7f}, {"f0_freq", 600}, {"f0_reso", 3},
            {"f1_freq", 1400}, {"f1_reso", 3}, {"f2_mode", 2}, {"f2_freq", 3500}, {"f2_reso", 1}, {"f2_level", 0.4f},
            {"revMix", 0.25f}, {"dlyMix", 0.2f}, {"sideLevel", 0.35f}, {"sideCents", 7}, {"volume", 0.5f} } },

        { "Sala - Gong (Schlagwerk)",
          { {"masterOct", 0}, {"ampMode", 1}, {"attack", 2}, {"release", 1800}, {"div_1_1", 5}, {"div_1_2", 7}, {"div_1_3", 11},
            {"lvl_1_1", 0.8f}, {"lvl_1_2", 0.7f}, {"lvl_1_3", 0.6f}, {"f0_freq", 900}, {"f0_reso", 18}, {"f1_freq", 2300},
            {"f1_reso", 22}, {"f2_freq", 4100}, {"f2_reso", 25}, {"f2_level", 1.5f}, {"f0_level", 1.5f}, {"f1_level", 1.5f}, {"volume", 1.0f},
            {"revMix", 0.3f}, {"glide", 0}, {"neon", 0.3f} } },

        { "Voegel (Birds-inspired)",
          { {"masterOct", 2}, {"glide", 220}, {"ampMode", 2}, {"schlagRate", 9}, {"attack", 40}, {"release", 60},
            {"noise", 0.35f}, {"sideLevel", 0.7f}, {"sideCents", 150}, {"vibDepth", 60}, {"f0_freq", 2600}, {"f0_reso", 12},
            {"f1_freq", 3800}, {"f1_reso", 15}, {"f2_mode", 1}, {"f2_freq", 1800}, {"f2_reso", 10}, {"shiftHz", -40}, {"shiftMix", 0.35f},
            {"dlyMix", 0.3f}, {"volume", 1.0f}, {"f0_level", 1.5f}, {"f1_level", 1.5f} } },

        { "Sala - Frequency Shift Cloud",
          { {"shiftHz", 7}, {"shiftMix", 0.55f}, {"dlyMix", 0.35f}, {"dlyFb", 0.45f}, {"revMix", 0.3f}, {"revSize", 0.8f},
            {"lvl_1_0", 0.9f}, {"div_1_1", 3}, {"div_1_2", 5}, {"div_1_3", 8}, {"sideLevel", 0.4f}, {"sideCents", 11} } },
    };
    return p;
}

} // namespace traut
