/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning) {
    if (NEO::isAnyPackedDeviceBinaryFormat(binary.deviceBinary)) {
        return std::vector<uint8_t>(binary.deviceBinary.begin(), binary.deviceBinary.end());
    }
    return packDeviceBinary<DeviceBinaryFormat::OclElf>(binary, outErrReason, outWarning);
}

TargetDevice targetDeviceFromHwInfo(const HardwareInfo &hwInfo) {
    TargetDevice targetDevice = {};
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    targetDevice.coreFamily = hwInfo.platform.eRenderCoreFamily;
    targetDevice.productFamily = hwInfo.platform.eProductFamily;
    targetDevice.aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);
    targetDevice.stepping = hwInfo.platform.usRevId;
    targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
    targetDevice.grfSize = hwInfo.capabilityTable.grfSize;
    targetDevice.minScratchSpaceSize = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getMinimalScratchSpaceSize();
    return targetDevice;
}
} // namespace NEO
