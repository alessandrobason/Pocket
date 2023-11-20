#pragma once

#include <cmath>
#include "std/maths.h"
#include "std/common.h"

#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union

template<typename T>
struct vec2T {
	union {
		T data[2];
		struct { T x, y; };
		struct { T u, v; };
	};

	//constexpr vec2T() : x(0), y(0) {}
	constexpr vec2T() = default;
	constexpr vec2T(T v) : x(v), y(v) {}
	constexpr vec2T(T x, T y) : x(x), y(y) {}
	template<typename Q>
	constexpr vec2T(const vec2T<Q>& v) : x((T)v.x), y((T)v.y) {}

	constexpr vec2T operator-() const { return { -x, -y }; }

	constexpr vec2T operator+(const vec2T& o) const { return { x + o.x, y + o.y }; }
	constexpr vec2T operator-(const vec2T& o) const { return { x - o.x, y - o.y }; }
	constexpr vec2T operator*(const vec2T& o) const { return { x * o.x, y * o.y }; }
	constexpr vec2T operator/(const vec2T& o) const { return { x / o.x, y / o.y }; }
	constexpr vec2T operator%(const vec2T& o) const { return { x % o.x, y % o.y }; }

	vec2T& operator+=(const vec2T& o) { x += o.x; y += o.y; return *this; }
	vec2T& operator-=(const vec2T& o) { x -= o.x; y -= o.y; return *this; }
	vec2T& operator*=(const vec2T& o) { x *= o.x; y *= o.y; return *this; }
	vec2T& operator/=(const vec2T& o) { x /= o.x; y /= o.y; return *this; }

	constexpr vec2T operator+(T o) const { return { x + o, y + o }; }
	constexpr vec2T operator-(T o) const { return { x - o, y - o }; }
	constexpr vec2T operator*(T o) const { return { x * o, y * o }; }
	constexpr vec2T operator/(T o) const { return { x / o, y / o }; }
	constexpr vec2T operator%(T o) const { return { x % o, y % o }; }

	vec2T& operator+=(T o) { x += o; y += o; return *this; }
	vec2T& operator-=(T o) { x -= o; y -= o; return *this; }
	vec2T& operator*=(T o) { x *= o; y *= o; return *this; }
	vec2T& operator/=(T o) { x /= o; y /= o; return *this; }

	constexpr vec2T<bool> operator==(const vec2T& o) const { return { x == o.x, y == o.y }; }
	constexpr vec2T<bool> operator==(T o) const { return { x == o, y == o }; }
	constexpr vec2T<bool> operator!=(const vec2T& o) const { return { x != o.x, y != o.y }; }
	constexpr vec2T<bool> operator!=(T o) const { return { x != o, y != o }; }

	constexpr vec2T<bool> operator<(const vec2T &o) const { return { x < o.x, y < o.y }; }
	constexpr vec2T<bool> operator>(const vec2T &o) const { return { x > o.x, y > o.y }; }
	constexpr vec2T<bool> operator<=(const vec2T &o) const { return { x <= o.x, y <= o.y }; }
	constexpr vec2T<bool> operator>=(const vec2T &o) const { return { x >= o.x, y >= o.y }; }

	constexpr vec2T<bool> operator<(T o) const { return { x < o, y < o }; }
	constexpr vec2T<bool> operator>(T o) const { return { x > o, y > o }; }
	constexpr vec2T<bool> operator<=(T o) const { return { x <= o, y <= o }; }
	constexpr vec2T<bool> operator>=(T o) const { return { x >= o, y >= o }; }

	T& operator[](size_t ind) { return data[ind]; }
	constexpr const T& operator[](size_t ind) const { return data[ind]; }

	constexpr T mag2() const { return x * x + y * y; }
	constexpr T mag() const { return sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	constexpr vec2T normalised() const { vec2T v = *this; v.norm(); return v; }
	constexpr static T dot(const vec2T& a, const vec2T& b) { return a.x * b.x + a.y * b.y; }
};

template<typename T>
struct vec3T {
	union {
		T data[3];
		struct { T x, y, z; };
		struct { T r, g, b; };
		struct { vec2T<T> v; T v1; };
	};

	//constexpr vec3T() : x(0), y(0), z(0) {}
	constexpr vec3T() = default;
	constexpr vec3T(T v) : x(v), y(v), z(v) {}
	constexpr vec3T(T x, T y, T z) : x(x), y(y), z(z) {}
	constexpr vec3T(const vec2T<T>& v, T z) : v(v), v1(z) {}
	template<typename Q>
	constexpr vec3T(const vec3T<Q>& v) : x((T)v.x), y((T)v.y), z((T)v.z) {}

	constexpr vec3T operator-() const { return { -x, -y, -z }; }

	constexpr vec3T operator+(const vec3T& o) const { return { x + o.x, y + o.y, z + o.z }; }
	constexpr vec3T operator-(const vec3T& o) const { return { x - o.x, y - o.y, z - o.z }; }
	constexpr vec3T operator*(const vec3T& o) const { return { x * o.x, y * o.y, z * o.z }; }
	constexpr vec3T operator/(const vec3T& o) const { return { x / o.x, y / o.y, z / o.z }; }
	constexpr vec3T operator%(const vec3T& o) const { return { x % o.x, y % o.y, z % o.z }; }

	vec3T& operator+=(const vec3T& o) { x += o.x; y += o.y; z += o.z; return *this; }
	vec3T& operator-=(const vec3T& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
	vec3T& operator*=(const vec3T& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
	vec3T& operator/=(const vec3T& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }

	constexpr vec3T operator+(T o) const { return { x + o, y + o, z + o }; }
	constexpr vec3T operator-(T o) const { return { x - o, y - o, z - o }; }
	constexpr vec3T operator*(T o) const { return { x * o, y * o, z * o }; }
	constexpr vec3T operator/(T o) const { return { x / o, y / o, z / o }; }
	constexpr vec3T operator%(T o) const { return { x % o, y % o, z % o }; }

	vec3T& operator+=(T o) { x += o; y += o; z += o; return *this; }
	vec3T& operator-=(T o) { x -= o; y -= o; z -= o; return *this; }
	vec3T& operator*=(T o) { x *= o; y *= o; z *= o; return *this; }
	vec3T& operator/=(T o) { x /= o; y /= o; z /= o; return *this; }

	constexpr vec3T<bool> operator==(const vec3T& o) const { return { x == o.x, y == o.y, z == o.z }; }
	constexpr vec3T<bool> operator==(T o) const { return { x == o, y == o, z == o }; }
	constexpr vec3T<bool> operator!=(const vec3T& o) const { return { x != o.x, y != o.y, z != o.z }; }
	constexpr vec3T<bool> operator!=(T o) const { return { x != o, y != o, z != o }; }

	T& operator[](size_t ind) { return data[ind]; }
	constexpr const T& operator[](size_t ind) const { return data[ind]; }

	constexpr T mag2() const { return x * x + y * y + z * z; }
	constexpr T mag() const { return (T)sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	constexpr vec3T normalised() const { vec3T o = *this; o.norm(); return o; }
	constexpr static vec3T cross(const vec3T& a, const vec3T& b) {
		return vec3T(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}
	constexpr static T dot(const vec3T& a, const vec3T& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
};

template<typename T>
struct vec4T {
	union {
		T data[4];
		struct { T x, y, z, w; };
		struct { T x, y, width, height; };
		struct { T r, g, b, a; };
		struct { vec2T<T> pos, size; };
		struct { vec3T<T> v; T s; };
	};

	//constexpr vec4T() : x(0), y(0), z(0), w(0) {}
	constexpr vec4T() = default;
	constexpr vec4T(T v) : x(v), y(v), z(v), w(v) {}
	constexpr vec4T(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	constexpr vec4T(const vec2T<T>& pos, const vec2T<T>& size) : pos(pos), size(size) {}
	constexpr vec4T(const vec3T<T>& v, T s) : v(v), s(s) {}
	template<typename Q>
	constexpr vec4T(const vec4T<Q>& v) : x((T)v.x), y((T)v.y), z((T)v.z), w((T)v.w) {}

	bool contains(const vec2T<T> &p) const { return p.x >= x && p.x <= (x + size.x) && p.y >= y && p.y <= (y + size.y); };

	constexpr vec2T<T> topLeft()     const { return { x,          y }; }
	constexpr vec2T<T> topRight()    const { return { x + size.x, y }; }
	constexpr vec2T<T> bottomLeft()  const { return { x,          y + size.y }; }
	constexpr vec2T<T> bottomRight() const { return { x + size.x, y + size.y }; }

	constexpr vec4T operator-() const { return { -x, -y, -z, -w }; }

	constexpr vec4T operator+(const vec4T& o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
	constexpr vec4T operator-(const vec4T& o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
	constexpr vec4T operator*(const vec4T& o) const { return { x * o.x, y * o.y, z * o.z, w * o.w }; }
	constexpr vec4T operator/(const vec4T& o) const { return { x / o.x, y / o.y, z / o.z, w / o.w }; }
	constexpr vec4T operator%(const vec4T& o) const { return { x % o.x, y % o.y, z % o.z, w % o.w }; }

	vec4T& operator+=(const vec4T& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
	vec4T& operator-=(const vec4T& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
	vec4T& operator*=(const vec4T& o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
	vec4T& operator/=(const vec4T& o) { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }

	constexpr vec4T operator+(T o) const { return { x + o, y + o, z + o, w + o }; }
	constexpr vec4T operator-(T o) const { return { x - o, y - o, z - o, w - o }; }
	constexpr vec4T operator*(T o) const { return { x * o, y * o, z * o, w * o }; }
	constexpr vec4T operator/(T o) const { return { x / o, y / o, z / o, w / o }; }
	constexpr vec4T operator%(T o) const { return { x % o, y % o, z % o, w % o }; }

	vec4T& operator+=(T o) { x += o; y += o; z += o; w += o; return *this; }
	vec4T& operator-=(T o) { x -= o; y -= o; z -= o; w -= o; return *this; }
	vec4T& operator*=(T o) { x *= o; y *= o; z *= o; w *= o; return *this; }
	vec4T& operator/=(T o) { x /= o; y /= o; z /= o; w /= o; return *this; }

	constexpr vec4T<bool> operator==(const vec4T& o) const { return { x == o.x, y == o.y, z == o.z, w == o.w }; }
	constexpr vec4T<bool> operator==(T o) const { return { x == o, y == o, z == o, w == o }; }
	constexpr vec4T<bool> operator!=(const vec4T& o) const { return { x != o.x, y != o.y, z != o.z, w != o.w }; }
	constexpr vec4T<bool> operator!=(T o) const { return { x != o, y != o, z != o, w != o }; }

	T& operator[](size_t ind) { return data[ind]; }
	constexpr const T& operator[](size_t ind) const { return data[ind]; }

	constexpr T mag2() const { return x * x + y * y + z * z + w * w; }
	constexpr T mag() const { return sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	constexpr vec4T normalised() const { vec4T o = *this; o.norm(); return o; }
	constexpr static T dot(const vec4T& a, const vec4T& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
};

namespace math {
	template<typename T>
	constexpr vec2T<T> clamp(const vec2T<T> &v, const vec2T<T> &minv, const vec2T<T> &maxv) {
		return vec2T<T>(
			math::min(math::max(v.x, minv.x), maxv.x),
			math::min(math::max(v.y, minv.y), maxv.y)
		);
	}

	template<typename T>
	constexpr vec3T<T> clamp(const vec3T<T> &v, const vec3T<T> &minv, const vec3T<T> &maxv) {
		return vec3T<T>(
			math::min(math::max(v.x, minv.x), maxv.x),
			math::min(math::max(v.y, minv.y), maxv.y),
			math::min(math::max(v.z, minv.z), maxv.z)
			);
	}

	template<typename T>
	constexpr vec4T<T> clamp(const vec4T<T> &v, const vec4T<T> &minv, const vec4T<T> &maxv) {
		return vec4T<T>(
			math::min(math::max(v.x, minv.x), maxv.x),
			math::min(math::max(v.y, minv.y), maxv.y),
			math::min(math::max(v.z, minv.z), maxv.z),
			math::min(math::max(v.w, minv.w), maxv.w)
			);
	}
} // namespace math

template<typename T>
constexpr T norm(const T& val) {
	return val.normalised();
}

template<typename T>
constexpr vec3T<T> cross(const vec3T<T>& a, const vec3T<T>& b) {
	return vec3T<T>::cross(a, b);
}

template<typename T>
constexpr T saturate(const T &val) {
	return math::clamp(val, T(0), T(1));
}

template<typename T> constexpr T dot(const vec2T<T> &v1, const vec2T<T> &v2) { return vec2T<T>::dot(v1, v2); }
template<typename T> constexpr T dot(const vec3T<T> &v1, const vec3T<T> &v2) { return vec3T<T>::dot(v1, v2); }
template<typename T> constexpr T dot(const vec4T<T> &v1, const vec4T<T> &v2) { return vec4T<T>::dot(v1, v2); }

#define VEC_FUN(name) \
	template<typename T> constexpr vec2T<T> name(const vec2T<T> &v) { return { name(v.x), name(v.y) }; } \
	template<typename T> constexpr vec3T<T> name(const vec3T<T> &v) { return { name(v.x), name(v.y), name(v.z) }; } \
	template<typename T> constexpr vec4T<T> name(const vec4T<T> &v) { return { name(v.x), name(v.y), name(v.z), name(v.w) }; }

VEC_FUN(abs)
VEC_FUN(round)
VEC_FUN(ceil)

#undef VEC_FUN

inline constexpr bool any(const vec2T<bool>& v) { return v.x || v.y; }
inline constexpr bool any(const vec3T<bool>& v) { return v.x || v.y || v.z; }
inline constexpr bool any(const vec4T<bool>& v) { return v.x || v.y || v.z || v.w; }

inline constexpr bool all(const vec2T<bool>& v) { return v.x && v.y; }
inline constexpr bool all(const vec3T<bool>& v) { return v.x && v.y && v.z; }
inline constexpr bool all(const vec4T<bool>& v) { return v.x && v.y && v.z && v.w; }

using vec2 = vec2T<float>;
using vec3 = vec3T<float>;
using vec4 = vec4T<float>;

using vec2d = vec2T<double>;
using vec3d = vec3T<double>;
using vec4d = vec4T<double>;

using vec2i = vec2T<i32>;
using vec3i = vec3T<i32>;
using vec4i = vec4T<i32>;

using vec2u = vec2T<u32>;
using vec3u = vec3T<u32>;
using vec4u = vec4T<u32>;

using vec2b = vec2T<bool>;
using vec3b = vec3T<bool>;
using vec4b = vec4T<bool>;

#pragma warning(pop)
