#pragma once

#include <tweeny/tweeny.h>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <cassert>
#include <stdexcept>

class Timeline {
public:
    using ms_t = std::int64_t;

private:
    // Runtime interface for started tweens
    struct IRun {
        virtual ~IRun() = default;
        virtual void step_ms(uint32_t dt_ms) = 0;
        virtual bool finished() const = 0;
        virtual uint32_t remaining_ms() const = 0;
    };

    // Concrete runtime wrapper for tweeny::tween<Ts...>
    template<typename Tw>
    struct RunHolder : IRun {
        Tw tw;
        uint32_t elapsed_ms = 0;

        explicit RunHolder(Tw t) : tw(std::move(t)), elapsed_ms(0) {}

        void step_ms(uint32_t dt_ms) override {
            if (dt_ms == 0) return;
            tw.step(static_cast<int32_t>(dt_ms));
            elapsed_ms += dt_ms;
        }

        bool finished() const override {
            uint32_t d = tw.duration();
            return (d > 0 && elapsed_ms >= d) || (tw.progress() >= 1.0f);
        }

        uint32_t remaining_ms() const override {
            uint32_t d = tw.duration();
            return (elapsed_ms < d) ? (d - elapsed_ms) : 0;
        }
    };

    // Proto holder base (type-erased)
    struct ProtoBase {
        virtual ~ProtoBase() = default;
        virtual std::unique_ptr<IRun> make_runtime() const = 0;
        virtual uint32_t duration_ms() const = 0;
    };

    template<typename Tw>
    struct ProtoHolder : ProtoBase {
        Tw proto;

        explicit ProtoHolder(Tw p) : proto(std::move(p)) {}

        std::unique_ptr<IRun> make_runtime() const override {
            return std::make_unique<RunHolder<Tw>>(proto);
        }

        uint32_t duration_ms() const override {
            return proto.duration();
        }
    };

    // Entry: either a prototype tween or a callback
    struct Entry {
        ms_t start_ms;
        std::unique_ptr<ProtoBase> prototype;
        std::function<void()> callback;
        uint32_t duration_cached = 0;
    };

    std::vector<Entry> entries_;
    std::vector<std::unique_ptr<IRun>> active_;
    size_t next_index_ = 0;
    ms_t last_time_ms_ = 0;

public:
    float framerate = 24.0f;

    Timeline() { entries_.reserve(32); }

    // Move semantics
    Timeline(Timeline&&) noexcept = default;
    Timeline& operator=(Timeline&&) noexcept = default;

    // Delete copy (Timeline contains unique_ptrs)
    Timeline(const Timeline&) = delete;
    Timeline& operator=(const Timeline&) = delete;

    // Add tween at specific time (seconds)
    template<typename... FromArgs>
    tweeny::tween<FromArgs...>& add(float start_sec, FromArgs&&... from_args) {
        using Tw = decltype(tweeny::from(std::declval<FromArgs>()...));
        Tw created = tweeny::from(std::forward<FromArgs>(from_args)...);

        auto holder = std::make_unique<ProtoHolder<Tw>>(std::move(created));
        uint32_t dur = holder->duration_ms();
        auto start_ms = static_cast<ms_t>(start_sec * 1000.0f);

        Entry e;
        e.start_ms = start_ms;
        e.prototype = std::move(holder);
        e.duration_cached = dur;

        auto it = std::lower_bound(entries_.begin(), entries_.end(), start_ms,
            [](const Entry& lhs, ms_t rhs) {
                return lhs.start_ms < rhs;
            });

        size_t pos = static_cast<size_t>(it - entries_.begin());
        entries_.insert(it, std::move(e));

        // Adjust next_index_ only if insertion is BEFORE the current next_index_ position
        // (i.e., we inserted something that should have already been processed)
        if (pos < next_index_) {
            ++next_index_;
        }

        // If timeline has already progressed PAST this start time, start it immediately
        if (entries_[pos].start_ms < last_time_ms_) {
            start_entry_immediate(pos);
        }

        auto* holder_ptr = static_cast<ProtoHolder<Tw>*>(entries_[pos].prototype.get());
        return holder_ptr->proto;
    }

    // Add tween at specific frame
    template<typename... FromArgs>
    tweeny::tween<FromArgs...>& add(uint32_t start_frame, FromArgs&&... from_args) {
        return add(static_cast<float>(start_frame) / framerate,
                   std::forward<FromArgs>(from_args)...);
    }

    // Add callback at specific time (seconds)
    template<typename F>
    void add_callback(float start_sec, F&& cb) {
        auto start_ms = static_cast<ms_t>(start_sec * 1000.0f);

        Entry e;
        e.start_ms = start_ms;
        e.callback = std::forward<F>(cb);
        e.duration_cached = 0;

        auto it = std::lower_bound(entries_.begin(), entries_.end(), start_ms,
            [](const Entry& lhs, ms_t rhs) {
                return lhs.start_ms < rhs;
            });

        size_t pos = static_cast<size_t>(it - entries_.begin());
        entries_.insert(it, std::move(e));

        // Adjust next_index_ only if insertion is BEFORE the current next_index_ position
        if (pos < next_index_) {
            ++next_index_;
        }

        // If this is in the past, invoke immediately
        if (entries_[pos].start_ms < last_time_ms_) {
            start_entry_immediate(pos);
        }
    }

    // Add callback at specific frame
    template<typename F>
    void add_callback(uint32_t start_frame, F&& cb) {
        add_callback(static_cast<float>(start_frame) / framerate,
                     std::forward<F>(cb));
    }

    // Advance timeline (monotonic)
    void update(ms_t now_ms) {
        if (now_ms < last_time_ms_) {
            reset();  // Handle rewind
        }

        uint32_t dt_ms = static_cast<uint32_t>(now_ms - last_time_ms_);

        // Step active tweens and remove finished ones
        for (auto it = active_.begin(); it != active_.end();) {
            (*it)->step_ms(dt_ms);
            if ((*it)->finished()) {
                it = active_.erase(it);
            } else {
                ++it;
            }
        }

        // Start scheduled entries whose start time has been reached
        while (next_index_ < entries_.size() &&
               entries_[next_index_].start_ms <= now_ms) {
            start_entry_from_update(next_index_, now_ms);
            ++next_index_;
        }

        last_time_ms_ = now_ms;
    }

    // Reset timeline to beginning
    void reset() {
        active_.clear();
        next_index_ = 0;
        last_time_ms_ = 0;
    }

    // Clear all entries and reset
    void clear() {
        entries_.clear();
        reset();
    }

    // Reserve capacity for entries
    void reserve(size_t capacity) {
        entries_.reserve(capacity);
    }

    // Query methods
    bool empty() const {
        return active_.empty() && next_index_ >= entries_.size();
    }

    size_t scheduled_count() const {
        return entries_.size() - next_index_;
    }

    size_t active_count() const {
        return active_.size();
    }

    size_t total_entries() const {
        return entries_.size();
    }

    ms_t current_time() const {
        return last_time_ms_;
    }

    // Get total duration of timeline (end time of last entry)
    ms_t total_duration() const {
        if (entries_.empty()) return 0;

        ms_t max_end = 0;
        for (const auto& e : entries_) {
            ms_t end = e.start_ms + e.duration_cached;
            max_end = std::max(max_end, end);
        }
        return max_end;
    }

    // Get remaining time until all tweens complete
    ms_t remaining_duration() const {
        ms_t max_remaining = 0;

        // Check active tweens
        for (const auto& tween : active_) {
            max_remaining = std::max(max_remaining,
                                    static_cast<ms_t>(tween->remaining_ms()));
        }

        // Check scheduled entries
        for (size_t i = next_index_; i < entries_.size(); ++i) {
            ms_t entry_end = entries_[i].start_ms + entries_[i].duration_cached;
            ms_t remaining = entry_end - last_time_ms_;
            if (remaining > 0) {
                max_remaining = std::max(max_remaining, remaining);
            }
        }

        return max_remaining;
    }

private:
    // Start an entry that was added in the past (called when adding retroactively)
    // This catches up the tween/callback to the current timeline position
    void start_entry_immediate(size_t idx) {
        assert(idx < entries_.size() && "start_entry_immediate: index out of bounds");
        if (idx >= entries_.size()) {
            return;
        }

        Entry& e = entries_[idx];

        // Handle callback - execute immediately
        if (e.callback) {
            e.callback();  // Re-throw any exceptions
            return;
        }

        // Handle tween - create and step to current time
        auto inst = e.prototype->make_runtime();
        e.duration_cached = e.prototype->duration_ms();

        // Step for elapsed time since entry's start
        ms_t since_start = last_time_ms_ - e.start_ms;
        if (since_start > 0) {
            inst->step_ms(static_cast<uint32_t>(since_start));
        }

        // Only add to active if not already finished
        if (!inst->finished()) {
            active_.push_back(std::move(inst));
        }
    }

    // Start an entry from the update loop (normal forward progression)
    // The entry starts at its designated time
    void start_entry_from_update(size_t idx, ms_t now_ms) {
        assert(idx < entries_.size() && "start_entry_from_update: index out of bounds");
        if (idx >= entries_.size()) {
            return;
        }

        Entry& e = entries_[idx];

        // Handle callback
        if (e.callback) {
            e.callback();  // Re-throw any exceptions
            return;
        }

        // Handle tween - create runtime instance
        auto inst = e.prototype->make_runtime();
        e.duration_cached = e.prototype->duration_ms();

        // Step the tween from its start time to now
        ms_t since_start = now_ms - e.start_ms;
        if (since_start > 0) {
            inst->step_ms(static_cast<uint32_t>(since_start));
        }

        // Add to active if not already finished
        if (!inst->finished()) {
            active_.push_back(std::move(inst));
        }
    }
};