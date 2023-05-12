/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning) {
    if (NEO::isAnyPackedDeviceBinaryFormat(binary.deviceBinary)) {
        return std::vector<uint8_t>(binary.deviceBinary.begin(), binary.deviceBinary.end());
    }
    return packDeviceBinary<DeviceBinaryFormat::OclElf>(binary, outErrReason, outWarning);
}

TargetDevice getTargetDevice(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    TargetDevice targetDevice = {};

    targetDevice.coreFamily = hwInfo.platform.eRenderCoreFamily;
    targetDevice.productFamily = hwInfo.platform.eProductFamily;
    targetDevice.aotConfig.value = productHelper.getHwIpVersion(hwInfo);
    targetDevice.stepping = hwInfo.platform.usRevId;
    targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
    targetDevice.grfSize = hwInfo.capabilityTable.grfSize;
    targetDevice.minScratchSpaceSize = gfxCoreHelper.getMinimalScratchSpaceSize();
    return targetDevice;
}
} // namespace NEO
