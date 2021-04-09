/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/root_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

RootDevice::RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) : Device(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {}

RootDevice::~RootDevice() {
    if (getRootDeviceEnvironment().tagsManager) {
        getRootDeviceEnvironment().tagsManager->shutdown();
    }
}

uint32_t RootDevice::getRootDeviceIndex() const {
    return rootDeviceIndex;
}

Device *RootDevice::getRootDevice() const {
    return const_cast<RootDevice *>(this);
}

SubDevice *RootDevice::createSubDevice(uint32_t subDeviceIndex) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *this);
}

bool RootDevice::createDeviceImpl() {
    auto deviceMask = executionEnvironment->rootDeviceEnvironments[this->rootDeviceIndex]->deviceAffinityMask;
    uint32_t subDeviceCount = HwHelper::getSubDevicesCount(&getHardwareInfo());
    deviceBitfield = maxNBitValue(subDeviceCount);
    deviceBitfield &= deviceMask;
    numSubDevices = static_cast<uint32_t>(deviceBitfield.count());
    if (numSubDevices == 1) {
        numSubDevices = 0;
    }
    UNRECOVERABLE_IF(!subdevices.empty());
    if (numSubDevices) {
        subdevices.resize(subDeviceCount, nullptr);
        for (auto i = 0u; i < subDeviceCount; i++) {
            if (!deviceBitfield.test(i)) {
                continue;
            }
            auto subDevice = createSubDevice(i);
            if (!subDevice) {
                return false;
            }
            subdevices[i] = subDevice;
        }
    }
    auto status = Device::createDeviceImpl();
    if (!status) {
        return status;
    }
    if (ApiSpecificConfig::getBindlessConfiguration()) {
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->createBindlessHeapsHelper(getMemoryManager(), getNumAvailableDevices() > 1, rootDeviceIndex);
    }

    return true;
}

bool RootDevice::createEngines() {
    if (getNumSubDevices() < 2) {
        return Device::createEngines();
    } else {
        this->engineGroups.resize(static_cast<uint32_t>(EngineGroupType::MaxEngineGroups));
        initializeRootCommandStreamReceiver();
    }
    return true;
}

void RootDevice::initializeRootCommandStreamReceiver() {
    std::unique_ptr<CommandStreamReceiver> rootCommandStreamReceiver(createCommandStream(*executionEnvironment, rootDeviceIndex, getDeviceBitfield()));

    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    auto osContext = getMemoryManager()->createAndRegisterOsContext(rootCommandStreamReceiver.get(),
                                                                    EngineTypeUsage{defaultEngineType, EngineUsage::Regular},
                                                                    getDeviceBitfield(),
                                                                    preemptionMode,
                                                                    true);

    rootCommandStreamReceiver->setupContext(*osContext);
    rootCommandStreamReceiver->initializeTagAllocation();
    rootCommandStreamReceiver->createGlobalFenceAllocation();
    rootCommandStreamReceiver->createWorkPartitionAllocation(*this);
    commandStreamReceivers.push_back(std::move(rootCommandStreamReceiver));

    EngineControl engine{commandStreamReceivers.back().get(), osContext};
    engines.push_back(engine);
    addEngineToEngineGroup(engine);
}

} // namespace NEO
