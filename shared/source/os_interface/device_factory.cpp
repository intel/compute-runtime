/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device/root_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"

#include "hw_device_id.h"

namespace NEO {

bool DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(ExecutionEnvironment &executionEnvironment) {
    auto numRootDevices = 1u;
    if (DebugManager.flags.CreateMultipleRootDevices.get()) {
        numRootDevices = DebugManager.flags.CreateMultipleRootDevices.get();
    }
    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);

    auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
    const HardwareInfo *hwInfoConst = &DEFAULT_PLATFORM::hwInfo;
    getHwInfoForPlatformString(productFamily, hwInfoConst);
    std::string hwInfoConfigStr;
    uint64_t hwInfoConfig = 0x0;
    DebugManager.getHardwareInfoOverride(hwInfoConfigStr);

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo();
        *hardwareInfo = *hwInfoConst;

        if (hwInfoConfigStr == "default") {
            hwInfoConfig = defaultHardwareInfoConfigTable[hwInfoConst->platform.eProductFamily];
        } else if (!parseHwInfoConfigString(hwInfoConfigStr, hwInfoConfig)) {
            return false;
        }
        setHwInfoValuesFromConfig(hwInfoConfig, *hardwareInfo);

        hardwareInfoSetup[hwInfoConst->platform.eProductFamily](hardwareInfo, true, hwInfoConfig);

        HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
        hwConfig->configureHardwareCustom(hardwareInfo, nullptr);

        if (DebugManager.flags.OverrideGpuAddressSpace.get() != -1) {
            hardwareInfo->capabilityTable.gpuAddressSpace = maxNBitValue(static_cast<uint64_t>(DebugManager.flags.OverrideGpuAddressSpace.get()));
        }

        if (DebugManager.flags.OverrideRevision.get() != -1) {
            hardwareInfo->platform.usRevId = static_cast<unsigned short>(DebugManager.flags.OverrideRevision.get());
        }

        if (DebugManager.flags.ForceDeviceId.get() != "unk") {
            hardwareInfo->platform.usDeviceID = static_cast<unsigned short>(std::stoi(DebugManager.flags.ForceDeviceId.get(), nullptr, 16));
        }

        [[maybe_unused]] bool result = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initAilConfiguration();
        DEBUG_BREAK_IF(!result);

        auto csrType = DebugManager.flags.SetCommandStreamReceiver.get();
        if (csrType > 0) {
            auto &hwHelper = HwHelper::get(hardwareInfo->platform.eRenderCoreFamily);
            auto localMemoryEnabled = hwHelper.getEnableLocalMemory(*hardwareInfo);
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initGmm();
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initAubCenter(localMemoryEnabled, "", static_cast<CommandStreamReceiverType>(csrType));
            auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = std::make_unique<AubMemoryOperationsHandler>(aubCenter->getAubManager());
        }
    }

    executionEnvironment.parseAffinityMask();
    executionEnvironment.calculateMaxOsContextCount();
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

bool DeviceFactory::prepareDeviceEnvironments(ExecutionEnvironment &executionEnvironment) {
    using HwDeviceIds = std::vector<std::unique_ptr<HwDeviceId>>;

    HwDeviceIds hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    if (hwDeviceIds.empty()) {
        return false;
    }

    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(hwDeviceIds.size()));

    uint32_t rootDeviceIndex = 0u;

    for (auto &hwDeviceId : hwDeviceIds) {
        if (!executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initOsInterface(std::move(hwDeviceId), rootDeviceIndex)) {
            return false;
        }

        if (DebugManager.flags.OverrideGpuAddressSpace.get() != -1) {
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->capabilityTable.gpuAddressSpace =
                maxNBitValue(static_cast<uint64_t>(DebugManager.flags.OverrideGpuAddressSpace.get()));
        }

        if (DebugManager.flags.OverrideRevision.get() != -1) {
            executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->platform.usRevId =
                static_cast<unsigned short>(DebugManager.flags.OverrideRevision.get());
        }

        rootDeviceIndex++;
    }

    executionEnvironment.sortNeoDevices();
    executionEnvironment.parseAffinityMask();
    executionEnvironment.calculateMaxOsContextCount();

    return true;
}

std::vector<std::unique_ptr<Device>> DeviceFactory::createDevices(ExecutionEnvironment &executionEnvironment) {
    std::vector<std::unique_ptr<Device>> devices;

    if (!NEO::prepareDeviceEnvironments(executionEnvironment)) {
        return devices;
    }

    if (!DeviceFactory::createMemoryManagerFunc(executionEnvironment)) {
        return devices;
    }

    auto discreteDeviceIndex = 0u;
    for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        auto device = createRootDeviceFunc(executionEnvironment, rootDeviceIndex);
        if (device) {
            if (device->getHardwareInfo().capabilityTable.isIntegratedDevice == false) {
                // If we are here, it means we are processing entry for discrete device.
                // And lets first insert discrete device's entry in devices vector.
                devices.insert(devices.begin() + discreteDeviceIndex, std::move(device));
                discreteDeviceIndex++;
                continue;
            }
            // Ensure to push integrated device's entry at the end of devices vector
            devices.push_back(std::move(device));
        }
    }

    return devices;
}

std::unique_ptr<Device> (*DeviceFactory::createRootDeviceFunc)(ExecutionEnvironment &, uint32_t) = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
    return std::unique_ptr<Device>(Device::create<RootDevice>(&executionEnvironment, rootDeviceIndex));
};

bool (*DeviceFactory::createMemoryManagerFunc)(ExecutionEnvironment &) = [](ExecutionEnvironment &executionEnvironment) -> bool {
    return executionEnvironment.initializeMemoryManager();
};

} // namespace NEO
