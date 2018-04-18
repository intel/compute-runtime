/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "config.h"
#include "runtime/api/dispatch.h"
#include <cstdint>

struct ClDispatch {
    SEntryPointsTable dispatch;
    ClDispatch() : dispatch(globalDispatchTable) {
    }
};

struct _cl_accelerator_intel : public ClDispatch {
};

struct _cl_command_queue : public ClDispatch {
};

// device_queue is a type used internally
struct _device_queue : public _cl_command_queue {
};
typedef _device_queue *device_queue;

struct _cl_context : public ClDispatch {
    bool isSharedContext = false;
};

struct _cl_device_id : public ClDispatch {
};

struct _cl_event : public ClDispatch {
};

struct _cl_kernel : public ClDispatch {
};

struct _cl_mem : public ClDispatch {
};

struct _cl_platform_id : public ClDispatch {
};

struct _cl_program : public ClDispatch {
};

struct _cl_sampler : public ClDispatch {
};

template <typename Type>
inline bool isValidObject(Type object) {
    return object && object->dispatch.icdDispatch == &icdGlobalDispatchTable;
}
