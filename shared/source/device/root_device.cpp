/*
 * Copyright (C) 2019-2022 Intel Corporation
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
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

RootDevice::RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) : Device(executionEnvironment, rootDeviceIndex) {}

RootDevice::~RootDevice() {
    if (getRootDeviceEnvironment().tagsManager) {
        getRootDeviceEnvironment().tagsManager->shutdown();
    }
}

Device *RootDevice::getRootDevice() const {
    return const_cast<RootDevice *>(this);
}

void RootDevice::createBindlessHeapsHelper() {
    if (ApiSpecificConfig::getBindlessConfiguration()) {
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->createBindlessHeapsHelper(getMemoryManager(), getNumGenericSubDevices() > 1, rootDeviceIndex, getDeviceBitfield());
    }
}

bool RootDevice::createEngines() {
    if (hasGenericSubDevices) {
        initializeRootCommandStreamReceiver();
    } else {
        return Device::createEngines();
    }

    return true;
}

void RootDevice::initializeRootCommandStreamReceiver() {
    rootCsrCreated = true;

    std::unique_ptr<CommandStreamReceiver> rootCommandStreamReceiver(createCommandStream(*executionEnvironment, rootDeviceIndex, getDeviceBitfield()));

    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    EngineDescriptor engineDescriptor(EngineTypeUsage{defaultEngineType, EngineUsage::Regular}, getDeviceBitfield(), preemptionMode, true, false);

    auto osContext = getMemoryManager()->createAndRegisterOsContext(rootCommandStreamReceiver.get(), engineDescriptor);

    osContext->ensureContextInitialized();
    rootCommandStreamReceiver->setupContext(*osContext);
    rootCommandStreamReceiver->initializeTagAllocation();
    rootCommandStreamReceiver->createGlobalFenceAllocation();
    rootCommandStreamReceiver->createWorkPartitionAllocation(*this);
    commandStreamReceivers.push_back(std::move(rootCommandStreamReceiver));

    EngineControl engine{commandStreamReceivers.back().get(), osContext};
    allEngines.push_back(engine);
    addEngineToEngineGroup(engine);
}

} // namespace NEO
