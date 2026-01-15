#pragma once
#include <glm/glm.hpp>

namespace color {

    inline glm::vec3 oklch_to_rgb(glm::vec3 oklch) {
        // 1. Polar to Cartesian
        float L = oklch.x;
        float a = oklch.y * glm::cos(glm::radians(oklch.z));
        float b = oklch.y * glm::sin(glm::radians(oklch.z));

        // 2. OKLAB to Linear LMS (Matrix Multiply)
        glm::vec3 lms_ =
                glm::vec3(L + 0.3963377774f * a + 0.2158037573f * b, L - 0.1055613458f * a - 0.0638541728f * b,
                     L - 0.0894841775f * a - 1.2914855480f * b);

        // Cubing LMS
        glm::vec3 lms = lms_ * lms_ * lms_;

        // 3. LMS to Linear RGB
        // You can use a mat3 here for cleaner code
        glm::vec3 linRGB;
        linRGB.r = glm::dot(glm::vec3(4.0767416621f, -3.3077115913f, 0.2309699295f), lms);
        linRGB.g = glm::dot(glm::vec3(-1.2684380046f, 2.6097574011f, -0.3413193965f), lms);
        linRGB.b = glm::dot(glm::vec3(-0.0041960863f, -0.7034186147f, 1.7076147010f), lms);

        // 4. Gamma
        auto gamma = [](float x) {
            return (x <= 0.0031308f) ? (12.92f * x) : (1.055f * pow(x, 1.0f / 2.4f) - 0.055f);
        };

        return glm::clamp(glm::vec3(gamma(linRGB.r), gamma(linRGB.g), gamma(linRGB.b)), 0.0f, 1.0f);
    }

    inline glm::vec3 hsv_to_rgb(glm::vec3 hsv) {
        float h = hsv.x;
        float s = hsv.y;
        float v = hsv.z;

        // If saturation is 0, the color is grayscale
        if (s <= 0.0f) {
            return glm::vec3(v);
        }

        float sector = h / 60.0f;
        int i = static_cast<int>(floor(sector));
        float f = sector - i;

        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        glm::vec3 rgb;

        switch (i % 6) {
            case 0: rgb = glm::vec3(v, t, p); break;
            case 1: rgb = glm::vec3(q, v, p); break;
            case 2: rgb = glm::vec3(p, v, t); break;
            case 3: rgb = glm::vec3(p, q, v); break;
            case 4: rgb = glm::vec3(t, p, v); break;
            case 5: default:
                rgb = glm::vec3(v, p, q); break;
        }

        return glm::clamp(rgb, 0.0f, 1.0f);
    }

} // namespace myNamespace
