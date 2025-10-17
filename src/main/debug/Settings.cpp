#include "Settings.h"

#include "../entity/ShadowCaster.h"

void Settings::Shadow::applyTo(ShadowCaster &caster) const {
    caster.dimension = dimension;
    caster.start = start;
    caster.end = end;
    caster.depthBiasConstant = depthBiasConstant;
    caster.depthBiasSlope = depthBiasSlope;
    caster.depthBiasClamp = depthBiasClamp;
    caster.sampleBias = sampleBias;
    caster.sampleBiasClamp = sampleBiasClamp;
    caster.normalBias = normalBias;
    caster.extrusionBias = extrusionBias;
}
