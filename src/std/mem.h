#pragma once

#include <string.h>
// #include <type_traits>

#include "common.h"

namespace mem {
	template<typename T>
	void zero(T &value) { memset(&value, 0, sizeof(T)); }

	template<typename T>
	void copy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }

	template<typename T> struct RemRef { using Type = T; };
	template<typename T> struct RemRef<T &> { using Type = T; };
	template<typename T> struct RemRef<T &&> { using Type = T; };
	template<typename T> using RemRefT = typename RemRef<T>::Type;

	template<typename T> struct RemArr { using Type = T; };
	template<typename T> struct RemArr<T[]> { using Type = T; };
	template<typename T> using RemArrT = typename RemArr<T>::Type;

	template<typename T>
	RemRefT<T> &&move(T &&val) { return (RemRefT<T> &&)val; }

	template<typename T>
	void swap(T &a, T &b) {
		RemArrT<T> temp = mem::move(a);
		a = mem::move(b);
		b = mem::move(temp);
	}

	template<typename T, typename ...TArgs>
	void placementNew(void *data, TArgs &&...args) {
		T *temp = (T *)data;
		*temp = T(mem::move(args)...);
	}

	// WARNING does NOT work with virtual classes! vtable is not initialised
	template<typename T>
	struct ptr {
		ptr() = default;
		ptr(void *p) : buf((T *)p) {}
		ptr(T *p) : buf(p) {}
		ptr(ptr &&p) { *this = mem::move(p); }
		ptr &operator=(ptr &&p) { if (buf != p.buf) swap(p); return *this; }
		~ptr() { destroy(); }

		template<typename ...TArgs>
		static ptr make(TArgs &&...args) {
			void *ptr = pk_malloc(sizeof(T));
			mem::placementNew<T>(ptr, mem::move(args)...);
			return (T *)ptr;
		}

		void swap(ptr &p) { mem::swap(buf, p.buf); }
		void destroy() { 
			if (!buf) return;
			// delete buf; 
			buf->~T();
			pk_free(buf);
			buf = nullptr; 
		}
		T *release() { T *temp = buf; buf = nullptr; return temp; }

		operator bool() const { return buf != nullptr; }
		T *get() { return buf; }
		T *operator->() { return buf; }

		const T *get() const { return buf; }
		const T *operator->() const { return buf; }

	private:
		T *buf = nullptr;
	};

	// reference wrapper
	template<typename T>
	struct ref {
		ref(T &&value) : ptr(&value) {}

		ref &operator=(const T &value) {
			pk_assert(ptr);
			*ptr = value;
			return *this;
		}

		T &operator*() {
			pk_assert(ptr);
			return *ptr;
		}

	private:
		T *ptr;
	};
} // namespace mem