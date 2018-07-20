/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/device/device.h"
#include "hw_cmds.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device_vector.h"
#include "runtime/device/driver_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/built_ins_helper.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include <cstring>
#include <map>

namespace OCLRT {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;

DeviceVector::DeviceVector(const cl_device_id *devices,
                           cl_uint numDevices) {
    for (cl_uint i = 0; i < numDevices; i++) {
        this->push_back(castToObject<Device>(devices[i]));
    }
}

void DeviceVector::toDeviceIDs(std::vector<cl_device_id> &devIDs) {
    int i = 0;
    devIDs.resize(this->size());

    for (auto &it : *this) {
        devIDs[i] = it;
        i++;
    }
}

CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo);

// Global table of hardware prefixes
const char *hardwarePrefix[IGFX_MAX_PRODUCT] = {
    nullptr,
};
// Global table of family names
const char *familyName[IGFX_MAX_CORE] = {
    nullptr,
};
// Global table of family names
bool familyEnabled[IGFX_MAX_CORE] = {
    false,
};

Device::Device(const HardwareInfo &hwInfo, ExecutionEnvironment *executionEnvironment)
    : enabledClVersion(false), hwInfo(hwInfo), tagAddress(nullptr), preemptionAllocation(nullptr),
      osTime(nullptr), slmWindowStartAddress(nullptr), executionEnvironment(executionEnvironment) {
    memset(&deviceInfo, 0, sizeof(deviceInfo));
    deviceExtensions.reserve(1000);
    name.reserve(100);
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);
    engineType = DebugManager.flags.NodeOrdinal.get() == -1
                     ? hwInfo.capabilityTable.defaultEngineType
                     : static_cast<EngineType>(DebugManager.flags.NodeOrdinal.get());

    sourceLevelDebugger.reset(SourceLevelDebugger::create());
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo.pPlatform->eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
    this->executionEnvironment->incRefInternal();
}

Device::~Device() {
    BuiltIns::shutDown();
    CompilerInterface::shutdown();
    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());
    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    if (executionEnvironment->commandStreamReceiver) {
        executionEnvironment->commandStreamReceiver->flushBatchedSubmissions();
    }

    if (deviceInfo.sourceLevelDebuggerActive && sourceLevelDebugger) {
        sourceLevelDebugger->notifyDeviceDestruction();
    }

    if (executionEnvironment->memoryManager) {
        if (preemptionAllocation) {
            executionEnvironment->memoryManager->freeGraphicsMemory(preemptionAllocation);
            preemptionAllocation = nullptr;
        }
        executionEnvironment->memoryManager->waitForDeletions();

        alignedFree(this->slmWindowStartAddress);
    }
    executionEnvironment->decRefInternal();
}

bool Device::createDeviceImpl(const HardwareInfo *pHwInfo, Device &outDevice) {
    auto executionEnvironment = outDevice.executionEnvironment;
    executionEnvironment->initGmm(pHwInfo);
    if (!executionEnvironment->initializeCommandStreamReceiver(pHwInfo)) {
        return false;
    }

    executionEnvironment->initializeMemoryManager(outDevice.getEnabled64kbPages());

    CommandStreamReceiver *commandStreamReceiver = executionEnvironment->commandStreamReceiver.get();
    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }

    executionEnvironment->memoryManager->csr = commandStreamReceiver;

    auto pDevice = &outDevice;
    if (!pDevice->osTime) {
        pDevice->osTime = OSTime::create(commandStreamReceiver->getOSInterface());
    }
    pDevice->driverInfo.reset(DriverInfo::create(commandStreamReceiver->getOSInterface()));
    pDevice->tagAddress = reinterpret_cast<uint32_t *>(commandStreamReceiver->getTagAllocation()->getUnderlyingBuffer());

    // Initialize HW tag to a known value
    *pDevice->tagAddress = DebugManager.flags.EnableNullHardware.get() ? -1 : initialHardwareTag;

    pDevice->initializeCaps();

    if (pDevice->osTime->getOSInterface()) {
        if (pHwInfo->capabilityTable.instrumentationEnabled) {
            pDevice->performanceCounters = createPerformanceCountersFunc(pDevice->osTime.get());
            pDevice->performanceCounters->initialize(pHwInfo);
        }
    }

    uint32_t deviceHandle = 0;
    if (commandStreamReceiver->getOSInterface()) {
        deviceHandle = commandStreamReceiver->getOSInterface()->getDeviceHandle();
    }

    if (pDevice->deviceInfo.sourceLevelDebuggerActive) {
        pDevice->sourceLevelDebugger->notifyNewDevice(deviceHandle);
    }

    outDevice.executionEnvironment->memoryManager->setForce32BitAllocations(pDevice->getDeviceInfo().force32BitAddressess);
    outDevice.executionEnvironment->memoryManager->device = pDevice;

    if (pDevice->preemptionMode == PreemptionMode::MidThread || pDevice->isSourceLevelDebuggerActive()) {
        size_t requiredSize = pHwInfo->capabilityTable.requiredPreemptionSurfaceSize;
        size_t alignment = 256 * MemoryConstants::kiloByte;
        bool uncacheable = pDevice->getWaTable()->waCSRUncachable;
        pDevice->preemptionAllocation = outDevice.executionEnvironment->memoryManager->allocateGraphicsMemory(requiredSize, alignment, false, uncacheable);
        if (!pDevice->preemptionAllocation) {
            return false;
        }
        commandStreamReceiver->setPreemptionCsrAllocation(pDevice->preemptionAllocation);
        auto sipType = SipKernel::getSipKernelType(pHwInfo->pPlatform->eRenderCoreFamily, pDevice->isSourceLevelDebuggerActive());
        initSipKernel(sipType, *pDevice);
    }

    if (DebugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
        commandStreamReceiver->setExperimentalCmdBuffer(
            std::unique_ptr<ExperimentalCommandBuffer>(new ExperimentalCommandBuffer(commandStreamReceiver, pDevice->getDeviceInfo().profilingTimerResolution)));
    }

    return true;
}

const HardwareInfo *Device::getDeviceInitHwInfo(const HardwareInfo *pHwInfoIn) {
    return pHwInfoIn ? pHwInfoIn : platformDevices[0];
}

const HardwareInfo &Device::getHardwareInfo() const { return hwInfo; }

const WorkaroundTable *Device::getWaTable() const { return hwInfo.pWaTable; }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

DeviceInfo *Device::getMutableDeviceInfo() {
    return &deviceInfo;
}

void *Device::getSLMWindowStartAddress() {
    prepareSLMWindow();
    return this->slmWindowStartAddress;
}

void Device::prepareSLMWindow() {
    if (this->slmWindowStartAddress == nullptr) {
        this->slmWindowStartAddress = executionEnvironment->memoryManager->allocateSystemMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }
}

const char *Device::getProductAbbrev() const {
    return hardwarePrefix[hwInfo.pPlatform->eProductFamily];
}

const std::string Device::getFamilyNameWithType() const {
    std::string platformName = familyName[hwInfo.pPlatform->eRenderCoreFamily];
    platformName.append(getPlatformType(hwInfo));
    return platformName;
}

double Device::getProfilingTimerResolution() {
    return osTime->getDynamicDeviceTimerResolution(hwInfo);
}

unsigned int Device::getSupportedClVersion() const {
    return hwInfo.capabilityTable.clVersionSupport;
}

/* We hide the retain and release function of BaseObject. */
void Device::retain() {
    DEBUG_BREAK_IF(!isValid());
}

unique_ptr_if_unused<Device> Device::release() {
    DEBUG_BREAK_IF(!isValid());
    return unique_ptr_if_unused<Device>(this, false);
}

bool Device::isSimulation() const {
    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.pPlatform->usDeviceID);
    if (executionEnvironment->commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        simulation = true;
    }
    if (hwInfo.pSkuTable->ftrSimulationMode) {
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
    return this->getHardwareInfo().pPlatform->eRenderCoreFamily;
}

bool Device::isSourceLevelDebuggerActive() const {
    return deviceInfo.sourceLevelDebuggerActive;
}
} // namespace OCLRT
