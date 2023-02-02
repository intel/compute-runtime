/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_gfx_core_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

createClGfxCoreHelperFunctionType clGfxCoreHelperFactory[IGFX_MAX_CORE] = {};

std::unique_ptr<ClGfxCoreHelper> ClGfxCoreHelper::create(GFXCORE_FAMILY gfxCore) {
    auto createClGfxCoreHelperFunc = clGfxCoreHelperFactory[gfxCore];
    if (createClGfxCoreHelperFunc == nullptr) {
        return nullptr;
    }
    auto clGfxCoreHelper = createClGfxCoreHelperFunc();
    return clGfxCoreHelper;
}

uint8_t ClGfxCoreHelper::makeDeviceRevision(const HardwareInfo &hwInfo) {
    return static_cast<uint8_t>(!hwInfo.capabilityTable.isIntegratedDevice);
}

cl_version ClGfxCoreHelper::makeDeviceIpVersion(uint16_t major, uint8_t minor, uint8_t revision) {
    return (major << 16) | (minor << 8) | revision;
}

template <>
ClGfxCoreHelper &RootDeviceEnvironment::getHelper<ClGfxCoreHelper>() const {
    return *static_cast<ClGfxCoreHelper *>(apiGfxCoreHelper.get());
}

void RootDeviceEnvironment::initApiGfxCoreHelper() {

    apiGfxCoreHelper = ClGfxCoreHelper::create(this->getHardwareInfo()->platform.eRenderCoreFamily);
}

} // namespace NEO
