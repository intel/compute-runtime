/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {
struct HwHelperTestsXeHpcCore : public ClDeviceFixture {
    void checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(HardwareInfo *hwInfo);
    void checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(HardwareInfo *hwInfo);
    void setupDeviceIdAndRevision(HardwareInfo *hwInfo, ClDevice &clDevice);
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
};
} // namespace NEO
