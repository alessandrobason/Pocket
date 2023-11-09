#pragma once

#include <limits.h>
#include <initializer_list>

#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "mem.h"
#include "maths.h"

template<typename T>
struct arr {
	using iterator = T *;
	using const_iterator = const T *;

	arr() = default;
	arr(const arr &a) { *this = a; }
	arr(arr &&a) { *this = mem::move(a); }
	arr(std::initializer_list<T> list) {
		reserve(list.size());
		for (auto &&v : list) push(mem::move(v));
	}
	~arr() { destroy(); }

	void destroy() {
		for (usize i = 0; i < len; ++i) {
			buf[i].~T();
		}
		pk_free(buf);
		buf = nullptr;
		cap = len = 0;
	}

	void release() {
		buf = nullptr;
		cap = len = 0;
	}

	void reserve(usize new_cap) {
		if (cap < new_cap) {
			reallocate(math::max(cap * 2, new_cap));
		}
	}

	void resize(usize new_len) {
		while (new_len < len) {
			pop();
		}

		if (new_len > len) {
			for (usize i = len; i < new_len; ++i) {
				push();
			}
		}
	}

	template<typename ...TArgs>
	T &push(TArgs &&...args) {
		reserve(len + 1);
		mem::placementNew<T>(buf + len, mem::move(args)...);
		return buf[len++];
	}

	void fill(const T &value) {
		for (usize i = 0; i < len; ++i) {
			buf[i] = value;
		}
	}

	void reallocate(usize newcap) {
		if (newcap < cap) {
			newcap = cap * 2;
		}
		T *newbuf = (T *)pk_calloc(1, sizeof(T) * newcap);
		pk_assert(newbuf);
		for (usize i = 0; i < len; ++i) {
			mem::placementNew<T>(newbuf + i, mem::move(buf[i]));
		}
		pk_free(buf);
		buf = newbuf;
		cap = newcap;
	}

	void clear() {
		for (usize i = 0; i < len; ++i) {
			buf[i].~T();
		}
		len = 0;
	}

	void pop() {
		if (len) {
			buf[--len].~T();
		}
	}

	void remove(usize index) {
		if (index >= len) return;
		mem::swap(buf[index], buf[len - 1]);
		pop();
	}

	void removeSlow(usize index) {
		if (index >= len) return;
		const usize arr_end = len - 1;
		for (usize i = index; i < arr_end; ++i) {
			buf[i] = mem::move(buf[i + 1]);
		}
		pop();
	}

	usize find(const T &value) const {
		for (usize i = 0; i < len; ++i) {
			if (buf[i] == value) {
				return i;
			}
		}
		return SIZE_MAX;
	}

	bool contains(const T &value) const {
		return find(value) != SIZE_MAX;
	}

	arr &operator=(const arr &a) {
		if (this != &a) {
			clear();
			reserve(a.len);
			for (usize i = 0; i < a.len; ++i) {
				push(a[i]);
			}
		}
		return *this;
	}

	arr &operator=(arr &&a) {
		if (this != &a) {
			mem::swap(buf, a.buf);
			mem::swap(len, a.len);
			mem::swap(cap, a.cap);
		}
		return *this;
	}

	bool empty() const { return len == 0; }
	usize size() const { return len; }
	usize capacity() const { return cap; }
	usize byteSize() const { return len * sizeof(T); }

	T *data() { return buf; }
	T &operator[](usize index) { pk_assert(index < len); return buf[index]; }
	T *begin() { return buf; }
	T *end() { return buf + len; }
	T &front() { pk_assert(buf); return buf[0]; }
	T &back() { pk_assert(buf); return buf[len - 1]; }

	const T *data() const { return buf; }
	const T &operator[](usize index) const { pk_assert(index < len); return buf[index]; }
	const T *begin() const { return buf; }
	const T *end() const { return buf + len; }
	const T &front() const { pk_assert(buf); return buf[0]; }
	const T &back() const { pk_assert(buf); return buf[len - 1]; }

	T *buf = nullptr;
	usize len = 0;
	usize cap = 0;
};