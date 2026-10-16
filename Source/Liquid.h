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
    // Shading: metaball height field -> normals (+ moving ripples) -> reflected studio environment,
    // specular glints, soft back-rim, anti-aliased edge from the field's signed distance.
    void render (juce::Image& img, float hue, float throat, float crush, float time, float energy)
    {
        const int G = img.getWidth();
        if ((int) field.size() != G * G) { field.assign ((size_t) (G * G), 0); tmp = field; hgt = field; }
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
        blur (field, std::max (1, (int) std::round (G / 90.0)), 2);

        // height: distance-to-edge droplet profile (smooth by construction) + travelling ripples
        const float T = 0.42f, INF = 1e6f;
        if ((int) dist.size() != G * G) dist.assign ((size_t) (G * G), 0);
        for (int i = 0; i < G * G; ++i) dist[(size_t) i] = field[(size_t) i] > T ? INF : 0;
        const float D1 = 1.0f, D2 = 1.4142f;
        for (int py0 = 1; py0 < G - 1; ++py0) for (int px0 = 1; px0 < G - 1; ++px0)
        { float& d = dist[(size_t) (py0 * G + px0)]; if (d == 0) continue;
          d = std::min ({ d, dist[(size_t) (py0 * G + px0 - 1)] + D1, dist[(size_t) ((py0 - 1) * G + px0)] + D1, dist[(size_t) ((py0 - 1) * G + px0 - 1)] + D2, dist[(size_t) ((py0 - 1) * G + px0 + 1)] + D2 }); }
        for (int py0 = G - 2; py0 > 0; --py0) for (int px0 = G - 2; px0 > 0; --px0)
        { float& d = dist[(size_t) (py0 * G + px0)]; if (d == 0) continue;
          d = std::min ({ d, dist[(size_t) (py0 * G + px0 + 1)] + D1, dist[(size_t) ((py0 + 1) * G + px0)] + D1, dist[(size_t) ((py0 + 1) * G + px0 + 1)] + D2, dist[(size_t) ((py0 + 1) * G + px0 - 1)] + D2 }); }
        const float Dm = G * 0.2f;
        const float amp = (0.025f + std::min (1.5f, energy) * 0.07f) * Dm;
        for (int py0 = 0; py0 < G; ++py0)
        {
            const float w = (py0 + 0.5f) / G * 2 - 1;
            for (int px0 = 0; px0 < G; ++px0)
            {
                const int i = py0 * G + px0; const float d = std::min (dist[(size_t) i], 1e5f);
                if (d <= 0) { hgt[(size_t) i] = 0; continue; }
                const float dd = std::min (d, Dm), k1 = 1 - dd / Dm, hh = Dm * (1 - k1 * k1);
                const float u = (px0 + 0.5f) / G * 2 - 1, r = std::sqrt (u * u + w * w);
                const float rip = std::sin (u * 11 + time * 2.3f) * std::sin (w * 9 - time * 1.9f) * 0.6f
                                + std::sin ((u * 0.7f - w) * 17 + time * 3.1f) * 0.3f
                                + std::sin (r * 24 - time * 7.0f) * std::exp (-r * 2.2f) * std::min (1.5f, energy) * 0.5f;
                hgt[(size_t) i] = hh + rip * amp * std::min (1.0f, d / (Dm * 0.6f));
            }
        }
        blur (hgt, std::max (2, (int) std::round (G / 40.0)), 2);

        const int lv = crush > 0.04f ? std::max (2, (int) std::round (16 - crush * 14)) : 0; const float q = lv ? 1.0f / (lv - 1) : 0;
        const float ns = 0.5f;
        const float envShift = std::sin (time * 0.23f) * 0.25f;
        auto sstep = [] (float e0, float e1, float xx) { float t = (xx - e0) / (e1 - e0); t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); };
        const float L1x = -0.45f, L1y = -0.68f, L1z = 0.58f, L2x = 0.62f, L2y = 0.28f, L2z = 0.73f;
        int bx0 = G, bx1 = -1, by0 = G, by1 = -1;
        for (int py0 = 0; py0 < G; ++py0) for (int px0 = 0; px0 < G; ++px0)
            if (field[(size_t) (py0 * G + px0)] > T * 0.8f) { bx0 = std::min (bx0, px0); bx1 = std::max (bx1, px0); by0 = std::min (by0, py0); by1 = std::max (by1, py0); }
        img.clear (img.getBounds());
        if (bx1 < 0) return;
        bx0 = std::max (1, bx0 - 2); by0 = std::max (1, by0 - 2); bx1 = std::min (G - 2, bx1 + 2); by1 = std::min (G - 2, by1 + 2);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int py0 = by0; py0 <= by1; ++py0)
        {
            for (int px0 = bx0; px0 <= bx1; ++px0)
            {
                const int i = py0 * G + px0; const float v = field[(size_t) i];
                // anti-aliased coverage from the signed distance to the iso-line, in pixels
                float a = 0;
                if (px0 > 0 && px0 < G - 1 && py0 > 0 && py0 < G - 1)
                {
                    const float fx = (field[(size_t) i + 1] - field[(size_t) i - 1]) * 0.5f, fy = (field[(size_t) (i + G)] - field[(size_t) (i - G)]) * 0.5f;
                    const float gm = std::max (1e-4f, std::sqrt (fx * fx + fy * fy));
                    a = juce::jlimit (0.0f, 1.0f, (v - T) / gm * 0.55f + 0.5f);
                }
                if (a <= 0.0f) { bd.setPixelColour (px0, py0, juce::Colours::transparentBlack); continue; }

                float nx = 0, ny = 0;
                if (px0 > 0 && px0 < G - 1 && py0 > 0 && py0 < G - 1)
                { nx = -(hgt[(size_t) i + 1] - hgt[(size_t) i - 1]) * ns; ny = -(hgt[(size_t) (i + G)] - hgt[(size_t) (i - G)]) * ns; }
                // at the rim, bend the normal outward so the edge curves away like a droplet
                const float edge = 1 - sstep (0.0f, 0.12f, v - T);
                if (edge > 0)
                {
                    const float fx = field[(size_t) std::min (i + 1, G * G - 1)] - field[(size_t) std::max (i - 1, 0)];
                    const float fy = field[(size_t) std::min (i + G, G * G - 1)] - field[(size_t) std::max (i - G, 0)];
                    const float gm = std::max (1e-4f, std::sqrt (fx * fx + fy * fy));
                    juce::ignoreUnused (fx, fy, gm);
                }
                float nz = 1; { const float nl = 1 / std::sqrt (nx * nx + ny * ny + nz * nz); nx *= nl; ny *= nl; nz *= nl; }

                // reflect the view ray (0,0,1) and look up the studio environment
                const float rx = 2 * nz * nx, ry = 2 * nz * ny, rz = 2 * nz * nz - 1;
                const float e = -ry;                       // elevation (up = +)
                float cr, cg, cb;
                {
                    const float sk = sstep (0.0f, 0.85f, e);
                    const float sr = 0.42f + 0.58f * sk, sg = 0.5f + 0.5f * sk, sb = 0.64f + 0.36f * sk;
                    const float gd = sstep (0.0f, -0.9f, e);
                    const float gr = hueR * (0.95f - 0.7f * gd), gg = hueG * (0.95f - 0.7f * gd), gb = hueB * (0.95f - 0.7f * gd);
                    const float up = sstep (-0.22f, 0.3f, e);
                    cr = gr + (sr - gr) * up; cg = gg + (sg - gg) * up; cb = gb + (sb - gb) * up;
                    const float hz = std::exp (-((e + 0.04f) * (e + 0.04f)) / (0.05f * 0.05f)) * 0.45f;           // dark horizon line
                    cr += (0.03f - cr) * hz; cg += (0.05f - cg) * hz; cb += (0.10f - cb) * hz;
                    const float az = rx + envShift;
                    const float box1 = sstep (0.2f, 0.06f, std::abs (az - 0.42f)) * sstep (-0.05f, 0.3f, e) * 0.8f;   // softbox reflections
                    const float box2 = sstep (0.1f, 0.03f, std::abs (az + 0.58f)) * sstep (0.1f, 0.4f, e) * 0.55f;
                    cr += box1 + box2; cg += box1 + box2; cb += box1 + box2;
                    const float back = sstep (-0.15f, -0.65f, rz);                                // rays heading away: dark dome
                    juce::ignoreUnused (back);
                }
                // metal tint + specular glints + soft coloured back-rim
                auto spec = [&] (float lx, float ly, float lz)
                {
                    float hx = lx, hy = ly, hz = lz + 1; const float hl = 1 / std::sqrt (hx * hx + hy * hy + hz * hz); hx *= hl; hy *= hl; hz *= hl;
                    float d = std::max (0.0f, nx * hx + ny * hy + nz * hz);
                    const float d2 = d * d, d4 = d2 * d2, d8 = d4 * d4, d16 = d8 * d8, d32 = d16 * d16, d64 = d32 * d32;
                    return d64 * d64 * 1.4f + d32 * 0.18f;
                };
                const float s1 = spec (L1x, L1y, L1z), s2 = spec (L2x, L2y, L2z) * 0.45f;
                const float rim = 0.0f;
                const float vol = 0.8f + 0.2f * sstep (0.0f, Dm * 0.55f, std::min (dist[(size_t) i], Dm));   // broad, soft body shading
                float r = cr * 0.94f * vol + s1 + s2 + rim * hue2R, g = cg * 0.95f * vol + s1 + s2 + rim * hue2G, b = cb * vol + s1 + s2 + rim * hue2B;
                if (lv) { r = std::round (r / q) * q; g = std::round (g / q) * q; b = std::round (b / q) * q; }
                bd.setPixelColour (px0, py0, juce::Colour::fromFloatRGBA (std::min (r, 1.0f), std::min (g, 1.0f), std::min (b, 1.0f), a));
            }
        }
#if VW_DEBUG
        { float sr=0; for(int i=0;i<N;++i) sr+=std::hypot(x[(size_t)i],y[(size_t)i]); fprintf(stderr,"render meanR=%f\n",sr/std::max(1,N)); }
#endif
    }

    bool ok = false;

private:
    int N = 0, nc = 0;
    float h = 0.12f, d = 0.045f, k = 60, kn = 150, Rb = 0.93f, rho0 = 1;
    std::vector<float> x, y, vx, vy, px, py, field, tmp, hgt, dist;
    float hueR = .2f, hueG = .4f, hueB = 1, hue2R = .8f, hue2G = .3f, hue2B = 1;
    std::vector<int> head, next;
    std::vector<float> lut = std::vector<float> (256 * 3, 0.0f); float lutHue = -100;
    std::mt19937 rng { 1234 }; std::uniform_real_distribution<float> uni { 0.0f, 1.0f };

    // separable running-sum box blur (radius r), repeated `iters` times ~ gaussian; cost independent of r
    void blur (std::vector<float>& f, int r, int iters = 2)
    {
        const int G = (int) std::lround (std::sqrt ((double) f.size()));
        if (r < 1) return;
        const float inv = 1.0f / (2 * r + 1);
        for (int it = 0; it < iters; ++it)
        {
            for (int y0 = 0; y0 < G; ++y0)
            {
                const float* src = f.data() + y0 * G; float* dst = tmp.data() + y0 * G; float acc = 0;
                for (int k = -r; k <= r; ++k) acc += src[juce::jlimit (0, G - 1, k)];
                for (int x0 = 0; x0 < G; ++x0) { dst[x0] = acc * inv; acc += src[std::min (G - 1, x0 + r + 1)] - src[std::max (0, x0 - r)]; }
            }
            for (int x0 = 0; x0 < G; ++x0)
            {
                float acc = 0;
                for (int k = -r; k <= r; ++k) acc += tmp[(size_t) (juce::jlimit (0, G - 1, k) * G + x0)];
                for (int y0 = 0; y0 < G; ++y0) { f[(size_t) (y0 * G + x0)] = acc * inv; acc += tmp[(size_t) (std::min (G - 1, y0 + r + 1) * G + x0)] - tmp[(size_t) (std::max (0, y0 - r) * G + x0)]; }
            }
        }
    }
    int cellX (float v) const { int c = (int) std::floor ((v + 1.2f) / h); return c < 0 ? 0 : c >= nc ? nc - 1 : c; }
    int cellOf (float xx, float yy) const { return cellX (yy) * nc + cellX (xx); }

    void buildLut (float hue)
    {
        lutHue = hue;
        const float hh = std::fmod (hue, 360.0f) / 360.0f;
        { const auto c1 = juce::Colour::fromHSL (hh, 0.85f, 0.5f, 1.0f), c2 = juce::Colour::fromHSL (std::fmod (hh + 0.12f, 1.0f), 0.95f, 0.62f, 1.0f);
          hueR = c1.getFloatRed(); hueG = c1.getFloatGreen(); hueB = c1.getFloatBlue(); hue2R = c2.getFloatRed(); hue2G = c2.getFloatGreen(); hue2B = c2.getFloatBlue(); }
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
