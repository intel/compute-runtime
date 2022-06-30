/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

ze_result_t handleAllocationExtensions(NEO::GraphicsAllocation *alloc, ze_memory_type_t type,
                                       void *pNext, struct DriverHandleImp *driverHandle);
} // namespace L0
