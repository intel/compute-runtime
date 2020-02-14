/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/device_factory.h"

#include "core/debug_settings/debug_settings_manager.h"
#include "core/device/device.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_helper.h"
#include "core/os_interface/aub_memory_operations_handler.h"
#include "core/os_interface/hw_info_config.h"
#include "core/os_interface/os_interface.h"
#include "runtime/aub/aub_center.h"

#include "hw_device_id.h"

namespace NEO {

bool DeviceFactory::getDevicesForProductFamilyOverride(size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);

    auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
    const HardwareInfo *hwInfoConst = &DEFAULT_PLATFORM::hwInfo;
    getHwInfoForPlatformString(productFamily, hwInfoConst);
    numDevices = 0;
    std::string hwInfoConfigStr;
    uint64_t hwInfoConfig = 0x0;
    DebugManager.getHardwareInfoOverride(hwInfoConfigStr);

    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    *hardwareInfo = *hwInfoConst;

    if (hwInfoConfigStr == "default") {
        hwInfoConfig = defaultHardwareInfoConfigTable[hwInfoConst->platform.eProductFamily];
    } else if (!parseHwInfoConfigString(hwInfoConfigStr, hwInfoConfig)) {
        return false;
    }
    setHwInfoValuesFromConfig(hwInfoConfig, *hardwareInfo);

    hardwareInfoSetup[hwInfoConst->platform.eProductFamily](hardwareInfo, true, hwInfoConfig);

    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    hardwareInfo->featureTable.ftrE2ECompression = 0;
    hwConfig->configureHardwareCustom(hardwareInfo, nullptr);

    executionEnvironment.calculateMaxOsContextCount();
    numDevices = numRootDevices;
    auto csrType = DebugManager.flags.SetCommandStreamReceiver.get();
    if (csrType > 0) {
        auto &hwHelper = HwHelper::get(hardwareInfo->platform.eRenderCoreFamily);
        auto localMemoryEnabled = hwHelper.getEnableLocalMemory(*hardwareInfo);
        for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initAubCenter(localMemoryEnabled, "", static_cast<CommandStreamReceiverType>(csrType));
            auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<AubMemoryOperationsHandler>(aubCenter->getAubManager());
        }
    }

    if (DebugManager.flags.OverrideGpuAddressSpace.get() != -1) {
        executionEnvironment.getMutableHardwareInfo()->capabilityTable.gpuAddressSpace =
            maxNBitValue(static_cast<uint64_t>(DebugManager.flags.OverrideGpuAddressSpace.get()));
    }

    executionEnvironment.initializeMemoryManager();
    return true;
}

bool DeviceFactory::isHwModeSelected() {
    int32_t csr = DebugManager.flags.SetCommandStreamReceiver.get();
    switch (csr) {
    case CSR_AUB:
    case CSR_TBX:
    case CSR_TBX_WITH_AUB:
        return false;
    default:
        return true;
    }
}

bool DeviceFactory::getDevices(size_t &totalNumRootDevices, ExecutionEnvironment &executionEnvironment) {
    using HwDeviceIds = std::vector<std::unique_ptr<HwDeviceId>>;

    HwDeviceIds hwDeviceIds;

    size_t numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    for (size_t i = 0; i < numRootDevices; i++) {
        auto hwDeviceId = OSInterface::discoverDevices();
        if (hwDeviceId) {
            hwDeviceIds.push_back(std::move(hwDeviceId));
        }
    }
    if (hwDeviceIds.empty()) {
        return false;
    }
    totalNumRootDevices = numRootDevices;

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(numRootDevices));

    uint32_t rootDeviceIndex = 0u;

    for (auto &hwDeviceId : hwDeviceIds) {
        if (!executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initOsInterface(std::move(hwDeviceId))) {
            return false;
        }

        rootDeviceIndex++;
    }

    executionEnvironment.calculateMaxOsContextCount();

    if (DebugManager.flags.OverrideGpuAddressSpace.get() != -1) {
        executionEnvironment.getMutableHardwareInfo()->capabilityTable.gpuAddressSpace =
            maxNBitValue(static_cast<uint64_t>(DebugManager.flags.OverrideGpuAddressSpace.get()));
    }

    executionEnvironment.initializeMemoryManager();
    return true;
}

} // namespace NEO
