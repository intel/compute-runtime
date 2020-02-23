/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "opencl/source/tracing/tracing_types.h"

#include <bitset>
#include <stdint.h>

namespace HostSideTracing {

struct TracingHandle {
  public:
    TracingHandle(cl_tracing_callback callback, void *userData) : callback(callback), userData(userData) {}

    void call(cl_function_id fid, cl_callback_data *callbackData) {
        callback(fid, callbackData, userData);
    }

    void setTracingPoint(cl_function_id fid, bool enable) {
        DEBUG_BREAK_IF(static_cast<uint32_t>(fid) >= CL_FUNCTION_COUNT);
        mask[static_cast<uint32_t>(fid)] = enable;
    }

    bool getTracingPoint(cl_function_id fid) const {
        DEBUG_BREAK_IF(static_cast<uint32_t>(fid) >= CL_FUNCTION_COUNT);
        return mask[static_cast<uint32_t>(fid)];
    }

  private:
    cl_tracing_callback callback;
    void *userData;
    std::bitset<CL_FUNCTION_COUNT> mask;
};

} // namespace HostSideTracing

struct _cl_tracing_handle {
    cl_device_id device;
    HostSideTracing::TracingHandle *handle;
};
