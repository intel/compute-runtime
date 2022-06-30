/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/dispatch.h"

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
