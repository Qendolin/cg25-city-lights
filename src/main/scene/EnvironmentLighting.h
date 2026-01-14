#pragma once

#include <glm/glm.hpp>

namespace lighting {

    inline glm::vec3 sunLightFromElevation(float elevationDeg) {
        elevationDeg = glm::clamp(elevationDeg, -1.0f, 90.0f);

        float visibility = glm::smoothstep(-1.0f, 0.0f, elevationDeg);

        if (visibility <= 0.0f)
            return glm::vec3(0.0f);

        // Zenith angle
        float zenithDeg = 90.0f - elevationDeg;
        float zenithRad = glm::radians(zenithDeg);

        // Kasten–Young air mass model
        float airMass = 1.0f / (std::cos(zenithRad) + 0.50572f * std::pow(96.07995f - zenithDeg, -1.6364f));

        // --- Optical depths (clear Central European day) ---
        const glm::vec3 betaRayleigh(5.8e-6f, 13.5e-6f, 33.1e-6f);
        const glm::vec3 betaMie(21e-6f);
        const glm::vec3 betaOzone(0.65e-6f, 1.15e-6f, 0.35e-6f);

        glm::vec3 tau = betaRayleigh * airMass + betaMie * airMass + betaOzone * airMass;

        glm::vec3 transmittance;
        transmittance.r = std::exp(-tau.r);
        transmittance.g = std::exp(-tau.g);
        transmittance.b = std::exp(-tau.b);

        // Extraterrestrial sun color (slightly warm)
        const glm::vec3 solarColor(1.0f, 0.98f, 0.95f);


        glm::vec3 radiance = solarColor * transmittance;

        return radiance * visibility;
    }

    inline glm::vec3 ambientSkyLightFromElevation(float elevationDeg) {
        elevationDeg = glm::clamp(elevationDeg, -18.0f, 90.0f);

        // --- Reference colors (linear RGB) ---
        // Deep night sky (no moon, clear)
        const glm::vec3 nightColor(0.01f, 0.01f, 0.02f);

        // Twilight sky (desaturated blue)
        const glm::vec3 twilightColor(0.15f, 0.20f, 0.35f);

        // Clear daylight zenith
        const glm::vec3 dayColor(0.45f, 0.60f, 1.00f);

        // --- Blend factors ---
        float nightToTwilight = glm::smoothstep(-18.0f, -6.0f, elevationDeg);
        float twilightToDay = glm::smoothstep(-6.0f, 10.0f, elevationDeg);

        glm::vec3 color = glm::mix(nightColor, twilightColor, nightToTwilight);

        color = glm::mix(color, dayColor, twilightToDay);

        // Slight desaturation near horizon
        float horizonFade = glm::smoothstep(-2.0f, 10.0f, elevationDeg);
        color = glm::mix(glm::vec3(glm::dot(color, glm::vec3(0.333f))), color, horizonFade);

        return color;
    }
} // namespace lighting
