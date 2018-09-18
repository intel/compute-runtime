/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include "CL/cl.h"
#include "opencl_c.h"
#include "runtime/helpers/string.h"

namespace BuiltinKernelsSimulation {

#define SCHEDULER_EMULATION 1

// globals
std::mutex gMutex;
unsigned int globalID[3];
unsigned int localID[3];
unsigned int localSize[3];

std::map<std::thread::id, uint32_t> threadIDToLocalIDmap;

SynchronizationBarrier *pGlobalBarrier = nullptr;

uint4 operator+(uint4 const &a, uint4 const &b) {
    uint4 c(0, 0, 0, 0);
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    c.z = a.z + b.z;
    c.w = a.w + b.w;
    return c;
}

int4 operator+(int4 const &a, int4 const &b) {
    int4 c(0, 0, 0, 0);
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    c.z = a.z + b.z;
    c.w = a.w + b.w;
    return c;
}

uint get_local_id(int dim) {
    uint LID = 0;

    // use thread id
    if (threadIDToLocalIDmap.size() > 0) {
        std::thread::id id = std::this_thread::get_id();
        LID = threadIDToLocalIDmap[id] % 24;
    }
    // use id from loop iteration
    else {
        LID = localID[dim];
    }
    return LID;
}

uint get_global_id(int dim) {
    uint GID = 0;

    // use thread id
    if (threadIDToLocalIDmap.size() > 0) {
        std::thread::id id = std::this_thread::get_id();
        GID = threadIDToLocalIDmap[id];
    }
    // use id from loop iteration
    else {
        GID = globalID[dim];
    }
    return GID;
}

uint get_local_size(int dim) {
    return localSize[dim];
}

uint get_num_groups(int dim) {
    return NUM_OF_THREADS / 24;
}

uint get_group_id(int dim) {
    return get_global_id(dim) / 24;
}

void barrier(int x) {
    pGlobalBarrier->enter();

    // int LID = get_local_id(0);
    volatile int BreakPointHere = 0;

    // PUT BREAKPOINT HERE to stop after each barrier
    BreakPointHere++;
}

uint4 read_imageui(image *im, int4 coord) {
    uint4 color = {0, 0, 0, 1};

    uint offset = ((coord.z * im->height + coord.y) * im->width + coord.x) * im->bytesPerChannel * im->channels;

    char *temp = &im->ptr[offset];
    char *colorDst = (char *)&color;

    for (uint i = 0; i < im->channels; i++) {
        memcpy_s(colorDst, sizeof(uint4), temp, im->bytesPerChannel);
        temp += im->bytesPerChannel;
        colorDst += 4;
    }
    return color;
}

uint4 write_imageui(image *im, uint4 coord, uint4 color) {
    uint offset = ((coord.z * im->height + coord.y) * im->width + coord.x) * im->bytesPerChannel * im->channels;

    char *temp = &im->ptr[offset];
    char *colorSrc = (char *)&color;

    size_t size = im->width * im->height * im->depth * im->bytesPerChannel * im->channels;

    for (uint i = 0; i < im->channels; i++) {
        memcpy_s(temp, size - offset, colorSrc, im->bytesPerChannel);
        temp += im->bytesPerChannel;
        colorSrc += 4;
    }
    return *(uint4 *)temp; // NOLINT
}

uchar convert_uchar_sat(uint c) {
    return (uchar)c;
}

ushort convert_ushort_sat(uint c) {
    return (ushort)c;
}

} // namespace BuiltinKernelsSimulation
