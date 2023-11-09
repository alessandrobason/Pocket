#pragma once

#include <limits.h>
#include <assert.h>
#include <initializer_list>

#include "common.h"
#include "arr.h"

template<typename T>
struct Slice {
	Slice() = default;
	Slice(const T *buf, usize len) : buf(buf), len(len) {}
	template<usize size>
	Slice(const T(&buf)[size]) : buf(buf), len(size) {}
	Slice(std::initializer_list<T> list) : buf(list.begin()), len(list.size()) {}
	Slice(const arr<T> &list) : buf(list.buf()), len(list.size()) {}

	bool empty() const { return len == 0; }
	const T *data() const { return buf;}
	usize size() const { return len; }
	usize byteSize() const { return len * sizeof(T); }

	Slice sub(usize start, usize end = SIZE_MAX) const {
		if (empty() || start >= len) return Slice();
		if (end >= len) end = len;
		return Slice(buf + start, end - start);
	}

	arr<T> dup() const {
		arr<T> out{};
		out.reserve(len);
		for (usize i = 0; i < len; ++i) {
			out.push(buf[i]);
		}
		return out;
	}

	const T &operator[](usize i) const {
		pk_assert(i < len);
		return buf[i];
	}

	bool operator==(const Slice &s) const {
		if (len != s.len) return false;
		for (usize i = 0; i < len; ++i) {
			if (buf[i] != s[i]) {
				return false;
			}
		}
		return true;
	}

	operator bool() const { return buf != nullptr && len > 0; }

	const T *begin() const { return buf; }
	const T *end() const { return buf + len; }
	const T &front() const { pk_assert(buf); return buf[0]; }
	const T &back() const { pk_assert(buf); return buf[len - 1]; }

	const T *buf = nullptr;
	usize len = 0;
};
