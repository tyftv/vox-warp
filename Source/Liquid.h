#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <cmath>
#include <random>

// Liquid chrome: a particle fluid (double-density relaxation) inside the dome,
// rendered as a smoothed metaball field with an environment-mapped chrome shade.
class Liquid
{
public:
    struct Forces { float gx = 0, gy = 0, mag = 5, spike = 10, swirl = 0, noise = 0, damp = 0.996f; };

    void init()
    {
        N = 330;
        x.assign (N, 0); y = vx = vy = px = py = x;
        nc = (int) std::ceil (2.4f / h); head.assign ((size_t) (nc * nc), -1); next.assign ((size_t) N, -1);
        int i = 0;
        for (int row = 0; i < N && row < 200; ++row)
        {
            const float yy = 0.45f - row * d * 0.866f;
            for (int col = 0; col < 60 && i < N; ++col)
            {
                const float xx = -0.7f + col * d + (row % 2) * d / 2;
                if (xx * xx + yy * yy < 0.5f * 0.5f) { x[(size_t) i] = xx; y[(size_t) i] = yy; ++i; }
            }
        }
        N = i;
        float r0 = 0;
        for (int a = -6; a <= 6; ++a) for (int b = -6; b <= 6; ++b)
        {
            if (! a && ! b) continue;
            const float xx = a * d + (b % 2 ? d / 2 : 0), yy = b * d * 0.866f, q = std::hypot (xx, yy) / h;
            if (q < 1) r0 += (1 - q) * (1 - q);
        }
        rho0 = r0 * 1.05f; ok = true;
    }

    void step (float dt, const Forces& F, const float* bands, int NB)
    {
        const float dt2 = dt * dt;
        float bmean = 0; for (int b = 0; b < NB; ++b) bmean += bands[b]; bmean /= (float) juce::jmax (1, NB);
        for (int i = 0; i < N; ++i)
        {
            const float r = std::hypot (x[(size_t) i], y[(size_t) i]) + 1e-6f, ux = x[(size_t) i] / r, uy = y[(size_t) i] / r;
            float ax = F.gx, ay = F.gy;
            const float m = F.mag * std::min (1.0f, r * 3); ax -= ux * m; ay -= uy * m;
            if (F.spike > 0 && r > 0.12f)
            {
                float a = std::atan2 (uy, ux) / 6.2831853f; if (a < 0) a += 1;
                const float mb = a < 0.5f ? a * 2 : 2 - a * 2;
                const float e = juce::jlimit (0.0f, 1.0f, (bands[std::min (NB - 1, (int) (mb * NB))] - bmean) * 2.5f + 0.15f);
                const float pp = e * e * F.spike; ax += ux * pp; ay += uy * pp;
            }
            if (F.swirl != 0) { ax += -uy * F.swirl; ay += ux * F.swirl; }
            if (F.noise > 0) { ax += (uni (rng) - 0.5f) * F.noise; ay += (uni (rng) - 0.5f) * F.noise; }
            vx[(size_t) i] = (vx[(size_t) i] + ax * dt) * F.damp; vy[(size_t) i] = (vy[(size_t) i] + ay * dt) * F.damp;
            px[(size_t) i] = x[(size_t) i]; py[(size_t) i] = y[(size_t) i];
            x[(size_t) i] += vx[(size_t) i] * dt; y[(size_t) i] += vy[(size_t) i] * dt;
        }
        std::fill (head.begin(), head.end(), -1);
        for (int i = 0; i < N; ++i)
        {
            const int c = cellOf (x[(size_t) i], y[(size_t) i]);
            next[(size_t) i] = head[(size_t) c]; head[(size_t) c] = i;
        }
        const float h2 = h * h;
        for (int i = 0; i < N; ++i)
        {
            const float xi = x[(size_t) i], yi = y[(size_t) i];
            const int cx = cellX (xi), cy = cellX (yi);
            float rho = 0, rhon = 0;
            for (int oy = -1; oy <= 1; ++oy) { const int yy = cy + oy; if (yy < 0 || yy >= nc) continue;
                for (int ox = -1; ox <= 1; ++ox) { const int xx = cx + ox; if (xx < 0 || xx >= nc) continue;
                    for (int j = head[(size_t) (yy * nc + xx)]; j >= 0; j = next[(size_t) j]) { if (j == i) continue;
                        const float dx = x[(size_t) j] - xi, dy = y[(size_t) j] - yi, r2 = dx * dx + dy * dy;
                        if (r2 < h2) { const float q = 1 - std::sqrt (r2) / h; rho += q * q; rhon += q * q * q; } } } }
            const float P = k * (rho - rho0), Pn = kn * rhon; float mx = 0, my = 0;
            for (int oy = -1; oy <= 1; ++oy) { const int yy = cy + oy; if (yy < 0 || yy >= nc) continue;
                for (int ox = -1; ox <= 1; ++ox) { const int xx = cx + ox; if (xx < 0 || xx >= nc) continue;
                    for (int j = head[(size_t) (yy * nc + xx)]; j >= 0; j = next[(size_t) j]) { if (j == i) continue;
                        const float dx = x[(size_t) j] - xi, dy = y[(size_t) j] - yi, r2 = dx * dx + dy * dy;
                        if (r2 < h2 && r2 > 1e-12f) { const float r = std::sqrt (r2), q = 1 - r / h, D = dt2 * (P * q + Pn * q * q) * 0.5f / r;
                            const float Dx = D * dx, Dy = D * dy; x[(size_t) j] += Dx; y[(size_t) j] += Dy; mx -= Dx; my -= Dy; } } } }
            x[(size_t) i] += mx; y[(size_t) i] += my;
        }
        for (int i = 0; i < N; ++i)
        {
            const float r = std::hypot (x[(size_t) i], y[(size_t) i]);
            if (r > Rb) { x[(size_t) i] *= Rb / r; y[(size_t) i] *= Rb / r; }
            float a = (x[(size_t) i] - px[(size_t) i]) / dt, b = (y[(size_t) i] - py[(size_t) i]) / dt;
            const float sp = std::hypot (a, b); if (sp > 5) { a *= 5 / sp; b *= 5 / sp; }
            if (! std::isfinite (a) || ! std::isfinite (b)) { a = b = 0; x[(size_t) i] = px[(size_t) i]; y[(size_t) i] = py[(size_t) i]; }
            vx[(size_t) i] = a; vy[(size_t) i] = b;
        }
    }

    void kick (float amt)
    {
        for (int i = 0; i < N; ++i)
        {
            const float r = std::hypot (x[(size_t) i], y[(size_t) i]) + 1e-3f, kk = amt * (0.8f + uni (rng) * 1.6f);
            vx[(size_t) i] += x[(size_t) i] / r * kk + (uni (rng) - 0.5f) * amt;
            vy[(size_t) i] += y[(size_t) i] / r * kk + (uni (rng) - 0.5f) * amt;
        }
    }

    // Render into an ARGB image of size G×G covering sim space [-1,1].
    void render (juce::Image& img, float hue, float throat, float crush, bool lowRes)
    {
        const int G = img.getWidth();
        if ((int) field.size() != G * G) { field.assign ((size_t) (G * G), 0); tmp = field; }
        if (std::abs (hue - lutHue) > 2) buildLut (hue);
        std::fill (field.begin(), field.end(), 0.0f);
        const float rk = 0.072f * (0.85f + throat * 0.4f), rkc = rk * G / 2, irk = 1 / (rkc * rkc); const int R = (int) std::ceil (rkc);
        for (int i = 0; i < N; ++i)
        {
            const float gx = (x[(size_t) i] + 1) * 0.5f * G, gy = (y[(size_t) i] + 1) * 0.5f * G;
            const int x0 = std::max (0, (int) std::floor (gx - R)), x1 = std::min (G - 1, (int) std::ceil (gx + R));
            const int y0 = std::max (0, (int) std::floor (gy - R)), y1 = std::min (G - 1, (int) std::ceil (gy + R));
            for (int cy = y0; cy <= y1; ++cy) { const float dy = cy + 0.5f - gy; float* row = field.data() + cy * G;
                for (int cx = x0; cx <= x1; ++cx) { const float dx = cx + 0.5f - gx, d2 = (dx * dx + dy * dy) * irk; if (d2 < 1) { const float w = 1 - d2; row[cx] += w * w; } } }
        }
        const int passes = std::max (2, (int) std::round (G / 70.0));
        for (int pass = 0; pass < passes; ++pass)
        {
            for (int py0 = 0; py0 < G; ++py0) { const int row = py0 * G; for (int px0 = 0; px0 < G; ++px0) { const int a = px0 > 0 ? px0 - 1 : 0, b = px0 < G - 1 ? px0 + 1 : G - 1; float s = 0; for (int q = a; q <= b; ++q) s += field[(size_t) (row + q)]; tmp[(size_t) (row + px0)] = s / (b - a + 1); } }
            for (int px0 = 0; px0 < G; ++px0) for (int py0 = 0; py0 < G; ++py0) { const int a = py0 > 0 ? py0 - 1 : 0, b = py0 < G - 1 ? py0 + 1 : G - 1; float s = 0; for (int q = a; q <= b; ++q) s += tmp[(size_t) (q * G + px0)]; field[(size_t) (py0 * G + px0)] = s / (b - a + 1); }
        }
        const float T = 0.42f, ns = 7.0f * G / 132.0f, cap = T + 1.6f;
        const int lv = crush > 0.04f ? std::max (2, (int) std::round (16 - crush * 14)) : 0; const float q = lv ? 255.0f / (lv - 1) : 0;
        const float Lx = -0.42f, Ly = -0.62f, Lz = 0.66f;
        auto cl = [&] (float u) { u = (u > cap ? cap : u) - T; return u > 0 ? std::sqrt (u) : 0.0f; };
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int py0 = 0; py0 < G; ++py0)
        {
            for (int px0 = 0; px0 < G; ++px0)
            {
                const int i = py0 * G + px0; const float v = field[(size_t) i];
                if (v < T - 0.14f) { bd.setPixelColour (px0, py0, juce::Colours::transparentBlack); continue; }
                const float gx = (px0 > 0 && px0 < G - 1) ? cl (field[(size_t) i + 1]) - cl (field[(size_t) i - 1]) : 0;
                const float gy = (py0 > 0 && py0 < G - 1) ? cl (field[(size_t) (i + G)]) - cl (field[(size_t) (i - G)]) : 0;
                float nx = -gx * ns, ny = -gy * ns, nz = 1; const float nl = 1 / std::sqrt (nx * nx + ny * ny + 1); nx *= nl; ny *= nl; nz *= nl;
                float t = 0.47f + ny * 0.75f + ((float) py0 / G - 0.5f) * 0.22f; t = t < 0 ? 0 : t > 1 ? 1 : t;
                const int li = (int) (t * 255);
                const float sp = nx * Lx + ny * Ly + nz * Lz, spec = sp > 0.6f ? std::pow (sp, 18.0f) * 300 : 0, rim = (1 - nz) * 70;
                float r = lut[(size_t) li * 3] + spec + rim * 0.3f, g = lut[(size_t) li * 3 + 1] + spec + rim * 0.6f, b = lut[(size_t) li * 3 + 2] + spec + rim;
                if (lv) { r = std::round (r / q) * q; g = std::round (g / q) * q; b = std::round (b / q) * q; }
                float a = (v - (T - 0.14f)) / 0.28f; a = a > 1 ? 1 : a; a = a * a * (3 - 2 * a);
                bd.setPixelColour (px0, py0, juce::Colour::fromFloatRGBA (std::min (r, 255.0f) / 255, std::min (g, 255.0f) / 255, std::min (b, 255.0f) / 255, a));
            }
        }
        juce::ignoreUnused (lowRes);
#if VW_DEBUG
        { float mx=0; for (auto v: field) mx=std::max(mx,v); float sx=0; for(int i=0;i<N;++i) sx+=std::abs(x[(size_t)i]); float sr=0; for(int i=0;i<N;++i) sr+=std::hypot(x[(size_t)i],y[(size_t)i]); fprintf(stderr,"render meanR=%f\n",sr/std::max(1,N)); }
#endif
    }

    bool ok = false;

private:
    int N = 0, nc = 0;
    float h = 0.12f, d = 0.045f, k = 60, kn = 150, Rb = 0.93f, rho0 = 1;
    std::vector<float> x, y, vx, vy, px, py, field, tmp;
    std::vector<int> head, next;
    std::vector<float> lut = std::vector<float> (256 * 3, 0.0f); float lutHue = -100;
    std::mt19937 rng { 1234 }; std::uniform_real_distribution<float> uni { 0.0f, 1.0f };

    int cellX (float v) const { int c = (int) std::floor ((v + 1.2f) / h); return c < 0 ? 0 : c >= nc ? nc - 1 : c; }
    int cellOf (float xx, float yy) const { return cellX (yy) * nc + cellX (xx); }

    void buildLut (float hue)
    {
        lutHue = hue;
        const float hh = std::fmod (hue, 360.0f) / 360.0f;
        struct Stop { float t; juce::Colour c; };
        const Stop st[] { { 0, juce::Colours::white }, { .26f, juce::Colour::fromHSL (hh, .45f, .86f, 1) }, { .45f, juce::Colour (0xff5a6a84) }, { .52f, juce::Colour (0xff0a101b) },
                          { .64f, juce::Colour::fromHSL (hh, .85f, .40f, 1) }, { .86f, juce::Colour::fromHSL (hh, .60f, .78f, 1) }, { 1, juce::Colours::white } };
        for (int i = 0; i < 256; ++i)
        {
            const float t = i / 255.0f; int s = 0; while (s < 5 && t > st[s + 1].t) ++s;
            const float f = (t - st[s].t) / (st[s + 1].t - st[s].t);
            const auto c = st[s].c.interpolatedWith (st[s + 1].c, juce::jlimit (0.0f, 1.0f, f));
            lut[(size_t) i * 3] = c.getRed(); lut[(size_t) i * 3 + 1] = c.getGreen(); lut[(size_t) i * 3 + 2] = c.getBlue();
        }
    }
};
