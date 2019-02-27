/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string.h>
#include <thread>

// OpenCL Types
typedef uint32_t uint;
typedef uint8_t uchar;
typedef uint16_t ushort;
typedef uint64_t ulong;

namespace BuiltinKernelsSimulation {

// number of threads in wkg
#define NUM_OF_THREADS 24

#define CLK_GLOBAL_MEM_FENCE 1
#define CLK_LOCAL_MEM_FENCE 2

class SynchronizationBarrier {
  public:
    SynchronizationBarrier(int count) : m_InitialCount(count) {
        m_Count = count;
        m_BarrierCounter = 0;
    }

    ~SynchronizationBarrier() {
    }

    void enter() {
        std::unique_lock<std::mutex> lck(m_Mutex);

        m_Count--;

        unsigned int BarrierCount = m_BarrierCounter;

        if (m_Count > 0) {
            while (BarrierCount == m_BarrierCounter) {
                m_AllHitBarrierCondition.wait(lck);
            }
        } else {
            m_Count = m_InitialCount;
            m_BarrierCounter++;
            m_AllHitBarrierCondition.notify_all();
        }
    }

  private:
    std::mutex m_Mutex;
    std::condition_variable m_AllHitBarrierCondition;
    int m_Count;
    const int m_InitialCount;
    unsigned int m_BarrierCounter;
};

// globals
extern std::mutex gMutex;
extern unsigned int globalID[3];
extern unsigned int localID[3];
extern unsigned int localSize[3];
extern std::map<std::thread::id, uint32_t> threadIDToLocalIDmap;
extern SynchronizationBarrier *pGlobalBarrier;

typedef struct taguint2 {
    taguint2(uint x, uint y) {
        this->x = x;
        this->y = y;
    }
    taguint2() {
        this->x = 0;
        this->y = 0;
    }
    uint x;
    uint y;
} uint2;

typedef struct taguint3 {
    taguint3(uint x, uint y, uint z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    taguint3() {
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }
    uint x;
    uint y;
    uint z;
} uint3;

typedef struct taguint4 {
    taguint4(uint x, uint y, uint z, uint w) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }
    uint x;
    uint y;
    uint z;
    uint w;
} uint4;

typedef struct tagint2 {
    tagint2(int x, int y) {
        this->x = x;
        this->y = y;
    }
    int x;
    int y;
} int2;

typedef struct tagint3 {
    tagint3(int x, int y, int z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
    int x;
    int y;
    int z;
} int3;

typedef struct tagint4 {
    tagint4(int x, int y, int z, int w) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }
    int x;
    int y;
    int z;
    int w;
} int4;

typedef struct tagushort2 {
    tagushort2(ushort x, ushort y) {
        this->x = x;
        this->y = y;
    }
    unsigned short x;
    unsigned short y;
} ushort2;

typedef struct tagushort8 {
    unsigned short xxx[8];
} ushort8;

typedef struct tagushort16 {
    unsigned short xxx[16];
} ushort16;

uint4 operator+(uint4 const &a, uint4 const &b);
int4 operator+(int4 const &a, int4 const &b);

typedef struct tagimage {
    char *ptr;
    uint width;
    uint height;
    uint depth;
    uint bytesPerChannel;
    uint channels;
} image;

// images as pointer
typedef image *image1d_t;
typedef image *image2d_t;
typedef image *image3d_t;

// OpenCL keywords
#define __global
#define __local
#define __private
#define __kernel
#define __attribute__(...)
#define __read_only
#define __write_only
#define queue_t void *

struct clk_event_t {
    clk_event_t() {
        value = 0;
    }
    clk_event_t(void *v) {
        value = static_cast<uint>(reinterpret_cast<uintptr_t>(v));
    }

    explicit operator void *() const {
        return reinterpret_cast<void *>(static_cast<uintptr_t>(value));
    }

    operator uint() {
        return (uint)value;
    }

    void operator=(uint input) {
        value = input;
    }

    uint value;
};

// OpenCL builtins
#define __builtin_astype(var, type) \
    (                               \
        (type)var)

#define select(a, b, c) (c ? b : a)

uint get_local_id(int dim);
uint get_global_id(int dim);
uint get_local_size(int dim);
uint get_num_groups(int dim);
uint get_group_id(int dim);
void barrier(int x);
uint4 read_imageui(image *im, int4 coord);
uint4 write_imageui(image *im, uint4 coord, uint4 color);
uchar convert_uchar_sat(uint c);
ushort convert_ushort_sat(uint c);

#define EMULATION_ENTER_FUNCTION() \
    uint __LOCAL_ID__ = 0;         \
    __LOCAL_ID__ = get_local_id(0);

template <class TYPE, class TYPE2>
void atomic_xchg(TYPE *dest, TYPE2 val) {
    gMutex.lock();
    dest[0] = (TYPE)val;
    gMutex.unlock();
}

template <class TYPE, class TYPE2>
TYPE atomic_add(TYPE *first, TYPE2 second) {
    gMutex.lock();
    TYPE temp = first[0];
    first[0] = (TYPE)(temp + (TYPE)second);
    gMutex.unlock();
    return temp;
}

template <class TYPE, class TYPE2>
TYPE atomic_sub(TYPE *first, TYPE2 second) {
    gMutex.lock();
    TYPE temp = first[0];
    first[0] = temp - second;
    gMutex.unlock();
    return temp;
}

template <class TYPE>
TYPE atomic_inc(TYPE *first) {
    gMutex.lock();
    TYPE temp = first[0];
    first[0] = temp + 1;
    gMutex.unlock();
    return temp;
}

template <class TYPE>
TYPE atomic_dec(TYPE *first) {
    gMutex.lock();
    TYPE temp = first[0];
    first[0] = temp - 1;
    gMutex.unlock();
    return temp;
}

template <class TYPE, class TYPE2>
TYPE atomic_min(TYPE *first, TYPE2 second) {
    gMutex.lock();
    TYPE temp = first[0];
    first[0] = (TYPE)((TYPE)second < temp ? (TYPE)second : temp);
    gMutex.unlock();
    return temp;
}
} // namespace BuiltinKernelsSimulation
