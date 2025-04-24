/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/root_device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

RootDevice::RootDevice(ExecutionEnvironment *executionEnvironment, uint32_t rootDeviceIndex) : Device(executionEnvironment, rootDeviceIndex) {}

RootDevice::~RootDevice() {
    if (getDebugSurface()) {

        for (auto *subDevice : this->getSubDevices()) {
            if (subDevice) {
                subDevice->setDebugSurface(nullptr);
            }
        }

        getMemoryManager()->freeGraphicsMemory(debugSurface);
        debugSurface = nullptr;
    }
    if (getRootDeviceEnvironment().tagsManager) {
        getRootDeviceEnvironment().tagsManager->shutdown();
    }
}

Device *RootDevice::getRootDevice() const {
    return const_cast<RootDevice *>(this);
}

void RootDevice::createBindlessHeapsHelper() {

    EnvironmentVariableReader envReader;
    bool disableGlobalBindless = envReader.getSetting("NEO_L0_SYSMAN_NO_CONTEXT_MODE", false);

    if (!disableGlobalBindless && ApiSpecificConfig::getGlobalBindlessHeapConfiguration(this->getReleaseHelper()) && ApiSpecificConfig::getBindlessMode(*this)) {
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->createBindlessHeapsHelper(this, getNumGenericSubDevices() > 1);
    }
}

bool RootDevice::createEngines() {
    if (hasGenericSubDevices) {
        if (getRootDeviceEnvironment().isExposeSingleDeviceMode()) {
            return createSingleDeviceEngines();
        }
        initializeRootCommandStreamReceiver();
    } else {
        return Device::createEngines();
    }

    return true;
}

void RootDevice::initializeRootCommandStreamReceiver() {
    auto &hwInfo = getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(hwInfo);
    EngineTypeUsage engineTypeUsage = {defaultEngineType, EngineUsage::regular};

    createRootDeviceEngine(engineTypeUsage, getDeviceBitfield());
}

bool RootDevice::createRootDeviceEngine(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield) {
    std::unique_ptr<CommandStreamReceiver> rootCommandStreamReceiver(createCommandStream(*executionEnvironment, rootDeviceIndex, deviceBitfield));

    auto &hwInfo = getHardwareInfo();
    auto engineType = engineTypeUsage.first;
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    EngineDescriptor engineDescriptor(EngineTypeUsage{engineType, EngineUsage::regular}, deviceBitfield, preemptionMode, deviceBitfield.count() > 1);

    auto &gfxCoreHelper = getGfxCoreHelper();
    bool isPrimaryEngine = EngineHelpers::isCcs(engineType);
    DEBUG_BREAK_IF(EngineHelpers::isBcs(engineType));

    isPrimaryEngine &= engineTypeUsage.second == EngineUsage::regular;

    if (debugManager.flags.SecondaryContextEngineTypeMask.get() != -1) {
        isPrimaryEngine &= (static_cast<uint32_t>(debugManager.flags.SecondaryContextEngineTypeMask.get()) & (1 << static_cast<uint32_t>(engineType))) != 0;
    }
    bool useContextGroup = isPrimaryEngine && gfxCoreHelper.areSecondaryContextsSupported();

    if ((static_cast<uint32_t>(debugManager.flags.SecondaryContextEngineTypeMask.get()) & (1 << static_cast<uint32_t>(engineType))) == 0) {
        useContextGroup = false;
    }

    auto osContext = getMemoryManager()->createAndRegisterOsContext(rootCommandStreamReceiver.get(), engineDescriptor);

    osContext->setContextGroup(useContextGroup);
    osContext->setIsPrimaryEngine(isPrimaryEngine);

    rootCommandStreamReceiver->setupContext(*osContext);
    rootCommandStreamReceiver->initializeResources(false, preemptionMode);
    rootCommandStreamReceiver->initializeTagAllocation();
    rootCommandStreamReceiver->createGlobalFenceAllocation();
    if (EngineHelpers::isComputeEngine(engineType)) {
        rootCsrCreated = true;
        rootCommandStreamReceiver->createWorkPartitionAllocation(*this);
    }
    commandStreamReceivers.push_back(std::move(rootCommandStreamReceiver));

    EngineControl engine{commandStreamReceivers.back().get(), osContext};
    allEngines.push_back(engine);
    addEngineToEngineGroup(engine);

    if (useContextGroup) {
        bool hpEngineAvailable = engineTypeUsage.second == EngineUsage::highPriority;
        auto contextCount = gfxCoreHelper.getContextGroupContextsCount();

        EngineGroupType engineGroupType = gfxCoreHelper.getEngineGroupType(engine.getEngineType(), engine.getEngineUsage(), hwInfo);
        auto highPriorityContextCount = gfxCoreHelper.getContextGroupHpContextsCount(engineGroupType, hpEngineAvailable);

        if (debugManager.flags.OverrideNumHighPriorityContexts.get() != -1) {
            highPriorityContextCount = static_cast<uint32_t>(debugManager.flags.OverrideNumHighPriorityContexts.get());
        }

        if (getRootDeviceEnvironment().osInterface && getRootDeviceEnvironment().osInterface->getAggregatedProcessCount() > 1) {
            const auto numProcesses = getRootDeviceEnvironment().osInterface->getAggregatedProcessCount();

            contextCount = std::max(contextCount / numProcesses, 2u);
            highPriorityContextCount = std::max(contextCount / 2, 1u);
        }

        UNRECOVERABLE_IF(secondaryEngines.find(engineType) != secondaryEngines.end());
        auto &secondaryEnginesForType = secondaryEngines[engineType];

        createSecondaryContexts(engine, secondaryEnginesForType, contextCount, contextCount - highPriorityContextCount, highPriorityContextCount);
    }
    return true;
}

bool RootDevice::createSingleDeviceEngines() {

    auto &gfxCoreHelper = getGfxCoreHelper();
    auto &gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(getRootDeviceEnvironment());

    for (auto &engine : gpgpuEngines) {

        if (EngineHelpers::isComputeEngine(engine.first)) {
            if (!createRootDeviceEngine(engine, getDeviceBitfield())) {
                return false;
            }
        }
    }

    return true;
}

} // namespace NEO
