#pragma once

#include "std/common.h"
#include "std/hashmap.h"
#include "std/arr.h"
#include "std/str.h"

#include "vk_fwd.h"
#include "vk_ptr.h"

struct Profiler;

struct ScopeTimer {
    ScopeTimer(VkCommandBuffer commands, Profiler &profiler, StrView name);
    ~ScopeTimer();

    Profiler &profiler;
    VkCommandBuffer cmd;
    StaticStr<64> name;
    u32 start_time;
    u32 end_time;
};

struct PipelineStatRecorder {
    PipelineStatRecorder(VkCommandBuffer commands, Profiler &profiler, StrView name);
    ~PipelineStatRecorder();

    Profiler &profiler;
    VkCommandBuffer cmd;
    StaticStr<64> name;
    u32 query;
};

struct Profiler {
    void init(VkDevice device, float period, u32 per_frame_pool_sizes = 100);

    void grabQueries(VkCommandBuffer cmd);
    //double getStat(StrView name);
    VkQueryPool getTimerPool();
    VkQueryPool getStatPool();

    u32 getTimestampId();
    u32 getStatId();

    void pushTimer(StrView name, u32 start_id, u32 end_id);
    void pushStat(StrView name, u32 query);

    HashMap<StrView, double> timing;
    HashMap<StrView, i32> stats;

private:
    struct Timer {
        StaticStr<64> name;
        u32 start_id;
        u32 end_id;
    };

    struct Stat {
        StaticStr<64> name;
        u32 query;
    };

    struct QueryFrameState {
        vkptr<VkQueryPool> timer_pool;
        arr<Timer> timers;
        u32 timer_last;

        vkptr<VkQueryPool> stat_pool;
        arr<Stat> stats;
        u32 stat_last;
    };

    static constexpr int kquery_frame_overlap = 3;

    int current_frame = 0;
    float period;
    QueryFrameState query_frames[kquery_frame_overlap];
    VkDevice device;
};
