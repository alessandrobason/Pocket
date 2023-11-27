#include "sort.h"

// convert number of bits to number of bytes (e.g. 24 -> 3)
constexpr u32 radix__bits_to_byte(u32 bits) {
    return bits >> 3;
}

static u32 radix__find_max(u32 *beg, u32 *end) {
    u32 max = *beg;
    for (; beg < end; ++beg) {
        if (max < *beg) max = *beg;
    }
    return max;
}

static u32 radix__find_max(u8 *beg, u8 *end, u32 stride) {
    u32 max = *((u32 *)beg);
    for (; beg < end; beg += stride) {
        u32 v = *((u32 *)beg);
        if (max < v) max = v;
    }
    return max;
}

void radixSort(u32 *buf, u32 len) {
    Arena temp = Arena::make(sizeof(u32) * len, Arena::Malloc);
    radixSort(buf, len, temp);
}

void radixSort(u32 *buf, u32 len, Arena scratch) {
    u32 *helper = nullptr;
    // bits sorted
    u32 exp = 0;
    // array of pointers to the helper array
    u32 *point[256];
    bool swap = false;

    // Define std preliminary, constrain and expression to check if all bytes are sorted
    // Set preliminary according to size
    const u32 preliminary = (len > 100) ? 100 : (len >> 3);
    
    // If we found a integer with more than 24 bits in preliminar,
    // will have to sort all bytes either way, so max = max_uint
    u32 max = radix__find_max(buf + 1, buf + preliminary);
    if(max <= (UINT_MAX >> 8)) {
        u32 new_max = radix__find_max(buf + preliminary, buf + len);
        if (new_max > max) max = new_max;
    }
    
    // create helper array
    helper = scratch.alloc<u32>(len);

    // while (bits_evaluated < bits_to_evaluate && max_has_bits_to_evaluate)
    while(exp < (sizeof(u32) << 3) && (max >> exp) > 0) {

        u32 *vec = buf, *help = helper;
        if (swap) {
            vec = helper;
            help = buf;
        }

        // Core algorithm: for a specific byte, fill the buckets array,
        // rearrange the array and reset the initial array accordingly.

        u32 bucket[256] = {0};
        u8 *beg = (u8*)(vec) + radix__bits_to_byte(exp);
        u8 *end = (u8*)(&vec[len & 0xfffffffc]);
        
        // to add to the bucket, we process 4 u32 at a time
        // (this is why we mask out the 2 least significant bits in size, if size < 4 -> end <= beg)
        // we set the bucket (++bucket[*beg]), then we skip to the next u32 (beg += sizeof(u32))
        while(beg < end) {
            ++bucket[*beg]; beg += sizeof(u32);
            ++bucket[*beg]; beg += sizeof(u32);
            ++bucket[*beg]; beg += sizeof(u32);
            ++bucket[*beg]; beg += sizeof(u32);
        }
        // then we add what is left byte by byte
        beg = (u8*)(&vec[len & 0xfffffffc]) + (exp >> 3);
        end = (u8*)(&vec[len]);
        while(beg < end) {
            ++bucket[*beg]; beg += sizeof(u32);
        }
        // check if the current byte is the same in the whole buffer
        bool next = false;
        for(u32 i = 0; i < 256; ++i) {
            if(bucket[i] == len) {
                next = true;
                break;
            }
        }
        if(next) {
            exp += 8;
            return;
        }
        // map bucket to helper (final) array
        for(u32 i = 0; i < 256; help += bucket[i++]) {
            point[i] = help;
        }
        // 
        for(help = vec; help < &vec[len]; ++help) {
            u8 cur_byte = (*help >> exp) & 0xFF;
            *point[cur_byte] = *help; 
            point[cur_byte]++; 
        }
        swap = !swap;
        exp += 8;
    }

    if(swap) {
	    memcpy(buf, helper, sizeof(u32) * len);
    }
}

void radixSort(void *data, u32 len, u32 stride) {
    Arena temp = Arena::make(stride * len, Arena::Malloc);
    radixSort(data, len, stride, temp);
}

void radixSort(void *data, u32 len, u32 stride, Arena scratch) {
    u8 *buf = (u8 *)data;

    // bits sorted
    u32 exp = 0;
    // array of pointers to the helper array
    u8 *point[256];
    bool swap = false;

    // Define std preliminary, constrain and expression to check if all bytes are sorted
    // Set preliminary according to size
    const u32 preliminary = (len > 100) ? 100 : (len >> 3);
    
    // If we found a integer with more than 24 bits in preliminar,
    // will have to sort all bytes either way, so max = max_uint
    u32 max = radix__find_max(buf + 1 * stride, buf + preliminary * stride, stride);
    if (max <= (UINT_MAX >> 8)) {
        u32 new_max = radix__find_max(buf + preliminary * stride, buf + len * stride, stride);
        if (new_max > max) max = new_max;
    }

    // create helper array
    u8 *helper = scratch.alloc<u8>(len * stride);

    // while (bits_evaluated < bits_to_evaluate && max_has_bits_to_evaluate)
    while (exp < (sizeof(u32) << 3) && (max >> exp) > 0) {
        u8 *vec = buf, *help = helper;
        if (swap) {
            vec = helper;
            help = buf;
        }

        // Core algorithm: for a specific byte, fill the buckets array,
        // rearrange the array and reset the initial array accordingly.

        u32 bucket[256] = {0};
        u8 *beg = vec + radix__bits_to_byte(exp) * stride;
        u8 *end = vec + (len & 0xFFFFFFFC) * stride;
        
        // to add to the bucket, we process 4 u32 at a time
        // (this is why we mask out the 2 least significant bits in size, if size < 4 -> end <= beg)
        // we set the bucket (++bucket[*beg]), then we skip to the next u32 (beg += sizeof(u32))
        while (beg < end) {
            ++bucket[*beg]; beg += stride;
            ++bucket[*beg]; beg += stride;
            ++bucket[*beg]; beg += stride;
            ++bucket[*beg]; beg += stride;
        }
        // then we add what is left byte by byte
        beg = vec + ((len & 0xFFFFFFFC) + (exp >> 3)) * stride;
        end = vec + len * stride;
        while (beg < end) {
            ++bucket[*beg]; beg += stride;
        }
        // check if the current byte is the same in the whole buffer
        bool next = false;
        for (u32 i = 0; i < 256; ++i) {
            if(bucket[i] == len) {
                next = true;
                break;
            }
        }
        if (next) {
            exp += 8;
            return;
        }
        // map bucket to helper (final) array
        for (u32 i = 0; i < 256; help += (bucket[i++] * stride)) {
            point[i] = help;
        }
        beg = vec;
        end = vec + len * stride;
        for (; beg < end; beg += stride) {
            u32 value = *(u32*)beg;
            u8 cur_byte = (value >> exp) & 0xFF;
            *point[cur_byte] = value; 
            point[cur_byte] += stride; 
        }
        swap = !swap;
        exp += 8;
    }

    if (swap) {
	    memcpy(buf, helper, stride * len);
    }
}
