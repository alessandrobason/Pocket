#include "coroutine.h"

#include "std/mem.h"

#include "minicoro.h"

namespace co {
    using mco_func = void(mco_coro*);

    Coro::Coro(Func func, void *userdata) {
        init(func, userdata);
    }

    Coro::Coro(Coro &&other) {
        *this = mem::move(other);
    }

    Coro::~Coro() {
        destroy();
    }

    Coro Coro::running() {
        Coro out;
        out.internal = mco_running();
        return out;
    }

    Result Coro::init(Func func, void *userdata) {
        mco_desc desc = mco_desc_init((mco_func *)func, 0);
        desc.user_data = userdata;
        return (Result)mco_create((mco_coro **)&internal, &desc);
    }

    Result Coro::destroy() {
        Result result = (Result)mco_destroy((mco_coro *)internal);
        internal = nullptr;
        return result;
    }

    Result Coro::resume() {
        return (Result)mco_resume((mco_coro *)internal);
    }

    Result Coro::yield() {
        return (Result)mco_yield((mco_coro *)internal);
    }

    Result Coro::status() {
        return (Result)mco_status((mco_coro *)internal);
    }

    void *Coro::getUserData() {
        if (!internal) return nullptr;
        return ((mco_coro *)internal)->user_data;
    }

    Coro &Coro::operator=(Coro &&other) {
        if (this != &other) {
            mem::swap(internal, other.internal);
        }
        return *this;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    Result resume() {
        return (Result)mco_resume(mco_running());
    }

    Result yield() {
        return (Result)mco_yield(mco_running());
    }

    Result status() {
        return (Result)mco_status(mco_running());
    }
} // namespace co