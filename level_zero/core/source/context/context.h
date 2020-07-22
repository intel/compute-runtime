/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "third_party/level_zero/ze_api_ext.h"

struct _ze_context_handle_t {
    virtual ~_ze_context_handle_t() = default;
};

namespace L0 {
struct DriverHandle;

struct Context : _ze_context_handle_t {
    virtual ~Context() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getStatus() = 0;
    virtual DriverHandle *getDriverHandle() = 0;
    virtual ze_result_t allocHostMem(ze_host_mem_alloc_flag_t flags,
                                     size_t size,
                                     size_t alignment,
                                     void **ptr) = 0;
    virtual ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                                       ze_device_mem_alloc_flag_t flags,
                                       size_t size,
                                       size_t alignment, void **ptr) = 0;
    virtual ze_result_t allocSharedMem(ze_device_handle_t hDevice,
                                       ze_device_mem_alloc_flag_t deviceFlags,
                                       ze_host_mem_alloc_flag_t hostFlags,
                                       size_t size,
                                       size_t alignment,
                                       void **ptr) = 0;
    virtual ze_result_t freeMem(const void *ptr) = 0;
    virtual ze_result_t getMemAddressRange(const void *ptr,
                                           void **pBase,
                                           size_t *pSize) = 0;
    virtual ze_result_t closeIpcMemHandle(const void *ptr) = 0;
    virtual ze_result_t getIpcMemHandle(const void *ptr,
                                        ze_ipc_mem_handle_t *pIpcHandle) = 0;
    virtual ze_result_t openIpcMemHandle(ze_device_handle_t hDevice,
                                         ze_ipc_mem_handle_t handle,
                                         ze_ipc_memory_flags_t flags,
                                         void **ptr) = 0;
    virtual ze_result_t getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) = 0;

    static Context *fromHandle(ze_context_handle_t handle) { return static_cast<Context *>(handle); }
    inline ze_context_handle_t toHandle() { return this; }
};

} // namespace L0
