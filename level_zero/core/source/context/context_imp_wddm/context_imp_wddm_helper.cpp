/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

ze_result_t ContextImp::getVirtualAddressSpaceIpcHandle(ze_device_handle_t hDevice,
                                                        ze_ipc_mem_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::putVirtualAddressSpaceIpcHandle(ze_ipc_mem_handle_t ipcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
