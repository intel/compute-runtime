/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary &binary, std::string &outErrReason, std::string &outWarning) {
    if (NEO::isAnyPackedDeviceBinaryFormat(binary.deviceBinary)) {
        return std::vector<uint8_t>(binary.deviceBinary.begin(), binary.deviceBinary.end());
    }
    return packDeviceBinary<DeviceBinaryFormat::oclElf>(binary, outErrReason, outWarning);
}

TargetDevice getTargetDevice(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto initialHwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    TargetDevice targetDevice = {};
    auto hwInfo = initialHwInfo;
    compilerProductHelper.adjustHwInfoForIgc(hwInfo);

    targetDevice.coreFamily = hwInfo.platform.eRenderCoreFamily;
    targetDevice.productFamily = hwInfo.platform.eProductFamily;
    targetDevice.aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);
    targetDevice.stepping = hwInfo.platform.usRevId;
    targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
    targetDevice.grfSize = hwInfo.capabilityTable.grfSize;
    targetDevice.minScratchSpaceSize = gfxCoreHelper.getMinimalScratchSpaceSize();
    targetDevice.samplerStateSize = static_cast<uint32_t>(gfxCoreHelper.getSamplerStateSize());
    targetDevice.samplerBorderColorStateSize = gfxCoreHelper.getSamplerBorderColorStateSize();

    if (auto ail = rootDeviceEnvironment.getAILConfigurationHelper(); nullptr != ail) {
        targetDevice.applyValidationWorkaround = ail->useLegacyValidationLogic();
    } else if (debugManager.flags.DoNotUseProductConfigForValidationWa.get()) {
        targetDevice.applyValidationWorkaround = true;
    }
    return targetDevice;
}
} // namespace NEO
