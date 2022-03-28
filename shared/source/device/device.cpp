/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
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

Device::Device(ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex)
    : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {
    this->executionEnvironment->incRefInternal();
}

Device::~Device() {
    if (false == commandStreamReceivers.empty()) {
        if (commandStreamReceivers[0]->skipResourceCleanup()) {
            return;
        }
    }

    finalizeRayTracing();

    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());

    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : allEngines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }
    allEngines.clear();

    for (auto subdevice : subdevices) {
        if (subdevice) {
            delete subdevice;
        }
    }
    subdevices.clear();

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
    bool notAllowed = !DebugManager.flags.EngineInstancedSubDevices.get();
    notAllowed |= engineInstanced;
    notAllowed |= (getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled < 2);
    notAllowed |= ((HwHelper::getSubDevicesCount(&getHardwareInfo()) < 2) && (!DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.get()));

    if (notAllowed) {
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

    executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initGmm();

    if (!getDebugger()) {
        this->executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->initDebugger();
    }

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (getDebugger() && hwHelper.disableL3CacheForDebug(hwInfo)) {
        getGmmHelper()->forceAllResourcesUncached();
    }

    if (!createEngines()) {
        return false;
    }

    getDefaultEngine().osContext->setDefaultContext(true);

    for (auto &engine : allEngines) {
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

    getRootDeviceEnvironmentRef().initOsTime();

    initializeCaps();

    if (getOSTime()->getOSInterface()) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    executionEnvironment->memoryManager->setForce32BitAllocations(getDeviceInfo().force32BitAddressess);

    if (DebugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
        for (auto &engine : allEngines) {
            auto csr = engine.commandStreamReceiver;
            csr->setExperimentalCmdBuffer(std::make_unique<ExperimentalCommandBuffer>(csr, getDeviceInfo().profilingTimerResolution));
        }
    }

    if (DebugManager.flags.EnableSWTags.get() && !getRootDeviceEnvironment().tagsManager->isInitialized()) {
        getRootDeviceEnvironment().tagsManager->initialize(*this);
    }

    createBindlessHeapsHelper();
    if (!isEngineInstanced()) {
        auto hardwareInfo = getRootDeviceEnvironment().getMutableHardwareInfo();
        uuid.isValid = false;

        if (DebugManager.flags.EnableChipsetUniqueUUID.get() == 1) {
            uuid.isValid = HwInfoConfig::get(hardwareInfo->platform.eProductFamily)->getUuid(this, uuid.id);
        }

        if (!uuid.isValid && getRootDeviceEnvironment().osInterface != nullptr) {
            PhysicalDevicePciBusInfo pciBusInfo = getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
            uuid.isValid = generateUuidFromPciBusInfo(pciBusInfo, uuid.id);
        }
    }

    return true;
}

bool Device::createEngines() {
    if (engineInstanced) {
        return createEngine(0, {engineInstancedType, EngineUsage::Regular});
    }

    auto &hwInfo = getHardwareInfo();
    auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    uint32_t deviceCsrIndex = 0;
    for (auto &engine : gpgpuEngines) {
        if (!createEngine(deviceCsrIndex++, engine)) {
            return false;
        }
    }
    return true;
}

void Device::addEngineToEngineGroup(EngineControl &engine) {
    const HardwareInfo &hardwareInfo = this->getHardwareInfo();
    const HwHelper &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const EngineGroupType engineGroupType = hwHelper.getEngineGroupType(engine.getEngineType(), engine.getEngineUsage(), hardwareInfo);

    if (!hwHelper.isSubDeviceEngineSupported(hardwareInfo, getDeviceBitfield(), engine.getEngineType())) {
        return;
    }

    if (EngineHelper::isCopyOnlyEngineType(engineGroupType) && DebugManager.flags.EnableBlitterOperationsSupport.get() == 0) {
        return;
    }

    if (this->regularEngineGroups.empty() || this->regularEngineGroups.back().engineGroupType != engineGroupType) {
        this->regularEngineGroups.push_back(EngineGroupT{});
        this->regularEngineGroups.back().engineGroupType = engineGroupType;
    }
    this->regularEngineGroups.back().engines.push_back(engine);
}

std::unique_ptr<CommandStreamReceiver> Device::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

bool Device::createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) {
    const auto &hwInfo = getHardwareInfo();
    const auto engineType = engineTypeUsage.first;
    const auto engineUsage = engineTypeUsage.second;
    const auto defaultEngineType = engineInstanced ? this->engineInstancedType : getChosenEngineType(hwInfo);
    const bool isDefaultEngine = defaultEngineType == engineType && engineUsage == EngineUsage::Regular;
    const bool createAsEngineInstanced = engineInstanced && EngineHelpers::isCcs(engineType);

    UNRECOVERABLE_IF(EngineHelpers::isBcs(engineType) && !hwInfo.capabilityTable.blitterOperationsSupported);

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    bool internalUsage = (engineUsage == EngineUsage::Internal);
    if (internalUsage) {
        commandStreamReceiver->initializeDefaultsForInternalEngine();
    }

    if (commandStreamReceiver->needsPageTableManager()) {
        commandStreamReceiver->createPageTableManager();
    }

    EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false, createAsEngineInstanced);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineDescriptor);
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
    allEngines.push_back(engine);
    if (engineUsage == EngineUsage::Regular) {
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
    return getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
}

uint64_t Device::getProfilingTimerClock() {
    return getOSTime()->getDynamicDeviceTimerClock(getHardwareInfo());
}

bool Device::isSimulation() const {
    auto &hwInfo = getHardwareInfo();

    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.platform.usDeviceID);
    for (const auto &engine : allEngines) {
        if (engine.commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
            simulation = true;
        }
    }

    if (hwInfo.featureTable.flags.ftrSimulationMode) {
        simulation = true;
    }
    return simulation;
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

bool Device::isDebuggerActive() const {
    return deviceInfo.debuggerActive;
}

bool Device::areSharedSystemAllocationsAllowed() const {
    auto sharedSystemAllocationsSupport = static_cast<bool>(getHardwareInfo().capabilityTable.sharedSystemMemCapabilities);
    if (DebugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        sharedSystemAllocationsSupport = DebugManager.flags.EnableSharedSystemUsmSupport.get();
    }
    return sharedSystemAllocationsSupport;
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

    if (DebugManager.flags.OverrideInvalidEngineWithDefault.get()) {
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
    auto retVal = getOSTime()->getCpuGpuTime(&timeStamp);
    if (retVal) {
        *hostTimestamp = timeStamp.CPUTimeinNS;
        if (DebugManager.flags.EnableDeviceBasedTimestamps.get()) {
            auto resolution = getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
            *deviceTimestamp = static_cast<uint64_t>(timeStamp.GPUTimeStamp * resolution);
        } else
            *deviceTimestamp = *hostTimestamp;
    }
    return retVal;
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
    /*
    * EngineInstanced: Upper level
    * Generic SubDevice: 'this'
    * RootCsr Device: Next level SubDevice (generic)
    */

    if (engineInstanced) {
        return getRootDevice()->getNearestGenericSubDevice(Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong())));
    }

    if (subdevices.empty() || !hasRootCsr()) {
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
    return getMemoryManager()->getPercentOfGlobalMemoryAvailable(this->getRootDeviceIndex());
}

NEO::SourceLevelDebugger *Device::getSourceLevelDebugger() {
    auto debugger = getDebugger();
    if (debugger) {
        return debugger->isLegacy() ? static_cast<NEO::SourceLevelDebugger *>(debugger) : nullptr;
    }
    return nullptr;
}

const std::vector<EngineControl> &Device::getAllEngines() const {
    return this->allEngines;
}

EngineControl &Device::getInternalEngine() {
    if (this->allEngines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        return this->getDefaultEngine();
    }

    auto engineType = getChosenEngineType(getHardwareInfo());

    return this->getNearestGenericSubDevice(0)->getEngine(engineType, EngineUsage::Internal);
}

EngineControl &Device::getNextEngineForCommandQueue() {
    this->initializeEngineRoundRobinControls();

    const auto &defaultEngine = this->getDefaultEngine();

    const auto &hardwareInfo = this->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto engineGroupType = hwHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hardwareInfo);

    const auto defaultEngineGroupIndex = this->getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto &engineGroup = this->getRegularEngineGroups()[defaultEngineGroupIndex];

    auto engineIndex = 0u;
    do {
        engineIndex = (this->regularCommandQueuesCreatedWithinDeviceCount++ / this->queuesPerEngineCount) % engineGroup.engines.size();
    } while (!this->availableEnginesForCommandQueueusRoundRobin.test(engineIndex));
    return engineGroup.engines[engineIndex];
}

EngineControl *Device::getInternalCopyEngine() {
    if (!getHardwareInfo().capabilityTable.blitterOperationsSupported) {
        return nullptr;
    }
    for (auto &engine : allEngines) {
        if (engine.osContext->getEngineType() == aub_stream::ENGINE_BCS &&
            engine.osContext->isInternalEngine()) {
            return &engine;
        }
    }
    return nullptr;
}

GraphicsAllocation *Device::getRTDispatchGlobals(uint32_t maxBvhLevels) {
    if (rtDispatchGlobals.size() == 0) {
        return nullptr;
    }

    size_t last = rtDispatchGlobals.size() - 1;
    if (maxBvhLevels > last) {
        return nullptr;
    }

    for (size_t i = last; i >= maxBvhLevels; i--) {
        if (rtDispatchGlobals[i] != nullptr) {
            return rtDispatchGlobals[i];
        }

        if (i == 0) {
            break;
        }
    }

    allocateRTDispatchGlobals(maxBvhLevels);
    return rtDispatchGlobals[maxBvhLevels];
}

void Device::initializeRayTracing(uint32_t maxBvhLevels) {
    if (rtMemoryBackedBuffer == nullptr) {
        auto size = RayTracingHelper::getTotalMemoryBackedFifoSize(*this);
        rtMemoryBackedBuffer = getMemoryManager()->allocateGraphicsMemoryWithProperties({getRootDeviceIndex(), size, AllocationType::BUFFER, getDeviceBitfield()});
    }

    while (rtDispatchGlobals.size() <= maxBvhLevels) {
        rtDispatchGlobals.push_back(nullptr);
    }
}

void Device::finalizeRayTracing() {
    getMemoryManager()->freeGraphicsMemory(rtMemoryBackedBuffer);
    rtMemoryBackedBuffer = nullptr;

    for (size_t i = 0; i < rtDispatchGlobals.size(); i++) {
        getMemoryManager()->freeGraphicsMemory(rtDispatchGlobals[i]);
        rtDispatchGlobals[i] = nullptr;
    }
}

void Device::initializeEngineRoundRobinControls() {
    if (this->availableEnginesForCommandQueueusRoundRobin.any()) {
        return;
    }

    uint32_t queuesPerEngine = 1u;

    if (DebugManager.flags.CmdQRoundRobindEngineAssignNTo1.get() != -1) {
        queuesPerEngine = DebugManager.flags.CmdQRoundRobindEngineAssignNTo1.get();
    }

    this->queuesPerEngineCount = queuesPerEngine;

    std::bitset<8> availableEngines = std::numeric_limits<uint8_t>::max();

    if (DebugManager.flags.CmdQRoundRobindEngineAssignBitfield.get() != -1) {
        availableEngines = DebugManager.flags.CmdQRoundRobindEngineAssignBitfield.get();
    }

    this->availableEnginesForCommandQueueusRoundRobin = availableEngines;
}

OSTime *Device::getOSTime() const { return getRootDeviceEnvironment().osTime.get(); };

bool Device::getUuid(std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) {
    if (this->uuid.isValid) {
        uuid = this->uuid.id;

        if (!isSubDevice() && deviceBitfield.count() == 1) {
            // In case of no sub devices created (bits set in affinity mask == 1), return the UUID of enabled sub-device.
            uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
            uuid[HwInfoConfig::uuidSize - 1] = subDeviceIndex + 1;
        }
    }
    return this->uuid.isValid;
}

bool Device::generateUuidFromPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != PhysicalDevicePciBusInfo::InvalidValue) {

        uuid.fill(0);
        memcpy_s(&uuid[0], 2, &pciBusInfo.pciDomain, 2);
        memcpy_s(&uuid[2], 1, &pciBusInfo.pciBus, 1);
        memcpy_s(&uuid[3], 1, &pciBusInfo.pciDevice, 1);
        memcpy_s(&uuid[4], 1, &pciBusInfo.pciFunction, 1);
        uuid[HwInfoConfig::uuidSize - 1] = isSubDevice() ? static_cast<SubDevice *>(this)->getSubDeviceIndex() + 1 : 0;
        return true;
    }
    return false;
}

void Device::generateUuid(std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) {
    const auto &deviceInfo = getDeviceInfo();
    const auto &hardwareInfo = getHardwareInfo();
    uint32_t rootDeviceIndex = getRootDeviceIndex();
    uuid.fill(0);
    memcpy_s(&uuid[0], sizeof(uint32_t), &deviceInfo.vendorId, sizeof(deviceInfo.vendorId));
    memcpy_s(&uuid[4], sizeof(uint32_t), &hardwareInfo.platform.usDeviceID, sizeof(hardwareInfo.platform.usDeviceID));
    memcpy_s(&uuid[8], sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));
}
} // namespace NEO
