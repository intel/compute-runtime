/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/sysman_device_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/os_sysman.h"

#include <vector>

namespace L0 {
namespace Sysman {

SysmanDeviceImp::SysmanDeviceImp(NEO::ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex)
    : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {
    this->executionEnvironment->incRefInternal();
}

SysmanDeviceImp::~SysmanDeviceImp() {
    executionEnvironment->decRefInternal();
}

ze_result_t SysmanDeviceImp::init() {
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
