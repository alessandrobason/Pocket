#pragma once

#include <functional>
#include <atomic>

#include "std/threads.h"
#include "std/arena.h"


struct ThreadPool {
	void start(uint initial_thread_count = 5);
    void stop();
    bool isBusy();
	void pushJob(std::function<void()> &&job);
    void setMaxJobsPerThread(uint max_jobs);

private:
    struct Queue {
        struct Node {
            std::function<void()> value;
            Node *next = nullptr;
        };

        void init(void *wait_handle, void *stop_handle);
        std::function<void()> pop();
        void push(std::function<void()> &&fn);

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
