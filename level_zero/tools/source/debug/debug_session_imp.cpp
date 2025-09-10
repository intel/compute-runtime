/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session_imp.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/zet_intel_gpu_debug.h"

namespace L0 {

DebugSession::DebugSession(const zet_debug_config_t &config, Device *device) : connectedDevice(device), config(config) {
}

const NEO::TopologyMap &DebugSession::getTopologyMap() {
    return connectedDevice->getOsInterface()->getDriverModel()->getTopologyMap();
};

void DebugSession::createEuThreads() {
    if (connectedDevice) {

        bool isSubDevice = connectedDevice->getNEODevice()->isSubDevice();

        auto &hwInfo = connectedDevice->getHwInfo();
        const uint32_t numSubslicesPerSlice = std::max(hwInfo.gtSystemInfo.MaxSubSlicesSupported, hwInfo.gtSystemInfo.MaxDualSubSlicesSupported) / hwInfo.gtSystemInfo.MaxSlicesSupported;
        const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
        const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu;
        uint32_t subDeviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
        UNRECOVERABLE_IF(isSubDevice && subDeviceCount > 1);

        for (uint32_t tileIndex = 0; tileIndex < subDeviceCount; tileIndex++) {

            if (isSubDevice || subDeviceCount == 1) {
                tileIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));
            }

            for (uint32_t sliceID = 0; sliceID < NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo); sliceID++) {
                for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
                    for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {

                        for (uint32_t threadID = 0; threadID < numThreadsPerEu; threadID++) {

                            EuThread::ThreadId thread = {tileIndex, sliceID, subsliceID, euID, threadID};

                            allThreads[uint64_t(thread)] = std::make_unique<EuThread>(thread);
                        }
                    }
                }
            }

            if (isSubDevice || subDeviceCount == 1) {
                break;
            }
        }
    }
}

uint32_t DebugSession::getDeviceIndexFromApiThread(ze_device_thread_t thread) {
    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();
    uint32_t deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
    const auto &topologyMap = getTopologyMap();

    if (connectedDevice->getNEODevice()->isSubDevice()) {
        return deviceIndex;
    }

    if (deviceCount > 1) {

        if (thread.slice == UINT32_MAX) {
            deviceIndex = UINT32_MAX;
        } else {
            uint32_t sliceId = thread.slice;
            for (uint32_t i = 0; i < topologyMap.size(); i++) {
                if (deviceBitfield.test(i)) {
                    if (sliceId < topologyMap.at(i).sliceIndices.size()) {
                        deviceIndex = i;
                    }
                    sliceId = sliceId - static_cast<uint32_t>(topologyMap.at(i).sliceIndices.size());
                }
            }
        }
    }

    return deviceIndex;
}

ze_device_thread_t DebugSession::convertToPhysicalWithinDevice(ze_device_thread_t thread, uint32_t deviceIndex) {
    auto deviceImp = static_cast<DeviceImp *>(connectedDevice);
    const auto &topologyMap = getTopologyMap();

    // set slice for single slice config to allow subslice remapping
    auto mapping = topologyMap.find(deviceIndex);
    if (thread.slice == UINT32_MAX && mapping != topologyMap.end() && mapping->second.sliceIndices.size() == 1) {
        thread.slice = 0;
    }

    if (thread.slice != UINT32_MAX) {
        if (thread.subslice != UINT32_MAX) {
            deviceImp->toPhysicalSliceId(topologyMap, thread.slice, thread.subslice, deviceIndex);
        } else {
            uint32_t dummy = 0;
            deviceImp->toPhysicalSliceId(topologyMap, thread.slice, dummy, deviceIndex);
        }
    }

    return thread;
}

EuThread::ThreadId DebugSession::convertToThreadId(ze_device_thread_t thread) {
    auto deviceImp = static_cast<DeviceImp *>(connectedDevice);
    UNRECOVERABLE_IF(!DebugSession::isSingleThread(thread));

    uint32_t deviceIndex = 0;
    deviceImp->toPhysicalSliceId(getTopologyMap(), thread.slice, thread.subslice, deviceIndex);

    EuThread::ThreadId threadId(deviceIndex, thread.slice, thread.subslice, thread.eu, thread.thread);
    return threadId;
}

ze_device_thread_t DebugSession::convertToApi(EuThread::ThreadId threadId) {
    auto deviceImp = static_cast<DeviceImp *>(connectedDevice);

    ze_device_thread_t thread = {static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)};
    deviceImp->toApiSliceId(getTopologyMap(), thread.slice, thread.subslice, threadId.tileIndex);

    return thread;
}

std::vector<EuThread::ThreadId> DebugSession::getSingleThreadsForDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread, const NEO::HardwareInfo &hwInfo) {

    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu;

    UNRECOVERABLE_IF(numThreadsPerEu > 16);

    std::vector<EuThread::ThreadId> threads;

    const uint32_t slice = physicalThread.slice;
    const uint32_t subslice = physicalThread.subslice;
    const uint32_t eu = physicalThread.eu;
    const uint32_t thread = physicalThread.thread;

    for (uint32_t sliceID = 0; sliceID < NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo); sliceID++) {
        if (hwInfo.gtSystemInfo.IsDynamicallyPopulated && !hwInfo.gtSystemInfo.SliceInfo[sliceID].Enabled) {
            continue;
        }

        if (slice != UINT32_MAX) {
            sliceID = slice;
        }

        for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
            if (subslice != UINT32_MAX) {
                subsliceID = subslice;
            }

            for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {
                if (eu != UINT32_MAX) {
                    euID = eu;
                }

                for (uint32_t threadID = 0; threadID < numThreadsPerEu; threadID++) {
                    if (thread != UINT32_MAX) {
                        threadID = thread;
                    }
                    threads.push_back({deviceIndex, sliceID, subsliceID, euID, threadID});

                    if (thread != UINT32_MAX) {
                        break;
                    }
                }

                if (eu != UINT32_MAX) {
                    break;
                }
            }
            if (subslice != UINT32_MAX) {
                break;
            }
        }
        if (slice != UINT32_MAX) {
            break;
        }
    }

    return threads;
}

bool DebugSession::areRequestedThreadsStopped(ze_device_thread_t thread) {
    auto &hwInfo = connectedDevice->getHwInfo();
    bool requestedThreadsStopped = true;

    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
    uint32_t deviceIndex = getDeviceIndexFromApiThread(thread);

    auto areAllThreadsStopped = [this, &hwInfo](uint32_t deviceIndex, const ze_device_thread_t &thread) -> bool {
        auto physicalThread = convertToPhysicalWithinDevice(thread, deviceIndex);
        auto singleThreads = getSingleThreadsForDevice(deviceIndex, physicalThread, hwInfo);

        for (auto &threadId : singleThreads) {

            if (allThreads[threadId]->isReportedAsStopped()) {
                continue;
            }
            return false;
        }
        return true;
    };

    if (deviceIndex != UINT32_MAX) {
        return areAllThreadsStopped(deviceIndex, thread);
    }

    for (uint32_t i = 0; i < deviceCount; i++) {

        if (areAllThreadsStopped(i, thread) == false) {
            return false;
        }
    }

    return requestedThreadsStopped;
}

ze_result_t DebugSession::sanityMemAccessThreadCheck(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc) {
    if (DebugSession::isThreadAll(thread)) {
        if (desc->type != ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        } else {
            return ZE_RESULT_SUCCESS;
        }
    } else if (DebugSession::isSingleThread(thread)) {
        if (!areRequestedThreadsStopped(thread)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else {
            return ZE_RESULT_SUCCESS;
        }
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

void DebugSession::fillDevicesFromThread(ze_device_thread_t thread, std::vector<uint8_t> &devices) {
    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
    UNRECOVERABLE_IF(devices.size() < deviceCount);

    uint32_t deviceIndex = getDeviceIndexFromApiThread(thread);
    bool singleDevice = (thread.slice != UINT32_MAX && deviceCount > 1) || deviceCount == 1;

    if (singleDevice) {
        devices[deviceIndex] = 1;
    } else {
        for (uint32_t i = 0; i < deviceCount; i++) {
            devices[i] = 1;
        }
    }
}

bool DebugSession::isBindlessSystemRoutine() {
    if (debugArea.reserved1 &= 1) {
        return true;
    }
    return false;
}

size_t DebugSession::getPerThreadScratchOffset(size_t ptss, EuThread::ThreadId threadId) {
    auto &hwInfo = connectedDevice->getHwInfo();
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = hwInfo.gtSystemInfo.NumThreadsPerEu;

    const auto &productHelper = connectedDevice->getProductHelper();
    uint32_t threadEuRatio = productHelper.getThreadEuRatioForScratch(hwInfo);
    uint32_t multiplyFactor = 1;
    if (threadEuRatio / numThreadsPerEu > 1) {
        multiplyFactor = threadEuRatio / numThreadsPerEu;
    }

    auto threadOffset = (((threadId.slice * numSubslicesPerSlice + threadId.subslice) * numEuPerSubslice + threadId.eu) * numThreadsPerEu * multiplyFactor + threadId.thread) * ptss;
    return threadOffset;
}

void DebugSession::printBitmask(uint8_t *bitmask, size_t bitmaskSize) {
    if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) {

        DEBUG_BREAK_IF(bitmaskSize % sizeof(uint64_t) != 0);

        PRINT_DEBUGGER_LOG(stdout, "\nINFO: Bitmask: ", "");

        for (size_t i = 0; i < bitmaskSize / sizeof(uint64_t); i++) {
            uint64_t bitmask64 = 0;
            memcpy_s(&bitmask64, sizeof(uint64_t), &bitmask[i * sizeof(uint64_t)], sizeof(uint64_t));
            PRINT_DEBUGGER_LOG(stdout, "\n [%lu] = %#018" PRIx64, static_cast<uint64_t>(i), bitmask64);
        }
    }
}

DebugSession *DebugSessionImp::attachTileDebugSession(Device *device) {
    std::unique_lock<std::mutex> lock(asyncThreadMutex);

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(device->getNEODevice()->getDeviceBitfield().to_ulong()));

    auto &[tileSession, attached] = tileSessions[subDeviceIndex];
    if (attached) {
        return nullptr;
    }

    tileSessions[subDeviceIndex].first->attachTile();
    attached = true;

    PRINT_DEBUGGER_INFO_LOG("TileDebugSession attached, deviceIndex = %lu\n", subDeviceIndex);
    return tileSession;
}

void DebugSessionImp::detachTileDebugSession(DebugSession *tileSession) {

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(tileSession->getConnectedDevice()->getNEODevice()->getDeviceBitfield().to_ulong()));

    tileSessions[subDeviceIndex].second = false;
    tileSessions[subDeviceIndex].first->detachTile();
    cleanRootSessionAfterDetach(subDeviceIndex);

    PRINT_DEBUGGER_INFO_LOG("TileDebugSession detached, deviceIndex = %lu\n", subDeviceIndex);
}

bool DebugSessionImp::areAllTileDebugSessionDetached() {
    for (const auto &session : tileSessions) {
        if (session.second == true) {
            return false;
        }
    }
    return true;
}

ze_result_t DebugSessionImp::interrupt(ze_device_thread_t thread) {
    if (areRequestedThreadsStopped(thread)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    {
        std::unique_lock<std::mutex> lock(interruptMutex);

        for (auto &previousThread : pendingInterrupts) {
            if (areThreadsEqual(thread, previousThread.first)) {
                return ZE_RESULT_NOT_READY;
            }
        }

        interruptRequests.push(thread);
    }

    return ZE_RESULT_SUCCESS;
}

uint32_t DebugSessionImp::readSipMemory(void *userArg, uint32_t offset, uint32_t size, void *destination) {
    struct SipMemoryAccessArgs *args = reinterpret_cast<struct SipMemoryAccessArgs *>(userArg);
    if (args->debugSession->readGpuMemory(args->contextHandle, static_cast<char *>(destination), size, offset + args->gpuVa) != ZE_RESULT_SUCCESS)
        return 0;
    return size;
}

uint32_t DebugSessionImp::writeSipMemory(void *userArg, uint32_t offset, uint32_t size, void *source) {
    struct SipMemoryAccessArgs *args = reinterpret_cast<struct SipMemoryAccessArgs *>(userArg);
    if (args->debugSession->writeGpuMemory(args->contextHandle, static_cast<const char *>(source), size, offset + args->gpuVa) != ZE_RESULT_SUCCESS)
        return 0;
    return size;
}

DebugSessionImp::Error DebugSessionImp::resumeThreadsWithinDevice(uint32_t deviceIndex, ze_device_thread_t apiThread) {
    auto &hwInfo = connectedDevice->getHwInfo();
    bool allThreadsRunning = true;
    auto physicalThread = convertToPhysicalWithinDevice(apiThread, deviceIndex);
    auto singleThreads = getSingleThreadsForDevice(deviceIndex, physicalThread, hwInfo);
    Error retVal = Error::unknown;
    uint64_t memoryHandle = EuThread::invalidHandle;

    std::vector<ze_device_thread_t> resumeThreads;
    std::vector<EuThread::ThreadId> resumeThreadIds;
    for (auto &threadId : singleThreads) {
        if (allThreads[threadId]->isRunning()) {
            continue;
        }
        if (!allThreads[threadId]->isReportedAsStopped()) {
            continue;
        }
        allThreadsRunning = false;
        resumeThreads.emplace_back(ze_device_thread_t{static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)});
        resumeThreadIds.push_back(threadId);
    }

    if (allThreadsRunning) {
        return Error::threadsRunning;
    }

    std::unique_lock<std::mutex> lock(threadStateMutex);
    memoryHandle = allThreads[resumeThreadIds[0]]->getMemoryHandle();

    [[maybe_unused]] auto sipCommandResult = writeResumeCommand(resumeThreadIds);
    DEBUG_BREAK_IF(sipCommandResult != true);

    auto result = resumeImp(resumeThreadIds, deviceIndex);

    // For resume(ALL) and multiple threads to resume - read whole state save area
    // to avoid multiple calls to KMD
    if (resumeThreadIds.size() > 1 && isThreadAll(apiThread)) {

        auto gpuVa = getContextStateSaveAreaGpuVa(memoryHandle);
        auto stateSaveAreaSize = getContextStateSaveAreaSize(memoryHandle);

        std::unique_ptr<char[]> stateSaveArea = nullptr;
        auto stateSaveReadResult = ZE_RESULT_ERROR_UNKNOWN;

        if (gpuVa != 0 && stateSaveAreaSize != 0) {
            stateSaveArea = std::make_unique<char[]>(stateSaveAreaSize);
            stateSaveReadResult = readGpuMemory(memoryHandle, stateSaveArea.get(), stateSaveAreaSize, gpuVa);
        } else {
            DEBUG_BREAK_IF(true);
        }

        for (auto &threadID : resumeThreadIds) {
            if (stateSaveReadResult == ZE_RESULT_SUCCESS) {
                while (checkThreadIsResumed(threadID, stateSaveArea.get()) == false) {
                    // read state save area again to update sr counters
                    stateSaveReadResult = readGpuMemory(allThreads[threadID]->getMemoryHandle(), stateSaveArea.get(), stateSaveAreaSize, gpuVa);
                }
            }
            allThreads[threadID]->resumeThread();
        }

    } else {

        for (auto &threadID : resumeThreadIds) {
            while (checkThreadIsResumed(threadID) == false)
                ;

            allThreads[threadID]->resumeThread();
        }
    }
    checkStoppedThreadsAndGenerateEvents(resumeThreadIds, memoryHandle, deviceIndex);

    if (sipCommandResult && result == ZE_RESULT_SUCCESS) {
        retVal = Error::success;
    }

    return retVal;
}

void DebugSessionImp::applyResumeWa(uint8_t *bitmask, size_t bitmaskSize) {

    UNRECOVERABLE_IF(bitmaskSize % 8 != 0);

    auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (l0GfxCoreHelper.isResumeWARequired()) {

        uint32_t *dwordBitmask = reinterpret_cast<uint32_t *>(bitmask);
        for (uint32_t i = 0; i < bitmaskSize / sizeof(uint32_t) - 1; i = i + 2) {
            dwordBitmask[i] = dwordBitmask[i] | dwordBitmask[i + 1];
            dwordBitmask[i + 1] = dwordBitmask[i] | dwordBitmask[i + 1];
        }
    }
    return;
}

bool DebugSessionImp::writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    bool success = true;

    if (stateSaveAreaHeader->versionHeader.version.major < 2u) {
        auto &hwInfo = connectedDevice->getHwInfo();
        auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

        if (l0GfxCoreHelper.isResumeWARequired()) {
            constexpr uint32_t sipResumeValue = 0x40000000;

            bool isBindlessSip = (debugArea.reserved1 == 1);
            auto registerType = ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU;
            uint32_t dword = 1;

            if (!isBindlessSip) {
                registerType = ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU;
                dword = 4;
            }

            const auto regSize = std::max(getRegisterSize(registerType), hwInfo.capabilityTable.grfSize);
            auto reg = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));

            for (auto &threadID : threadIds) {
                memset(reg.get(), 0, regSize);

                if (readRegistersImp(threadID, registerType, 0, 1, reg.get()) != ZE_RESULT_SUCCESS) {
                    success = false;
                } else {
                    reg[dword] |= sipResumeValue;
                    if (writeRegistersImp(threadID, registerType, 0, 1, reg.get()) != ZE_RESULT_SUCCESS) {
                        success = false;
                    }
                }
            }
        }
    } else // >= 2u
    {
        SIP::sip_command resumeCommand = {0};
        resumeCommand.command = static_cast<uint32_t>(NEO::SipKernel::Command::resume);

        for (auto &threadID : threadIds) {
            ze_result_t result = cmdRegisterAccessHelper(threadID, resumeCommand, true);
            if (result != ZE_RESULT_SUCCESS) {
                success = false;
            }
        }
    }
    return success;
}

bool DebugSessionImp::checkThreadIsResumed(const EuThread::ThreadId &threadID) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    bool resumed = true;

    if (stateSaveAreaHeader->versionHeader.version.major >= 2u) {
        SIP::sr_ident srMagic = {{0}};
        const auto thread = allThreads[threadID].get();

        if (!readSystemRoutineIdent(thread, thread->getMemoryHandle(), srMagic)) {
            return resumed;
        }

        PRINT_DEBUGGER_THREAD_LOG("checkThreadIsResumed - Read counter for thread %s, counter == %d\n", EuThread::toString(threadID).c_str(), (int)srMagic.count);

        // Counter greater than last one means thread was resumed
        if (srMagic.count == thread->getLastCounter()) {
            resumed = false;
        }
    }
    return resumed;
}

bool DebugSessionImp::checkThreadIsResumed(const EuThread::ThreadId &threadID, const void *stateSaveArea) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    bool resumed = true;

    if (stateSaveAreaHeader->versionHeader.version.major >= 2u) {
        SIP::sr_ident srMagic = {{0}};
        const auto thread = allThreads[threadID].get();

        if (!readSystemRoutineIdentFromMemory(thread, stateSaveArea, srMagic)) {
            return resumed;
        }

        PRINT_DEBUGGER_THREAD_LOG("checkThreadIsResumed - Read counter for thread %s, counter == %d\n", EuThread::toString(threadID).c_str(), (int)srMagic.count);

        // Counter greater than last one means thread was resumed
        if (srMagic.count == thread->getLastCounter()) {
            resumed = false;
        }
    }
    return resumed;
}

ze_result_t DebugSessionImp::resume(ze_device_thread_t thread) {

    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
    bool singleDevice = (thread.slice != UINT32_MAX && deviceCount > 1) || deviceCount == 1;
    ze_result_t retVal = ZE_RESULT_SUCCESS;

    if (singleDevice) {
        uint32_t deviceIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));

        if (connectedDevice->getNEODevice()->isSubDevice()) {
            deviceIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));
        } else if (thread.slice != UINT32_MAX) {
            deviceIndex = getDeviceIndexFromApiThread(thread);
        }

        auto result = resumeThreadsWithinDevice(deviceIndex, thread);

        if (result == Error::threadsRunning) {
            retVal = ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else if (result != Error::success) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    } else {
        bool allThreadsRunning = true;

        for (uint32_t deviceId = 0; deviceId < deviceCount; deviceId++) {
            auto result = resumeThreadsWithinDevice(deviceId, thread);

            if (result == Error::threadsRunning) {
                continue;
            } else if (result != Error::success) {
                retVal = ZE_RESULT_ERROR_UNKNOWN;
            }
            allThreadsRunning = false;
        }

        if (allThreadsRunning) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    return retVal;
}

void DebugSessionImp::sendInterrupts() {

    if (interruptSent) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(interruptMutex);

        while (interruptRequests.size() > 0) {
            auto thread = interruptRequests.front();
            pendingInterrupts.push_back(std::pair<ze_device_thread_t, bool>(thread, false));
            interruptRequests.pop();
        }
    }

    if (pendingInterrupts.size() == 0) {
        return;
    }

    expectedAttentionEvents = 0;
    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());

    if (deviceCount == 1) {
        uint32_t deviceIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));

        ze_result_t result;
        {
            std::unique_lock<std::mutex> lock(threadStateMutex);
            result = interruptImp(deviceIndex);
        }

        if (result == ZE_RESULT_SUCCESS) {
            interruptTime = std::chrono::high_resolution_clock::now();
            interruptSent = true;
        } else {
            zet_debug_event_t debugEvent = {};
            debugEvent.type = ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE;

            for (auto &request : pendingInterrupts) {
                debugEvent.info.thread.thread = request.first;
                enqueueApiEvent(debugEvent);
            }

            {
                std::unique_lock<std::mutex> lock(interruptMutex);
                pendingInterrupts.clear();
            }
        }
    } else {
        std::vector<uint8_t> devices(deviceCount);

        for (auto &request : pendingInterrupts) {
            auto thread = request.first;
            fillDevicesFromThread(thread, devices);
        }

        std::vector<ze_result_t> results(deviceCount);

        for (uint32_t i = 0; i < deviceCount; i++) {
            if (devices[i]) {
                std::unique_lock<std::mutex> lock(threadStateMutex);
                results[i] = interruptImp(i);
                if (results[i] == ZE_RESULT_SUCCESS) {
                    expectedAttentionEvents++;
                }
            } else {
                results[i] = ZE_RESULT_SUCCESS;
            }
        }

        const bool allFailed = std::all_of(results.begin(), results.end(),
                                           [](const auto &result) { return result != ZE_RESULT_SUCCESS; });

        PRINT_DEBUGGER_INFO_LOG("Successful interrupt requests = %u \n", expectedAttentionEvents);

        if (allFailed) {
            zet_debug_event_t debugEvent = {};
            debugEvent.type = ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE;

            for (auto &request : pendingInterrupts) {
                debugEvent.info.thread.thread = request.first;
                enqueueApiEvent(debugEvent);
            }

            expectedAttentionEvents = 0;

            {
                std::unique_lock<std::mutex> lock(interruptMutex);
                pendingInterrupts.clear();
            }
        } else {
            interruptTime = std::chrono::high_resolution_clock::now();
            interruptSent = true;
        }
    }
}

size_t DebugSessionImp::calculateSrMagicOffset(const NEO::StateSaveAreaHeader *stateSaveAreaHeader, EuThread *thread) {
    auto threadSlotOffset = calculateThreadSlotOffset(thread->getThreadId());
    size_t srMagicOffset = 0;
    if (stateSaveAreaHeader->versionHeader.version.major == 3) {
        srMagicOffset = threadSlotOffset + stateSaveAreaHeader->regHeaderV3.sr_magic_offset;
    } else if (stateSaveAreaHeader->versionHeader.version.major < 3) {
        srMagicOffset = threadSlotOffset + stateSaveAreaHeader->regHeader.sr_magic_offset;
    } else {
        PRINT_DEBUGGER_ERROR_LOG("%s: Unsupported version of State Save Area Header\n", __func__);
        DEBUG_BREAK_IF(true);
    }
    return srMagicOffset;
}

bool DebugSessionImp::readSystemRoutineIdent(EuThread *thread, uint64_t memoryHandle, SIP::sr_ident &srIdent) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    if (!stateSaveAreaHeader) {
        return false;
    }

    auto gpuVa = getContextStateSaveAreaGpuVa(memoryHandle);
    if (gpuVa == 0) {
        return false;
    }

    auto srMagicOffset = calculateSrMagicOffset(stateSaveAreaHeader, thread);

    if (ZE_RESULT_SUCCESS != readGpuMemory(memoryHandle, reinterpret_cast<char *>(&srIdent), sizeof(srIdent), gpuVa + srMagicOffset)) {
        return false;
    }

    if (0 != strcmp(srIdent.magic, "srmagic")) {
        PRINT_DEBUGGER_ERROR_LOG("readSystemRoutineIdent - Failed to read srMagic for thread %s\n", EuThread::toString(thread->getThreadId()).c_str());
        return false;
    }

    return true;
}
bool DebugSessionImp::readSystemRoutineIdentFromMemory(EuThread *thread, const void *stateSaveArea, SIP::sr_ident &srIdent) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();

    auto srMagicOffset = calculateSrMagicOffset(stateSaveAreaHeader, thread);
    auto threadSlot = ptrOffset(stateSaveArea, srMagicOffset);
    memcpy_s(&srIdent, sizeof(SIP::sr_ident), threadSlot, sizeof(SIP::sr_ident));

    PRINT_DEBUGGER_INFO_LOG("readSystemRoutineIdentFromMemory - srMagicOffset %lu for thread %s\n", srMagicOffset, EuThread::toString(thread->getThreadId()).c_str());
    if (0 != strcmp(srIdent.magic, "srmagic")) {
        PRINT_DEBUGGER_ERROR_LOG("readSystemRoutineIdentFromMemory - Failed to read srMagic for thread %s\n", EuThread::toString(thread->getThreadId()).c_str());
        return false;
    }

    return true;
}

void DebugSessionImp::addThreadToNewlyStoppedFromRaisedAttention(EuThread::ThreadId threadId, uint64_t memoryHandle, const void *stateSaveArea) {

    SIP::sr_ident srMagic = {{0}};
    srMagic.count = 0;

    DEBUG_BREAK_IF(stateSaveArea == nullptr);

    bool wasStopped = false;
    {
        wasStopped = allThreads[threadId]->isStopped();

        if (!wasStopped) {

            if (!readSystemRoutineIdentFromMemory(allThreads[threadId].get(), stateSaveArea, srMagic)) {
                PRINT_DEBUGGER_ERROR_LOG("Failed to read SR IDENT\n", "");
                return;
            } else {
                PRINT_DEBUGGER_INFO_LOG("SIP version == %d.%d.%d\n", (int)srMagic.version.major, (int)srMagic.version.minor, (int)srMagic.version.patch);
            }

            if (!allThreads[threadId]->verifyStopped(srMagic.count)) {
                return;
            }

            allThreads[threadId]->stopThread(memoryHandle);
        }
    }

    if (!wasStopped) {
        newlyStoppedThreads.push_back(threadId);
    }
}

void DebugSessionImp::generateEventsAndResumeStoppedThreads() {

    if (interruptSent && !triggerEvents) {
        auto timeDiff = getTimeDifferenceMilliseconds(interruptTime);
        if (timeDiff > interruptTimeout) {
            triggerEvents = true;
            interruptTime = std::chrono::high_resolution_clock::now();
        }
    }

    if (triggerEvents) {
        std::vector<EuThread::ThreadId> resumeThreads;
        std::vector<EuThread::ThreadId> stoppedThreadsToReport;
        std::vector<EuThread::ThreadId> interruptedThreads;

        fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreadsToReport, interruptedThreads);

        resumeAccidentallyStoppedThreads(resumeThreads);
        generateEventsForPendingInterrupts();
        generateEventsForStoppedThreads(stoppedThreadsToReport);

        interruptSent = false;
        triggerEvents = false;
    }
}

bool DebugSessionImp::isForceExceptionOrForceExternalHaltOnlyExceptionReason(uint32_t *cr0) {
    const uint32_t cr0ExceptionBitmask = 0xFC810000;
    const uint32_t cr0ForcedExcpetionBitmask = 0x44000000;

    return (((cr0[1] & cr0ExceptionBitmask) & (~cr0ForcedExcpetionBitmask)) == 0);
}

bool DebugSessionImp::isAIPequalToThreadStartIP(uint32_t *cr0, uint32_t *dbg0) {
    if (cr0[2] == dbg0[0]) {
        return true;
    } else {
        return false;
    }
}

void DebugSessionImp::fillResumeAndStoppedThreadsFromNewlyStopped(std::vector<EuThread::ThreadId> &resumeThreads, std::vector<EuThread::ThreadId> &stoppedThreadsToReport, std::vector<EuThread::ThreadId> &interruptedThreads) {

    if (newlyStoppedThreads.empty()) {
        PRINT_DEBUGGER_INFO_LOG("%s", "No newly stopped threads found. Returning");
        return;
    }
    const auto regSize = std::max(getRegisterSize(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), 64u);
    auto reg = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));

    for (auto &newlyStopped : newlyStoppedThreads) {
        if (allThreads[newlyStopped]->isStopped()) {
            memset(reg.get(), 0, regSize);
            readRegistersImp(newlyStopped, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, reg.get());

            if (allThreads[newlyStopped]->getPageFault()) {
                uint32_t dbgreg[64u] = {0};
                readRegistersImp(newlyStopped, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, 0, 1, dbgreg);
                if (isAIPequalToThreadStartIP(reg.get(), dbgreg)) {
                    PRINT_DEBUGGER_THREAD_LOG("Thread %s with PF AIP is equal to StartIP. Filtering out\n", allThreads[newlyStopped]->toString().c_str());
                } else {
                    const uint32_t cr0PFBit16 = 0x10000;
                    reg[1] = reg[1] | cr0PFBit16;
                    writeRegistersImp(newlyStopped, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, reg.get());
                }
            }
            if (isForceExceptionOrForceExternalHaltOnlyExceptionReason(reg.get())) {
                bool threadWasInterrupted = false;

                for (auto &request : pendingInterrupts) {
                    ze_device_thread_t apiThread = convertToApi(newlyStopped);

                    auto isInterrupted = checkSingleThreadWithinDeviceThread(apiThread, request.first);

                    if (isInterrupted) {
                        // mark pending interrupt as completed successfully
                        request.second = true;
                        threadWasInterrupted = true;
                        allThreads[newlyStopped]->reportAsStopped();
                    }
                }

                if (threadWasInterrupted) {
                    interruptedThreads.push_back(newlyStopped);
                } else {
                    PRINT_DEBUGGER_THREAD_LOG("RESUME accidentally stopped thread = %s\n", allThreads[newlyStopped]->toString().c_str());
                    resumeThreads.push_back(newlyStopped);
                }
            } else {
                PRINT_DEBUGGER_THREAD_LOG("Newly stopped thread = %s, exception bits = %#010" PRIx32 "\n", allThreads[newlyStopped]->toString().c_str(), reg[1]);
                allThreads[newlyStopped]->setPageFault(false);
                stoppedThreadsToReport.push_back(newlyStopped);
            }
        }
    }

    newlyStoppedThreads.clear();
}

void DebugSessionImp::generateEventsForPendingInterrupts() {
    zet_debug_event_t debugEvent = {};

    for (auto &request : pendingInterrupts) {
        if (request.second == true) {
            debugEvent.type = ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED;
            debugEvent.info.thread.thread = request.first;
            enqueueApiEvent(debugEvent);
        } else {
            debugEvent.type = ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE;
            debugEvent.info.thread.thread = request.first;
            enqueueApiEvent(debugEvent);
        }
    }

    {
        std::unique_lock<std::mutex> lock(interruptMutex);
        pendingInterrupts.clear();
    }
}

void DebugSessionImp::resumeAccidentallyStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds) {
    std::vector<EuThread::ThreadId> threadIdsPerDevice[4];

    for (auto &threadID : threadIds) {
        uint32_t deviceIndex = static_cast<uint32_t>(threadID.tileIndex);

        UNRECOVERABLE_IF((connectedDevice->getNEODevice()->getNumSubDevices() > 0) &&
                         (deviceIndex >= connectedDevice->getNEODevice()->getNumSubDevices()));

        threadIdsPerDevice[deviceIndex].push_back(threadID);
    }

    for (uint32_t i = 0; i < 4; i++) {
        std::unique_lock<std::mutex> lock(threadStateMutex);

        if (threadIdsPerDevice[i].size() > 0) {
            [[maybe_unused]] auto writeSipCommandResult = writeResumeCommand(threadIdsPerDevice[i]);
            DEBUG_BREAK_IF(writeSipCommandResult != true);
            resumeImp(threadIdsPerDevice[i], i);
        }

        for (auto &threadID : threadIdsPerDevice[i]) {
            while (checkThreadIsResumed(threadID) == false)
                ;

            allThreads[threadID]->resumeThread();
        }
    }
}

void DebugSessionImp::generateEventsForStoppedThreads(const std::vector<EuThread::ThreadId> &threadIds) {
    zet_debug_event_t debugEvent = {};
    for (auto &threadID : threadIds) {
        ze_device_thread_t thread = convertToApi(threadID);

        debugEvent.type = ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED;
        debugEvent.info.thread.thread = thread;
        enqueueApiEvent(debugEvent);
    }
}

ze_result_t DebugSessionImp::readEvent(uint64_t timeout, zet_debug_event_t *outputEvent) {

    if (outputEvent) {
        outputEvent->type = ZET_DEBUG_EVENT_TYPE_INVALID;
        outputEvent->flags = 0;
    } else {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    do {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);

        if (timeout > 0 && apiEvents.size() == 0) {
            apiEventCondition.wait_for(lock, std::chrono::milliseconds(timeout));
        }

        if (apiEvents.size() > 0) {
            zet_debug_event_t temp = apiEvents.front();

            if (temp.type == ZET_DEBUG_EVENT_TYPE_THREAD_STOPPED) {

                if (isSingleThread(temp.info.thread.thread)) {
                    auto threadID = convertToThreadId(temp.info.thread.thread);
                    allThreads[threadID]->reportAsStopped();
                }
            }
            *outputEvent = temp;
            apiEvents.pop();
            return ZE_RESULT_SUCCESS;
        }
    } while (timeout == UINT64_MAX && asyncThread.threadActive);

    return ZE_RESULT_NOT_READY;
}

void DebugSessionImp::dumpDebugSurfaceToFile(uint64_t vmHandle, uint64_t gpuVa, const std::string &path) {
    auto stateSaveAreaSize = getContextStateSaveAreaSize(vmHandle);
    std::vector<char> data(stateSaveAreaSize);
    auto retVal = readGpuMemory(vmHandle, data.data(), data.size(), gpuVa);
    if (retVal != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Reading context state save area failed, error = %d\n", retVal);
        return;
    }
    writeDataToFile(path.c_str(), std::string_view(data.data(), data.size()));
}

void DebugSessionImp::validateAndSetStateSaveAreaHeader(uint64_t vmHandle, uint64_t gpuVa) {
    const std::string dumpDebugSurfacePath = NEO::debugManager.flags.DumpDebugSurfaceFile.get();
    if (dumpDebugSurfacePath != "unk") {
        dumpDebugSurfaceToFile(vmHandle, gpuVa, dumpDebugSurfacePath);
    }

    auto headerSize = sizeof(NEO::StateSaveAreaHeader);
    std::vector<char> data(headerSize);
    auto retVal = readGpuMemory(vmHandle, data.data(), sizeof(SIP::StateSaveArea), gpuVa);

    if (retVal != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Reading Context State Save Area Version Header failed, error = %d\n", retVal);
        return;
    }

    auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(data.data());
    if (0 == strcmp(pStateSaveArea->versionHeader.magic, "tssarea")) {
        size_t size = pStateSaveArea->versionHeader.size * 8u;
        size_t regHeaderSize = 0;
        if (pStateSaveArea->versionHeader.version.major == 3) {
            DEBUG_BREAK_IF(size != sizeof(NEO::StateSaveAreaHeader));
            regHeaderSize = sizeof(SIP::intelgt_state_save_area_V3);
        } else if (pStateSaveArea->versionHeader.version.major < 3) {
            DEBUG_BREAK_IF(size != sizeof(NEO::StateSaveAreaHeader::regHeader) + sizeof(NEO::StateSaveAreaHeader::versionHeader));
            regHeaderSize = sizeof(SIP::intelgt_state_save_area);
        } else {
            PRINT_DEBUGGER_ERROR_LOG("Setting Context State Save Area: unsupported version == %d.%d.%d\n", (int)pStateSaveArea->versionHeader.version.major, (int)pStateSaveArea->versionHeader.version.minor, (int)pStateSaveArea->versionHeader.version.patch);
            DEBUG_BREAK_IF(true);
            return;
        }
        auto retVal = readGpuMemory(vmHandle, data.data() + sizeof(SIP::StateSaveArea), regHeaderSize, gpuVa + sizeof(SIP::StateSaveArea));
        if (retVal != ZE_RESULT_SUCCESS) {
            PRINT_DEBUGGER_ERROR_LOG("Reading Context State Save Area Reg Header failed, error = %d\n", retVal);
            return;
        }
        stateSaveAreaHeader.assign(data.begin(), data.begin() + size);
        PRINT_DEBUGGER_INFO_LOG("Context State Save Area : version == %d.%d.%d\n", (int)pStateSaveArea->versionHeader.version.major, (int)pStateSaveArea->versionHeader.version.minor, (int)pStateSaveArea->versionHeader.version.patch);
        slmSipVersionCheck();
    } else {
        PRINT_DEBUGGER_ERROR_LOG("Setting Context State Save Area: failed to match magic numbers\n", "");
    }
}

void DebugSessionImp::slmSipVersionCheck() {

    SIP::version sipVersion = getStateSaveAreaHeader()->versionHeader.version;
    if ((sipVersion.major < minSlmSipVersion.major) ||
        ((sipVersion.major == minSlmSipVersion.major) && (sipVersion.minor < minSlmSipVersion.minor)) ||
        ((sipVersion.major == minSlmSipVersion.major) && (sipVersion.minor == minSlmSipVersion.minor) && (sipVersion.patch < minSlmSipVersion.patch))) {

        sipSupportsSlm = false;
    } else {
        sipSupportsSlm = true;
    }
}

const NEO::StateSaveAreaHeader *DebugSessionImp::getStateSaveAreaHeader() {
    if (stateSaveAreaHeader.empty()) {
        readStateSaveAreaHeader();
    }
    return reinterpret_cast<NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
}

const SIP::regset_desc *DebugSessionImp::getModeFlagsRegsetDesc() {
    static const SIP::regset_desc mode = {0, 1, 32, 4};
    return &mode;
}

const SIP::regset_desc *DebugSessionImp::getDebugScratchRegsetDesc() {
    static const SIP::regset_desc debugScratch = {0, 2, 64, 8};
    return &debugScratch;
}

const SIP::regset_desc *DebugSessionImp::getThreadScratchRegsetDesc() {
    static const SIP::regset_desc threadScratch = {0, 2, 64, 8};
    return &threadScratch;
}

bool DebugSessionImp::isHeaplessMode(const SIP::intelgt_state_save_area_V3 &ssa) {
    return (ssa.sip_flags & SIP::SIP_FLAG_HEAPLESS);
}

const SIP::regset_desc *DebugSessionImp::getSbaRegsetDesc(const NEO::StateSaveAreaHeader &ssah) {

    static const SIP::regset_desc sbaHeapless = {0, 0, 0, 0};
    static const SIP::regset_desc sba = {0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, 64, 8};
    if (ssah.versionHeader.version.major > 3) {
        DEBUG_BREAK_IF(true);
        PRINT_DEBUGGER_ERROR_LOG("Unsupported version of State Save Area Header\n", "");
        return nullptr;
    } else if (ssah.versionHeader.version.major == 3 && isHeaplessMode(ssah.regHeaderV3)) {
        return &sbaHeapless;
    } else {
        return &sba;
    }
}

const SIP::regset_desc *DebugSessionImp::typeToRegsetDesc(uint32_t type) {
    auto pStateSaveAreaHeader = getStateSaveAreaHeader();

    if (pStateSaveAreaHeader == nullptr) {
        DEBUG_BREAK_IF(pStateSaveAreaHeader == nullptr);
        return nullptr;
    }
    if (pStateSaveAreaHeader->versionHeader.version.major == 3) {
        switch (type) {
        case ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.grf;
        case ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.addr;
        case ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.flag;
        case ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.emask;
        case ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.sr;
        case ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.cr;
        case ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.tdr;
        case ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.acc;
        case ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.mme;
        case ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.sp;
        case ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.dbg_reg;
        case ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.fc;
        case ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU:
            return DebugSessionImp::getModeFlagsRegsetDesc();
        case ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU:
            return DebugSessionImp::getDebugScratchRegsetDesc();
        case ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU:
            return DebugSessionImp::getThreadScratchRegsetDesc();
        case ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.scalar;
        case ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeaderV3.msg;
        case ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU: {
            auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*connectedDevice->getNEODevice()).getStateSaveAreaHeader();
            auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
            return DebugSessionImp::getSbaRegsetDesc(*pStateSaveArea);
        }
        default:
            return nullptr;
        }
    } else if (pStateSaveAreaHeader->versionHeader.version.major < 3) {
        switch (type) {
        case ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.grf;
        case ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.addr;
        case ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.flag;
        case ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.emask;
        case ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.sr;
        case ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.cr;
        case ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.tdr;
        case ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.acc;
        case ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.mme;
        case ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.sp;
        case ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.dbg_reg;
        case ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU:
            return &pStateSaveAreaHeader->regHeader.fc;
        case ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU: {
            auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*connectedDevice->getNEODevice()).getStateSaveAreaHeader();
            auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
            return DebugSessionImp::getSbaRegsetDesc(*pStateSaveArea);
        }
        default:
            return nullptr;
        }
    } else {
        PRINT_DEBUGGER_ERROR_LOG("Unsupported version of State Save Area Header\n", "");
        DEBUG_BREAK_IF(true);
        return nullptr;
    }
}

uint32_t DebugSessionImp::getRegisterSize(uint32_t type) {
    auto regset = typeToRegsetDesc(type);
    if (regset) {
        return regset->bytes;
    }
    return 0;
}

uint32_t DebugSessionImp::typeToRegsetFlags(uint32_t type) {
    switch (type) {
    case ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU:
        return ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE;

    case ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU:
        return ZET_DEBUG_REGSET_FLAG_READABLE;

    default:
        return 0;
    }
}

size_t DebugSessionImp::calculateThreadSlotOffset(EuThread::ThreadId threadId) {
    auto pStateSaveAreaHeader = getStateSaveAreaHeader();
    if (pStateSaveAreaHeader->versionHeader.version.major > 3) {
        DEBUG_BREAK_IF(true);
        PRINT_DEBUGGER_ERROR_LOG("Unsupported version of State Save Area Header\n", "");
        return 0;
    } else if (pStateSaveAreaHeader->versionHeader.version.major == 3) {
        return pStateSaveAreaHeader->versionHeader.size * 8 + pStateSaveAreaHeader->regHeaderV3.state_area_offset + ((((threadId.slice * pStateSaveAreaHeader->regHeaderV3.num_subslices_per_slice + threadId.subslice) * pStateSaveAreaHeader->regHeaderV3.num_eus_per_subslice + threadId.eu) * pStateSaveAreaHeader->regHeaderV3.num_threads_per_eu + threadId.thread) * pStateSaveAreaHeader->regHeaderV3.state_save_size);
    } else {
        return pStateSaveAreaHeader->versionHeader.size * 8 + pStateSaveAreaHeader->regHeader.state_area_offset + ((((threadId.slice * pStateSaveAreaHeader->regHeader.num_subslices_per_slice + threadId.subslice) * pStateSaveAreaHeader->regHeader.num_eus_per_subslice + threadId.eu) * pStateSaveAreaHeader->regHeader.num_threads_per_eu + threadId.thread) * pStateSaveAreaHeader->regHeader.state_save_size);
    }
}

size_t DebugSessionImp::calculateRegisterOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start) {
    return regdesc->offset + regdesc->bytes * start;
}

ze_result_t DebugSessionImp::readModeFlags(uint32_t start, uint32_t count, void *pRegisterValues) {
    if (start != 0 || count != 1) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*connectedDevice->getNEODevice()).getStateSaveAreaHeader();
    auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
    const size_t size = 4;
    memcpy_s(pRegisterValues, size, &pStateSaveArea->regHeaderV3.sip_flags, size);
    return ZE_RESULT_SUCCESS;
}
ze_result_t DebugSessionImp::readDebugScratchRegisters(uint32_t start, uint32_t count, void *pRegisterValues) {

    auto debugScratchRegDesc = DebugSessionImp::getDebugScratchRegsetDesc();

    if (start >= debugScratchRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (start + count > debugScratchRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto info = getModuleDebugAreaInfo();

    std::vector<uint64_t> packed;
    packed.push_back(info.gpuVa);
    packed.push_back(info.size);

    size_t size = count * debugScratchRegDesc->bytes;
    memcpy_s(pRegisterValues, size, &packed[start], size);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::readSbaRegisters(EuThread::ThreadId threadId, uint32_t start, uint32_t count, void *pRegisterValues) {

    auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*connectedDevice->getNEODevice()).getStateSaveAreaHeader();
    auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
    auto sbaRegDesc = DebugSessionImp::getSbaRegsetDesc(*pStateSaveArea);

    if (start >= sbaRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (start + count > sbaRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t ret = ZE_RESULT_SUCCESS;

    alignas(64) NEO::SbaTrackedAddresses sbaBuffer{};
    ret = readSbaBuffer(threadId, sbaBuffer);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    const auto &hwInfo = connectedDevice->getHwInfo();
    const auto regSize = std::max(getRegisterSize(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), hwInfo.capabilityTable.grfSize);
    auto r0 = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));

    ret = readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0.get());
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    uint64_t bindingTableBaseAddress = ((r0[4] >> 5) << 5) + sbaBuffer.surfaceStateBaseAddress;
    uint64_t scratchSpaceBaseAddress = 0;

    auto &gfxCoreHelper = connectedDevice->getGfxCoreHelper();
    if (gfxCoreHelper.isScratchSpaceSurfaceStateAccessible()) {
        auto surfaceStateForScratch = ((r0[5] >> 10) << 6);

        if (surfaceStateForScratch > 0) {
            uint64_t renderSurfaceStateGpuVa = surfaceStateForScratch + sbaBuffer.surfaceStateBaseAddress;
            constexpr size_t renderSurfaceStateSize = 64;
            std::vector<char> renderSurfaceState(renderSurfaceStateSize, 0);

            ret = readGpuMemory(allThreads[threadId]->getMemoryHandle(), renderSurfaceState.data(), renderSurfaceStateSize, renderSurfaceStateGpuVa);

            if (ret != ZE_RESULT_SUCCESS) {
                return ret;
            }

            auto scratchSpacePTSize = gfxCoreHelper.getRenderSurfaceStatePitch(renderSurfaceState.data(), connectedDevice->getProductHelper());
            auto threadOffset = getPerThreadScratchOffset(scratchSpacePTSize, threadId);
            auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
            auto scratchAllocationBase = gmmHelper->decanonize(gfxCoreHelper.getRenderSurfaceStateBaseAddress(renderSurfaceState.data()));
            if (scratchAllocationBase != 0) {
                scratchSpaceBaseAddress = threadOffset + scratchAllocationBase;
            }
        }
    } else {
        auto scratchPointer = ((r0[5] >> 10) << 10);
        if (scratchPointer != 0) {
            scratchSpaceBaseAddress = scratchPointer + sbaBuffer.generalStateBaseAddress;
        }
    }

    std::vector<uint64_t> packed;
    packed.push_back(sbaBuffer.generalStateBaseAddress);
    packed.push_back(sbaBuffer.surfaceStateBaseAddress);
    packed.push_back(sbaBuffer.dynamicStateBaseAddress);
    packed.push_back(sbaBuffer.indirectObjectBaseAddress);
    packed.push_back(sbaBuffer.instructionBaseAddress);
    packed.push_back(sbaBuffer.bindlessSurfaceStateBaseAddress);
    packed.push_back(sbaBuffer.bindlessSamplerStateBaseAddress);
    packed.push_back(bindingTableBaseAddress);
    packed.push_back(scratchSpaceBaseAddress);

    PRINT_DEBUGGER_INFO_LOG("Debug session : SBA ssh = %" SCNx64
                            " gsba = %" SCNx64
                            " dsba =  %" SCNx64
                            " ioba =  %" SCNx64
                            " iba =  %" SCNx64
                            " bsurfsba =  %" SCNx64
                            " btba =  %" SCNx64
                            " scrsba =  %" SCNx64 "\n",
                            sbaBuffer.surfaceStateBaseAddress, sbaBuffer.generalStateBaseAddress, sbaBuffer.dynamicStateBaseAddress,
                            sbaBuffer.indirectObjectBaseAddress, sbaBuffer.instructionBaseAddress, sbaBuffer.bindlessSurfaceStateBaseAddress, bindingTableBaseAddress, scratchSpaceBaseAddress);

    size_t size = count * sbaRegDesc->bytes;
    memcpy_s(pRegisterValues, size, &packed[start], size);

    return ZE_RESULT_SUCCESS;
}

void DebugSession::updateGrfRegisterSetProperties(EuThread::ThreadId thread, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    if (pRegisterSetProperties == nullptr) {
        return;
    }

    auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    auto regsetType = l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection();
    const auto regSize = std::max(getRegisterSize(regsetType), 64u);
    auto reg = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));
    memset(reg.get(), 0, regSize);
    readRegistersImp(thread, regsetType, 0, 1, reg.get());
    for (uint32_t i = 0; i < *pCount; i++) {
        if (pRegisterSetProperties[i].type == ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU) {
            pRegisterSetProperties[i].count = l0GfxCoreHelper.getGrfRegisterCount(reg.get());
        }
    }
}

ze_result_t DebugSession::getThreadRegisterSetProperties(ze_device_thread_t thread, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    if (!isSingleThread(thread)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    auto threadId = convertToThreadId(thread);

    if (!allThreads[threadId]->isReportedAsStopped()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto ret = getRegisterSetProperties(this->connectedDevice, pCount, pRegisterSetProperties);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    updateGrfRegisterSetProperties(threadId, pCount, pRegisterSetProperties);
    return ret;
}

ze_result_t DebugSession::getRegisterSetProperties(Device *device, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    if (nullptr == pCount) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (*pCount && !pRegisterSetProperties) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*device->getNEODevice()).getStateSaveAreaHeader();

    if (stateSaveAreaHeader.size() == 0) {
        *pCount = 0;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t totalRegsetNum = 0;
    auto parseRegsetDesc = [&](const SIP::regset_desc &regsetDesc, zet_debug_regset_type_intel_gpu_t regsetType) {
        if (regsetDesc.num) {
            if (totalRegsetNum < *pCount) {
                uint16_t num = (regsetType == ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU) ? 128 : regsetDesc.num;
                zet_debug_regset_properties_t regsetProps = {
                    ZET_STRUCTURE_TYPE_DEBUG_REGSET_PROPERTIES,
                    nullptr,
                    static_cast<uint32_t>(regsetType),
                    0,
                    DebugSessionImp::typeToRegsetFlags(regsetType),
                    0,
                    num,
                    regsetDesc.bits,
                    regsetDesc.bytes,
                };
                pRegisterSetProperties[totalRegsetNum] = regsetProps;
            }
            ++totalRegsetNum;
        }
    };

    auto pStateSaveArea = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    if (pStateSaveArea->versionHeader.version.major == 3) {
        parseRegsetDesc(pStateSaveArea->regHeaderV3.grf, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.addr, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.flag, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.emask, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.sr, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.cr, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.tdr, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.acc, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.mme, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.sp, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU);
        parseRegsetDesc(*DebugSessionImp::getSbaRegsetDesc(*pStateSaveArea), ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.dbg_reg, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.fc, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeaderV3.msg, ZET_DEBUG_REGSET_TYPE_MSG_INTEL_GPU);
        parseRegsetDesc(*DebugSessionImp::getModeFlagsRegsetDesc(), ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU);
        parseRegsetDesc(*DebugSessionImp::getDebugScratchRegsetDesc(), ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU);
        if (DebugSessionImp::isHeaplessMode(pStateSaveArea->regHeaderV3)) {
            parseRegsetDesc(*DebugSessionImp::getThreadScratchRegsetDesc(), ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU);
        }
        parseRegsetDesc(pStateSaveArea->regHeaderV3.scalar, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU);

    } else if (pStateSaveArea->versionHeader.version.major < 3) {
        parseRegsetDesc(pStateSaveArea->regHeader.grf, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.addr, ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.flag, ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.emask, ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.sr, ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.cr, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.tdr, ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.acc, ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.mme, ZET_DEBUG_REGSET_TYPE_MME_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.sp, ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU);
        parseRegsetDesc(*DebugSessionImp::getSbaRegsetDesc(*pStateSaveArea), ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.dbg_reg, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU);
        parseRegsetDesc(pStateSaveArea->regHeader.fc, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU);
    } else {
        PRINT_DEBUGGER_ERROR_LOG("Unsupported version of State Save Area Header\n", "");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (!*pCount || (*pCount > totalRegsetNum)) {
        *pCount = totalRegsetNum;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::registersAccessHelper(const EuThread *thread, const SIP::regset_desc *regdesc,
                                                   uint32_t start, uint32_t count, void *pRegisterValues, bool write) {
    if (start >= regdesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (start + count > regdesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto gpuVa = getContextStateSaveAreaGpuVa(thread->getMemoryHandle());
    if (gpuVa == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto threadSlotOffset = calculateThreadSlotOffset(thread->getThreadId());
    auto startRegOffset = threadSlotOffset + calculateRegisterOffsetInThreadSlot(regdesc, start);

    int ret = 0;
    if (write) {
        ret = writeGpuMemory(thread->getMemoryHandle(), static_cast<const char *>(pRegisterValues), count * regdesc->bytes, gpuVa + startRegOffset);
    } else {
        ret = readGpuMemory(thread->getMemoryHandle(), static_cast<char *>(pRegisterValues), count * regdesc->bytes, gpuVa + startRegOffset);
    }

    return ret == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionImp::cmdRegisterAccessHelper(const EuThread::ThreadId &threadId, SIP::sip_command &command, bool write) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    const SIP::regset_desc *regdesc = nullptr;
    if (stateSaveAreaHeader->versionHeader.version.major == 3) {
        regdesc = &stateSaveAreaHeader->regHeaderV3.cmd;
    } else if (stateSaveAreaHeader->versionHeader.version.major < 3) {
        regdesc = &stateSaveAreaHeader->regHeader.cmd;
    } else {
        PRINT_DEBUGGER_ERROR_LOG("%s: Unsupported version of State Save Area Header\n", __func__);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    PRINT_DEBUGGER_INFO_LOG("Access CMD %d for thread %s\n", command.command, EuThread::toString(threadId).c_str());

    ze_result_t result = registersAccessHelper(allThreads[threadId].get(), regdesc, 0, 1, &command, write);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Failed to access CMD for thread %s\n", EuThread::toString(threadId).c_str());
    }

    return result;
}

ze_result_t DebugSessionImp::readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    if (!isSingleThread(thread)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    auto threadId = convertToThreadId(thread);

    if (!allThreads[threadId]->isReportedAsStopped()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    if (stateSaveAreaHeader == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (type == ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU) {
        return readSbaRegisters(threadId, start, count, pRegisterValues);
    } else if (type == ZET_DEBUG_REGSET_TYPE_MODE_FLAGS_INTEL_GPU) {
        return readModeFlags(start, count, pRegisterValues);
    } else if (type == ZET_DEBUG_REGSET_TYPE_DEBUG_SCRATCH_INTEL_GPU) {
        return readDebugScratchRegisters(start, count, pRegisterValues);
    } else if (type == ZET_DEBUG_REGSET_TYPE_THREAD_SCRATCH_INTEL_GPU) {
        return readThreadScratchRegisters(threadId, start, count, pRegisterValues);
    }

    return readRegistersImp(threadId, type, start, count, pRegisterValues);
}

ze_result_t DebugSessionImp::readRegistersImp(EuThread::ThreadId threadId, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    auto regdesc = typeToRegsetDesc(type);
    if (nullptr == regdesc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return registersAccessHelper(allThreads[threadId].get(), regdesc, start, count, pRegisterValues, false);
}

ze_result_t DebugSessionImp::writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    if (!isSingleThread(thread)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    auto threadId = convertToThreadId(thread);

    if (!allThreads[threadId]->isReportedAsStopped()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    if (stateSaveAreaHeader == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return writeRegistersImp(threadId, type, start, count, pRegisterValues);
}

ze_result_t DebugSessionImp::writeRegistersImp(EuThread::ThreadId threadId, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    auto regdesc = typeToRegsetDesc(type);
    if (nullptr == regdesc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (ZET_DEBUG_REGSET_FLAG_WRITEABLE != ((DebugSessionImp::typeToRegsetFlags(type) & ZET_DEBUG_REGSET_FLAG_WRITEABLE))) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return registersAccessHelper(allThreads[threadId].get(), regdesc, start, count, pRegisterValues, true);
}

bool DebugSessionImp::isValidGpuAddress(const zet_debug_memory_space_desc_t *desc) const {

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
        auto decanonizedAddress = gmmHelper->decanonize(desc->address);
        bool validAddress = gmmHelper->isValidCanonicalGpuAddress(desc->address);

        if (desc->address == decanonizedAddress || validAddress) {
            return true;
        }
    } else if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_SLM) {
        if (desc->address & (1 << slmAddressSpaceTag)) { // IGC sets bit 28 to identify SLM address
            return true;
        }
    }

    return false;
}

ze_result_t DebugSessionImp::validateThreadAndDescForMemoryAccess(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc) {
    if (!isValidGpuAddress(desc)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t status = sanityMemAccessThreadCheck(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::waitForCmdReady(EuThread::ThreadId threadId, uint16_t retryCount) {
    ze_result_t status;
    SIP::sip_command sipCommand = {0};

    for (uint16_t attempts = 0; attempts < retryCount; attempts++) {
        status = cmdRegisterAccessHelper(threadId, sipCommand, false);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        if (sipCommand.command == static_cast<uint32_t>(NEO::SipKernel::Command::ready)) {
            break;
        }
        NEO::sleep(std::chrono::microseconds(100));
    }

    if (sipCommand.command != static_cast<uint32_t>(NEO::SipKernel::Command::ready)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

void DebugSessionImp::getNotStoppedThreads(const std::vector<EuThread::ThreadId> &threadsWithAtt, std::vector<EuThread::ThreadId> &notStoppedThreads) {
    for (const auto &threadId : threadsWithAtt) {

        bool wasStopped = false;

        if (tileSessionsEnabled) {
            wasStopped = tileSessions[threadId.tileIndex].first->allThreads[threadId]->isStopped();
        } else {
            wasStopped = allThreads[threadId]->isStopped();
        }

        if (!wasStopped) {
            notStoppedThreads.push_back(threadId);
        }
    }
}

ze_result_t DebugSessionImp::isValidNode(uint64_t vmHandle, uint64_t gpuVa, SIP::fifo_node &node) {
    constexpr uint32_t failsafeTimeoutMax = 100, failsafeTimeoutWait = 50;
    uint32_t timeCount = 0;
    while (!node.valid && (timeCount < failsafeTimeoutMax)) {
        auto retVal = readGpuMemory(vmHandle, reinterpret_cast<char *>(&node), sizeof(SIP::fifo_node), gpuVa);
        if (retVal != ZE_RESULT_SUCCESS) {
            PRINT_DEBUGGER_ERROR_LOG("Reading FIFO failed, error = %d\n", retVal);
            return retVal;
        }
        NEO::sleep(std::chrono::milliseconds(failsafeTimeoutWait));
        timeCount += failsafeTimeoutWait;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::readFifo(uint64_t vmHandle, std::vector<EuThread::ThreadId> &threadsWithAttention) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    if (!stateSaveAreaHeader) {
        return ZE_RESULT_ERROR_UNKNOWN;
    } else if (stateSaveAreaHeader->versionHeader.version.major != 3) {
        return ZE_RESULT_SUCCESS;
    }

    auto gpuVa = getContextStateSaveAreaGpuVa(vmHandle);

    // Drain the fifo
    uint32_t drainRetries = 2, lastHead = ~0u;
    const uint64_t offsetTail = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_tail);
    const uint64_t offsetFifoSize = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_size);
    const uint64_t offsetFifo = gpuVa + (stateSaveAreaHeader->versionHeader.size * 8) + stateSaveAreaHeader->regHeaderV3.fifo_offset;

    while (drainRetries--) {
        constexpr uint32_t failsafeTimeoutWait = 50;
        std::vector<uint32_t> fifoIndices(3);
        uint32_t fifoHeadIndex = 0, fifoTailIndex = 0, fifoSize = 0;

        auto retVal = readGpuMemory(vmHandle, reinterpret_cast<char *>(fifoIndices.data()), fifoIndices.size() * sizeof(uint32_t), gpuVa + offsetFifoSize);
        if (retVal != ZE_RESULT_SUCCESS) {
            PRINT_DEBUGGER_ERROR_LOG("Reading FIFO indices failed, error = %d\n", retVal);
            return retVal;
        }
        fifoSize = fifoIndices[0];
        fifoHeadIndex = fifoIndices[1];
        fifoTailIndex = fifoIndices[2];

        if (lastHead != fifoHeadIndex) {
            drainRetries++;
        }
        PRINT_DEBUGGER_FIFO_LOG("fifoHeadIndex: %u fifoTailIndex: %u fifoSize: %u lastHead: %u drainRetries: %u\n",
                                fifoHeadIndex, fifoTailIndex, fifoSize, lastHead, drainRetries);

        lastHead = fifoHeadIndex;
        bool updateTailIndex = false;
        while (fifoTailIndex != fifoHeadIndex) {
            uint32_t readSize = fifoTailIndex < fifoHeadIndex ? fifoHeadIndex - fifoTailIndex : fifoSize - fifoTailIndex;
            std::vector<SIP::fifo_node> nodes(readSize);
            uint64_t currentFifoOffset = offsetFifo + (sizeof(SIP::fifo_node) * fifoTailIndex);

            retVal = readGpuMemory(vmHandle, reinterpret_cast<char *>(nodes.data()), readSize * sizeof(SIP::fifo_node), currentFifoOffset);
            if (retVal != ZE_RESULT_SUCCESS) {
                PRINT_DEBUGGER_ERROR_LOG("Reading FIFO failed, error = %d\n", retVal);
                return retVal;
            }
            for (uint32_t i = 0; i < readSize; i++) {
                const uint64_t gpuVa = currentFifoOffset + (i * sizeof(SIP::fifo_node));
                PRINT_DEBUGGER_FIFO_LOG("Validate entry at index %u in SW Fifo:: vmHandle: %" SCNx64
                                        " gpuVa = %" SCNx64
                                        " valid = %" SCNx8
                                        " slice = %" SCNx8
                                        " subslice = %" SCNx8
                                        " eu = %" SCNx8
                                        " thread = %" SCNx8
                                        "\n",
                                        (i + fifoTailIndex), vmHandle, gpuVa, nodes[i].valid, nodes[i].slice_id, nodes[i].subslice_id, nodes[i].eu_id, nodes[i].thread_id);
                retVal = isValidNode(vmHandle, gpuVa, nodes[i]);
                if (retVal != ZE_RESULT_SUCCESS) {
                    return retVal;
                }
                UNRECOVERABLE_IF(!nodes[i].valid);
                threadsWithAttention.emplace_back(0, nodes[i].slice_id, nodes[i].subslice_id, nodes[i].eu_id, nodes[i].thread_id);
                nodes[i].valid = 0;
            }
            retVal = writeGpuMemory(vmHandle, reinterpret_cast<char *>(nodes.data()), readSize * sizeof(SIP::fifo_node), currentFifoOffset);
            if (retVal != ZE_RESULT_SUCCESS) {
                PRINT_DEBUGGER_ERROR_LOG("Writing FIFO failed, error = %d\n", retVal);
                return retVal;
            }

            if (fifoTailIndex < fifoHeadIndex) {
                // then we read to the head and are done
                fifoTailIndex = fifoHeadIndex;
            } else {
                // wrap around
                fifoTailIndex = 0;
            }
            updateTailIndex = true;
        }

        if (updateTailIndex) {
            retVal = writeGpuMemory(vmHandle, reinterpret_cast<char *>(&fifoTailIndex), sizeof(uint32_t), gpuVa + offsetTail);
            if (retVal != ZE_RESULT_SUCCESS) {
                PRINT_DEBUGGER_ERROR_LOG("Writing FIFO failed, error = %d\n", retVal);
                return retVal;
            }
            NEO::sleep(std::chrono::milliseconds(failsafeTimeoutWait));
        } else {
            break;
        }
    }
    return ZE_RESULT_SUCCESS;
}

void DebugSessionImp::pollFifo() {
    if (attentionEventContext.empty()) {
        return;
    }
    auto now = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    auto timeSinceLastFifoRead = currentTime - lastFifoReadTime;
    if (timeSinceLastFifoRead.count() > fifoPollInterval) {
        PRINT_DEBUGGER_FIFO_LOG("%s", "Polling FIFO start\n");
        handleStoppedThreads();
        PRINT_DEBUGGER_FIFO_LOG("%s", "Polling FIFO ends\n");
    }
}

void DebugSessionImp::handleStoppedThreads() {
    for (const auto &entry : attentionEventContext) {
        auto vmHandle = entry.first;
        std::vector<EuThread::ThreadId> threadsWithAttention;
        auto result = readFifo(vmHandle, threadsWithAttention);
        if (result != ZE_RESULT_SUCCESS) {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        lastFifoReadTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

        if (threadsWithAttention.empty()) {
            return;
        }
        updateStoppedThreadsAndCheckTriggerEvents(entry.second, 0, threadsWithAttention);
    }
}

} // namespace L0
