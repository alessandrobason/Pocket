#pragma once

namespace co {
    struct Coro;
    // using Func = void(Coro *co);
    using Func = void();

    enum State {
        Dead,
        Normal,
        Running,
        Suspended,
    };

    enum Result {
        Success,
        GenericError,
        InvalidPointer,
        InvalidCoroutine,
        NotSuspended,
        NotRunning,
        MakeContextError,
        SwitchContextError,
        NotEnoughSpace,
        OutOfMemory,
        InvalidArguments,
        InvalidOperation,
        StackOverflow,
    };

    struct Coro {
        Coro() = default;
        Coro(Func func, void *userdata = nullptr);
        Coro(Coro &&other);
        ~Coro();

        static Coro running();

        Result init(Func func, void *userdata = nullptr);
        Result destroy();

        Result resume();
        Result yield();
        Result status();

        void *getUserData();

        Coro &operator=(Coro &&other);

        void *internal = nullptr;
    };

    Result resume();
    Result yield();
    Result status();
} // namespace co

/*

void func() {
    hello
    world 
    while (cond) {
        co::yield();
    }

    return;
}

void co_func(mco_coro *co) {
    Data *data = co->udata;
    data->func();

}

*/