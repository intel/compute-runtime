/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"
#include "level_zero/tools/source/debug/linux/drm_helper.h"
#include "level_zero/zet_intel_gpu_debug.h"

namespace L0 {
DebugSessionLinuxAllocatorFn debugSessionLinuxFactory[DEBUG_SESSION_LINUX_TYPE_MAX] = {};

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {

    if (!device->getOsInterface()->isDebugAttachAvailable()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }
    auto drm = device->getOsInterface()->getDriverModel()->as<NEO::Drm>();
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

void DebugSessionLinux::handleEventsAsync() {
    handleInternalEvent();
}

ze_result_t DebugSessionLinux::initialize() {
    struct pollfd pollFd = {
        .fd = this->fd,
        .events = POLLIN,
        .revents = 0,
    };

    auto numberOfFds = ioctlHandler->poll(&pollFd, 1, 1000);
    PRINT_DEBUGGER_INFO_LOG("initialization poll() retCode: %d\n", numberOfFds);

    if (numberOfFds <= 0) {
        return ZE_RESULT_NOT_READY;
    }

    bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
    if (isRootDevice && !tileAttachEnabled) {
        createEuThreads();
    }
    createTileSessionsIfEnabled();
    startInternalEventsThread();

    bool allEventsCollected = false;
    bool eventAvailable = true;
    float timeDelta = 0;
    float timeStart = clock();
    do {
        if (internalThreadHasStarted) {
            eventAvailable = handleInternalEvent();
            allEventsCollected = checkAllEventsCollected();
        } else {
            timeDelta = float(clock() - timeStart) / CLOCKS_PER_SEC;
        }
    } while ((eventAvailable && !allEventsCollected) && timeDelta < getThreadStartLimitTime());

    internalThreadHasStarted = false;

    if (clientHandleClosed == clientHandle && clientHandle != invalidClientHandle) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    if (allEventsCollected) {
        if (!readModuleDebugArea()) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_NOT_READY;
}

void DebugSessionLinux::createTileSessionsIfEnabled() {
    auto numTiles = connectedDevice->getNEODevice()->getNumSubDevices();
    if (numTiles > 0 && tileAttachEnabled) {
        tileSessions.resize(numTiles);

        for (uint32_t i = 0; i < numTiles; i++) {
            auto subDevice = connectedDevice->getNEODevice()->getSubDevice(i)->getSpecializedDevice<Device>();
            tileSessions[i] = std::pair<DebugSessionImp *, bool>{createTileSession(config, subDevice, this), false};
        }
        tileSessionsEnabled = true;
    }
}

bool DebugSessionLinux::checkAllEventsCollected() {
    bool allEventsCollected = false;
    bool clientConnected = (this->clientHandle != invalidClientHandle);
    if (clientConnected) {
        if (getClientConnection(clientHandle)->vmToModuleDebugAreaBindInfo.size() > 0) {
            allEventsCollected = true;
        }
    }
    PRINT_DEBUGGER_INFO_LOG("checkAllEventsCollected() returned %d, clientHandle = %ull\n", static_cast<int>(allEventsCollected), this->clientHandle);
    return allEventsCollected;
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
    auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (threads.size() > 1 && l0GfxCoreHelper.isThreadControlStoppedSupported()) {
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

ze_result_t DebugSessionLinux::readGpuMemory(uint64_t vmHandle, char *output, size_t size, uint64_t gpuVa) {

    int vmDebugFd = openVmFd(vmHandle, true);
    if (vmDebugFd < 0) {
        PRINT_DEBUGGER_ERROR_LOG("VM_OPEN failed = %d\n", vmDebugFd);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int64_t retVal = 0;
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);
    if (flushVmCache(vmDebugFd) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (NEO::debugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        uint64_t alignedMem = alignDown(gpuVa, MemoryConstants::pageSize);
        uint64_t alignedDiff = gpuVa - alignedMem;
        uint64_t alignedSize = size + alignedDiff;

        void *mappedPtr = ioctlHandler->mmap(NULL, alignedSize, PROT_READ, MAP_SHARED, vmDebugFd, alignedMem);

        if (mappedPtr == MAP_FAILED) {
            PRINT_DEBUGGER_ERROR_LOG("Reading memory failed, errno = %d\n", errno);
            retVal = -1;
        } else {
            char *realSourceVA = static_cast<char *>(mappedPtr) + alignedDiff;
            retVal = memcpy_s(output, size, static_cast<void *>(realSourceVA), size);
            ioctlHandler->munmap(mappedPtr, alignedSize);
        }
    } else {
        size_t pendingSize = size;
        uint8_t retry = 0;
        size_t retrySize = size;
        do {
            PRINT_DEBUGGER_MEM_ACCESS_LOG("Reading (pread) memory from gpu va = %#" PRIx64 ", size = %zu\n", gpuVa, pendingSize);
            retVal = ioctlHandler->pread(vmDebugFd, output, pendingSize, gpuVa);
            output += retVal;
            gpuVa += retVal;
            pendingSize -= retVal;
            if (retVal == 0) {
                if (pendingSize < retrySize) {
                    retry = 0;
                }
                retry++;
                retrySize = pendingSize;
            }
        } while (((retVal == 0) && (retry < maxRetries)) || ((retVal > 0) && (pendingSize > 0)));

        if (retVal < 0) {
            PRINT_DEBUGGER_ERROR_LOG("Reading memory failed, errno = %d\n", errno);
        }

        retVal = pendingSize;
    }
    if (flushVmCache(vmDebugFd) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    NEO::SysCalls::close(vmDebugFd);

    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::writeGpuMemory(uint64_t vmHandle, const char *input, size_t size, uint64_t gpuVa) {

    int vmDebugFd = openVmFd(vmHandle, false);
    if (vmDebugFd < 0) {
        PRINT_DEBUGGER_ERROR_LOG("VM_OPEN failed = %d\n", vmDebugFd);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    int64_t retVal = 0;
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);
    if (flushVmCache(vmDebugFd) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (NEO::debugManager.flags.EnableDebuggerMmapMemoryAccess.get()) {
        uint64_t alignedMem = alignDown(gpuVa, MemoryConstants::pageSize);
        uint64_t alignedDiff = gpuVa - alignedMem;
        uint64_t alignedSize = size + alignedDiff;

        void *mappedPtr = ioctlHandler->mmap(NULL, alignedSize, PROT_WRITE, MAP_SHARED, vmDebugFd, alignedMem);

        if (mappedPtr == MAP_FAILED) {
            PRINT_DEBUGGER_ERROR_LOG("Writing memory failed, errno = %d\n", errno);
            retVal = -1;
        } else {
            char *realDestVA = static_cast<char *>(mappedPtr) + alignedDiff;
            retVal = memcpy_s(static_cast<void *>(realDestVA), size, input, size);
            ioctlHandler->munmap(mappedPtr, alignedSize);
        }
    } else {
        size_t pendingSize = size;
        uint8_t retry = 0;
        size_t retrySize = size;
        do {
            PRINT_DEBUGGER_MEM_ACCESS_LOG("Writing (pwrite) memory to gpu va = %#" PRIx64 ", size = %zu\n", gpuVa, pendingSize);
            retVal = ioctlHandler->pwrite(vmDebugFd, input, pendingSize, gpuVa);
            input += retVal;
            gpuVa += retVal;
            pendingSize -= retVal;
            if (retVal == 0) {
                if (pendingSize < retrySize) {
                    retry = 0;
                }
                retry++;
                retrySize = pendingSize;
            }
        } while (((retVal == 0) && (retry < maxRetries)) || ((retVal > 0) && (pendingSize > 0)));

        if (retVal < 0) {
            PRINT_DEBUGGER_ERROR_LOG("Writing memory failed, errno = %d\n", errno);
        }

        retVal = pendingSize;
    }
    if (flushVmCache(vmDebugFd) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    NEO::SysCalls::close(vmDebugFd);

    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer,
                                            const char *&elfData, const uint64_t offset) {

    int retVal = -1;
    elfData += offset;
    retVal = memcpy_s(buffer, size, elfData, size);
    return (retVal == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionLinux::readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = readDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<void *, false>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionLinux::readDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    bool isa = tryReadIsa(connectedDevice->getNEODevice()->getDeviceBitfield(), desc, size, buffer, status);
    if (isa) {
        return status;
    }

    bool elf = tryReadElf(desc, size, buffer, status);
    if (elf) {
        return status;
    }

    if (DebugSession::isThreadAll(thread)) {
        return accessDefaultMemForThreadAll(desc, size, const_cast<void *>(buffer), false);
    }

    auto threadId = convertToThreadId(thread);
    auto vmHandle = allThreads[threadId]->getMemoryHandle();
    if (vmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return readGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
}

ze_result_t DebugSessionLinux::writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = writeDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<const void *, true>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionLinux::writeDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    auto deviceBitfield = connectedDevice->getNEODevice()->getDeviceBitfield();

    bool isa = tryWriteIsa(deviceBitfield, desc, size, buffer, status);
    if (isa) {
        return status;
    }

    if (DebugSession::isThreadAll(thread)) {
        return accessDefaultMemForThreadAll(desc, size, const_cast<void *>(buffer), true);
    }

    auto threadId = convertToThreadId(thread);
    auto threadVmHandle = allThreads[threadId]->getMemoryHandle();
    if (threadVmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return writeGpuMemory(threadVmHandle, static_cast<const char *>(buffer), size, desc->address);
}

bool DebugSessionLinux::tryWriteIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer, ze_result_t &status) {
    return tryAccessIsa(deviceBitfield, desc, size, const_cast<void *>(buffer), true, status);
}

bool DebugSessionLinux::tryReadIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status) {
    return tryAccessIsa(deviceBitfield, desc, size, buffer, false, status);
}

bool DebugSessionLinux::tryReadElf(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status) {
    const char *elfData = nullptr;
    uint64_t offset = 0;

    std::lock_guard<std::mutex> memLock(asyncThreadMutex);

    status = getElfOffset(desc, size, elfData, offset);
    if (status == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return true;
    }

    if (elfData) {
        status = readElfSpace(desc, size, buffer, elfData, offset);
        return true;
    }
    return false;
}

ze_result_t DebugSessionLinux::accessDefaultMemForThreadAll(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write) {
    auto status = ZE_RESULT_ERROR_UNINITIALIZED;
    std::vector<uint64_t> allVms;

    allVms = getAllMemoryHandles();

    if (allVms.size() > 0) {
        for (auto vmHandle : allVms) {
            if (write) {
                status = writeGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
            } else {
                status = readGpuMemory(vmHandle, static_cast<char *>(buffer), size, desc->address);
            }
            if (status == ZE_RESULT_SUCCESS) {
                return status;
            }
        }

        status = ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return status;
}

bool DebugSessionLinux::tryAccessIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write, ze_result_t &status) {
    status = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint64_t vmHandle[NEO::EngineLimits::maxHandleCount] = {invalidHandle};
    uint32_t deviceIndex = Math::getMinLsbSet(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    bool isaAccess = false;

    auto checkIfAnyFailed = [](const auto &result) { return result != ZE_RESULT_SUCCESS; };

    {
        std::lock_guard<std::mutex> memLock(asyncThreadMutex);

        if (deviceBitfield.count() == 1) {
            status = getISAVMHandle(deviceIndex, desc, size, vmHandle[deviceIndex]);
            if (status == ZE_RESULT_SUCCESS) {
                isaAccess = true;
            }
            if (status == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
                return true;
            }
        } else {
            isaAccess = getIsaInfoForAllInstances(deviceBitfield, desc, size, vmHandle, status);
        }
    }

    if (isaAccess && status == ZE_RESULT_SUCCESS) {

        if (write) {
            if (deviceBitfield.count() == 1) {
                if (vmHandle[deviceIndex] != invalidHandle) {
                    status = writeGpuMemory(vmHandle[deviceIndex], static_cast<char *>(buffer), size, desc->address);
                } else {
                    status = ZE_RESULT_ERROR_UNINITIALIZED;
                }
            } else {
                std::vector<ze_result_t> results(NEO::EngineLimits::maxHandleCount);

                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    results[i] = ZE_RESULT_SUCCESS;

                    if (deviceBitfield.test(i) && vmHandle[i] != invalidHandle) {
                        results[i] = writeGpuMemory(vmHandle[i], static_cast<char *>(buffer), size, desc->address);

                        if (results[i] != ZE_RESULT_SUCCESS) {
                            break;
                        }
                    }
                }

                const bool anyFailed = std::any_of(results.begin(), results.end(), checkIfAnyFailed);

                if (anyFailed) {
                    status = ZE_RESULT_ERROR_UNKNOWN;
                }
            }
        } else {

            if (deviceBitfield.count() > 1) {
                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    if (vmHandle[i] != invalidHandle) {
                        deviceIndex = i;
                        break;
                    }
                }
            }

            if (vmHandle[deviceIndex] != invalidHandle) {
                status = readGpuMemory(vmHandle[deviceIndex], static_cast<char *>(buffer), size, desc->address);
            } else {
                status = ZE_RESULT_ERROR_UNINITIALIZED;
            }
        }
    }
    return isaAccess;
}

std::vector<uint64_t> DebugSessionLinux::getAllMemoryHandles() {
    std::vector<uint64_t> allVms;
    std::unique_lock<std::mutex> memLock(asyncThreadMutex);

    auto &vmIds = getClientConnection(clientHandle)->vmIds;
    allVms.resize(vmIds.size());
    std::copy(vmIds.begin(), vmIds.end(), allVms.begin());
    return allVms;
}

ze_result_t DebugSessionLinux::getElfOffset(const zet_debug_memory_space_desc_t *desc, size_t size, const char *&elfData, uint64_t &offset) {
    auto clientConnection = getClientConnection(clientHandle);
    auto &elfMap = clientConnection->elfMap;
    auto accessVA = desc->address;
    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    elfData = nullptr;

    if (elfMap.size() > 0) {
        uint64_t baseVa;
        uint64_t ceilVa;
        for (auto &elf : elfMap) {
            baseVa = elf.first;
            ceilVa = elf.first + clientConnection->getElfSize(elf.second);
            if (accessVA >= baseVa && accessVA < ceilVa) {
                if (accessVA + size > ceilVa) {
                    status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
                } else {
                    DEBUG_BREAK_IF(clientConnection->getElfData(elf.second) == nullptr);
                    elfData = clientConnection->getElfData(elf.second);
                    offset = accessVA - baseVa;
                    status = ZE_RESULT_SUCCESS;
                }
                break;
            }
        }
    }

    return status;
}

ze_result_t DebugSessionLinux::updateStoppedThreadsAndCheckTriggerEvents(const AttentionEventFields &attention, uint32_t tileIndex, std::vector<EuThread::ThreadId> &threadsWithAttention) {
    auto vmHandle = getVmHandleFromClientAndlrcHandle(attention.clientHandle, attention.lrcHandle);
    if (vmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();

    if (threadsWithAttention.size() == 0) {
        threadsWithAttention = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, tileIndex, attention.bitmask, attention.bitmaskSize);
        printBitmask(attention.bitmask, attention.bitmaskSize);
    }

    PRINT_DEBUGGER_THREAD_LOG("ATTENTION for tile = %d thread count = %d\n", tileIndex, (int)threadsWithAttention.size());

    if (threadsWithAttention.size() > 0) {
        auto gpuVa = getContextStateSaveAreaGpuVa(vmHandle);
        auto stateSaveAreaSize = getContextStateSaveAreaSize(vmHandle);
        auto stateSaveReadResult = ZE_RESULT_ERROR_UNKNOWN;

        std::unique_lock<std::mutex> lock;

        if (tileSessionsEnabled) {
            lock = getThreadStateMutexForTileSession(tileIndex);
        } else {
            lock = std::unique_lock<std::mutex>(threadStateMutex);
        }

        if (gpuVa != 0 && stateSaveAreaSize != 0) {

            std::vector<EuThread::ThreadId> newThreads;
            getNotStoppedThreads(threadsWithAttention, newThreads);

            if (newThreads.size() > 0) {
                allocateStateSaveAreaMemory(stateSaveAreaSize);
                stateSaveReadResult = readGpuMemory(vmHandle, stateSaveAreaMemory.data(), stateSaveAreaSize, gpuVa);
            }
        } else {
            PRINT_DEBUGGER_ERROR_LOG("Context state save area bind info invalid\n", "");
            DEBUG_BREAK_IF(true);
        }

        if (stateSaveReadResult == ZE_RESULT_SUCCESS) {
            for (auto &threadId : threadsWithAttention) {
                PRINT_DEBUGGER_THREAD_LOG("ATTENTION event for thread: %s\n", EuThread::toString(threadId).c_str());
                updateContextAndLrcHandlesForThreadsWithAttention(threadId, attention);

                if (tileSessionsEnabled) {
                    addThreadToNewlyStoppedFromRaisedAttentionForTileSession(threadId, vmHandle, stateSaveAreaMemory.data(), tileIndex);
                } else {
                    addThreadToNewlyStoppedFromRaisedAttention(threadId, vmHandle, stateSaveAreaMemory.data());
                }
            }
        }
    }
    if (tileSessionsEnabled) {
        checkTriggerEventsForAttentionForTileSession(tileIndex);
    } else {
        checkTriggerEventsForAttention();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionLinux::getISAVMHandle(uint32_t deviceIndex, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle) {
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto accessVA = gmmHelper->decanonize(desc->address);
    auto &isaMap = getClientConnection(clientHandle)->isaMap[deviceIndex];
    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    vmHandle = invalidHandle;

    if (isaMap.size() > 0) {
        uint64_t baseVa;
        uint64_t ceilVa;
        for (const auto &isa : isaMap) {
            baseVa = isa.second->bindInfo.gpuVa;
            ceilVa = isa.second->bindInfo.gpuVa + isa.second->bindInfo.size;
            if (accessVA >= baseVa && accessVA < ceilVa) {
                if (accessVA + size > ceilVa) {
                    status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
                } else {
                    vmHandle = isa.second->vmHandle;
                    status = ZE_RESULT_SUCCESS;
                }
                break;
            }
        }
    }

    return status;
}

bool DebugSessionLinux::getIsaInfoForAllInstances(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t vmHandles[], ze_result_t &status) {
    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto accessVA = gmmHelper->decanonize(desc->address);

    status = ZE_RESULT_ERROR_UNINITIALIZED;

    bool tileInstancedIsa = false;
    bool invalidIsaRange = false;
    uint32_t isaFound = 0;

    for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
        vmHandles[i] = invalidHandle;

        if (deviceBitfield.test(i)) {
            auto &isaMap = getClientConnection(clientHandle)->isaMap[i];
            if (isaMap.size() > 0) {
                uint64_t baseVa;
                uint64_t ceilVa;
                for (const auto &isa : isaMap) {
                    baseVa = isa.second->bindInfo.gpuVa;
                    ceilVa = isa.second->bindInfo.gpuVa + isa.second->bindInfo.size;
                    if (accessVA >= baseVa && accessVA < ceilVa) {
                        isaFound++;
                        if (accessVA + size > ceilVa) {
                            invalidIsaRange = true;
                        } else {
                            vmHandles[i] = isa.second->vmHandle;
                        }
                        tileInstancedIsa = isa.second->tileInstanced;
                        break;
                    }
                }
            }
        }
    }
    if (invalidIsaRange) {
        status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else if (isaFound > 0) {
        if ((tileInstancedIsa && deviceBitfield.count() == isaFound) ||
            !tileInstancedIsa) {
            status = ZE_RESULT_SUCCESS;
        }
    }

    return isaFound > 0;
}

bool DebugSessionLinux::readModuleDebugArea() {
    auto vm = getClientConnection(clientHandle)->vmToModuleDebugAreaBindInfo.begin()->first;
    auto gpuVa = getClientConnection(clientHandle)->vmToModuleDebugAreaBindInfo.begin()->second.gpuVa;

    memset(this->debugArea.magic, 0, sizeof(this->debugArea.magic));
    auto retVal = readGpuMemory(vm, reinterpret_cast<char *>(&this->debugArea), sizeof(this->debugArea), gpuVa);

    if (retVal != ZE_RESULT_SUCCESS || strncmp(this->debugArea.magic, "dbgarea", sizeof(NEO::DebugAreaHeader::magic)) != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Reading Module Debug Area failed, error = %d\n", retVal);
        return false;
    }

    return true;
}

DebugSessionLinux::DebugAreaInfo DebugSessionLinux::getModuleDebugAreaInfo() {
    auto gpuVa = getClientConnection(clientHandle)->vmToModuleDebugAreaBindInfo.begin()->second.gpuVa;
    auto size = getClientConnection(clientHandle)->vmToModuleDebugAreaBindInfo.begin()->second.size;
    return {gpuVa, size};
}

ze_result_t DebugSessionLinux::readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) {
    auto vmHandle = allThreads[threadId]->getMemoryHandle();

    if (vmHandle == invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto gpuVa = getSbaBufferGpuVa(vmHandle);
    if (gpuVa == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return readGpuMemory(vmHandle, reinterpret_cast<char *>(&sbaBuffer), sizeof(sbaBuffer), gpuVa);
}

uint64_t DebugSessionLinux::getSbaBufferGpuVa(uint64_t memoryHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    auto bindInfo = getClientConnection(clientHandle)->vmToStateBaseAreaBindInfo.find(memoryHandle);
    if (bindInfo == getClientConnection(clientHandle)->vmToStateBaseAreaBindInfo.end()) {
        return 0;
    }

    return bindInfo->second.gpuVa;
}

uint64_t DebugSessionLinux::getContextStateSaveAreaGpuVa(uint64_t memoryHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    auto bindInfo = getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.find(memoryHandle);
    if (bindInfo == getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.end()) {
        return 0;
    }

    return bindInfo->second.gpuVa;
}

size_t DebugSessionLinux::getContextStateSaveAreaSize(uint64_t memoryHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);
    if (getClientConnection(clientHandle)->contextStateSaveAreaSize != 0) {
        return getClientConnection(clientHandle)->contextStateSaveAreaSize;
    }

    auto bindInfo = getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.find(memoryHandle);
    if (bindInfo == getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.end()) {
        return 0;
    }
    getClientConnection(clientHandle)->contextStateSaveAreaSize = static_cast<size_t>(bindInfo->second.size);
    return getClientConnection(clientHandle)->contextStateSaveAreaSize;
}

void DebugSessionLinux::readStateSaveAreaHeader() {
    if (clientHandle == invalidClientHandle) {
        return;
    }

    uint64_t vm = 0;
    uint64_t gpuVa = 0;
    size_t totalSize = 0;

    {
        std::lock_guard<std::mutex> lock(asyncThreadMutex);
        if (getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.size() > 0) {
            vm = getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.begin()->first;
            gpuVa = getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.begin()->second.gpuVa;
            totalSize = getClientConnection(clientHandle)->vmToContextStateSaveAreaBindInfo.begin()->second.size;
        }
    }

    if (gpuVa > 0) {
        auto headerSize = sizeof(SIP::StateSaveAreaHeader);

        if (totalSize < headerSize) {
            PRINT_DEBUGGER_ERROR_LOG("Context State Save Area size incorrect\n", "");
            return;
        } else {
            validateAndSetStateSaveAreaHeader(vm, gpuVa);
        }
    }
}

bool DebugSessionLinux::ackIsaEvents(uint32_t deviceIndex, uint64_t isaVa) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    auto connection = getClientConnection(clientHandle).get();

    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    auto isaVaStart = gmmHelper->decanonize(isaVa);
    auto isa = connection->isaMap[deviceIndex].find(isaVaStart);

    if (isa != connection->isaMap[deviceIndex].end()) {

        // zebin modules do not store ackEvents per ISA
        UNRECOVERABLE_IF(isa->second->ackEvents.size() > 0 && isa->second->perKernelModule == false);

        for (auto &event : isa->second->ackEvents) {
            eventAckIoctl(event);
        }

        isa->second->ackEvents.clear();
        isa->second->moduleLoadEventAck = true;
        return true;
    }
    return false;
}

bool DebugSessionLinux::ackModuleEvents(uint32_t deviceIndex, uint64_t moduleHandle) {
    std::lock_guard<std::mutex> lock(asyncThreadMutex);

    auto &module = getModule(moduleHandle);
    for (auto &event : module.ackEvents[deviceIndex]) {
        eventAckIoctl(event);
    }
    module.ackEvents[deviceIndex].clear();
    module.moduleLoadEventAcked[deviceIndex] = true;
    return true;
}

ze_result_t DebugSessionLinux::acknowledgeEvent(const zet_debug_event_t *event) {

    const zet_debug_event_t apiEventToAck = *event;
    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);

        for (size_t i = 0; i < eventsToAck.size(); i++) {
            if (apiEventCompare(apiEventToAck, eventsToAck[i].first)) {

                auto moduleHandle = eventsToAck[i].second;
                auto iter = eventsToAck.begin() + i;
                eventsToAck.erase(iter);

                lock.unlock();

                for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
                    if (connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                        ackModuleEvents(i, moduleHandle);
                    }
                }

                return ZE_RESULT_SUCCESS;
            }
        }
    }

    if (apiEventToAck.type == ZET_DEBUG_EVENT_TYPE_MODULE_LOAD) {
        bool allIsaAcked = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (!ackIsaEvents(i, apiEventToAck.info.module.load)) {
                    allIsaAcked = false;
                }
            }
        }
        if (allIsaAcked) {
            return ZE_RESULT_SUCCESS;
        }
    }

    return ZE_RESULT_ERROR_UNINITIALIZED;
}

bool DebugSessionLinux::closeConnection() {
    closeAsyncThread();
    closeInternalEventsThread();

    if (clientHandle != invalidClientHandle) {
        auto numTiles = std::max(1u, connectedDevice->getNEODevice()->getNumSubDevices());
        for (uint32_t tileIndex = 0; tileIndex < numTiles; tileIndex++) {
            for (const auto &eventToAck : eventsToAck) {
                auto moduleHandle = eventToAck.second;
                ackModuleEvents(tileIndex, moduleHandle);
            }
            cleanRootSessionAfterDetach(tileIndex);
        }
    }

    return closeFd();
}

void DebugSessionLinux::cleanRootSessionAfterDetach(uint32_t deviceIndex) {
    auto connection = getClientConnection(clientHandle).get();

    for (const auto &isa : connection->isaMap[deviceIndex]) {

        // zebin modules do not store ackEvents per ISA
        UNRECOVERABLE_IF(isa.second->ackEvents.size() > 0 && isa.second->perKernelModule == false);

        for (auto &event : isa.second->ackEvents) {
            eventAckIoctl(event);
        }

        isa.second->ackEvents.clear();
        isa.second->moduleLoadEventAck = true;
    }
}

void DebugSessionLinux::handlePageFaultEvent(PageFaultEvent &pfEvent) {

    DEBUG_BREAK_IF(pfEvent.bitmaskSize % 3u != 0u);
    size_t size = pfEvent.bitmaskSize / 3;
    uint8_t *bitmaskBefore = &pfEvent.bitmask[0];
    uint8_t *bitmaskAfter = &pfEvent.bitmask[size];
    uint8_t *bitmaskResolved = &pfEvent.bitmask[size * 2];
    PRINT_DEBUGGER_INFO_LOG("PageFault event BEFORE", 0);
    printBitmask(bitmaskBefore, size);
    PRINT_DEBUGGER_INFO_LOG("PageFault event AFTER", 0);
    printBitmask(bitmaskAfter, size);
    PRINT_DEBUGGER_INFO_LOG("PageFault event RESOLVED", 0);
    printBitmask(bitmaskResolved, size);

    if (!connectedDevice->getNEODevice()->getDeviceBitfield().test(pfEvent.tileIndex)) {
        return;
    }

    std::unique_ptr<uint8_t[]> bitmaskPF = std::make_unique<uint8_t[]>(size);
    std::transform(bitmaskAfter, bitmaskAfter + size, bitmaskResolved, bitmaskPF.get(), std::bit_xor<uint8_t>());
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getL0GfxCoreHelper();
    auto threadsWithPF = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, pfEvent.tileIndex, bitmaskPF.get(), size);
    auto stoppedThreads = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, pfEvent.tileIndex, bitmaskResolved, size);

    if (threadsWithPF.size() == 0) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_PAGE_FAULT;
        debugEvent.info.page_fault.address = pfEvent.pageFaultAddress;
        PRINT_DEBUGGER_INFO_LOG("PageFault event for unknown thread", 0);
        if (tileSessionsEnabled) {
            pushApiEventForTileSession(pfEvent.tileIndex, debugEvent);
        } else {
            enqueueApiEvent(debugEvent);
        }
    }

    auto gpuVa = getContextStateSaveAreaGpuVa(pfEvent.vmHandle);
    auto stateSaveAreaSize = getContextStateSaveAreaSize(pfEvent.vmHandle);
    allocateStateSaveAreaMemory(stateSaveAreaSize);
    auto stateSaveReadResult = readGpuMemory(pfEvent.vmHandle, stateSaveAreaMemory.data(), stateSaveAreaSize, gpuVa);
    if (stateSaveReadResult == ZE_RESULT_SUCCESS) {

        std::unique_lock<std::mutex> lock;
        if (tileSessionsEnabled) {
            lock = getThreadStateMutexForTileSession(pfEvent.tileIndex);
        } else {
            lock = std::unique_lock<std::mutex>(threadStateMutex);
        }
        for (auto &threadId : threadsWithPF) {
            PRINT_DEBUGGER_INFO_LOG("PageFault event for thread %s", EuThread::toString(threadId).c_str());
            if (tileSessionsEnabled) {
                setPageFaultForTileSession(pfEvent.tileIndex, threadId, true);
            } else {
                allThreads[threadId]->setPageFault(true);
            }
        }
        for (auto &threadId : stoppedThreads) {
            if (tileSessionsEnabled) {
                addThreadToNewlyStoppedFromRaisedAttentionForTileSession(threadId, pfEvent.vmHandle, stateSaveAreaMemory.data(), pfEvent.tileIndex);
            } else {
                addThreadToNewlyStoppedFromRaisedAttention(threadId, pfEvent.vmHandle, stateSaveAreaMemory.data());
            }
        }
    }

    if (tileSessionsEnabled) {
        checkTriggerEventsForAttentionForTileSession(pfEvent.tileIndex);
    } else {
        checkTriggerEventsForAttention();
    }
    return;
}

} // namespace L0
