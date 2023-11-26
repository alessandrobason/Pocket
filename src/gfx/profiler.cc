#include "profiler.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

// VULKAN SCOPE TIMER /////////////////////////////////////////////////////////

ScopeTimer::ScopeTimer(VkCommandBuffer commands, Profiler &profiler, StrView name) 
    : profiler(profiler),
      cmd(commands),
      name(name)
{
    start_time = profiler.getTimestampId();

    vkCmdWriteTimestamp(
        cmd,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        profiler.getTimerPool(),
        start_time
    );
}

ScopeTimer::~ScopeTimer() {
    end_time = profiler.getTimestampId();
    vkCmdWriteTimestamp(
        cmd, 
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
        profiler.getTimerPool(), 
        end_time
    );

    profiler.pushTimer(name, start_time, end_time);
}

// VULKAN PIPELINE STAT RECORDER //////////////////////////////////////////////

PipelineStatRecorder::PipelineStatRecorder(VkCommandBuffer commands, Profiler &profiler, StrView name)
    : profiler(profiler),
      cmd(commands),
      name(name)
{
    query = profiler.getStatId();
    vkCmdBeginQuery(cmd, profiler.getStatPool(), query, 0);
}

PipelineStatRecorder::~PipelineStatRecorder() {
    vkCmdEndQuery(cmd, profiler.getStatPool(), query);
    profiler.pushStat(name, query);
}

// VULKAN PROFILER ////////////////////////////////////////////////////////////

void Profiler::init(VkDevice new_device, float new_period, u32 per_frame_pool_sizes) {
    device = new_device;
    period = new_period;
    u32 pool_sizes = per_frame_pool_sizes;

    VkQueryPoolCreateInfo query_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = pool_sizes,
    };

    for (QueryFrameState &frame : query_frames) {
        vkCreateQueryPool(device, &query_info, nullptr, frame.timer_pool.getRef());
        frame.timer_last = 0;
    }

    query_info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;

    for (QueryFrameState &frame : query_frames) {
        vkCreateQueryPool(device, &query_info, nullptr, frame.stat_pool.getRef());
        frame.stat_last = 0;
    }
}

void Profiler::grabQueries(VkCommandBuffer cmd) {
    QueryFrameState &frame = query_frames[current_frame];
    current_frame = (current_frame + 1) % kquery_frame_overlap;
    QueryFrameState &new_frame = query_frames[current_frame];

    vkCmdResetQueryPool(cmd, new_frame.timer_pool, 0, new_frame.timer_last);
    vkCmdResetQueryPool(cmd, new_frame.stat_pool, 0, new_frame.stat_last);
    new_frame.timer_last = 0;
    new_frame.stat_last = 0;

    arr<u64> query_state;
    if (frame.timer_last != 0) {
        query_state.resize(frame.timer_last);

        // We use vkGetQueryResults to copy the results into a host visible buffer
        vkGetQueryPoolResults(
            device,
            frame.timer_pool,
            0,
            frame.timer_last,
            query_state.byteSize(),
            query_state.data(),
            sizeof(u64),
            // Store results a 64 bit values and wait until the results have been finished
            // If you don't want to wait, you can use VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
            // which also returns the state of the result (ready) in the result
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );
    }

    arr<u64> stat_results;
    if (frame.stat_last != 0) {
        stat_results.resize(frame.stat_last);

        // We use vkGetQueryResults to copy the results into a host visible buffer
        vkGetQueryPoolResults(
            device,
            frame.stat_pool,
            0,
            frame.stat_last,
            stat_results.byteSize(),
            stat_results.data(),
            sizeof(u64),
            // Store results a 64 bit values and wait until the results have been finished
            // If you don't want to wait, you can use VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
            // which also returns the state of the result (ready) in the result
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );
    }

    for (Timer &timer : frame.timers) {
        u64 begin = query_state[timer.start_id];
        u64 end = query_state[timer.end_id];
        u64 timestamp = end - begin;
        // store as milliseconds
        timing.push(timer.name, (double)timestamp * period / 1'000'000.0);
    }

    for (Stat &stat : frame.stats) {
        u64 result = stat_results[stat.query];
        stats.push(stat.name, (i32)result);
    }
}

VkQueryPool Profiler::getTimerPool() {
    return query_frames[current_frame].timer_pool;
}

VkQueryPool Profiler::getStatPool() {
    return query_frames[current_frame].stat_pool;
}

u32 Profiler::getTimestampId() {
    QueryFrameState &frame = query_frames[current_frame];
    u32 out = frame.timer_last;
    frame.timer_last++;
    return out;
}

u32 Profiler::getStatId() {
    QueryFrameState &frame = query_frames[current_frame];
    u32 out = frame.stat_last;
    frame.stat_last++;
    return out;
}

void Profiler::pushTimer(StrView name, u32 start_id, u32 end_id) {
    query_frames[current_frame].timers.push({ name, start_id, end_id });
}

void Profiler::pushStat(StrView name, u32 query) {
    query_frames[current_frame].stats.push({ name, query });
}
