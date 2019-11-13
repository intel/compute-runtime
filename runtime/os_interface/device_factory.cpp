/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper.h"
#include "core/os_interface/aub_memory_operations_handler.h"
#include "runtime/aub/aub_center.h"
#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

bool DeviceFactory::getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    executionEnvironment.rootDeviceEnvironments.resize(numRootDevices);

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

    numDevices = numRootDevices;
    DeviceFactory::numDevices = numDevices;
    auto csr = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csr > 0) {
        auto &hwHelper = HwHelper::get(hardwareInfo->platform.eRenderCoreFamily);
        auto localMemoryEnabled = hwHelper.getEnableLocalMemory(*hardwareInfo);
        executionEnvironment.initAubCenter(localMemoryEnabled, "", static_cast<CommandStreamReceiverType>(csr));
        auto aubCenter = executionEnvironment.rootDeviceEnvironments[0].aubCenter.get();
        executionEnvironment.memoryOperationsInterface = std::make_unique<AubMemoryOperationsHandler>(aubCenter->getAubManager());
    }
    return true;
}

} // namespace NEO
