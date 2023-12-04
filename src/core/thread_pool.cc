#include "thread_pool.h"

#include "std/logging.h"

#include <windows.h>

void ThreadPool::start(uint initial_thread_count) {
    stop_handle = CreateEvent(
        nullptr,
        TRUE,               // manual-reset event
        FALSE,              // initial state is nonsignaled
        nullptr
    );
    
    for (uint i = 0; i < initial_thread_count; ++i) {
        pushThread();
    }
}

bool ThreadPool::isBusy() {
    for (Queue *queue : thread_queues) {
        if (queue->getJobCount() > 0) {
            return true;
        }
    }
    return false;
}

void ThreadPool::stop() {
    should_stop = true;
    SetEvent((HANDLE)stop_handle);
    Thread::joinAll(threads);
    ResetEvent((HANDLE)stop_handle);
    thread_queues.clear();
    threads.clear();

    CloseHandle((HANDLE)stop_handle);
    for (void *handle : wait_handles) {
        CloseHandle((HANDLE)handle);
    }
}

void ThreadPool::pushJob(Job &&job) {
    if (threads.empty()) {
        pushThread();
    }

    int min_count = INT_MAX;
    Queue *smallest = nullptr;

    // try to find a queue with not enough jobs
    for (Queue *queue : thread_queues) {
        int count = queue->getJobCount();
        if (count < min_count) {
            min_count = count;
            smallest = queue;

            // no need to wait if we found an empty queue
            if (count == 0) {
                break;
            }
        }
    }

    if (min_count >= (int)max_jobs_per_thread) {
        pushThread();
        smallest = thread_queues.back();
    }

    pk_assert(smallest);
    smallest->push(mem::move(job));
}

void ThreadPool::setMaxJobsPerThread(uint max_jobs) {
    max_jobs_per_thread = max_jobs;
}

void ThreadPool::threadLoop() {
    Job job;

    while (true) {
        {
            MtxLock stop_lock = stop_mtx;
            if (should_stop) return;
        }

        Queue *queue = getQueue();
        pk_assert(queue);
        JobData *job_data = queue->pop();
        // if quitting
        if (!job_data) {
            break;
        }
        co::Result result = job_data->coroutine.resume();
        if (result == co::Dead) {
            queue->finishJob();
        }
        else {
            queue->yieldJob();
        }
    }
}

void ThreadPool::pushThread() {
    HANDLE wait_handle = CreateEvent(
        nullptr,
        TRUE,               // manual-reset event
        FALSE,              // initial state is nonsignaled
        nullptr
    );
    wait_handles.push(wait_handle);

    Queue *queue = queue_arena.alloc<Queue>();
    queue->init(wait_handle, stop_handle);

    queue_mtx.lock();
    thread_queues.push(queue);
    queue_mtx.unlock();

    threads.push(
        Thread::create(
            [](void *udata) {
                ThreadPool *self = (ThreadPool *)udata;
                self->threadLoop();
                return 0;
            },
            this
        )
    );
}

ThreadPool::Queue *ThreadPool::getQueue() {
    uptr id = Thread::currentId();

    MtxLock lock = queue_mtx;
    for (usize i = 0; i < threads.len; ++i) {
        if (threads[i].getId() == id) {
            return thread_queues[i];
        }
    }

    return nullptr;
}

void ThreadPool::Queue::init(void *in_wait_handle, void *in_stop_handle) {
    arena = Arena::make(gb(1), Arena::Virtual);
    head_mtx.init();
    tail_mtx.init();
    free_mtx.init();
    count_mtx.init();
    arena_mtx.init();

    wait_handle = in_wait_handle;
    stop_handle = in_stop_handle;
}

ThreadPool::JobData *ThreadPool::Queue::pop() {
    head_mtx.lock();

    while (!head) {
        head_mtx.unlock();
        Slice<HANDLE> handles = { stop_handle, wait_handle };
        DWORD result = WaitForMultipleObjects((DWORD)handles.len, handles.buf, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) {
            return nullptr;
        }
        head_mtx.lock();
    }

    ResetEvent((HANDLE)wait_handle);

    pk_assert(head);

    return &head->job_data;

#if 0
    head_mtx.lock();

    while (!head) {
        head_mtx.unlock();
        Slice<HANDLE> handles = { stop_handle, wait_handle };
        DWORD result = WaitForMultipleObjects((DWORD)handles.len, handles.buf, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) {
            return {};
        }
        head_mtx.lock();
    }

    ResetEvent((HANDLE)wait_handle);

    // remove count
    {
        MtxLock count_lock = count_mtx;
        job_count--;
    }

    pk_assert(head);

    if (head == tail) {
        MtxLock tail_lock = tail_mtx;
        // get value
        Job fn = mem::move(head->job);
        // co::Coro coro = mem::move(head->coroutine);

        // clear queue
        arena_mtx.lock();
        arena.rewind(0);
        arena_mtx.unlock();

        tail = nullptr;
        head = nullptr;

        head_mtx.unlock();

        return { mem::move(fn), co::Coro(nullptr, nullptr) };
    }

    // get function
    Job fn = mem::move(head->job);
    // co::Coro coro = mem::move(head->coroutine);

    // pop head
    Node *node = head;
    head = head->next;
    // we don't need the head anymore
    head_mtx.unlock();

    // save old head in freelist
    {
        MtxLock freelock = free_mtx;
        node->next = freelist;
        freelist = node;
    }
    return { mem::move(fn), co::Coro(nullptr, nullptr) };
#endif
}

void ThreadPool::Queue::push(Job &&fn) {
    Node *node = nullptr;

    // try grabbing a free node
    if (free_mtx.tryLock()) {
        if (freelist) {
            node = freelist;
            freelist = freelist->next;
        }
        free_mtx.unlock();
    }

    // if there are no free nodes, or if the freelist is busy, make a new one
    if (!node) {
        arena_mtx.lock();
        node = arena.alloc<Node>(1, Arena::SoftFail);
        arena_mtx.unlock();
        if (!node) {
            err("Arena out of memory! can't add the job");
            return;
        }
    }

    pk_assert(node);

    auto coroutine__function = []() {
        co::Coro coro = co::Coro::running();
        Node *node = (Node *)coro.getUserData();
        node->job_data.job();
    };

    node->job_data.job = mem::move(fn);
    node->job_data.coroutine.init(coroutine__function, node);

    // append to the end
    MtxLock lock = tail_mtx;
    if (tail) {
        tail->next = node;
        tail = node;
    }
    // if there are no nodes, make this the head too
    else {
        MtxLock head_lock = head_mtx;
        head = tail = node;
    }

    // add count
    MtxLock count_lock = count_mtx;
    job_count++;
    if (job_count == 1) {
        SetEvent((HANDLE)wait_handle);
    }
}

void ThreadPool::Queue::yieldJob() {
    pk_assert(head && tail);

    // job is yielded but not finished, but at the end of the queue

    // append to the end
    if (head != tail) {
        MtxLock tail_lock = tail_mtx;
        MtxLock head_lock = head_mtx;

        tail->next = head;
        tail = head;

        head = head->next;
    }
}

void ThreadPool::Queue::finishJob() {
    pk_assert(head);

    // remove count
    {
        MtxLock count_lock = count_mtx;
        job_count--;
    }

    // TODO: use arena allocator for these too
    head->job_data.job.destroy();
    head->job_data.coroutine.destroy();

    // if head is tail, clear the whole queue and reset the allocator
    if (head == tail) {
        MtxLock tail_lock = tail_mtx;

        // clear queue
        arena_mtx.lock();
        arena.rewind(0);
        arena_mtx.unlock();

        tail = nullptr;
        head = nullptr;

        head_mtx.unlock();
    }

    // otherwise, pop the head and put it in the freelist

    // pop head
    Node *old_head = head;
    head = head->next;
    // we don't need the head anymore
    head_mtx.unlock();

    // save old head in freelist
    MtxLock freelock = free_mtx;
    old_head->next = freelist;
    freelist = old_head;
}

int ThreadPool::Queue::getJobCount() {
    MtxLock lock = count_mtx;
    return job_count;
}
