#pragma once

#include "common.h"
#include "arr.h"

template<typename T>
struct HashSet {
    HashSet() {
        reserve(64);
    }

    void setTombstone(const T &tomb) {
        tombstone = tomb;
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

		values.resize(next_pow2);
    }

    // returns true if unique
    bool push(const T &key) {
        extern u32 hash_impl(const T &);

        return pushImpl(key, hash_impl(key));
    }

    bool has(const T &key) const {
        extern u32 hash_impl(const T &);

        u32 hash = hash_impl(key) & ((u32)values.len - 1);

        // maximum number of iterations
        for (usize i = 0; i < values.len; ++i) {
            // unique, doesn't exist
            if (values[hash] == tombstone) {
				return false;
            }
            
            // found it!
            if (values[hash] == key) {
                return true;
            }

            // theres something, but not what we're looking for,
			// linear probe
            hash = (hash + 1) & (u32)(values.len - 1);
        }

        return false;
    }

    // return false if item wasn't in hash set
    bool remove(const T &key) {
        extern u32 hash_impl(const T &);

        u32 hash = hash_impl(key) & ((u32)values.len - 1);
        // maximum number of iterations
        for (usize i = 0; i < values.len; ++i) {
            // doesn't exist
            if (values[hash] == tombstone) {
				return false;
            }
            
            // found it!
            if (values[hash] == key) {
                values[hash] = tombstone;
                return true;
            }

            // theres something, but not what we're looking for,
			// linear probe
            hash = (hash + 1) & (u32)(values.len - 1);
        }

        return false;
    }

    struct HashSetIter {
		HashSetIter(HashSet &set, usize index) : set(set), cur_index(index) {}

		T &operator*() { set.values[cur_index]; }
		T *operator->() { &set.values[cur_index]; }

		const T &operator*() const { set.values[cur_index]; }
		const T *operator->() const { &set.values[cur_index]; }

		HashSetIter &operator++() {
			for (++cur_index; cur_index < set.values.size(); ++cur_index) {
				if (set.values[cur_index] != set.tombstone) {
					break;
				}
			}

			return *this;
		}

		bool operator==(const HashSetIter &other) const {
			return cur_index == other.cur_index;
		}

		bool operator!=(const HashSetIter &other) const {
			return cur_index != other.cur_index;
		}

		HashSet &set;
		usize cur_index = 0;
    };

	HashSetIter begin() { return HashIter(*this, 0); }
	HashSetIter end() { return HashIter(*this, values.size()); }
	const HashSetIter begin() const { return HashIter(*this, 0); }
	const HashSetIter end() const { return HashIter(*this, values.size()); }

private:
    bool pushImpl(const T &key, u32 hash) {
        hash &= (u32)values.len - 1;

        // maximum number of iterations
        for (usize i = 0; i < values.len; ++i) {
            // unique
            if (values[hash] == tombstone) {
                values[hash] = key;
                return true;
            }
            
            // not unique
            if (values[hash] == key) {
                return false;
            }

            // linear probe
            hash = (hash + 1) & (u32)(values.len - 1);
        }

		// new item and no empty spots, lets resize
		usize old_len = values.len;
		values.resize(old_len * 2);

		// TODO avoid stack overflow
		return pushImpl(key, hash);
    }

    arr<T> values;
    T tombstone = {};
};