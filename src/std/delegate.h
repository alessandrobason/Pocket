#pragma once

#include <new>
#include "mem.h"
#include "arena.h"

template <class T>
constexpr T&& fwd(std::remove_reference_t<T>& arg) noexcept {
    return static_cast<T&&>(arg);
}

template<typename T>
struct CallableBase;

template<typename T, typename TRet, typename ...TArgs>
struct Callable;

template<typename T>
struct Delegate;

template<typename TRet, typename ...TArgs>
struct CallableBase<TRet(TArgs...)> {
    using Func = TRet(TArgs...);

    CallableBase() = default;
    virtual ~CallableBase() {}
    virtual TRet invoke(TArgs &&...args) = 0;
};

template<typename T, typename TRet, typename ...TArgs>
struct Callable<T, TRet(TArgs...)> : CallableBase<TRet(TArgs...)> {
    T storage;

    Callable() = default;
    Callable(T &&storage) : storage(mem::move(storage)) {}
    // Callable(T &&storage) { init(mem::move(storage)); }

    void init(T &&store) {
        storage = mem::move(store);
    }

    TRet invoke(TArgs &&...args) {
        return storage(mem::move(args)...);
    }
};

template<typename TRet, typename ...TArgs>
struct Delegate<TRet(TArgs...)> {
    using Func = TRet(TArgs...);

    Delegate() = default;
    Delegate(Func *func_ptr) { init(func_ptr); }
    template<typename T>
    Delegate(T &&lambda) { init(mem::move(lambda)); }
    template<typename T>
    Delegate(Arena &arena, T &&lambda) { init(arena, mem::move(lambda)); }

    Delegate(Delegate &&other) { *this = mem::move(other); }

    ~Delegate() { destroy(); }

    enum Flags : u8 {
        None         = 0,
        IsFunctor    = 1 << 0,
        IsArenaAlloc = 1 << 1,
    };

    void *functor = nullptr;
    // TODO can be put as two leftmost bit in functor ptr and then use mask
    Flags flags = None;

    void init(Func *func_ptr) {
        functor = func_ptr;
    }

    template<typename T>
    void init(T &&lambda) {
        static_assert(!std::is_function_v<T> && !std::is_member_function_pointer_v<T>);
        
        flags = IsFunctor;
        functor = pk_calloc(sizeof(Callable<T, Func>), 1);
        // TODO figure out how to use custom placementNew here instead
        new (functor) Callable<T, Func>(mem::forward<T>(lambda));
    }

    template<typename T>
    void init(Arena &arena, T &&lambda) {
        static_assert(!std::is_function_v<T> && !std::is_member_function_pointer_v<T>);
        
        flags = IsFunctor | IsArenaAlloc;
        functor = arena.alloc<Callable<T, Func>>();
        // TODO figure out how to use custom placementNew here instead
        new (functor) Callable<T, Func>(mem::forward<T>(lambda));
    }

    void destroy() {
        if (flags & IsFunctor && !(flags & IsArenaAlloc)) {
            CallableBase<Func> *call = (CallableBase<Func>*)functor;
            call->~CallableBase();
            pk_free(functor);
        }
        flags = None;
        functor = nullptr;
    }

    operator bool() const {
        return functor != nullptr;
    }

    Delegate &operator=(Delegate &&other) {
        if (this != &other) {
            mem::swap(functor, other.functor);
            mem::swap(flags, other.flags);
        }
        return *this;
    }

    TRet operator()(TArgs &&...args) {
        if (flags & IsFunctor) {
            return ((CallableBase<Func> *)functor)->invoke(mem::move(args)...);
        }
        return ((Func *)functor)(mem::move(args)...);
    }
};