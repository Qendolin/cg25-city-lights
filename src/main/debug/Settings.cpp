#include "Settings.h"

#include "../entity/ShadowCaster.h"

void Settings::Shadow::applyTo(ShadowCaster &caster) const {
    caster.depthBiasConstant = depthBiasConstant;
    caster.depthBiasSlope = depthBiasSlope;
    caster.depthBiasClamp = depthBiasClamp;
    caster.sampleBias = sampleBias;
    caster.sampleBiasClamp = sampleBiasClamp;
    caster.normalBias = normalBias;
    caster.extrusionBias = extrusionBias;
}
