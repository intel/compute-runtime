/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"
#include "level_zero/tools/source/debug/linux/drm_helper.h"

namespace L0 {
DebugSessionLinuxAllocatorFn debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_MAX] = {};

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {

    if (!device->getOsInterface().isDebugAttachAvailable()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }
    auto drm = device->getOsInterface().getDriverModel()->as<NEO::Drm>();
    auto drmVersion = NEO::Drm::getDrmVersion(drm->getFileDescriptor());

    DebugSessionLinuxAllocatorFn allocator = nullptr;
    if ("xe" == drmVersion) {
        allocator = debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_XE];
    } else {
        allocator = debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_I915];
    }
    UNRECOVERABLE_IF(!allocator)

    auto debugSession = allocator(config, device, result, isRootAttach);
    if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
    }
    debugSession->setAttachMode(isRootAttach);
    result = debugSession->initialize();
    if (result != ZE_RESULT_SUCCESS) {
        debugSession->closeConnection();
        delete debugSession;
        debugSession = nullptr;
    } else {
        debugSession->startAsyncThread();
    }
    return debugSession;
}

ze_result_t DebugSessionLinux::translateDebuggerOpenErrno(int error) {

    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    switch (error) {
    case ENODEV:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    case EBUSY:
        result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        break;
    case EACCES:
        result = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        break;
    }
    return result;
}

bool DebugSessionLinux::closeFd() {
    if (fd == 0) {
        return false;
    }

    auto ret = NEO::SysCalls::close(fd);

    if (ret != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Debug connection close() on fd: %d failed: retCode: %d\n", fd, ret);
        return false;
    }
    fd = 0;
    return true;
}

void *DebugSessionLinux::readInternalEventsThreadFunction(void *arg) {
    DebugSessionLinux *self = reinterpret_cast<DebugSessionLinux *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread started\n", "");
    self->internalThreadHasStarted = true;

    while (self->internalEventThread.threadActive) {
        self->readInternalEventsAsync();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger internal event thread closing\n", "");

    return nullptr;
}

std::unique_ptr<uint64_t[]> DebugSessionLinux::getInternalEvent() {
    std::unique_ptr<uint64_t[]> eventMemory;

    {
        std::unique_lock<std::mutex> lock(internalEventThreadMutex);

        if (internalEventQueue.empty()) {
            NEO::waitOnCondition(internalEventCondition, lock, std::chrono::milliseconds(100));
        }

        if (!internalEventQueue.empty()) {
            eventMemory = std::move(internalEventQueue.front());
            internalEventQueue.pop();
        }
    }
    return eventMemory;
}

void DebugSessionLinux::closeAsyncThread() {
    asyncThread.close();
    internalEventThread.close();
}

bool DebugSessionLinux::checkForceExceptionBit(uint64_t memoryHandle, EuThread::ThreadId threadId, uint32_t *cr0, const SIP::regset_desc *regDesc) {

    auto gpuVa = getContextStateSaveAreaGpuVa(memoryHandle);
    auto threadSlotOffset = calculateThreadSlotOffset(threadId);
    auto startRegOffset = threadSlotOffset + calculateRegisterOffsetInThreadSlot(regDesc, 0);

    [[maybe_unused]] int ret = readGpuMemory(memoryHandle, reinterpret_cast<char *>(cr0), 1 * regDesc->bytes, gpuVa + startRegOffset);
    DEBUG_BREAK_IF(ret != ZE_RESULT_SUCCESS);

    const uint32_t cr0ForcedExcpetionBitmask = 0x04000000;
    if (cr0[1] & cr0ForcedExcpetionBitmask) {
        return true;
    }
    return false;
}

void DebugSessionLinux::checkStoppedThreadsAndGenerateEvents(const std::vector<EuThread::ThreadId> &threads, uint64_t memoryHandle, uint32_t deviceIndex) {

    std::vector<EuThread::ThreadId> threadsWithAttention;
    std::vector<EuThread::ThreadId> stoppedThreadsToReport;
    NEO::sleep(std::chrono::microseconds(1));

    if (threads.size() > 1) {
        auto hwInfo = connectedDevice->getHwInfo();
        auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

        std::unique_ptr<uint8_t[]> bitmask;
        size_t bitmaskSize;
        [[maybe_unused]] auto attReadResult = threadControl(threads, deviceIndex, ThreadControlCmd::stopped, bitmask, bitmaskSize);

        // error querying STOPPED threads - no threads available ( for example: threads have completed )
        if (attReadResult != 0) {
            PRINT_DEBUGGER_ERROR_LOG("checkStoppedThreadsAndGenerateEvents ATTENTION read failed: %d errno = %d \n", (int)attReadResult, DrmHelper::getErrno(connectedDevice));
            return;
        }

        threadsWithAttention = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, deviceIndex, bitmask.get(), bitmaskSize);

        if (threadsWithAttention.size() == 0) {
            return;
        }
    }

    const auto &threadsToCheck = threadsWithAttention.size() > 0 ? threadsWithAttention : threads;
    stoppedThreadsToReport.reserve(threadsToCheck.size());

    const auto regSize = std::max(getRegisterSize(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU), 64u);
    auto cr0 = std::make_unique<uint32_t[]>(regSize / sizeof(uint32_t));
    auto regDesc = typeToRegsetDesc(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU);

    for (auto &threadId : threadsToCheck) {
        SIP::sr_ident srMagic = {{0}};
        srMagic.count = 0;

        if (readSystemRoutineIdent(allThreads[threadId].get(), memoryHandle, srMagic)) {
            bool wasStopped = allThreads[threadId]->isStopped();
            bool checkIfStopped = true;

            if (srMagic.count % 2 == 1) {
                memset(cr0.get(), 0, regSize);
                checkIfStopped = !checkForceExceptionBit(memoryHandle, threadId, cr0.get(), regDesc);
            }

            if (checkIfStopped && allThreads[threadId]->verifyStopped(srMagic.count)) {
                allThreads[threadId]->stopThread(memoryHandle);
                if (!wasStopped) {
                    stoppedThreadsToReport.push_back(threadId);
                }
            }

        } else {
            break;
        }
    }

    generateEventsForStoppedThreads(stoppedThreadsToReport);
}

ze_result_t DebugSessionLinux::resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) {
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize;

    auto result = threadControl(threads, deviceIndex, ThreadControlCmd::resume, bitmask, bitmaskSize);

    return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
}

ze_result_t DebugSessionLinux::interruptImp(uint32_t deviceIndex) {
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize;

    auto result = threadControl({}, deviceIndex, ThreadControlCmd::interruptAll, bitmask, bitmaskSize);

    return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
}

} // namespace L0
