#pragma once

#include "sdf.h"

namespace blob {

    struct BlobSdf : public Sdf {
    private:
        float time{}; // time in seconds, wraps at 1 to 0

    public:
        void advanceTime(float dt);
        float value(glm::vec3 p) const override;
    };

}
