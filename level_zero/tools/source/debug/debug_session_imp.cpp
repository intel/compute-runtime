/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session_imp.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/include/zet_intel_gpu_debug.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {

DebugSession::DebugSession(const zet_debug_config_t &config, Device *device) : connectedDevice(device) {
}

void DebugSession::createEuThreads() {
    if (connectedDevice) {

        bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
        bool isSubDevice = connectedDevice->getNEODevice()->isSubDevice();

        if ((isRootDevice && NEO::DebugManager.flags.ExperimentalEnableTileAttach.get() == 0) ||
            (isSubDevice && NEO::DebugManager.flags.ExperimentalEnableTileAttach.get() == 1)) {
            auto &hwInfo = connectedDevice->getHwInfo();
            const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
            const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
            const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
            uint32_t subDeviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());

            for (uint32_t tileIndex = 0; tileIndex < subDeviceCount; tileIndex++) {
                for (uint32_t sliceID = 0; sliceID < hwInfo.gtSystemInfo.MaxSlicesSupported; sliceID++) {
                    for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
                        for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {

                            for (uint32_t threadID = 0; threadID < numThreadsPerEu; threadID++) {

                                EuThread::ThreadId thread = {tileIndex, sliceID, subsliceID, euID, threadID};

                                allThreads[uint64_t(thread)] = std::make_unique<EuThread>(thread);
                            }
                        }
                    }
                }
            }
        }
    }
}

uint32_t DebugSession::getDeviceIndexFromApiThread(ze_device_thread_t thread) {
    uint32_t deviceIndex = 0;
    auto &hwInfo = connectedDevice->getHwInfo();
    auto deviceCount = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    if (connectedDevice->getNEODevice()->isSubDevice()) {
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    } else {
        if (thread.slice != UINT32_MAX) {
            deviceIndex = thread.slice / hwInfo.gtSystemInfo.SliceCount;
        } else if (deviceCount > 1) {
            deviceIndex = UINT32_MAX;
        }
    }
    return deviceIndex;
}

ze_device_thread_t DebugSession::convertToPhysicalWithinDevice(ze_device_thread_t thread, uint32_t deviceIndex) {
    auto &hwInfo = connectedDevice->getHwInfo();

    if (thread.slice != UINT32_MAX) {
        thread.slice = thread.slice % hwInfo.gtSystemInfo.SliceCount;
    }

    return thread;
}

EuThread::ThreadId DebugSession::convertToThreadId(ze_device_thread_t thread) {
    auto &hwInfo = connectedDevice->getHwInfo();
    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    UNRECOVERABLE_IF(!isSingleThread(thread));

    uint32_t deviceIndex = 0;
    if (connectedDevice->getNEODevice()->isSubDevice()) {
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    } else {
        deviceIndex = thread.slice / hwInfo.gtSystemInfo.SliceCount;
        thread.slice = thread.slice % hwInfo.gtSystemInfo.SliceCount;
    }

    EuThread::ThreadId threadId(deviceIndex, thread.slice, thread.subslice, thread.eu, thread.thread);
    return threadId;
}

ze_device_thread_t DebugSession::convertToApi(EuThread::ThreadId threadId) {
    auto &hwInfo = connectedDevice->getHwInfo();

    ze_device_thread_t thread = {static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)};

    if (!connectedDevice->getNEODevice()->isSubDevice()) {
        thread.slice = thread.slice + static_cast<uint32_t>(threadId.tileIndex * hwInfo.gtSystemInfo.SliceCount);
    }
    return thread;
}

std::vector<EuThread::ThreadId> DebugSession::getSingleThreadsForDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread, const NEO::HardwareInfo &hwInfo) {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    UNRECOVERABLE_IF(numThreadsPerEu > 8);

    std::vector<EuThread::ThreadId> threads;

    const uint32_t slice = physicalThread.slice;
    const uint32_t subslice = physicalThread.subslice;
    const uint32_t eu = physicalThread.eu;
    const uint32_t thread = physicalThread.thread;

    for (uint32_t sliceID = 0; sliceID < hwInfo.gtSystemInfo.MaxSlicesSupported; sliceID++) {
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

    for (uint32_t i = 0; i < deviceCount; i++) {
        if (i == deviceIndex || deviceIndex == UINT32_MAX) {
            auto physicalThread = convertToPhysicalWithinDevice(thread, i);
            auto singleThreads = getSingleThreadsForDevice(i, physicalThread, hwInfo);

            for (auto &threadId : singleThreads) {

                if (allThreads[threadId]->isStopped()) {
                    continue;
                }
                return false;
            }
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
        if (desc->type != ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

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
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t threadEuRatio = hwInfoConfig.getThreadEuRatioForScratch(hwInfo);

    if (threadEuRatio / numThreadsPerEu > 1) {
        ptss *= threadEuRatio / numThreadsPerEu;
    }

    auto threadOffset = (((threadId.slice * numSubslicesPerSlice + threadId.subslice) * numEuPerSubslice + threadId.eu) * numThreadsPerEu + threadId.thread) * ptss;
    return threadOffset;
}

void DebugSession::printBitmask(uint8_t *bitmask, size_t bitmaskSize) {
    if (NEO::DebugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_INFO) {

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
    attached = true;

    return tileSession;
}

void DebugSessionImp::detachTileDebugSession(DebugSession *tileSession) {
    std::unique_lock<std::mutex> lock(asyncThreadMutex);

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(tileSession->getConnectedDevice()->getNEODevice()->getDeviceBitfield().to_ulong()));
    tileSessions[subDeviceIndex].second = false;
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

DebugSessionImp::Error DebugSessionImp::resumeThreadsWithinDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread) {
    auto &hwInfo = connectedDevice->getHwInfo();
    bool allThreadsRunning = true;
    auto singleThreads = getSingleThreadsForDevice(deviceIndex, physicalThread, hwInfo);
    Error retVal = Error::Unknown;

    std::vector<ze_device_thread_t> resumeThreads;
    std::vector<EuThread::ThreadId> resumeThreadIds;

    for (auto &threadId : singleThreads) {
        if (allThreads[threadId]->isRunning()) {
            continue;
        }
        allThreadsRunning = false;
        resumeThreads.emplace_back(ze_device_thread_t{static_cast<uint32_t>(threadId.slice), static_cast<uint32_t>(threadId.subslice), static_cast<uint32_t>(threadId.eu), static_cast<uint32_t>(threadId.thread)});
        resumeThreadIds.push_back(threadId);
    }

    if (allThreadsRunning) {
        return Error::ThreadsRunning;
    }

    std::unique_lock<std::mutex> lock(threadStateMutex);

    [[maybe_unused]] auto sipCommandResult = writeResumeCommand(resumeThreadIds);
    DEBUG_BREAK_IF(sipCommandResult != true);

    auto result = resumeImp(resumeThreadIds, deviceIndex);

    for (auto &threadID : resumeThreadIds) {
        while (checkThreadIsResumed(threadID) == false)
            ;

        allThreads[threadID]->resumeThread();
    }

    if (sipCommandResult && result == ZE_RESULT_SUCCESS) {
        retVal = Error::Success;
    }

    return retVal;
}

bool DebugSessionImp::writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    bool success = true;

    if (stateSaveAreaHeader->versionHeader.version.major < 2u) {
        auto &hwInfo = connectedDevice->getHwInfo();
        auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

        if (l0HwHelper.isResumeWARequired()) {
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
        auto *regdesc = &stateSaveAreaHeader->regHeader.cmd;
        SIP::sip_command resumeCommand = {0};
        resumeCommand.command = static_cast<uint32_t>(NEO::SipKernel::COMMAND::RESUME);

        for (auto &threadID : threadIds) {
            PRINT_DEBUGGER_INFO_LOG("Write RESUME for %s\n", EuThread::toString(threadID).c_str());
            auto result = registersAccessHelper(allThreads[threadID].get(), regdesc, 0, 1, &resumeCommand, true);
            if (result != ZE_RESULT_SUCCESS) {
                success = false;
                PRINT_DEBUGGER_ERROR_LOG("Failed to write RESUME command for thread %s\n", EuThread::toString(threadID).c_str());
            }
        }
    }
    return success;
}

bool DebugSessionImp::checkThreadIsResumed(const EuThread::ThreadId &threadID) {
    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    bool resumed = true;

    if (stateSaveAreaHeader->versionHeader.version.major >= 2u) {
        const auto thread = allThreads[threadID].get();
        auto gpuVa = getContextStateSaveAreaGpuVa(thread->getMemoryHandle());
        if (gpuVa == 0) {
            PRINT_DEBUGGER_ERROR_LOG("Failed to get Context State Save Area GPU Virtual Address\n", "");
            return resumed;
        }

        auto threadSlotOffset = calculateThreadSlotOffset(thread->getThreadId());
        auto srMagicOffset = threadSlotOffset + getStateSaveAreaHeader()->regHeader.sr_magic_offset;
        SIP::sr_ident srMagic;
        memset(srMagic.magic, 0, sizeof(SIP::sr_ident::magic));

        auto status = readGpuMemory(thread->getMemoryHandle(), reinterpret_cast<char *>(&srMagic), sizeof(srMagic), gpuVa + srMagicOffset);
        DEBUG_BREAK_IF(status != ZE_RESULT_SUCCESS);

        if (status != ZE_RESULT_SUCCESS || 0 != strcmp(srMagic.magic, "srmagic")) {
            PRINT_DEBUGGER_ERROR_LOG("checkThreadIsResumed - Failed to read srMagic for thread %s\n", EuThread::toString(threadID).c_str());
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
        uint32_t deviceIndex = 0;
        if (thread.slice != UINT32_MAX) {
            deviceIndex = getDeviceIndexFromApiThread(thread);
        }
        auto physicalThread = convertToPhysicalWithinDevice(thread, deviceIndex);
        auto result = resumeThreadsWithinDevice(deviceIndex, physicalThread);

        if (result == Error::ThreadsRunning) {
            retVal = ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else if (result != Error::Success) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    } else {
        bool allThreadsRunning = true;

        for (uint32_t deviceId = 0; deviceId < deviceCount; deviceId++) {

            auto physicalThread = convertToPhysicalWithinDevice(thread, deviceId);
            auto result = resumeThreadsWithinDevice(deviceId, physicalThread);

            if (result == Error::ThreadsRunning) {
                continue;
            } else if (result != Error::Success) {
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
        uint32_t deviceIndex = 0;
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

void DebugSessionImp::markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(EuThread::ThreadId threadId, uint64_t memoryHandle) {

    SIP::sr_ident srMagic = {};
    srMagic.count = 0;

    bool wasStopped = false;
    {
        std::unique_lock<std::mutex> lock(threadStateMutex);

        if (!readSystemRoutineIdent(allThreads[threadId].get(), memoryHandle, srMagic)) {
            PRINT_DEBUGGER_ERROR_LOG("Failed to read SR IDENT\n", "");
            return;
        } else {
            PRINT_DEBUGGER_INFO_LOG("SIP version == %d.%d.%d\n", (int)srMagic.version.major, (int)srMagic.version.minor, (int)srMagic.version.patch);
        }

        wasStopped = allThreads[threadId]->isStopped();

        if (!allThreads[threadId]->verifyStopped(srMagic.count)) {
            return;
        }

        allThreads[threadId]->stopThread(memoryHandle);
    }

    bool threadWasInterrupted = false;

    for (auto &request : pendingInterrupts) {
        ze_device_thread_t apiThread = convertToApi(threadId);

        auto isInterrupted = checkSingleThreadWithinDeviceThread(apiThread, request.first);

        if (isInterrupted) {
            request.second = true;
            threadWasInterrupted = true;
        }
    }

    if (!threadWasInterrupted && !wasStopped) {
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

        fillResumeAndStoppedThreadsFromNewlyStopped(resumeThreads, stoppedThreadsToReport);

        resumeAccidentallyStoppedThreads(resumeThreads);
        generateEventsForPendingInterrupts();
        generateEventsForStoppedThreads(stoppedThreadsToReport);

        interruptSent = false;
        triggerEvents = false;
    }
}

bool DebugSessionImp::isForceExceptionOrForceExternalHaltOnlyExceptionReason(uint32_t *cr0) {
    const uint32_t cr0ExceptionBitmask = 0xFC000000;
    const uint32_t cr0ForcedExcpetionBitmask = 0x44000000;

    return (((cr0[1] & cr0ExceptionBitmask) & (~cr0ForcedExcpetionBitmask)) == 0);
}

void DebugSessionImp::fillResumeAndStoppedThreadsFromNewlyStopped(std::vector<EuThread::ThreadId> &resumeThreads, std::vector<EuThread::ThreadId> &stoppedThreadsToReport) {

    const auto regSize = std::max(getRegisterSize(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), 64u);
    auto reg = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));

    for (auto &newlyStopped : newlyStoppedThreads) {
        if (allThreads[newlyStopped]->isStopped()) {
            memset(reg.get(), 0, regSize);
            readRegistersImp(newlyStopped, ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, 0, 1, reg.get());

            if (isForceExceptionOrForceExternalHaltOnlyExceptionReason(reg.get())) {
                PRINT_DEBUGGER_THREAD_LOG("RESUME accidentally stopped thread = %s\n", allThreads[newlyStopped]->toString().c_str());
                resumeThreads.push_back(newlyStopped);
            } else {
                PRINT_DEBUGGER_THREAD_LOG("Newly stopped thread = %s, exception bits = %#010" PRIx32 "\n", allThreads[newlyStopped]->toString().c_str(), reg[1]);
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
    std::vector<ze_device_thread_t> threads[4];
    std::vector<EuThread::ThreadId> threadIdsPerDevice[4];

    for (auto &threadID : threadIds) {
        ze_device_thread_t thread = {static_cast<uint32_t>(threadID.slice), static_cast<uint32_t>(threadID.subslice), static_cast<uint32_t>(threadID.eu), static_cast<uint32_t>(threadID.thread)};
        uint32_t deviceIndex = static_cast<uint32_t>(threadID.tileIndex);

        UNRECOVERABLE_IF((connectedDevice->getNEODevice()->getNumSubDevices() > 0) &&
                         (deviceIndex >= connectedDevice->getNEODevice()->getNumSubDevices()));

        threads[deviceIndex].push_back(thread);
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
            *outputEvent = apiEvents.front();
            apiEvents.pop();
            return ZE_RESULT_SUCCESS;
        }
    } while (timeout == UINT64_MAX && asyncThread.threadActive);

    return ZE_RESULT_NOT_READY;
}

void DebugSessionImp::validateAndSetStateSaveAreaHeader(const std::vector<char> &data) {
    auto pStateSaveArea = reinterpret_cast<const SIP::StateSaveAreaHeader *>(data.data());
    if (0 == strcmp(pStateSaveArea->versionHeader.magic, "tssarea")) {
        size_t size = pStateSaveArea->versionHeader.size * 8u;
        DEBUG_BREAK_IF(size != sizeof(SIP::StateSaveAreaHeader));
        stateSaveAreaHeader.assign(data.begin(), data.begin() + size);
        PRINT_DEBUGGER_INFO_LOG("Context State Save Area : version == %d.%d.%d\n", (int)pStateSaveArea->versionHeader.version.major, (int)pStateSaveArea->versionHeader.version.minor, (int)pStateSaveArea->versionHeader.version.patch);
    } else {
        PRINT_DEBUGGER_ERROR_LOG("Setting Context State Save Area: failed to match magic numbers\n", "");
    }
}

const SIP::StateSaveAreaHeader *DebugSessionImp::getStateSaveAreaHeader() {
    if (stateSaveAreaHeader.empty()) {
        readStateSaveAreaHeader();
    }
    return reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
}

const SIP::regset_desc *DebugSessionImp::getSbaRegsetDesc() {
    // SBA virtual register set is always present
    static const SIP::regset_desc sba = {0, ZET_DEBUG_SBA_COUNT_INTEL_GPU, 64, 8};
    return &sba;
}

const SIP::regset_desc *DebugSessionImp::typeToRegsetDesc(uint32_t type) {
    auto pStateSaveAreaHeader = getStateSaveAreaHeader();

    DEBUG_BREAK_IF(pStateSaveAreaHeader == nullptr);
    if (pStateSaveAreaHeader == nullptr) {
        return nullptr;
    }

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
        return &pStateSaveAreaHeader->regHeader.dbg;
    case ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU:
        return &pStateSaveAreaHeader->regHeader.fc;
    case ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU:
        return DebugSessionImp::getSbaRegsetDesc();
    default:
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
    case ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU:
        return ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE;

    case ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU:
    case ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU:
        return ZET_DEBUG_REGSET_FLAG_READABLE;

    default:
        return 0;
    }
}

size_t DebugSessionImp::calculateThreadSlotOffset(EuThread::ThreadId threadId) {
    auto pStateSaveAreaHeader = getStateSaveAreaHeader();
    return pStateSaveAreaHeader->versionHeader.size * 8 + pStateSaveAreaHeader->regHeader.state_area_offset + ((((threadId.slice * pStateSaveAreaHeader->regHeader.num_subslices_per_slice + threadId.subslice) * pStateSaveAreaHeader->regHeader.num_eus_per_subslice + threadId.eu) * pStateSaveAreaHeader->regHeader.num_threads_per_eu + threadId.thread) * pStateSaveAreaHeader->regHeader.state_save_size);
}

size_t DebugSessionImp::calculateRegisterOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start) {
    return regdesc->offset + regdesc->bytes * start;
}

ze_result_t DebugSessionImp::readSbaRegisters(EuThread::ThreadId threadId, uint32_t start, uint32_t count, void *pRegisterValues) {
    auto sbaRegDesc = DebugSessionImp::getSbaRegsetDesc();

    if (start >= sbaRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (start + count > sbaRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t ret = ZE_RESULT_SUCCESS;

    NEO::SbaTrackedAddresses sbaBuffer;
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

    uint64_t bindingTableBaseAddress = ((r0[4] >> 5) << 5) + sbaBuffer.SurfaceStateBaseAddress;
    uint64_t scratchSpaceBaseAddress = 0;

    auto &hwHelper = NEO::HwHelper::get(connectedDevice->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.isScratchSpaceSurfaceStateAccessible()) {
        auto surfaceStateForScratch = ((r0[5] >> 10) << 6);

        if (surfaceStateForScratch > 0) {
            uint64_t renderSurfaceStateGpuVa = surfaceStateForScratch + sbaBuffer.SurfaceStateBaseAddress;
            constexpr size_t renderSurfaceStateSize = 64;
            std::vector<char> renderSurfaceState(renderSurfaceStateSize, 0);

            ret = readGpuMemory(allThreads[threadId]->getMemoryHandle(), renderSurfaceState.data(), renderSurfaceStateSize, renderSurfaceStateGpuVa);

            if (ret != ZE_RESULT_SUCCESS) {
                return ret;
            }

            auto scratchSpacePTSize = hwHelper.getRenderSurfaceStatePitch(renderSurfaceState.data());
            auto threadOffset = getPerThreadScratchOffset(scratchSpacePTSize, threadId);
            auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
            auto scratchAllocationBase = gmmHelper->decanonize(hwHelper.getRenderSurfaceStateBaseAddress(renderSurfaceState.data()));
            if (scratchAllocationBase != 0) {
                scratchSpaceBaseAddress = threadOffset + scratchAllocationBase;
            }
        }
    } else {
        auto scratchPointer = ((r0[5] >> 10) << 10);
        if (scratchPointer != 0) {
            scratchSpaceBaseAddress = scratchPointer + sbaBuffer.GeneralStateBaseAddress;
        }
    }

    std::vector<uint64_t> packed;
    packed.push_back(sbaBuffer.GeneralStateBaseAddress);
    packed.push_back(sbaBuffer.SurfaceStateBaseAddress);
    packed.push_back(sbaBuffer.DynamicStateBaseAddress);
    packed.push_back(sbaBuffer.IndirectObjectBaseAddress);
    packed.push_back(sbaBuffer.InstructionBaseAddress);
    packed.push_back(sbaBuffer.BindlessSurfaceStateBaseAddress);
    packed.push_back(sbaBuffer.BindlessSamplerStateBaseAddress);
    packed.push_back(bindingTableBaseAddress);
    packed.push_back(scratchSpaceBaseAddress);

    size_t size = count * sbaRegDesc->bytes;
    memcpy_s(pRegisterValues, size, &packed[start], size);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSession::getRegisterSetProperties(Device *device, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    if (nullptr == pCount) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (*pCount && !pRegisterSetProperties) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto &stateSaveAreaHeader = NEO::SipKernel::getBindlessDebugSipKernel(*device->getNEODevice()).getStateSaveAreaHeader();

    if (stateSaveAreaHeader.size() == 0) {
        *pCount = 0;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t totalRegsetNum = 0;
    auto parseRegsetDesc = [&](const SIP::regset_desc &regsetDesc, zet_debug_regset_type_intel_gpu_t regsetType) {
        if (regsetDesc.num) {
            if (totalRegsetNum < *pCount) {
                zet_debug_regset_properties_t regsetProps = {
                    ZET_STRUCTURE_TYPE_DEBUG_REGSET_PROPERTIES,
                    nullptr,
                    static_cast<uint32_t>(regsetType),
                    0,
                    DebugSessionImp::typeToRegsetFlags(regsetType),
                    0,
                    regsetDesc.num,
                    regsetDesc.bits,
                    regsetDesc.bytes,
                };
                pRegisterSetProperties[totalRegsetNum] = regsetProps;
            }
            ++totalRegsetNum;
        }
    };

    auto pStateSaveArea = reinterpret_cast<const SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data());

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

    parseRegsetDesc(*DebugSessionImp::getSbaRegsetDesc(), ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU);

    parseRegsetDesc(pStateSaveArea->regHeader.dbg, ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU);
    parseRegsetDesc(pStateSaveArea->regHeader.fc, ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU);

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

    char tssMagic[8] = {0};
    readGpuMemory(thread->getMemoryHandle(), tssMagic, sizeof(tssMagic), gpuVa);
    if (0 != strcmp(tssMagic, "tssarea")) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto threadSlotOffset = calculateThreadSlotOffset(thread->getThreadId());

    auto srMagicOffset = threadSlotOffset + getStateSaveAreaHeader()->regHeader.sr_magic_offset;
    SIP::sr_ident srMagic;
    memset(srMagic.magic, 0, sizeof(SIP::sr_ident::magic));

    readGpuMemory(thread->getMemoryHandle(), reinterpret_cast<char *>(&srMagic), sizeof(srMagic), gpuVa + srMagicOffset);
    if (0 != strcmp(srMagic.magic, "srmagic")) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto startRegOffset = threadSlotOffset + calculateRegisterOffsetInThreadSlot(regdesc, start);

    int ret = 0;
    if (write) {
        ret = writeGpuMemory(thread->getMemoryHandle(), static_cast<const char *>(pRegisterValues), count * regdesc->bytes, gpuVa + startRegOffset);
    } else {
        ret = readGpuMemory(thread->getMemoryHandle(), static_cast<char *>(pRegisterValues), count * regdesc->bytes, gpuVa + startRegOffset);
    }

    return ret == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionImp::readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    if (!isSingleThread(thread)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    auto threadId = convertToThreadId(thread);

    if (!allThreads[threadId]->isStopped()) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto stateSaveAreaHeader = getStateSaveAreaHeader();
    if (stateSaveAreaHeader == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (type == ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU) {
        return readSbaRegisters(threadId, start, count, pRegisterValues);
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

    if (!allThreads[threadId]->isStopped()) {
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

bool DebugSessionImp::isValidGpuAddress(uint64_t address) const {
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto decanonizedAddress = gmmHelper->decanonize(address);
    bool validAddress = gmmHelper->isValidCanonicalGpuAddress(address);

    if (address == decanonizedAddress || validAddress) {
        return true;
    }
    return false;
}

ze_result_t DebugSessionImp::validateThreadAndDescForMemoryAccess(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc) {
    if (!isValidGpuAddress(desc->address)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t status = sanityMemAccessThreadCheck(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
