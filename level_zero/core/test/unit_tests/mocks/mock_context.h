/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Context> : public ::L0::Context {};

using Context = WhiteBox<::L0::Context>;

template <>
struct Mock<Context> : public Context {
    Mock();
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                getStatus,
                (),
                (override));
    MOCK_METHOD(DriverHandle *,
                getDriverHandle,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                allocHostMem,
                (ze_host_mem_alloc_flag_t flags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocDeviceMem,
                (ze_device_handle_t hDevice,
                 ze_device_mem_alloc_flag_t flags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocSharedMem,
                (ze_device_handle_t hDevice,
                 ze_device_mem_alloc_flag_t deviceFlags,
                 ze_host_mem_alloc_flag_t hostFlags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                freeMem,
                (const void *ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemAllocProperties,
                (const void *ptr,
                 ze_memory_allocation_properties_t *pMemAllocProperties,
                 ze_device_handle_t *phDevice),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemAddressRange,
                (const void *ptr,
                 void **pBase,
                 size_t *pSize),
                (override));
    MOCK_METHOD(ze_result_t,
                getIpcMemHandle,
                (const void *ptr,
                 ze_ipc_mem_handle_t *pIpcHandle),
                (override));
    MOCK_METHOD(ze_result_t,
                closeIpcMemHandle,
                (const void *ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                openIpcMemHandle,
                (ze_device_handle_t hDevice,
                 ze_ipc_mem_handle_t handle,
                 ze_ipc_memory_flags_t flags,
                 void **ptr),
                (override));
};

} // namespace ult
} // namespace L0
