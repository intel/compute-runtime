/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

Device::Device(ExecutionEnvironment *executionEnvironment)
    : executionEnvironment(executionEnvironment) {
    this->executionEnvironment->incRefInternal();
}

Device::~Device() {
    getMemoryManager()->freeGraphicsMemory(rtMemoryBackedBuffer);
    rtMemoryBackedBuffer = nullptr;

    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());

    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : engines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }

    for (auto subdevice : subdevices) {
        if (subdevice) {
            delete subdevice;
        }
    }

    syncBufferHandler.reset();
    commandStreamReceivers.clear();
    executionEnvironment->memoryManager->waitForDeletions();

    executionEnvironment->decRefInternal();
}

SubDevice *Device::createSubDevice(uint32_t subDeviceIndex) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *getRootDevice());
}

SubDevice *Device::createEngineInstancedSubDevice(uint32_t subDeviceIndex, aub_stream::EngineType engineType) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *getRootDevice(), engineType);
}

bool Device::genericSubDevicesAllowed() {
    auto deviceMask = executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->deviceAffinityMask.getGenericSubDevicesMask();
    uint32_t subDeviceCount = HwHelper::getSubDevicesCount(&getHardwareInfo());
    deviceBitfield = maxNBitValue(subDeviceCount);
    deviceBitfield &= deviceMask;
    numSubDevices = static_cast<uint32_t>(deviceBitfield.count());
    if (numSubDevices == 1) {
        numSubDevices = 0;
    }

    return (numSubDevices > 0);
}

bool Device::engineInstancedSubDevicesAllowed() {
    if ((DebugManager.flags.EngineInstancedSubDevices.get() != 1) || engineInstanced ||
        (getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled < 2)) {
        return false;
    }

    UNRECOVERABLE_IF(deviceBitfield.count() != 1);
    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);
    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    numSubDevices = std::min(ccsCount, static_cast<uint32_t>(enginesMask.count()));

    if (numSubDevices == 1) {
        numSubDevices = 0;
    }

    return (numSubDevices > 0);
}

bool Device::createEngineInstancedSubDevices() {
    UNRECOVERABLE_IF(deviceBitfield.count() != 1);
    UNRECOVERABLE_IF(!subdevices.empty());

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);
    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    subdevices.resize(ccsCount, nullptr);

    for (uint32_t i = 0; i < ccsCount; i++) {
        if (!enginesMask.test(i)) {
            continue;
        }

        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
        auto subDevice = createEngineInstancedSubDevice(subDeviceIndex, engineType);
        UNRECOVERABLE_IF(!subDevice);
        subdevices[i] = subDevice;
    }

    return true;
}

bool Device::createGenericSubDevices() {
    UNRECOVERABLE_IF(!subdevices.empty());
    uint32_t subDeviceCount = HwHelper::getSubDevicesCount(&getHardwareInfo());

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

    if (engineInstancedSubDevicesAllowed()) {
        return createEngineInstancedSubDevices();
    }

    return true;
}

void Device::setAsEngineInstanced() {
    if (subdevices.size() > 0) {
        return;
    }

    UNRECOVERABLE_IF(deviceBitfield.count() != 1);

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);

    if (enginesMask.count() != 1) {
        return;
    }

    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    for (uint32_t i = 0; i < ccsCount; i++) {
        if (!enginesMask.test(i)) {
            continue;
        }

        UNRECOVERABLE_IF(engineInstanced);
        engineInstanced = true;
        engineInstancedType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
    }

    UNRECOVERABLE_IF(!engineInstanced);
}

bool Device::createDeviceImpl() {
    if (!createSubDevices()) {
        return false;
    }

    setAsEngineInstanced();

    auto &hwInfo = getHardwareInfo();
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&this->hardwareCapabilities, hwInfo);
    executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initGmm();

    if (!getDebugger()) {
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initDebugger();
    }

    if (!createEngines()) {
        return false;
    }

    getDefaultEngine().osContext->setDefaultContext(true);

    for (auto &engine : engines) {
        auto commandStreamReceiver = engine.commandStreamReceiver;
        commandStreamReceiver->postInitFlagsSetup();
    }

    uint32_t defaultEngineIndexWithinMemoryManager = 0;
    for (auto engineIndex = 0u; engineIndex < executionEnvironment->memoryManager->getRegisteredEnginesCount(); engineIndex++) {
        OsContext *engine = executionEnvironment->memoryManager->getRegisteredEngines()[engineIndex].osContext;
        if (engine == getDefaultEngine().osContext) {
            defaultEngineIndexWithinMemoryManager = engineIndex;
            break;
        }
    }
    executionEnvironment->memoryManager->setDefaultEngineIndex(getRootDeviceIndex(), defaultEngineIndexWithinMemoryManager);

    auto osInterface = getRootDeviceEnvironment().osInterface.get();

    if (!osTime) {
        osTime = OSTime::create(osInterface);
    }

    initializeCaps();

    if (osTime->getOSInterface()) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    executionEnvironment->memoryManager->setForce32BitAllocations(getDeviceInfo().force32BitAddressess);

    if (DebugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
        for (auto &engine : engines) {
            auto csr = engine.commandStreamReceiver;
            csr->setExperimentalCmdBuffer(std::make_unique<ExperimentalCommandBuffer>(csr, getDeviceInfo().profilingTimerResolution));
        }
    }

    if (DebugManager.flags.EnableSWTags.get() && !getRootDeviceEnvironment().tagsManager->isInitialized()) {
        getRootDeviceEnvironment().tagsManager->initialize(*this);
    }

    createBindlessHeapsHelper();

    return true;
}

bool Device::engineSupported(const EngineTypeUsage &engineTypeUsage) const {
    if (engineInstanced) {
        return (EngineHelpers::isBcs(engineTypeUsage.first) || (engineTypeUsage.first == this->engineInstancedType));
    }

    return true;
}

bool Device::createEngines() {
    auto &hwInfo = getHardwareInfo();
    auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    this->engineGroups.resize(static_cast<uint32_t>(EngineGroupType::MaxEngineGroups));

    uint32_t deviceCsrIndex = 0;
    for (auto &engine : gpgpuEngines) {
        if (engineSupported(engine) && !createEngine(deviceCsrIndex++, engine)) {
            return false;
        }
    }
    return true;
}

void Device::addEngineToEngineGroup(EngineControl &engine) {
    const HardwareInfo &hardwareInfo = this->getHardwareInfo();
    const HwHelper &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const EngineGroupType engineGroupType = hwHelper.getEngineGroupType(engine.getEngineType(), hardwareInfo);

    if (!hwHelper.isSubDeviceEngineSupported(hardwareInfo, getDeviceBitfield(), engine.getEngineType())) {
        return;
    }

    if (hwHelper.isCopyOnlyEngineType(engineGroupType) && DebugManager.flags.EnableBlitterOperationsSupport.get() == 0) {
        return;
    }

    const uint32_t engineGroupIndex = static_cast<uint32_t>(engineGroupType);
    this->engineGroups[engineGroupIndex].push_back(engine);
}

std::unique_ptr<CommandStreamReceiver> Device::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

bool Device::createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) {
    const auto &hwInfo = getHardwareInfo();
    const auto engineType = engineTypeUsage.first;
    const auto engineUsage = engineTypeUsage.second;
    const auto defaultEngineType = getChosenEngineType(hwInfo);
    const bool isDefaultEngine = defaultEngineType == engineType && engineUsage == EngineUsage::Regular;

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    bool internalUsage = (engineTypeUsage.second == EngineUsage::Internal);
    if (internalUsage) {
        commandStreamReceiver->initializeDefaultsForInternalEngine();
    }

    if (commandStreamReceiver->needsPageTableManager(engineType)) {
        commandStreamReceiver->createPageTableManager();
    }

    bool lowPriority = (engineTypeUsage.second == EngineUsage::LowPriority);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                     engineTypeUsage,
                                                                                     getDeviceBitfield(),
                                                                                     preemptionMode,
                                                                                     false);
    if (osContext->isImmediateContextInitializationEnabled(isDefaultEngine)) {
        osContext->ensureContextInitialized();
    }
    commandStreamReceiver->setupContext(*osContext);

    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }

    if (!commandStreamReceiver->createGlobalFenceAllocation()) {
        return false;
    }

    if (isDefaultEngine) {
        defaultEngineIndex = deviceCsrIndex;
    }

    if (preemptionMode == PreemptionMode::MidThread && !commandStreamReceiver->createPreemptionAllocation()) {
        return false;
    }

    EngineControl engine{commandStreamReceiver.get(), osContext};
    engines.push_back(engine);
    if (!lowPriority && !internalUsage) {
        addEngineToEngineGroup(engine);
    }

    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

const HardwareInfo &Device::getHardwareInfo() const { return *getRootDeviceEnvironment().getHardwareInfo(); }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(getHardwareInfo());
}

uint64_t Device::getProfilingTimerClock() {
    return osTime->getDynamicDeviceTimerClock(getHardwareInfo());
}

bool Device::isSimulation() const {
    auto &hwInfo = getHardwareInfo();

    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    for (const auto &engine : engines) {
        if (engine.commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
            simulation = true;
        }
    }

    if (hwInfo.featureTable.ftrSimulationMode) {
        simulation = true;
    }
    return simulation;
}

double Device::getPlatformHostTimerResolution() const {
    if (osTime.get())
        return osTime->getHostTimerResolution();
    return 0.0;
}

GFXCORE_FAMILY Device::getRenderCoreFamily() const {
    return this->getHardwareInfo().platform.eRenderCoreFamily;
}

bool Device::isDebuggerActive() const {
    return deviceInfo.debuggerActive;
}

const std::vector<EngineControl> *Device::getNonEmptyEngineGroup(size_t index) const {
    auto nonEmptyGroupIndex = 0u;
    for (auto groupIndex = 0u; groupIndex < engineGroups.size(); groupIndex++) {
        const std::vector<EngineControl> *currentGroup = &engineGroups[groupIndex];
        if (currentGroup->empty()) {
            continue;
        }

        if (index == nonEmptyGroupIndex) {
            return currentGroup;
        }

        nonEmptyGroupIndex++;
    }
    return nullptr;
}

size_t Device::getIndexOfNonEmptyEngineGroup(EngineGroupType engineGroupType) const {
    const auto groupIndex = static_cast<size_t>(engineGroupType);
    UNRECOVERABLE_IF(groupIndex >= engineGroups.size());
    UNRECOVERABLE_IF(engineGroups[groupIndex].empty());

    size_t result = 0u;
    for (auto currentGroupIndex = 0u; currentGroupIndex < groupIndex; currentGroupIndex++) {
        if (!engineGroups[currentGroupIndex].empty()) {
            result++;
        }
    }

    return result;
}

EngineControl *Device::tryGetEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    for (auto &engine : engines) {
        if (engine.osContext->getEngineType() == engineType &&
            engine.osContext->isLowPriority() == (engineUsage == EngineUsage::LowPriority) &&
            engine.osContext->isInternalEngine() == (engineUsage == EngineUsage::Internal)) {
            return &engine;
        }
    }
    if (DebugManager.flags.OverrideInvalidEngineWithDefault.get()) {
        return &engines[0];
    }
    return nullptr;
}

EngineControl &Device::getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    auto engine = tryGetEngine(engineType, engineUsage);
    UNRECOVERABLE_IF(!engine);
    return *engine;
}

EngineControl &Device::getEngine(uint32_t index) {
    UNRECOVERABLE_IF(index >= engines.size());
    return engines[index];
}

bool Device::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const {
    TimeStampData queueTimeStamp;
    bool retVal = getOSTime()->getCpuGpuTime(&queueTimeStamp);
    if (retVal) {
        uint64_t resolution = (uint64_t)getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
        *deviceTimestamp = queueTimeStamp.GPUTimeStamp * resolution;
    }

    retVal = getOSTime()->getCpuTime(hostTimestamp);
    return retVal;
}

bool Device::getHostTimer(uint64_t *hostTimestamp) const {
    return getOSTime()->getCpuTime(hostTimestamp);
}

uint32_t Device::getNumAvailableDevices() const {
    if (subdevices.empty()) {
        return 1u;
    }
    return getNumSubDevices();
}

Device *Device::getDeviceById(uint32_t deviceId) const {
    if (subdevices.empty()) {
        return const_cast<Device *>(this);
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

    return globalMemorySize;
}

double Device::getPercentOfGlobalMemoryAvailable() const {
    if (DebugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get() != -1) {
        return 0.01 * static_cast<double>(DebugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get());
    }
    return 0.8;
}

NEO::SourceLevelDebugger *Device::getSourceLevelDebugger() {
    auto debugger = getDebugger();
    if (debugger) {
        return debugger->isLegacy() ? static_cast<NEO::SourceLevelDebugger *>(debugger) : nullptr;
    }
    return nullptr;
}

const std::vector<EngineControl> &Device::getEngines() const {
    return this->engines;
}

EngineControl &Device::getInternalEngine() {
    if (this->engines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        return this->getDefaultEngine();
    }

    auto engineType = getChosenEngineType(getHardwareInfo());

    return this->getDeviceById(0)->getEngine(engineType, EngineUsage::Internal);
}

void Device::initializeRayTracing() {
    if (rtMemoryBackedBuffer == nullptr) {
        auto size = RayTracingHelper::getTotalMemoryBackedFifoSize(*this);
        rtMemoryBackedBuffer = getMemoryManager()->allocateGraphicsMemoryWithProperties({getRootDeviceIndex(), size, GraphicsAllocation::AllocationType::BUFFER, getDeviceBitfield()});
    }
}
} // namespace NEO
