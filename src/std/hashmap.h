#pragma once

#include "common.h"
#include "arr.h"

template<typename Key, typename Value>
struct HashMap {
	Value *push(const Key &key, const Value &value = {}) {
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			keys.push(key);
			return &values.push(value);
		}
		else {
			values[pos] = value;
			return &values[pos];
		}
	}

	Value* push(const Key &key, Value &&value) {
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			keys.push(key);
			return &values.push(mem::move(value));
		}
		else {
			values[pos] = mem::move(value);
			return &values[pos];
		}
	}

	Value *get(const Key& key) {
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			return nullptr;
		}
		else {
			return &values[pos];
		}
	}

	Value *begin() { return values.begin(); }
	Value *end() { return values.end(); }
	const Value *begin() const { return values.begin(); }
	const Value *end() const { return values.end(); }

	arr<Key> keys;
	arr<Value> values;
};