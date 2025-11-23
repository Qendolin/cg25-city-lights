#include "Annotation.h"

#include "../util/Logger.h"
util::ScopedCommandLabel::~ScopedCommandLabel() {
    if (mCount == 0)
        return;
    if (mCount == 1 && mCmd) {
        end();
        return;
    }

    Logger::fatal("Command debug label start and end mismatch");
}
