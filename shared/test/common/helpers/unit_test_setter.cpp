/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "neo_igfxfmid.h"

namespace NEO {

void UnitTestSetter::setCcsExposure(RootDeviceEnvironment &rootDeviceEnvironment) {
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    if (hwInfo && (hwInfo->platform.eRenderCoreFamily >= IGFX_XE3P_CORE)) {
        hwInfo->featureTable.flags.ftrCCSNode = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0;
    }
}

} // namespace NEO
