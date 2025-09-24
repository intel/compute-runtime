/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/sip_external_lib/sip_external_lib.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

Device::Device(ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex)
    : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex), isaPoolAllocator(this), deviceTimestampPoolAllocator(this) {
    this->executionEnvironment->incRefInternal();
    this->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setDummyBlitProperties(rootDeviceIndex);
    if (auto ailHelper = this->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getAILConfigurationHelper(); ailHelper && ailHelper->isAdjustMicrosecondResolutionRequired()) {
        microsecondResolution = ailHelper->getMicrosecondResolution();
    }
}

Device::~Device() {
    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());

    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : allEngines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }
    allEngines.clear();
    finalizeRayTracing();

    for (auto subdevice : subdevices) {
        if (subdevice) {
            delete subdevice;
        }
    }
    subdevices.clear();

    syncBufferHandler.reset();
    isaPoolAllocator.releasePools();
    deviceTimestampPoolAllocator.releasePools();
    if (deviceUsmMemAllocPoolsManager) {
        deviceUsmMemAllocPoolsManager->cleanup();
    }
    if (usmConstantSurfaceAllocPool) {
        usmConstantSurfaceAllocPool->cleanup();
    }
    if (usmGlobalSurfaceAllocPool) {
        usmGlobalSurfaceAllocPool->cleanup();
    }

    secondaryCsrs.clear();
    executionEnvironment->memoryManager->releaseSecondaryOsContexts(this->getRootDeviceIndex());
    commandStreamReceivers.clear();
    executionEnvironment->memoryManager->waitForDeletions();

    executionEnvironment->decRefInternal();
}

SubDevice *Device::createSubDevice(uint32_t subDeviceIndex) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *getRootDevice());
}

bool Device::genericSubDevicesAllowed() {
    auto deviceMask = executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->deviceAffinityMask.getGenericSubDevicesMask();
    uint32_t subDeviceCount = GfxCoreHelper::getSubDevicesCount(&getHardwareInfo());
    deviceBitfield = maxNBitValue(subDeviceCount);

    if (!executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->isExposeSingleDeviceMode()) {
        deviceBitfield &= deviceMask;
    }

    numSubDevices = static_cast<uint32_t>(deviceBitfield.count());
    if (numSubDevices == 1 && (executionEnvironment->getDeviceHierarchyMode() != DeviceHierarchyMode::combined || subDeviceCount == 1)) {
        numSubDevices = 0;
    }

    return (numSubDevices > 0);
}

bool Device::createGenericSubDevices() {
    UNRECOVERABLE_IF(!subdevices.empty());
    uint32_t subDeviceCount = GfxCoreHelper::getSubDevicesCount(&getHardwareInfo());

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

    hasGenericSubDevices = true;
    return true;
}

bool Device::createSubDevices() {
    if (genericSubDevicesAllowed()) {
        return createGenericSubDevices();
    }

    return true;
}

bool Device::createDeviceImpl() {
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(getHardwareInfo());

    if (!isSubDevice()) {
        // init sub devices first
        if (!createSubDevices()) {
            return false;
        }

        // initialize common resources once
        if (!initializeCommonResources()) {
            return false;
        }
    }

    // create engines
    if (!initDeviceWithEngines()) {
        return false;
    }

    // go back to root-device init
    if (isSubDevice()) {
        return true;
    }

    if (getL0Debugger()) {
        getL0Debugger()->initialize();
    }

    // continue proper init for all devices
    return initDeviceFully();
}

bool Device::initDeviceWithEngines() {
    auto &productHelper = getProductHelper();
    if (getDebugger() && productHelper.disableL3CacheForDebug(getHardwareInfo())) {
        getGmmHelper()->forceAllResourcesUncached();
    }

    getRootDeviceEnvironmentRef().initOsTime();

    initializeCaps();

    return createEngines();
}

bool Device::initializeCommonResources() {
    if (getExecutionEnvironment()->isDebuggingEnabled()) {
        const auto rootDeviceIndex = getRootDeviceIndex();
        auto rootDeviceEnvironment = getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex].get();
        rootDeviceEnvironment->initDebuggerL0(this);
        if (rootDeviceEnvironment->debugger == nullptr) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Debug mode is not enabled in the system.\n");
            return false;
        }
    }

    if (this->isStateSipRequired()) {
        bool ret = SipKernel::initSipKernel(SipKernel::getSipKernelType(*this), *this);
        UNRECOVERABLE_IF(!ret);
        const bool isDebugSurfaceRequired = getL0Debugger();
        if (isDebugSurfaceRequired) {
            auto debugSurfaceSize = NEO::SipKernel::getSipKernel(*this, nullptr).getStateSaveAreaSize(this);
            if (debugSurfaceSize) {
                allocateDebugSurface(debugSurfaceSize);
            } else {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Unable to determine debug surface size.\n");
                return false;
            }
        }
    }

    this->resetUsmConstantSurfaceAllocPool(new UsmMemAllocPool);
    this->resetUsmGlobalSurfaceAllocPool(new UsmMemAllocPool);

    return true;
}

void Device::initUsmReuseLimits() {
    const bool usmDeviceAllocationsCacheEnabled = NEO::ApiSpecificConfig::isDeviceAllocationCacheEnabled() && this->getProductHelper().isDeviceUsmAllocationReuseSupported();
    auto ailConfiguration = this->getAilConfigurationHelper();
    const bool limitDeviceMemoryForReuse = ailConfiguration && ailConfiguration->limitAmountOfDeviceMemoryForRecycling();
    auto fractionOfTotalMemoryForRecycling = (limitDeviceMemoryForReuse || !usmDeviceAllocationsCacheEnabled) ? 0 : 0.08;
    if (debugManager.flags.ExperimentalEnableDeviceAllocationCache.get() != -1) {
        fractionOfTotalMemoryForRecycling = 0.01 * std::min(100, debugManager.flags.ExperimentalEnableDeviceAllocationCache.get());
    }
    const auto totalDeviceMemory = this->getGlobalMemorySize(static_cast<uint32_t>(this->getDeviceBitfield().to_ulong()));
    auto maxAllocationsSavedForReuseSize = static_cast<uint64_t>(fractionOfTotalMemoryForRecycling * totalDeviceMemory);

    auto limitAllocationsReuseThreshold = static_cast<uint64_t>(0.8 * totalDeviceMemory);
    const auto limitFlagValue = debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.get();
    if (limitFlagValue != -1) {
        if (limitFlagValue == 0) {
            limitAllocationsReuseThreshold = UsmReuseInfo::notLimited;
        } else {
            const auto fractionOfTotalMemoryToLimitReuse = limitFlagValue / 100.0;
            limitAllocationsReuseThreshold = static_cast<uint64_t>(fractionOfTotalMemoryToLimitReuse * totalDeviceMemory);
        }
    }
    this->usmReuseInfo.init(maxAllocationsSavedForReuseSize, limitAllocationsReuseThreshold);
}

bool Device::shouldLimitAllocationsReuse() const {
    const bool isIntegratedDevice = getHardwareInfo().capabilityTable.isIntegratedDevice;
    if (isIntegratedDevice) {
        return getMemoryManager()->shouldLimitAllocationsReuse();
    }
    return getMemoryManager()->getUsedLocalMemorySize(getRootDeviceIndex()) >= this->usmReuseInfo.getLimitAllocationsReuseThreshold();
}

void Device::resetUsmAllocationPool(UsmMemAllocPool *usmMemAllocPool) {
    this->usmMemAllocPool.reset(usmMemAllocPool);
}

void Device::resetUsmAllocationPoolManager(UsmMemAllocPoolsManager *usmMemAllocPoolManager) {
    this->deviceUsmMemAllocPoolsManager.reset(usmMemAllocPoolManager);
}

void Device::cleanupUsmAllocationPool() {
    if (usmMemAllocPool) {
        usmMemAllocPool->cleanup();
    }
    if (deviceUsmMemAllocPoolsManager) {
        deviceUsmMemAllocPoolsManager->cleanup();
    }
}

void Device::resetUsmConstantSurfaceAllocPool(UsmMemAllocPool *usmMemAllocPool) {
    this->usmConstantSurfaceAllocPool.reset(usmMemAllocPool);
}

void Device::resetUsmGlobalSurfaceAllocPool(UsmMemAllocPool *usmMemAllocPool) {
    this->usmGlobalSurfaceAllocPool.reset(usmMemAllocPool);
}

bool Device::initDeviceFully() {

    if (!getRootDeviceEnvironment().isExposeSingleDeviceMode()) {
        for (auto &subdevice : this->subdevices) {
            if (subdevice && !subdevice->initDeviceFully()) {
                return false;
            }
        }
    }

    if (!initializeEngines()) {
        return false;
    }

    getDefaultEngine().osContext->setDefaultContext(true);

    for (auto &engine : allEngines) {
        auto commandStreamReceiver = engine.commandStreamReceiver;
        commandStreamReceiver->postInitFlagsSetup();
    }

    auto &registeredEngines = executionEnvironment->memoryManager->getRegisteredEngines(rootDeviceIndex);
    uint32_t defaultEngineIndexWithinMemoryManager = 0;
    for (auto engineIndex = 0u; engineIndex < registeredEngines.size(); engineIndex++) {
        OsContext *engine = registeredEngines[engineIndex].osContext;
        if (engine == getDefaultEngine().osContext) {
            defaultEngineIndexWithinMemoryManager = engineIndex;
            break;
        }
    }
    executionEnvironment->memoryManager->setDefaultEngineIndex(getRootDeviceIndex(), defaultEngineIndexWithinMemoryManager);

    auto &hwInfo = getHardwareInfo();
    if (getRootDeviceEnvironment().osInterface) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    executionEnvironment->memoryManager->setForce32BitAllocations(getDeviceInfo().force32BitAddresses);

    if (debugManager.flags.EnableSWTags.get() && !getRootDeviceEnvironment().tagsManager->isInitialized()) {
        getRootDeviceEnvironment().tagsManager->initialize(*this);
    }

    createBindlessHeapsHelper();
    uuid.isValid = false;
    initUsmReuseLimits();

    if (getRootDeviceEnvironment().osInterface == nullptr) {
        return true;
    }

    auto &gfxCoreHelper = getGfxCoreHelper();
    auto &productHelper = getProductHelper();
    if (debugManager.flags.EnableChipsetUniqueUUID.get() != 0) {
        if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {

            auto deviceIndex = isSubDevice() ? static_cast<SubDevice *>(this)->getSubDeviceIndex() + 1 : 0;
            uuid.isValid = productHelper.getUuid(getRootDeviceEnvironment().osInterface->getDriverModel(), getRootDevice()->getNumSubDevices(), deviceIndex, uuid.id);
        }
    }

    if (!uuid.isValid) {
        PhysicalDevicePciBusInfo pciBusInfo = getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
        uuid.isValid = generateUuidFromPciBusInfo(pciBusInfo, uuid.id);
    }

    return true;
}

bool Device::createEngines() {
    auto &gfxCoreHelper = getGfxCoreHelper();
    auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(getRootDeviceEnvironment());

    for (auto &engine : gpgpuEngines) {

        if (isSubDevice() && getRootDeviceEnvironment().isExposeSingleDeviceMode() && EngineHelpers::isComputeEngine(engine.first)) {
            continue;
        }

        if (!createEngine(engine)) {
            return false;
        }
    }

    if (gfxCoreHelper.areSecondaryContextsSupported()) {
        auto &hwInfo = this->getHardwareInfo();

        auto hpCopyEngine = getHpCopyEngine();

        for (auto engineGroupType : {EngineGroupType::compute, EngineGroupType::copy, EngineGroupType::linkedCopy}) {
            auto engineGroup = tryGetRegularEngineGroup(engineGroupType);

            if (!engineGroup) {
                continue;
            }

            auto contextCount = gfxCoreHelper.getContextGroupContextsCount();
            bool hpEngineAvailable = false;

            if (NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType)) {
                hpEngineAvailable = hpCopyEngine != nullptr;
            }

            auto highPriorityContextCount = gfxCoreHelper.getContextGroupHpContextsCount(engineGroupType, hpEngineAvailable);

            if (debugManager.flags.OverrideNumHighPriorityContexts.get() != -1) {
                highPriorityContextCount = static_cast<uint32_t>(debugManager.flags.OverrideNumHighPriorityContexts.get());
            }

            if (getRootDeviceEnvironment().osInterface && getRootDeviceEnvironment().osInterface->getAggregatedProcessCount() > 1) {
                const auto numProcesses = getRootDeviceEnvironment().osInterface->getAggregatedProcessCount();

                contextCount = std::max(contextCount / numProcesses, 2u);
                highPriorityContextCount = std::max(contextCount / 2, 1u);

            } else {
                if (engineGroupType == EngineGroupType::compute && hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1) {
                    contextCount = contextCount / hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
                    highPriorityContextCount = highPriorityContextCount / hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
                }

                if (engineGroupType == EngineGroupType::copy || engineGroupType == EngineGroupType::linkedCopy) {
                    gfxCoreHelper.adjustCopyEngineRegularContextCount(engineGroup->engines.size(), contextCount);
                }
            }
            for (uint32_t engineIndex = 0; engineIndex < static_cast<uint32_t>(engineGroup->engines.size()); engineIndex++) {
                auto engineType = engineGroup->engines[engineIndex].getEngineType();

                if ((static_cast<uint32_t>(debugManager.flags.SecondaryContextEngineTypeMask.get()) & (1 << static_cast<uint32_t>(engineType))) == 0) {
                    continue;
                }

                UNRECOVERABLE_IF(secondaryEngines.find(engineType) != secondaryEngines.end());
                auto &secondaryEnginesForType = secondaryEngines[engineType];

                auto primaryEngine = engineGroup->engines[engineIndex];

                createSecondaryContexts(primaryEngine, secondaryEnginesForType, contextCount, contextCount - highPriorityContextCount, highPriorityContextCount);
            }
        }

        if (hpCopyEngine) {
            auto engineType = hpCopyEngine->getEngineType();
            if ((static_cast<uint32_t>(debugManager.flags.SecondaryContextEngineTypeMask.get()) & (1 << static_cast<uint32_t>(engineType))) != 0) {

                UNRECOVERABLE_IF(secondaryEngines.find(engineType) != secondaryEngines.end());
                auto &secondaryEnginesForType = secondaryEngines[engineType];

                auto primaryEngine = *hpCopyEngine;

                auto contextCount = gfxCoreHelper.getContextGroupContextsCount();

                if (getRootDeviceEnvironment().osInterface && getRootDeviceEnvironment().osInterface->getAggregatedProcessCount() > 1) {
                    const auto numProcesses = getRootDeviceEnvironment().osInterface->getAggregatedProcessCount();

                    contextCount = std::max(contextCount / numProcesses, 2u);
                }

                createSecondaryContexts(primaryEngine, secondaryEnginesForType, contextCount, 0, contextCount);
            }
        }
    }

    return true;
}

void Device::createSecondaryContexts(const EngineControl &primaryEngine, SecondaryContexts &secondaryEnginesForType, uint32_t contextCount, uint32_t regularPriorityCount, uint32_t highPriorityContextCount) {
    secondaryEnginesForType.regularEnginesTotal = contextCount - highPriorityContextCount;
    secondaryEnginesForType.highPriorityEnginesTotal = highPriorityContextCount;
    secondaryEnginesForType.regularCounter = 0;
    secondaryEnginesForType.highPriorityCounter = 0;
    secondaryEnginesForType.assignedContextsCounter = 1;

    NEO::EngineTypeUsage engineTypeUsage;
    engineTypeUsage.first = primaryEngine.getEngineType();
    engineTypeUsage.second = primaryEngine.getEngineUsage();

    UNRECOVERABLE_IF(engineTypeUsage.second != EngineUsage::regular && engineTypeUsage.second != EngineUsage::highPriority);

    secondaryEnginesForType.engines.push_back(primaryEngine);

    for (uint32_t i = 1; i < contextCount; i++) {

        if (i >= contextCount - highPriorityContextCount) {
            engineTypeUsage.second = EngineUsage::highPriority;
        }
        this->createSecondaryEngine(primaryEngine.commandStreamReceiver, engineTypeUsage);
    }

    UNRECOVERABLE_IF(primaryEngine.osContext->isPartOfContextGroup() == false);
}

void Device::allocateDebugSurface(size_t debugSurfaceSize) {
    this->debugSurface = getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {getRootDeviceIndex(), true,
         debugSurfaceSize,
         NEO::AllocationType::debugContextSaveArea,
         false,
         false,
         getDeviceBitfield()});

    for (auto &subdevice : this->subdevices) {
        if (subdevice) {
            subdevice->debugSurface = this->debugSurface;
        }
    }
}

void Device::addEngineToEngineGroup(EngineControl &engine) {
    auto &hardwareInfo = this->getHardwareInfo();
    auto &gfxCoreHelper = getGfxCoreHelper();
    auto &productHelper = getProductHelper();
    auto &rootDeviceEnvironment = this->getRootDeviceEnvironment();

    EngineGroupType engineGroupType = gfxCoreHelper.getEngineGroupType(engine.getEngineType(), engine.getEngineUsage(), hardwareInfo);
    productHelper.adjustEngineGroupType(engineGroupType);

    if (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, getDeviceBitfield(), engine.getEngineType())) {
        return;
    }

    if (EngineHelper::isCopyOnlyEngineType(engineGroupType) && debugManager.flags.EnableBlitterOperationsSupport.get() == 0) {
        return;
    }

    if (this->regularEngineGroups.empty() || this->regularEngineGroups.back().engineGroupType != engineGroupType) {
        this->regularEngineGroups.push_back(EngineGroupT{});
        this->regularEngineGroups.back().engineGroupType = engineGroupType;
    }

    auto &engines = this->regularEngineGroups.back().engines;

    if (engines.size() > 0 && engines.back().getEngineType() == engine.getEngineType()) {
        return; // Type already added. Exposing multiple contexts for the same engine is disabled.
    }

    engines.push_back(engine);
}

std::unique_ptr<CommandStreamReceiver> Device::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

bool Device::createEngine(EngineTypeUsage engineTypeUsage) {
    const auto &hwInfo = getHardwareInfo();
    auto &gfxCoreHelper = getGfxCoreHelper();
    const auto engineType = engineTypeUsage.first;
    const auto engineUsage = engineTypeUsage.second;
    const auto defaultEngineType = getChosenEngineType(hwInfo);
    const bool isDefaultEngine = defaultEngineType == engineType && engineUsage == EngineUsage::regular;

    bool primaryEngineTypeAllowed = (EngineHelpers::isCcs(engineType) || EngineHelpers::isBcs(engineType));

    if (debugManager.flags.SecondaryContextEngineTypeMask.get() != -1) {
        primaryEngineTypeAllowed &= (static_cast<uint32_t>(debugManager.flags.SecondaryContextEngineTypeMask.get()) & (1 << static_cast<uint32_t>(engineType))) != 0;
    }

    const bool isPrimaryEngine = primaryEngineTypeAllowed && (engineUsage == EngineUsage::regular || engineUsage == EngineUsage::highPriority);
    const bool useContextGroup = isPrimaryEngine && gfxCoreHelper.areSecondaryContextsSupported();

    UNRECOVERABLE_IF(EngineHelpers::isBcs(engineType) && !hwInfo.capabilityTable.blitterOperationsSupported);

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    if (commandStreamReceiver->needsPageTableManager()) {
        commandStreamReceiver->createPageTableManager();
    }

    EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineDescriptor);
    osContext->setContextGroupCount(useContextGroup ? gfxCoreHelper.getContextGroupContextsCount() : 0);
    osContext->setIsPrimaryEngine(isPrimaryEngine);
    osContext->setIsDefaultEngine(isDefaultEngine);

    DEBUG_BREAK_IF(getDeviceBitfield().count() > 1 && !osContext->isRootDevice());

    commandStreamReceiver->setupContext(*osContext);

    if (osContext->isImmediateContextInitializationEnabled(isDefaultEngine)) {
        if (!commandStreamReceiver->initializeResources(false, this->getPreemptionMode())) {
            return false;
        }
    }

    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }

    if (!commandStreamReceiver->createGlobalFenceAllocation()) {
        return false;
    }

    EngineControl engine{commandStreamReceiver.get(), osContext};
    allEngines.push_back(engine);
    if (engineUsage == EngineUsage::regular) {
        addEngineToEngineGroup(engine);
    }

    if (NEO::EngineHelpers::isBcs(engine.osContext->getEngineType()) && engine.osContext->isHighPriority()) {
        hpCopyEngine = &allEngines[allEngines.size() - 1];
    }

    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

bool Device::initializeEngines() {
    uint32_t deviceCsrIndex = 0;
    bool defaultEngineAlreadySet = false;
    for (auto &engine : allEngines) {
        bool firstSubmissionDone = false;
        if (engine.osContext->getIsDefaultEngine() && !defaultEngineAlreadySet) {
            defaultEngineAlreadySet = true;
            defaultEngineIndex = deviceCsrIndex;

            if (engine.osContext->isDebuggableContext() ||
                this->isInitDeviceWithFirstSubmissionSupported(engine.commandStreamReceiver->getType())) {
                if (SubmissionStatus::success != engine.commandStreamReceiver->initializeDeviceWithFirstSubmission(*this)) {
                    return false;
                }
                firstSubmissionDone = true;
            }
        }

        auto &compilerProductHelper = this->getCompilerProductHelper();
        auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(this->getHardwareInfo());

        bool isHeaplessStateInit = engine.osContext->getIsPrimaryEngine() && compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);
        bool initializeDevice = (engine.osContext->isPartOfContextGroup() || isHeaplessStateInit) && !firstSubmissionDone;

        if (initializeDevice) {
            engine.commandStreamReceiver->initializeResources(false, this->getPreemptionMode());

            if (debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.get() != 1) {
                engine.commandStreamReceiver->initializeDeviceWithFirstSubmission(*this);
            }
        }
        deviceCsrIndex++;
    }
    return true;
}

bool Device::createSecondaryEngine(CommandStreamReceiver *primaryCsr, EngineTypeUsage engineTypeUsage) {
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    EngineDescriptor engineDescriptor(engineTypeUsage, primaryCsr->peekDeviceBitfield(), preemptionMode, primaryCsr->getOsContext().isRootDevice());

    auto osContext = executionEnvironment->memoryManager->createAndRegisterSecondaryOsContext(&primaryCsr->getOsContext(), commandStreamReceiver.get(), engineDescriptor);
    osContext->incRefInternal();
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->setPrimaryCsr(primaryCsr);

    DEBUG_BREAK_IF(osContext->getDeviceBitfield().count() > 1 && !osContext->isRootDevice());

    EngineControl engine{commandStreamReceiver.get(), osContext};

    secondaryEngines[engineTypeUsage.first].engines.push_back(engine);
    secondaryCsrs.push_back(std::move(commandStreamReceiver));

    return true;
}

EngineControl *Device::getSecondaryEngineCsr(EngineTypeUsage engineTypeUsage, std::optional<int> priorityLevel, bool allocateInterrupt) {
    if (secondaryEngines.find(engineTypeUsage.first) == secondaryEngines.end()) {
        return nullptr;
    }

    auto &secondaryEnginesForType = secondaryEngines[engineTypeUsage.first];

    auto engineControl = secondaryEnginesForType.getEngine(engineTypeUsage.second, priorityLevel);

    bool isPrimaryContextInGroup = engineControl->osContext->getIsPrimaryEngine() && engineControl->osContext->isPartOfContextGroup();

    if (isPrimaryContextInGroup && allocateInterrupt) {
        // Context 0 is already pre-initialized. We need non-initialized context, to pass context creation flag.
        // If all contexts are already initialized, just take next available. Interrupt request is only a hint.
        engineControl = secondaryEnginesForType.getEngine(engineTypeUsage.second, priorityLevel);
    }

    isPrimaryContextInGroup = engineControl->osContext->getIsPrimaryEngine() && engineControl->osContext->isPartOfContextGroup();

    if (!isPrimaryContextInGroup) {
        auto commandStreamReceiver = engineControl->commandStreamReceiver;

        auto lock = commandStreamReceiver->obtainUniqueOwnership();

        if (!commandStreamReceiver->isInitialized()) {

            if (commandStreamReceiver->needsPageTableManager()) {
                commandStreamReceiver->createPageTableManager();
            }

            EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false);

            if (!commandStreamReceiver->initializeResources(allocateInterrupt, this->getPreemptionMode())) {
                return nullptr;
            }

            if (!commandStreamReceiver->initializeTagAllocation()) {
                return nullptr;
            }
        }
    }
    return engineControl;
}

const HardwareInfo &Device::getHardwareInfo() const { return *getRootDeviceEnvironment().getHardwareInfo(); }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

double Device::getProfilingTimerResolution() {
    return getOSTime()->getDynamicDeviceTimerResolution();
}

uint64_t Device::getProfilingTimerClock() {
    return getOSTime()->getDynamicDeviceTimerClock();
}

bool Device::isBcsSplitSupported() {
    auto &productHelper = getProductHelper();
    auto bcsSplit = productHelper.getBcsSplitSettings(getHardwareInfo()).enabled && Device::isBlitSplitEnabled();

    if (debugManager.flags.SplitBcsCopy.get() != -1) {
        bcsSplit = debugManager.flags.SplitBcsCopy.get();
    }

    return bcsSplit;
}

bool Device::isInitDeviceWithFirstSubmissionSupported(CommandStreamReceiverType csrType) {
    return getProductHelper().isInitDeviceWithFirstSubmissionRequired(getHardwareInfo()) &&
           Device::isInitDeviceWithFirstSubmissionEnabled(csrType);
}

double Device::getPlatformHostTimerResolution() const {
    if (getOSTime()) {
        return getOSTime()->getHostTimerResolution();
    }

    return 0.0;
}

GFXCORE_FAMILY Device::getRenderCoreFamily() const {
    return this->getHardwareInfo().platform.eRenderCoreFamily;
}

Debugger *Device::getDebugger() const {
    return getRootDeviceEnvironment().debugger.get();
}

bool Device::areSharedSystemAllocationsAllowed() const {
    if ((debugManager.flags.EnableRecoverablePageFaults.get() == 0) || (debugManager.flags.EnableSharedSystemUsmSupport.get() != 1)) {
        return false;
    }
    uint64_t mask = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    if (getHardwareInfo().capabilityTable.sharedSystemMemCapabilities & mask) {
        return true;
    }
    return false;
}

size_t Device::getEngineGroupIndexFromEngineGroupType(EngineGroupType engineGroupType) const {
    for (size_t i = 0; i < regularEngineGroups.size(); i++) {
        if (regularEngineGroups[i].engineGroupType == engineGroupType) {
            return i;
        }
    }
    UNRECOVERABLE_IF(true);
    return 0;
}

EngineControl *Device::tryGetEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    for (auto &engine : allEngines) {
        if ((engine.getEngineType() == engineType) &&
            (engine.getEngineUsage() == engineUsage)) {
            return &engine;
        }
    }

    if (debugManager.flags.OverrideInvalidEngineWithDefault.get()) {
        return &allEngines[0];
    }
    return nullptr;
}

EngineControl &Device::getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    auto engine = tryGetEngine(engineType, engineUsage);
    UNRECOVERABLE_IF(!engine);
    return *engine;
}

EngineControl &Device::getEngine(uint32_t index) {
    UNRECOVERABLE_IF(index >= allEngines.size());
    return allEngines[index];
}

bool Device::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const {
    TimeStampData timeStamp;
    auto retVal = getOSTime()->getGpuCpuTime(&timeStamp, true);
    if (retVal == TimeQueryStatus::success) {
        *hostTimestamp = timeStamp.cpuTimeinNS;
        if (debugManager.flags.EnableDeviceBasedTimestamps.get()) {
            auto resolution = getOSTime()->getDynamicDeviceTimerResolution();
            *deviceTimestamp = getGfxCoreHelper().getGpuTimeStampInNS(timeStamp.gpuTimeStamp, resolution);
        } else {
            *deviceTimestamp = *hostTimestamp;
        }
        return true;
    }
    return false;
}

bool Device::getHostTimer(uint64_t *hostTimestamp) const {
    return getOSTime()->getCpuTime(hostTimestamp);
}

uint32_t Device::getNumGenericSubDevices() const {
    return (hasRootCsr() ? getNumSubDevices() : 0);
}

Device *Device::getSubDevice(uint32_t deviceId) const {
    UNRECOVERABLE_IF(deviceId >= subdevices.size());
    return subdevices[deviceId];
}

Device *Device::getNearestGenericSubDevice(uint32_t deviceId) {
    if (subdevices.empty() || !hasRootCsr()) {
        return this;
    }
    UNRECOVERABLE_IF(deviceId >= subdevices.size());
    return subdevices[deviceId];
}

BindlessHeapsHelper *Device::getBindlessHeapsHelper() const {
    return getRootDeviceEnvironment().getBindlessHeapsHelper();
}

GmmClientContext *Device::getGmmClientContext() const {
    return getGmmHelper()->getClientContext();
}

void Device::allocateSyncBufferHandler() {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (syncBufferHandler.get() == nullptr) {
        syncBufferHandler = std::make_unique<SyncBufferHandler>(*this);
        UNRECOVERABLE_IF(syncBufferHandler.get() == nullptr);
    }
}

uint64_t Device::getGlobalMemorySize(uint32_t deviceBitfield) const {
    auto globalMemorySize = getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())
                                ? getMemoryManager()->getLocalMemorySize(this->getRootDeviceIndex(), deviceBitfield)
                                : getMemoryManager()->getSystemSharedMemory(this->getRootDeviceIndex());
    globalMemorySize = std::min(globalMemorySize, getMemoryManager()->getMaxApplicationAddress() + 1);
    double percentOfGlobalMemoryAvailable = getPercentOfGlobalMemoryAvailable();
    globalMemorySize = static_cast<uint64_t>(static_cast<double>(globalMemorySize) * percentOfGlobalMemoryAvailable);

    if (debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get() == -1 &&
        !getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())) {
        const uint64_t internalResourcesSize = 450 * MemoryConstants::megaByte;
        globalMemorySize = std::max(static_cast<uint64_t>(0), globalMemorySize - internalResourcesSize);
    }

    return globalMemorySize;
}

double Device::getPercentOfGlobalMemoryAvailable() const {
    if (debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get() != -1) {
        return 0.01 * static_cast<double>(debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get());
    }
    return getMemoryManager()->getPercentOfGlobalMemoryAvailable(this->getRootDeviceIndex());
}

NEO::DebuggerL0 *Device::getL0Debugger() {
    auto debugger = getDebugger();
    return debugger ? static_cast<NEO::DebuggerL0 *>(debugger) : nullptr;
}

const std::vector<EngineControl> &Device::getAllEngines() const {
    return this->allEngines;
}

const RootDeviceEnvironment &Device::getRootDeviceEnvironment() const {
    return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()];
}

RootDeviceEnvironment &Device::getRootDeviceEnvironmentRef() const {
    return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()];
}

bool Device::isFullRangeSvm() const {
    return getRootDeviceEnvironment().isFullRangeSvm();
}

EngineControl &Device::getInternalEngine() {
    if (this->allEngines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::hardware) {
        return this->getDefaultEngine();
    }

    auto engineType = getChosenEngineType(getHardwareInfo());

    return this->getNearestGenericSubDevice(0)->getEngine(engineType, EngineUsage::internal);
}

EngineControl *Device::getInternalCopyEngine() {
    if (!getHardwareInfo().capabilityTable.blitterOperationsSupported) {
        return nullptr;
    }

    const auto &productHelper = this->getProductHelper();
    auto expectedEngine = productHelper.getDefaultCopyEngine();

    if (debugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
        expectedEngine = EngineHelpers::mapBcsIndexToEngineType(debugManager.flags.ForceBCSForInternalCopyEngine.get(), true);
    }

    for (auto &engine : allEngines) {
        if (engine.osContext->getEngineType() == expectedEngine &&
            engine.osContext->isInternalEngine()) {
            return &engine;
        }
    }
    return nullptr;
}

EngineControl *Device::getHpCopyEngine() {
    return hpCopyEngine;
}

RTDispatchGlobalsInfo *Device::getRTDispatchGlobals(uint32_t maxBvhLevels) {
    if (rtDispatchGlobalsInfos.size() == 0) {
        return nullptr;
    }

    size_t last = rtDispatchGlobalsInfos.size() - 1;
    if (maxBvhLevels > last) {
        return nullptr;
    }

    for (size_t i = last; i >= maxBvhLevels; i--) {
        if (rtDispatchGlobalsInfos[i] != nullptr) {
            return rtDispatchGlobalsInfos[i];
        }

        if (i == 0) {
            break;
        }
    }

    allocateRTDispatchGlobals(maxBvhLevels);
    return rtDispatchGlobalsInfos[maxBvhLevels];
}

void Device::initializeRayTracing(uint32_t maxBvhLevels) {
    initializeRTMemoryBackedBuffer();

    while (rtDispatchGlobalsInfos.size() <= maxBvhLevels) {
        rtDispatchGlobalsInfos.push_back(nullptr);
    }
}

void Device::initializeRTMemoryBackedBuffer() {
    if (rtMemoryBackedBuffer == nullptr) {
        auto size = RayTracingHelper::getTotalMemoryBackedFifoSize(*this);

        AllocationProperties allocProps(getRootDeviceIndex(), true, size, AllocationType::buffer, true, getDeviceBitfield());
        auto &productHelper = getProductHelper();
        allocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
        allocProps.flags.isUSMDeviceAllocation = true;

        rtMemoryBackedBuffer = getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProps);
    }
}

void Device::finalizeRayTracing() {
    getMemoryManager()->freeGraphicsMemory(rtMemoryBackedBuffer);
    rtMemoryBackedBuffer = nullptr;

    for (size_t i = 0; i < rtDispatchGlobalsInfos.size(); i++) {
        auto rtDispatchGlobalsInfo = rtDispatchGlobalsInfos[i];
        if (rtDispatchGlobalsInfo == nullptr) {
            continue;
        }
        for (size_t j = 0; j < rtDispatchGlobalsInfo->rtStacks.size(); j++) {
            getMemoryManager()->freeGraphicsMemory(rtDispatchGlobalsInfo->rtStacks[j]);
            rtDispatchGlobalsInfo->rtStacks[j] = nullptr;
        }

        getMemoryManager()->freeGraphicsMemory(rtDispatchGlobalsInfo->rtDispatchGlobalsArray);
        rtDispatchGlobalsInfo->rtDispatchGlobalsArray = nullptr;

        delete rtDispatchGlobalsInfos[i];
        rtDispatchGlobalsInfos[i] = nullptr;
    }
}

OSTime *Device::getOSTime() const { return getRootDeviceEnvironment().osTime.get(); };

bool Device::getUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    if (this->uuid.isValid) {
        uuid = this->uuid.id;

        auto hwInfo = getHardwareInfo();
        auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);

        if (subDevicesCount > 1 && deviceBitfield.count() == 1) {
            // In case of no sub devices created (bits set in affinity mask == 1), return the UUID of enabled sub-device.
            uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
            uuid[ProductHelper::uuidSize - 1] = subDeviceIndex + 1;
        }
    }
    return this->uuid.isValid;
}

bool Device::generateUuidFromPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != PhysicalDevicePciBusInfo::invalidValue) {
        generateUuid(uuid);

        /* Device UUID uniquely identifies a device within a system.
         * We generate it based on device information along with PCI information
         * This guarantees uniqueness of UUIDs on a system even when multiple
         * identical Intel GPUs are present.
         */

        /* We want to have UUID matching between different GPU APIs (including outside
         * of compute_runtime project - i.e. other than L0 or OCL). This structure definition
         * has been agreed upon by various Intel driver teams.
         *
         * Consult other driver teams before changing this.
         */

        struct DeviceUUID {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t revisionID;
            uint16_t pciDomain;
            uint8_t pciBus;
            uint8_t pciDev;
            uint8_t pciFunc;
            uint8_t reserved[4];
            uint8_t subDeviceID;
        };
        static_assert(sizeof(DeviceUUID) == ProductHelper::uuidSize);

        DeviceUUID deviceUUID{};
        memcpy_s(&deviceUUID, sizeof(DeviceUUID), uuid.data(), uuid.size());

        deviceUUID.pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        deviceUUID.pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
        deviceUUID.pciDev = static_cast<uint8_t>(pciBusInfo.pciDevice);
        deviceUUID.pciFunc = static_cast<uint8_t>(pciBusInfo.pciFunction);

        memcpy_s(uuid.data(), uuid.size(), &deviceUUID, sizeof(DeviceUUID));

        return true;
    }
    return false;
}

void Device::generateUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    const auto &deviceInfo = getDeviceInfo();
    const auto &hardwareInfo = getHardwareInfo();
    uint32_t rootDeviceIndex = getRootDeviceIndex();
    uint16_t vendorId = static_cast<uint16_t>(deviceInfo.vendorId);
    uint16_t deviceId = static_cast<uint16_t>(hardwareInfo.platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(hardwareInfo.platform.usRevId);
    uint8_t subDeviceId = isSubDevice() ? static_cast<SubDevice *>(this)->getSubDeviceIndex() + 1 : 0;
    uuid.fill(0);
    memcpy_s(&uuid[0], sizeof(uint32_t), &vendorId, sizeof(vendorId));
    memcpy_s(&uuid[2], sizeof(uint32_t), &deviceId, sizeof(deviceId));
    memcpy_s(&uuid[4], sizeof(uint32_t), &revisionId, sizeof(revisionId));
    memcpy_s(&uuid[6], sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));
    uuid[15] = subDeviceId;
}

void Device::getAdapterMask(uint32_t &nodeMask) {
    if (verifyAdapterLuid()) {
        nodeMask = 1;
    }
}

const GfxCoreHelper &Device::getGfxCoreHelper() const {
    return getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
}

const ProductHelper &Device::getProductHelper() const {
    return getRootDeviceEnvironment().getHelper<ProductHelper>();
}

const CompilerProductHelper &Device::getCompilerProductHelper() const {
    return getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
}

ReleaseHelper *Device::getReleaseHelper() const {
    return getRootDeviceEnvironment().getReleaseHelper();
}

AILConfiguration *Device::getAilConfigurationHelper() const {
    return getRootDeviceEnvironment().getAILConfigurationHelper();
}

void Device::stopDirectSubmissionAndWaitForCompletion() {
    for (auto &engine : allEngines) {
        auto csr = engine.commandStreamReceiver;
        if (csr->isAnyDirectSubmissionEnabled()) {
            csr->stopDirectSubmission(true, true);
        }
    }
}

void Device::pollForCompletion() {
    if (allEngines.size() == 0 || !getDefaultEngine().commandStreamReceiver->isAubMode()) {
        return;
    }

    for (auto &engine : allEngines) {
        auto csr = engine.commandStreamReceiver;
        csr->pollForCompletion();
    }

    for (auto &subDevice : subdevices) {
        if (subDevice != nullptr) {
            subDevice->pollForCompletion();
        }
    }
}

bool Device::isAnyDirectSubmissionEnabled() const {
    return this->isAnyDirectSubmissionEnabledImpl(false);
}

bool Device::isAnyDirectSubmissionLightEnabled() const {
    return this->isAnyDirectSubmissionEnabledImpl(true);
}

bool Device::isAnyDirectSubmissionEnabledImpl(bool light) const {
    for (const auto &engine : allEngines) {
        auto enabled = light ? engine.osContext->isDirectSubmissionLightActive() : engine.commandStreamReceiver->isAnyDirectSubmissionEnabled();
        if (enabled) {
            return true;
        }
    }
    return false;
}

void Device::allocateRTDispatchGlobals(uint32_t maxBvhLevels) {
    UNRECOVERABLE_IF(rtDispatchGlobalsInfos.size() < maxBvhLevels + 1);
    UNRECOVERABLE_IF(rtDispatchGlobalsInfos[maxBvhLevels] != nullptr);

    uint32_t extraBytesLocal = 0;
    uint32_t extraBytesGlobal = 0;
    uint32_t dispatchGlobalsStride = MemoryConstants::pageSize64k;
    UNRECOVERABLE_IF(RayTracingHelper::getDispatchGlobalSize() > dispatchGlobalsStride);

    bool allocFailed = false;

    uint32_t tileCount = 1;
    if (this->getNumSubDevices() > 1) {
        // If device encompasses multiple tiles, allocate RTDispatchGlobals for each tile
        tileCount = this->getNumSubDevices();
    }

    auto dispatchGlobalsSize = tileCount * dispatchGlobalsStride;
    auto rtStackSize = RayTracingHelper::getRTStackSizePerTile(*this, tileCount, maxBvhLevels, extraBytesLocal, extraBytesGlobal);

    std::unique_ptr<RTDispatchGlobalsInfo> dispatchGlobalsInfo = std::make_unique<RTDispatchGlobalsInfo>();

    auto &productHelper = getProductHelper();

    GraphicsAllocation *dispatchGlobalsArrayAllocation = nullptr;

    AllocationProperties arrayAllocProps(getRootDeviceIndex(), true, dispatchGlobalsSize,
                                         AllocationType::globalSurface, true, getDeviceBitfield());
    arrayAllocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
    arrayAllocProps.flags.isUSMDeviceAllocation = true;
    dispatchGlobalsArrayAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(arrayAllocProps);

    if (dispatchGlobalsArrayAllocation == nullptr) {
        return;
    }

    for (unsigned int tile = 0; tile < tileCount; tile++) {
        DeviceBitfield deviceBitfield =
            (tileCount == 1)
                ? this->getDeviceBitfield()
                : subdevices[tile]->getDeviceBitfield();

        AllocationProperties allocProps(getRootDeviceIndex(), true, rtStackSize, AllocationType::buffer, true, deviceBitfield);
        allocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
        allocProps.flags.isUSMDeviceAllocation = true;

        auto rtStackAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProps);

        if (rtStackAllocation == nullptr) {
            allocFailed = true;
            break;
        }

        RTDispatchGlobals dispatchGlobals = {0};

        dispatchGlobals.rtMemBasePtr = rtStackAllocation->getGpuAddress() + rtStackSize;
        dispatchGlobals.callStackHandlerKSP = reinterpret_cast<uint64_t>(nullptr);
        auto releaseHelper = getReleaseHelper();
        dispatchGlobals.stackSizePerRay = releaseHelper ? releaseHelper->getStackSizePerRay() : 0;

        auto rtStacksPerDss = RayTracingHelper::getNumRtStacksPerDss(*this);
        dispatchGlobals.numDSSRTStacks = rtStacksPerDss;
        dispatchGlobals.maxBVHLevels = maxBvhLevels;
        uint32_t *dispatchGlobalsAsArray = reinterpret_cast<uint32_t *>(&dispatchGlobals);
        dispatchGlobalsAsArray[7] = 1;

        if (releaseHelper) {
            bool heaplessEnabled = this->getCompilerProductHelper().isHeaplessModeEnabled(this->getHardwareInfo());
            releaseHelper->adjustRTDispatchGlobals(static_cast<void *>(&dispatchGlobals), rtStacksPerDss, heaplessEnabled, maxBvhLevels);
        }

        MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(this->getRootDeviceEnvironment(), *dispatchGlobalsArrayAllocation),
                                                         *this,
                                                         dispatchGlobalsArrayAllocation,
                                                         tile * dispatchGlobalsStride,
                                                         &dispatchGlobals,
                                                         sizeof(RTDispatchGlobals));

        dispatchGlobalsInfo->rtStacks.push_back(rtStackAllocation);
    }

    if (allocFailed) {
        for (auto allocation : dispatchGlobalsInfo->rtStacks) {
            getMemoryManager()->freeGraphicsMemory(allocation);
        }

        getMemoryManager()->freeGraphicsMemory(dispatchGlobalsArrayAllocation);
        return;
    }

    dispatchGlobalsInfo->rtDispatchGlobalsArray = dispatchGlobalsArrayAllocation;
    rtDispatchGlobalsInfos[maxBvhLevels] = dispatchGlobalsInfo.release();
}

MemoryManager *Device::getMemoryManager() const {
    return executionEnvironment->memoryManager.get();
}

GmmHelper *Device::getGmmHelper() const {
    return getRootDeviceEnvironment().getGmmHelper();
}

CompilerInterface *Device::getCompilerInterface() const {
    return executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->getCompilerInterface();
}

SipExternalLib *Device::getSipExternalLibInterface() const {
    return executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->getSipExternalLibInterface();
}

BuiltIns *Device::getBuiltIns() const {
    return executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->getBuiltIns();
}

const EngineGroupT *Device::tryGetRegularEngineGroup(EngineGroupType engineGroupType) const {
    for (auto &engineGroup : regularEngineGroups) {
        if (engineGroup.engineGroupType == engineGroupType) {
            return &engineGroup;
        }
    }
    return nullptr;
}

EngineControl *SecondaryContexts::getEngine(EngineUsage usage, std::optional<int> priorityLevel) {
    auto secondaryEngineIndex = 0;

    std::lock_guard<std::mutex> guard(mutex);

    if (usage == EngineUsage::highPriority) {
        if (highPriorityEnginesTotal == 0) {
            return nullptr;
        }
        // Use index from  reserved HP pool
        if (hpIndices.size() < highPriorityEnginesTotal) {
            secondaryEngineIndex = (highPriorityCounter.fetch_add(1)) % (highPriorityEnginesTotal);
            secondaryEngineIndex += regularEnginesTotal;
            hpIndices.push_back(secondaryEngineIndex);
        }
        // Check if there is free index
        else if (assignedContextsCounter < regularEnginesTotal) {
            secondaryEngineIndex = assignedContextsCounter.fetch_add(1);
            highPriorityCounter.fetch_add(1);
            hpIndices.push_back(secondaryEngineIndex);
        }
        // Assign from existing indices
        else {
            auto index = (highPriorityCounter.fetch_add(1)) % (hpIndices.size());
            secondaryEngineIndex = hpIndices[index];
        }

        if (engines[secondaryEngineIndex].osContext->getEngineUsage() != EngineUsage::highPriority) {
            engines[secondaryEngineIndex].osContext->overrideEngineUsage(EngineUsage::highPriority);
        }

    } else if (usage == EngineUsage::regular) {
        if (regularEnginesTotal == 0) {
            return nullptr;
        }
        if (npIndices.size() == 0) {
            regularCounter.fetch_add(1);
            npIndices.push_back(secondaryEngineIndex);
        }
        // Check if there is free index
        else if (assignedContextsCounter < regularEnginesTotal) {
            secondaryEngineIndex = assignedContextsCounter.fetch_add(1);
            regularCounter.fetch_add(1);
            npIndices.push_back(secondaryEngineIndex);
        }
        // Assign from existing indices
        else {
            auto index = (regularCounter.fetch_add(1)) % (npIndices.size());
            secondaryEngineIndex = npIndices[index];
        }
    } else {
        DEBUG_BREAK_IF(true);
    }
    if (priorityLevel.has_value()) {
        engines[secondaryEngineIndex].osContext->overridePriority(priorityLevel.value());
    }

    return &engines[secondaryEngineIndex];
}

void Device::stopDirectSubmissionForCopyEngine() {
    auto internalBcsEngine = getInternalCopyEngine();
    if (internalBcsEngine == nullptr || getHardwareInfo().featureTable.ftrBcsInfo.count() > 1) {
        return;
    }
    auto regularBcsEngine = tryGetEngine(internalBcsEngine->osContext->getEngineType(), EngineUsage::regular);
    if (regularBcsEngine == nullptr) {
        return;
    }
    auto regularBcs = regularBcsEngine->commandStreamReceiver;
    if (regularBcs->isAnyDirectSubmissionEnabled()) {
        regularBcs->stopDirectSubmission(false, true);
    }
}

std::vector<DeviceVector> Device::groupDevices(DeviceVector devices) {
    std::map<PRODUCT_FAMILY, size_t> productsMap;
    std::vector<DeviceVector> outDevices;
    for (auto &device : devices) {
        if (device) {
            auto productFamily = device->getHardwareInfo().platform.eProductFamily;
            auto result = productsMap.find(productFamily);
            if (result == productsMap.end()) {
                productsMap.insert({productFamily, productsMap.size()});
                outDevices.push_back(DeviceVector{});
            }
            auto productId = productsMap[productFamily];
            outDevices[productId].push_back(std::move(device));
        }
    }
    std::sort(outDevices.begin(), outDevices.end(), [](DeviceVector &lhs, DeviceVector &rhs) -> bool {
        auto &leftHwInfo = lhs[0]->getHardwareInfo();  // NOLINT(clang-analyzer-cplusplus.Move) - MSVC assumes usage of moved vector
        auto &rightHwInfo = rhs[0]->getHardwareInfo(); // NOLINT(clang-analyzer-cplusplus.Move)
        if (leftHwInfo.capabilityTable.isIntegratedDevice != rightHwInfo.capabilityTable.isIntegratedDevice) {
            return rightHwInfo.capabilityTable.isIntegratedDevice;
        }
        return leftHwInfo.platform.eProductFamily > rightHwInfo.platform.eProductFamily;
    });
    return outDevices;
}

bool Device::canAccessPeer(QueryPeerAccessFunc queryPeerAccess, Device *peerDevice, bool &canAccess) {
    bool retVal = true;

    if (NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.get() != -1) {
        canAccess = !!NEO::debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.get();
        return retVal;
    }

    const uint32_t rootDeviceIndex = this->getRootDeviceIndex();
    const uint32_t peerRootDeviceIndex = peerDevice->getRootDeviceIndex();

    if (rootDeviceIndex == peerRootDeviceIndex) {
        canAccess = true;
        return retVal;
    }

    auto lock = executionEnvironment->obtainPeerAccessQueryLock();
    if (this->crossAccessEnabledDevices.find(peerRootDeviceIndex) == this->crossAccessEnabledDevices.end()) {
        retVal = queryPeerAccess(*this, *peerDevice, canAccess);
        this->updatePeerAccessCache(peerDevice, canAccess);
    }
    canAccess = this->crossAccessEnabledDevices[peerRootDeviceIndex];

    return retVal;
}

void Device::initializePeerAccessForDevices(QueryPeerAccessFunc queryPeerAccess, const std::vector<NEO::Device *> &devices) {
    for (auto &device : devices) {
        if (device->getReleaseHelper() && device->getReleaseHelper()->shouldQueryPeerAccess()) {
            device->hasPeerAccess = false;
            auto rootDeviceIndex = device->getRootDeviceIndex();

            for (auto &peerDevice : devices) {
                auto peerRootDeviceIndex = peerDevice->getRootDeviceIndex();
                if (rootDeviceIndex == peerRootDeviceIndex) {
                    continue;
                }

                bool canAccess = false;
                if (device->crossAccessEnabledDevices.find(peerRootDeviceIndex) == device->crossAccessEnabledDevices.end()) {
                    auto lock = device->getExecutionEnvironment()->obtainPeerAccessQueryLock();
                    queryPeerAccess(*device, *peerDevice, canAccess);
                    device->updatePeerAccessCache(peerDevice, canAccess);
                } else {
                    canAccess = device->crossAccessEnabledDevices[peerRootDeviceIndex];
                }

                if (canAccess) {
                    device->hasPeerAccess = true;
                }
            }
        }
    }
}
} // namespace NEO
