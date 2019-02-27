/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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
