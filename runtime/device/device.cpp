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

#include "hw_cmds.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device.h"
#include "runtime/device/device_vector.h"
#include "runtime/device/driver_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
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

Device::Device(const HardwareInfo &hwInfo)
    : memoryManager(nullptr), enabledClVersion(false), hwInfo(hwInfo), commandStreamReceiver(nullptr),
      tagAddress(nullptr), tagAllocation(nullptr), preemptionAllocation(nullptr),
      osTime(nullptr), slmWindowStartAddress(nullptr) {
    memset(&deviceInfo, 0, sizeof(deviceInfo));
    deviceExtensions.reserve(1000);
    name.reserve(100);
    GmmHelper::hwInfo = &hwInfo;
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);
    engineType = DebugManager.flags.NodeOrdinal.get() == -1
                     ? hwInfo.capabilityTable.defaultEngineType
                     : static_cast<EngineType>(DebugManager.flags.NodeOrdinal.get());

    sourceLevelDebugger.reset(SourceLevelDebugger::create());
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo.pPlatform->eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
}

Device::~Device() {
    BuiltIns::shutDown();
    CompilerInterface::shutdown();
    DEBUG_BREAK_IF(nullptr == memoryManager);
    if (performanceCounters) {
        performanceCounters->shutdown();
    }
    if (commandStreamReceiver) {
        commandStreamReceiver->flushBatchedSubmissions();
        delete commandStreamReceiver;
        commandStreamReceiver = nullptr;
    }

    if (deviceInfo.sourceLevelDebuggerActive && sourceLevelDebugger) {
        sourceLevelDebugger->notifyDeviceDestruction();
    }

    if (memoryManager) {
        if (preemptionAllocation) {
            memoryManager->freeGraphicsMemory(preemptionAllocation);
            preemptionAllocation = nullptr;
        }
        memoryManager->waitForDeletions();

        memoryManager->freeGraphicsMemory(tagAllocation);
        alignedFree(this->slmWindowStartAddress);
    }
    tagAllocation = nullptr;
    delete memoryManager;
    memoryManager = nullptr;
    if (executionEnvironment) {
        executionEnvironment->decRefInternal();
    }
}

bool Device::createDeviceImpl(const HardwareInfo *pHwInfo, Device &outDevice) {
    outDevice.executionEnvironment->initGmm(pHwInfo);
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(pHwInfo);
    if (!commandStreamReceiver) {
        return false;
    }

    outDevice.commandStreamReceiver = commandStreamReceiver;

    if (!outDevice.memoryManager) {
        outDevice.memoryManager = commandStreamReceiver->createMemoryManager(outDevice.deviceInfo.enabled64kbPages);
    } else {
        commandStreamReceiver->setMemoryManager(outDevice.memoryManager);
    }

    DEBUG_BREAK_IF(nullptr == outDevice.memoryManager);

    outDevice.memoryManager->csr = commandStreamReceiver;

    auto pTagAllocation = outDevice.memoryManager->allocateGraphicsMemory(
        sizeof(uint32_t), sizeof(uint32_t));
    if (!pTagAllocation) {
        return false;
    }
    auto pTagMemory = reinterpret_cast<uint32_t *>(pTagAllocation->getUnderlyingBuffer());
    // Initialize HW tag to a known value
    *pTagMemory = DebugManager.flags.EnableNullHardware.get() ? -1 : initialHardwareTag;

    commandStreamReceiver->setTagAllocation(pTagAllocation);

    auto pDevice = &outDevice;

    if (!pDevice->osTime) {
        pDevice->osTime = OSTime::create(commandStreamReceiver->getOSInterface());
    }
    pDevice->driverInfo.reset(DriverInfo::create(commandStreamReceiver->getOSInterface()));
    pDevice->memoryManager = outDevice.memoryManager;
    pDevice->tagAddress = pTagMemory;

    pDevice->initializeCaps();
    pDevice->tagAllocation = pTagAllocation;

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

    outDevice.memoryManager->setForce32BitAllocations(pDevice->getDeviceInfo().force32BitAddressess);
    outDevice.memoryManager->device = pDevice;

    if (pDevice->preemptionMode == PreemptionMode::MidThread || pDevice->isSourceLevelDebuggerActive()) {
        size_t requiredSize = pHwInfo->capabilityTable.requiredPreemptionSurfaceSize;
        size_t alignment = 256 * MemoryConstants::kiloByte;
        bool uncacheable = pDevice->getWaTable()->waCSRUncachable;
        pDevice->preemptionAllocation = outDevice.memoryManager->allocateGraphicsMemory(requiredSize, alignment, false, uncacheable);
        if (!pDevice->preemptionAllocation) {
            return false;
        }
        commandStreamReceiver->setPreemptionCsrAllocation(pDevice->preemptionAllocation);
        auto sipType = SipKernel::getSipKernelType(pHwInfo->pPlatform->eRenderCoreFamily, pDevice->isSourceLevelDebuggerActive());
        initSipKernel(sipType, *pDevice);
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
        this->slmWindowStartAddress = memoryManager->allocateSystemMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
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

bool Device::isSimulation() {
    bool simulation = hwInfo.capabilityTable.isSimulation(hwInfo.pPlatform->usDeviceID);
    if (commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
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
void Device::connectToExecutionEnvironment(ExecutionEnvironment *executionEnvironment) {
    executionEnvironment->incRefInternal();
    this->executionEnvironment = executionEnvironment;
}
} // namespace OCLRT
