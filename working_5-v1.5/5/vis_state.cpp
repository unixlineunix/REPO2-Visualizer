#include "vis_state.hxx"

#include <algorithm>
#include <cmath>

Color3 resolveColor(const VisState& state, Color3 base, float t) {
    const float brightness = std::clamp(std::max({base.r, base.g, base.b}), 0.3f, 1.0f);

    switch (state.colorMode) {
        case ColorMode::Normal:
            return base;

        case ColorMode::RandomSolid:
            return {state.randomSolid.r * brightness,
                    state.randomSolid.g * brightness,
                    state.randomSolid.b * brightness};

        case ColorMode::RandomGradient: {
            const auto& colors = state.gradientColors;
            if (colors.empty()) {
                return base;
            }
            if (colors.size() == 1) {
                return {colors[0].r * brightness, colors[0].g * brightness, colors[0].b * brightness};
            }

            const float scaled = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(colors.size() - 1);
            const size_t i0 = static_cast<size_t>(scaled);
            const size_t i1 = std::min(i0 + 1, colors.size() - 1);
            const float frac = scaled - static_cast<float>(i0);

            const Color3 c{
                colors[i0].r + (colors[i1].r - colors[i0].r) * frac,
                colors[i0].g + (colors[i1].g - colors[i0].g) * frac,
                colors[i0].b + (colors[i1].b - colors[i0].b) * frac
            };

            return {c.r * brightness, c.g * brightness, c.b * brightness};
        }
    }

    return base;
}