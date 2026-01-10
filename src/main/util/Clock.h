#pragma once

namespace util {

    struct Clock {
        double interval;

        Clock(double interval) : interval(interval) {}

        bool isDue(double time) {
            double elapsed = time - mLastTime;
            bool due = elapsed >= interval || mLastTime < 0.0;
            if (due) {
                mLastTime = time;
            }
            return due;
        }

    private:
        double mLastTime = -1;
    };

} // namespace util
