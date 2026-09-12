#ifndef FE_NOISE_H
#define FE_NOISE_H

// Cheap deterministic value noise + fBm, for the procedural marble surface
// passes (frontgui_ingame_relief.cpp's panel face, frontgui_widgets.cpp's
// menu-list background). Header-only / inline -- both call sites keep their
// own marble *parameters* (frequency, warp amp, colours), just share the
// field. docs/refactor/ingame-gui/10-maintainability-refactors.md §2.
//
// C++ only.

#ifdef __cplusplus

#include <cmath>

namespace fe {

inline float noise_hash(int x, int y)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) * (1.0f / 65535.0f);
}

// Bilinear-interpolated value noise, one period per integer cell.
inline float value_noise(float x, float y)
{
    const float fx = std::floor(x), fy = std::floor(y);
    const int xi = (int)fx, yi = (int)fy;
    const float xf = x - fx, yf = y - fy;
    const float u = xf * xf * (3.0f - 2.0f * xf);
    const float v = yf * yf * (3.0f - 2.0f * yf);
    const float a = noise_hash(xi, yi),     b = noise_hash(xi + 1, yi);
    const float c = noise_hash(xi, yi + 1), d = noise_hash(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * v + (a - b + d - c) * u * v;
}

// 3-octave fractal Brownian motion, ~0..1.
inline float fbm(float x, float y)
{
    return 0.6f  * value_noise(x, y)
         + 0.3f  * value_noise(x * 2.1f + 5.2f, y * 2.1f + 1.3f)
         + 0.15f * value_noise(x * 4.3f + 9.1f, y * 4.3f + 7.7f);
}

} // namespace fe

#endif // __cplusplus
#endif // FE_NOISE_H
