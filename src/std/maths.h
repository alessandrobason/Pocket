#pragma once

namespace math {
	constexpr float pi  = 3.14159265359f;
	constexpr float pi2 = 6.28318530718f;

	template<typename T>
	constexpr T toRad(T deg) {
		return deg * (pi / 180.f);
	}

	template<typename T>
	constexpr T toDeg(T rad) {
		return rad * (180.f / pi);
	}

	template<typename T>
	constexpr T min(const T &a, const T &b) {
		return a < b ? a : b;
	}
	
	template<typename T>
	constexpr T max(const T &a, const T &b) {
		return a > b ? a : b;
	}

	template<typename T>
	constexpr T clamp(const T &value, const T &minv, const T &maxv) {
		return math::min(math::max(value, minv), maxv);
	}

	template<typename T>
	constexpr T lerp(const T &start, const T &end, float alpha) {
		return start * (1.f - alpha) + end * alpha;
	}
} // namespace math 
