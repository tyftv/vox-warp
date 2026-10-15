#pragma once
// Vox Warp DSP engine — one grain engine where hard-tune, formant, harmony,
// the harmony cloud and stutter all come from the same phase-locked grains.
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

struct EngineParams
{
    float snap = 1.0f;      // 0..1 (1 = instant robot)
    int   key = 0;          // 0..11
    int   scale = 1;        // 0 off, 1 chrom, 2 major, 3 minor, 4 penta, 5 hijaz
    float shift = 0.0f;     // semitones
    float formant = 0.0f;   // semitones
    float throat = 1.0f;    // grain size multiplier 0.5..4
    float width = 0.0f;     // 0..1
    int   h1 = 6, h2 = 8;   // harmony interval index
    float hblend = 0.0f, hspread = 0.6f, hhuman = 0.3f;
    float spray = 0.0f, scatter = 0.0f, stutter = 0.0f, reverse = 0.0f;
    float crush = 16.0f;    // bits (16 = off)
    float decim = 1.0f;     // sample-hold factor (1 = off)
    float fold = 0.0f;
};

class VoxEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        buf.assign (N, 0.0f); obuf.assign (N, 0.0f);
        dec.assign (2048, 0.0f); yd.assign (440, 0.0f);
        w = 0; dw = 0; dacc = 0; dn = 0; dhop = 0;
        lastF = 140; voiced = false; curMidi = tgtMidi = inMidi = 50;
        mark = 0; aMark = -1; holdC = 0; hL = hR = 0;
        stT = 0; stOn = false; stBase = 0; stLen = 1; stPh = 0;
        cmark = 0; caS = 0; hbS = 0;
        for (auto& g : main) g.on = false;
        for (auto& g : cloud) g.on = false;
        for (auto& h : hv) { h.mark = 0; h.aM = -1; h.hasTgt = false; h.det = 0; for (auto& g : h.g) g.on = false; }
    }

    // in: mono input. outL/outR: fully wet signal (after ROT), no dry, no output gain.
    void process (const float* in, float* outL, float* outR, int n)
    {
        const float tau = 400.0f * (1.0f - p.snap) * (1.0f - p.snap);
        const float coef = (p.scale == 0 || tau < 0.5f) ? 1.0f : 1.0f - std::exp (-((float) n / sr) / (tau / 1000.0f));
        curMidi += (tgtMidi - curMidi) * coef;
        const float fOut = 440.0f * std::pow (2.0f, (curMidi - 69.0f) / 12.0f);
        const double Pout = std::max (16.0, (double) sr / fOut);
        const double Pin = (double) sr / lastF;
        const double rate0 = std::pow (2.0, p.formant / 12.0);
        const float ca = std::max (p.spray, std::max (p.scatter, p.reverse));
        caS += (ca - caS) * 0.15f;
        const double haas = p.width * sr * 0.012;
        hbS += (p.hblend - hbS) * 0.15f;
        const float hb = hbS;

        double HP[2] = { 0, 0 };
        for (int v = 0; v < 2; ++v)
        {
            auto& H = hv[v];
            if (! H.hasTgt) continue;
            H.cur += (H.tgt - H.cur) * coef;
            H.det = std::clamp (H.det * 0.995f + (rnd() - 0.5f) * 0.06f, -1.0f, 1.0f);
            const float mid = H.cur + H.det * p.hhuman * 0.22f;
            HP[v] = std::max (16.0, (double) sr / (440.0 * std::pow (2.0, (mid - 69.0) / 12.0)));
            const float pan = (v ? 1.0f : -1.0f) * p.hspread;
            H.pl = std::sqrt (0.5f * (1 - pan)); H.pr = std::sqrt (0.5f * (1 + pan));
        }

        int cnt = 0;
        for (int s = 0; s < n; ++s)
        {
            const float x = in[s];
            buf[(size_t) (w & M)] = x;
            dacc += x;
            if (++dn == 4) { dec[(size_t) (dw & 2047)] = dacc * 0.25f; ++dw; dacc = 0; dn = 0; if (++dhop == 128) { dhop = 0; detect(); } }

            // stutter: capture a slice and loop it as the grain source
            if (p.stutter > 0.001f)
            {
                if (stOn) { if (++stPh >= stLen) stPh = 0; }
                if (--stT <= 0)
                {
                    const int64_t seg = std::max<int64_t> (256, (int64_t) (sr * (0.25f - 0.22f * p.stutter) * (rnd() < 0.5f ? 1.0f : 0.5f)));
                    if (stOn) { stOn = false; stT = (int64_t) (seg * (1 + rnd() * 4 * (1 - p.stutter))); }
                    else if (rnd() < 0.3f + 0.65f * p.stutter) { stOn = true; stLen = seg; stBase = w - seg; stPh = 0; stT = seg * (2 + (int64_t) (rnd() * 3)); }
                    else stT = seg;
                }
            }
            else stOn = false;

            const double src = (double) (stOn ? stBase + stPh : w);

            // tuned voice: pitch-synchronous grains locked to whole input periods
            if (--mark <= 0)
            {
                const double L = std::clamp (2.0 * Pin * p.throat, 96.0, 8192.0), outLen = L / rate0;
                mark += Pout;
                const double want = src - L * 0.5 - 64;
                if (aMark < 0 || want - aMark > 4 * Pin || want < aMark - 2 * Pin) aMark = want;
                else { aMark += std::round ((want - aMark) / Pin) * Pin; if (aMark > want) aMark -= Pin; }
                for (auto& g : main) if (! g.on) { g.on = true; g.start = aMark - L * 0.5 - 32; g.rate = rate0; g.len = outLen; g.age = 0; g.amp = (float) std::min (1.0, 2.0 * Pout / outLen); break; }
            }
            float m = 0;
            cnt = 0;
            for (auto& g : main)
            {
                if (! g.on) continue;
                const double ph = g.age / g.len; if (ph >= 1) { g.on = false; continue; }
                m += rd (buf, g.start + g.age * g.rate) * (float) (0.5 - 0.5 * std::cos (6.283185307 * ph)) * g.amp;
                g.age += 1; ++cnt;
            }

            // harmonizer: two more phase-locked voices at in-key intervals
            float hl = 0, hr = 0, hm = 0;
            if (hb > 0.0005f)
            {
                for (int v = 0; v < 2; ++v)
                {
                    auto& H = hv[v];
                    if (H.hasTgt && --H.mark <= 0)
                    {
                        const double L = std::clamp (2.0 * Pin * p.throat, 96.0, 8192.0), outLen = L / rate0;
                        H.mark += HP[v];
                        const double want = src - L * 0.5 - 64 - p.hhuman * sr * (v ? 0.028 : 0.016);
                        if (H.aM < 0 || want - H.aM > 4 * Pin || want < H.aM - 2 * Pin) H.aM = want;
                        else { H.aM += std::round ((want - H.aM) / Pin) * Pin; if (H.aM > want) H.aM -= Pin; }
                        for (auto& g : H.g) if (! g.on) { g.on = true; g.start = H.aM - L * 0.5 - 32; g.rate = rate0; g.len = outLen; g.age = 0; g.amp = (float) std::min (1.0, 2.0 * HP[v] / outLen); break; }
                    }
                    float hs = 0;
                    for (auto& g : H.g)
                    {
                        if (! g.on) continue;
                        const double ph = g.age / g.len; if (ph >= 1) { g.on = false; continue; }
                        hs += rd (buf, g.start + g.age * g.rate) * (float) (0.5 - 0.5 * std::cos (6.283185307 * ph)) * g.amp;
                        g.age += 1;
                    }
                    hs *= hb * 0.85f; hl += hs * H.pl * 1.414f; hr += hs * H.pr * 1.414f; hm += hs;
                }
            }
            obuf[(size_t) (w & M)] = m + hm * 0.7f;

            // harmony cloud: long grains of the tuned voice, re-pitched by musical intervals
            float cl = 0, cr = 0;
            if (ca > 0.001f && --cmark <= 0)
            {
                const double cper = sr * (0.05 - 0.028 * ca);
                cmark += cper * (0.6 + 0.8 * rnd());
                const double outLen = sr * (0.06 + 0.035 * p.throat);
                const int nI = 1 + (int) (p.scatter * 7.99f);
                const int iv = INTV[(size_t) std::min (7, (int) (rnd() * nI))];
                const double rate = std::pow (2.0, iv / 12.0), Lin = outLen * rate;
                for (auto& c : cloud)
                {
                    if (c.on) continue;
                    c.on = true; c.rate = rate; c.len = outLen; c.Lin = Lin; c.age = 0;
                    c.start = (double) w - Lin - 64 - p.spray * sr * 0.5 * rnd();
                    c.dir = rnd() < p.reverse ? -1 : 1; c.amp = (float) std::min (1.0, 2.0 * cper / outLen) * 0.9f;
                    const float pan = (rnd() * 2 - 1) * (0.35f + 0.65f * p.width);
                    c.pl = std::sqrt (0.5f * (1 - pan)); c.pr = std::sqrt (0.5f * (1 + pan));
                    break;
                }
            }
            for (auto& c : cloud)
            {
                if (! c.on) continue;
                const double ph = c.age / c.len; if (ph >= 1) { c.on = false; continue; }
                const double pos = c.dir > 0 ? c.start + c.age * c.rate : c.start + c.Lin - c.age * c.rate;
                const float v = rd (obuf, pos) * (float) (0.5 - 0.5 * std::cos (6.283185307 * ph)) * c.amp;
                cl += v * c.pl; cr += v * c.pr; c.age += 1;
            }

            const float mg = 1 - 0.35f * caS, cw = caS * 1.5f;
            float l = m * mg + cl * cw + hl;
            float r = (haas > 1 ? rd (obuf, (double) w - haas) - hm * 0.7f : m) * mg + cr * cw + hr;

            holdC += 1; if (holdC >= p.decim) { holdC -= p.decim; hL = l; hR = r; }
            l = hL; r = hR;
            if (p.crush < 15.9f) { const float q = std::pow (2.0f, p.crush - 1); l = std::round (l * q) / q; r = std::round (r * q) / q; }
            if (p.fold > 0.001f) { const float dr = 1 + p.fold * 10, k = std::min (1.0f, p.fold * 3); l += (std::sin (l * dr) - l) * k; r += (std::sin (r * dr) - r) * k; }
            outL[s] = l; outR[s] = r;
            ++w;
        }
        grains = cnt; stut = stOn; outMidi = curMidi;
    }

    EngineParams p;
    // telemetry (read by UI)
    float inMidi = 50, outMidi = 50, rms = 0; bool voiced = false, stut = false; int grains = 0;

private:
    static constexpr int64_t N = 131072, M = N - 1;
    struct Grain { bool on = false; double start = 0, rate = 1, len = 1, age = 0; float amp = 0; };
    struct CGrain { bool on = false; double start = 0, rate = 1, len = 1, Lin = 1, age = 0; float amp = 0, pl = .7f, pr = .7f; int dir = 1; };
    struct HVoice { double mark = 0, aM = -1; bool hasTgt = false; float tgt = 50, cur = 50, det = 0, pl = .7f, pr = .7f; std::array<Grain, 32> g; };

    float sr = 48000;
    std::vector<float> buf, obuf, dec, yd;
    int64_t w = 0, dw = 0; float dacc = 0; int dn = 0, dhop = 0;
    float lastF = 140, curMidi = 50, tgtMidi = 50;
    double mark = 0, aMark = -1;
    float holdC = 0, hL = 0, hR = 0;
    int64_t stT = 0, stBase = 0, stLen = 1, stPh = 0; bool stOn = false;
    double cmark = 0; float caS = 0, hbS = 0;
    std::array<Grain, 56> main;
    std::array<CGrain, 24> cloud;
    std::array<HVoice, 2> hv;
    uint32_t seed = 987654321u;
    const std::array<int, 8> INTV { 0, 12, -12, 7, 19, -5, 24, 5 };
    const std::array<int, 11> HI { 0, -7, -5, -4, -3, -2, 2, 3, 4, 5, 7 };

    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed / 4294967296.0); }
    static float rd (const std::vector<float>& b, double pos)
    {
        const double fl = std::floor (pos); const int64_t i = (int64_t) fl; const float f = (float) (pos - fl);
        const float a = b[(size_t) ((uint64_t) i & (uint64_t) M)], c = b[(size_t) ((uint64_t) (i + 1) & (uint64_t) M)];
        return a + (c - a) * f;
    }
    static const std::vector<int>& scaleSet (int s)
    {
        static const std::vector<int> S[6] = { {}, { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,3,5,7,10 }, { 0,1,4,5,7,8,10 } };
        return S[std::clamp (s, 0, 5)];
    }
    float snapTo (float m) const
    {
        const auto& sc = scaleSet (p.scale); float best = m, bd = 99; const int base = (int) std::floor (m) - 12;
        for (int n = base; n <= base + 24; ++n) { const int pc = ((n - p.key) % 12 + 12) % 12; if (std::find (sc.begin(), sc.end(), pc) == sc.end()) continue; const float d = std::abs (n - m); if (d < bd) { bd = d; best = (float) n; } }
        return best;
    }
    float degShift (float m, int deg) const
    {
        const auto& sc = scaleSet (p.scale); const int n = (int) std::round (m); const float fine = m - n;
        if (sc.size() == 7)
        {
            const int pc = ((n - p.key) % 12 + 12) % 12;
            auto it = std::find (sc.begin(), sc.end(), pc);
            if (it != sc.end())
            {
                const int idx = (int) (it - sc.begin()), base = n - sc[(size_t) idx], tot = idx + deg;
                const int o = (int) std::floor (tot / 7.0), di = ((tot % 7) + 7) % 7;
                return (float) (base + sc[(size_t) di] + 12 * o) + fine;
            }
        }
        const int a = std::abs (deg); const int semi = a == 2 ? 4 : a == 3 ? 5 : a == 4 ? 7 : a == 5 ? 9 : 12;
        return m + (deg < 0 ? -semi : semi);
    }
    void detect()
    {
        const float sr4 = sr / 4; const int W = 320;
        const int minL = std::max (6, (int) (sr4 / 1100)), maxL = std::min (420 - 1, (int) (sr4 / 65));
        const int64_t st = dw - W - maxL;
        auto D = [&] (int64_t i) { return dec[(size_t) ((uint64_t) i & 2047u)]; };
        float e = 0; for (int i = 0; i < W; ++i) { const float x = D (st + maxL + i); e += x * x; }
        const float r = std::sqrt (e / W);
        float run = 0; yd[0] = 1;
        for (int t = 1; t <= maxL; ++t) { float s = 0; for (int i = 0; i < W; ++i) { const float a = D (st + i) - D (st + i + t); s += a * a; } run += s; yd[(size_t) t] = run > 0 ? s * t / run : 1; }
        int best = -1; float bv = 1;
        for (int t = minL; t <= maxL; ++t) { if (yd[(size_t) t] < 0.2f) { while (t + 1 <= maxL && yd[(size_t) t + 1] < yd[(size_t) t]) ++t; best = t; bv = yd[(size_t) t]; break; } }
        if (best < 0) for (int t = minL; t <= maxL; ++t) if (yd[(size_t) t] < bv) { bv = yd[(size_t) t]; best = t; }
        float tauv = (float) best;
        if (best > minL && best < maxL) { const float a = yd[(size_t) best - 1], b = yd[(size_t) best], c = yd[(size_t) best + 1], den = a - 2 * b + c; if (std::abs (den) > 1e-9f) tauv = best + 0.5f * (a - c) / den; }
        rms = r; const float conf = 1 - bv; const float gate = 0.0025f;
        if (r > gate && conf > (voiced ? 0.55f : 0.75f) && tauv > 0) { lastF = sr4 / tauv; voiced = true; }
        else if (r < gate * 0.6f || conf < 0.45f) voiced = false;
        if (! voiced) return;
        const float m = 69 + 12 * std::log2 (lastF / 440.0f); inMidi = m;
        float t = m + p.shift;
        if (p.scale > 0) t = snapTo (t);
        tgtMidi = t;
        for (int v = 0; v < 2; ++v)
        {
            const int idx = v ? p.h2 : p.h1; auto& H = hv[v]; const bool was = H.hasTgt;
            if (idx > 0) { H.tgt = degShift (t, HI[(size_t) std::clamp (idx, 0, 10)]); H.hasTgt = true; if (! was) H.cur = H.tgt; }
            else H.hasTgt = false;
        }
    }
};
