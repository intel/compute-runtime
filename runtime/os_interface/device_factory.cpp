/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/aub_memory_operations_handler.h"
#include "runtime/aub/aub_center.h"
#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

bool DeviceFactory::getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto totalDeviceCount = 1u;
    if (DebugManager.flags.CreateMultipleDevices.get()) {
        totalDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }
    auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
    auto hwInfoConst = *platformDevices;
    getHwInfoForPlatformString(productFamily, hwInfoConst);
    numDevices = 0;
    std::string hwInfoConfig;
    DebugManager.getHardwareInfoOverride(hwInfoConfig);

    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    *hardwareInfo = *hwInfoConst;

    hardwareInfoSetup[hwInfoConst->platform.eProductFamily](hardwareInfo, true, hwInfoConfig);

    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    hwConfig->configureHardwareCustom(hardwareInfo, nullptr);

    numDevices = totalDeviceCount;
    DeviceFactory::numDevices = numDevices;
    auto csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr > 0) {
        executionEnvironment.initAubCenter(DebugManager.flags.EnableLocalMemory.get(), "", static_cast<CommandStreamReceiverType>(csr));
        auto aubCenter = executionEnvironment.aubCenter.get();
        executionEnvironment.memoryOperationsInterface = std::make_unique<AubMemoryOperationsHandler>(aubCenter->getAubManager());
    }
    return true;
}

} // namespace NEO
