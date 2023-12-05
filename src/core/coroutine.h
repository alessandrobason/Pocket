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

        Result init(Func func, void *userdata = nullptr);
        Result destroy();

        Result resume();
        Result yield();
        State status();

        void *getUserData();

        Coro &operator=(Coro &&other);

        void *internal = nullptr;
    };

    Result resume();
    Result yield();
    Result status();
    void *getUserData();
} // namespace co
