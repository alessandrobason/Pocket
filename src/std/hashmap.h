#pragma once

#include "common.h"
#include "arr.h"

// TODO: if < X items, fallback to linear search and array
// TODO: better api?

template<typename Key, typename Value>
struct HashMap {
	HashMap() {
		reserve(64);
	}

	void setTombstone(const Key &new_tomb) {
		tombstone = new_tomb;
	}

	void setTombstone(Key &&new_tomb) {
		tombstone = mem::move(new_tomb);
	}
	
	void reserve(usize n) {
		usize next_pow2 = n;
		
        // find next power of two
        next_pow2--;
        next_pow2 |= next_pow2 >> 1;
        next_pow2 |= next_pow2 >> 2;
        next_pow2 |= next_pow2 >> 4;
        next_pow2 |= next_pow2 >> 8;
        next_pow2 |= next_pow2 >> 16;
        next_pow2++;

		keys.clear();
		keys.resize(next_pow2, tombstone);
		values.resize(next_pow2);
	}

	Value *push(const Key &key, const Value &value = {}) {
		//Value &val = pushImpl(key, key.hash());
		Value &val = pushImpl(key, hash_impl(key));
		val = value;
		return &val;
#if 0
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			keys.push(key);
			return &values.push(value);
		}
		else {
			values[pos] = value;
			return &values[pos];
		}
#endif
	}

	Value* push(const Key &key, Value &&value) {
		//Value &val = pushImpl(key, key.hash());
		Value &val = pushImpl(key, hash_impl(key));
		val = mem::move(value);
		return &val;
#if 0
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			keys.push(key);
			return &values.push(mem::move(value));
		}
		else {
			values[pos] = mem::move(value);
			return &values[pos];
		}
#endif
	}

	Value *get(const Key& key) {
		u32 hash = hash_impl(key) & ((u32)keys.size() - 1);
		//u32 hash = key.hash() & ((u32)keys.size() - 1);

        // maximum number of iterations
        for (usize i = 0; i < keys.size(); ++i) {
            // unique, doesn't exist
            if (keys[hash] == tombstone) {
				return nullptr;
            }
            
            // found it!
            if (keys[hash] == key) {
                return &values[hash];
            }

            // theres something, but not what we're looking for,
			// linear probe
            hash = (hash + 1) & (u32)(keys.size() - 1);
        }

		return nullptr;
#if 0
		usize pos = keys.find(key);
		if (pos == SIZE_MAX) {
			return nullptr;
		}
		else {
			return &values[pos];
		}
#endif
	}

	struct HashIter {
		HashIter(HashMap &map, usize index) : map(map), cur_index(index) {}

		Value &operator*() { return map.values[cur_index]; }
		Value *operator->() { return &map.values[cur_index]; }

		const Value &operator*() const { return map.values[cur_index]; }
		const Value *operator->() const { return &map.values[cur_index]; }

		HashIter &operator++() {
			for (++cur_index; cur_index < map.keys.size(); ++cur_index) {
				if (map.keys[cur_index] != map.tombstone) {
					break;
				}
			}

			return *this;
		}

		bool operator==(const HashIter &other) const {
			return cur_index == other.cur_index;
		}

		bool operator!=(const HashIter &other) const {
			return cur_index != other.cur_index;
		}

		HashMap &map;
		usize cur_index = 0;
	};

	HashIter begin() { return HashIter(*this, 0); }
	HashIter end() { return HashIter(*this, keys.size()); }
	const HashIter begin() const { return HashIter(*this, 0); }
	const HashIter end() const { return HashIter(*this, keys.size()); }

private:
	Value &pushImpl(const Key &key, u32 hash) {
		// u32 hash = key.hash() & ((u32)keys.size() - 1);
		hash &= (u32)keys.size() - 1;

        // maximum number of iterations
        for (usize i = 0; i < keys.size(); ++i) {
            // unique
            if (keys[hash] == tombstone) {
                keys[hash] = key;
                return values[hash];
            }
            
            // not unique
            if (keys[hash] == key) {
                return values[hash];
            }

            // linear probe
            hash = (hash + 1) & (u32)(keys.size() - 1);
        }

		// new item and no empty spots, lets resize
		usize old_len = keys.len;
		keys.resize(old_len * 2);
		values.resize(old_len * 2);

		// TODO avoid stack overflow
		return pushImpl(key, hash);
	}

	arr<Key> keys;
	arr<Value> values;
	Key tombstone = {};
};