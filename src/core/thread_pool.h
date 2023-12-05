#pragma once

// #include <functional>
// #include <atomic>

#include "std/threads.h"
#include "std/arena.h"
#include "std/pair.h"
#include "std/delegate.h"

#include "core/coroutine.h"

struct ThreadPool {
    using Job = Delegate<void()>;

	void start(uint initial_thread_count = 5);
    void stop();
    bool isBusy();
	void pushJob(Job &&job);
    void setMaxJobsPerThread(uint max_jobs);

    usize getNumOfThreads() const;
    usize getThreadIndex(uptr thread_id) const;

private:
    struct JobData {
        Job job;
        co::Coro coroutine;
    };

    struct Queue {
        struct Node {
            JobData job_data;
            Node *next = nullptr;
        };

        void init(void *wait_handle, void *stop_handle);
        JobData *pop();
        void push(Job &&fn);
        void yieldJob();
        void finishJob();

        int getJobCount();

    private:
        Node *head = nullptr;
        Node *tail = nullptr;
        Node *freelist = nullptr;
        int job_count = 0;
        Arena arena;

        Mutex head_mtx;
        Mutex tail_mtx;
        Mutex free_mtx;
        Mutex count_mtx;
        Mutex arena_mtx;

        void *wait_handle;
        void *stop_handle;
    };

	void threadLoop();
    void pushThread();
    Queue *getQueue();

    bool should_stop = false;
    arr<Thread> threads;
    arr<Queue*> thread_queues;
    arr<void*> wait_handles;
    Arena queue_arena = Arena::make(gb(1), Arena::Virtual);
    uint max_jobs_per_thread = 5;

    Mutex stop_mtx;
    Mutex queue_mtx;

    void *stop_handle = nullptr;
};
