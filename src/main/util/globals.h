#pragma once

namespace globals {
    inline constexpr int MaxFramesInFlight = 2;
#ifdef NDEBUG
    inline bool Debug = false;
#else
    inline bool Debug = true;
#endif
}
