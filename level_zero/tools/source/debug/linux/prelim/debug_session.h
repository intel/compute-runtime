/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace NEO {
class Thread;
struct EngineClassInstance;
} // namespace NEO

namespace L0 {
struct TileDebugSessionLinux;

struct DebugSessionLinux : DebugSessionImp {

    friend struct TileDebugSessionLinux;

    ~DebugSessionLinux() override;
    DebugSessionLinux(const zet_debug_config_t &config, Device *device, int debugFd);

    ze_result_t initialize() override;

    bool closeConnection() override;
    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override;
    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override;
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override;

    uint32_t getDeviceIndexFromApiThread(ze_device_thread_t thread) override;
    ze_device_thread_t convertToPhysicalWithinDevice(ze_device_thread_t thread, uint32_t deviceIndex) override;
    EuThread::ThreadId convertToThreadId(ze_device_thread_t thread) override;
    ze_device_thread_t convertToApi(EuThread::ThreadId threadId) override;

    struct IoctlHandler {
        MOCKABLE_VIRTUAL ~IoctlHandler() = default;
        MOCKABLE_VIRTUAL int ioctl(int fd, unsigned long request, void *arg) {
            int ret = 0;
            do {
                ret = NEO::SysCalls::ioctl(fd, request, arg);
            } while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EBUSY));
            return ret;
        }

        MOCKABLE_VIRTUAL int poll(pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
            return NEO::SysCalls::poll(pollFd, numberOfFds, timeout);
        }

        MOCKABLE_VIRTUAL int64_t pread(int fd, void *buf, size_t count, off_t offset) {
            return NEO::SysCalls::pread(fd, buf, count, offset);
        }

        MOCKABLE_VIRTUAL int64_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
            return NEO::SysCalls::pwrite(fd, buf, count, offset);
        }

        MOCKABLE_VIRTUAL void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
            return NEO::SysCalls::mmap(addr, size, prot, flags, fd, off);
        }

        MOCKABLE_VIRTUAL int munmap(void *addr, size_t size) {
            return NEO::SysCalls::munmap(addr, size);
        }
    };
    static constexpr size_t maxEventSize = 4096;

    using ContextHandle = uint64_t;

    struct ContextParams {
        ContextHandle handle = 0;
        uint64_t vm = UINT64_MAX;
        std::vector<i915_engine_class_instance> engines;
    };

    struct UuidData {
        uint64_t handle = 0;
        uint64_t classHandle = 0;
        NEO::DrmResourceClass classIndex = NEO::DrmResourceClass::MaxSize;
        std::unique_ptr<char[]> data;
        size_t dataSize = 0;

        uint64_t ptr = 0;
    };

    struct BindInfo {
        uint64_t gpuVa = 0;
        uint64_t size = 0;
    };

    struct IsaAllocation {
        BindInfo bindInfo;
        uint64_t elfUuidHandle;
        uint64_t vmHandle;
        bool tileInstanced = false;
        bool perKernelModule = true;
        NEO::DeviceBitfield deviceBitfield;

        uint64_t moduleBegin;
        uint64_t moduleEnd;

        std::unordered_set<uint64_t> cookies;
        int vmBindCounter = 0;
        bool moduleLoadEventAck = false;
        std::vector<prelim_drm_i915_debug_event> ackEvents;
    };

    struct Module {
        std::unordered_set<uint64_t> loadAddresses[NEO::EngineLimits::maxHandleCount];
        uint64_t elfUuidHandle;
        uint32_t segmentCount;
        int segmentVmBindCounter[NEO::EngineLimits::maxHandleCount];
    };

    static bool apiEventCompare(const zet_debug_event_t &event1, const zet_debug_event_t &event2) {
        return memcmp(&event1, &event2, sizeof(zet_debug_event_t)) == 0;
    };

    struct ClientConnection {
        prelim_drm_i915_debug_event_client client = {};

        std::unordered_map<ContextHandle, ContextParams> contextsCreated;
        std::unordered_map<uint64_t, std::pair<std::string, uint32_t>> classHandleToIndex;
        std::unordered_map<uint64_t, UuidData> uuidMap;
        std::unordered_set<uint64_t> vmIds;

        std::unordered_map<uint64_t, BindInfo> vmToModuleDebugAreaBindInfo;
        std::unordered_map<uint64_t, BindInfo> vmToContextStateSaveAreaBindInfo;
        std::unordered_map<uint64_t, BindInfo> vmToStateBaseAreaBindInfo;
        std::unordered_map<uint64_t, uint32_t> vmToTile;

        std::unordered_map<uint64_t, std::unique_ptr<IsaAllocation>> isaMap[NEO::EngineLimits::maxHandleCount];
        std::unordered_map<uint64_t, uint64_t> elfMap;
        std::unordered_map<uint64_t, ContextHandle> lrcToContextHandle;

        uint64_t moduleDebugAreaGpuVa = 0;
        uint64_t contextStateSaveAreaGpuVa = 0;
        uint64_t stateBaseAreaGpuVa = 0;

        std::unordered_map<uint64_t, Module> uuidToModule;
    };

    static ze_result_t translateDebuggerOpenErrno(int error);

    constexpr static uint64_t invalidClientHandle = std::numeric_limits<uint64_t>::max();
    constexpr static uint64_t invalidHandle = std::numeric_limits<uint64_t>::max();

  protected:
    enum class ThreadControlCmd {
        Interrupt,
        Resume,
        Stopped,
        InterruptAll
    };

    MOCKABLE_VIRTUAL void handleEvent(prelim_drm_i915_debug_event *event);
    bool checkAllEventsCollected();
    ze_result_t readEventImp(prelim_drm_i915_debug_event *drmDebugEvent);
    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override;
    ze_result_t interruptImp(uint32_t deviceIndex) override;

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        pushApiEvent(debugEvent, nullptr);
    }

    void pushApiEvent(zet_debug_event_t &debugEvent, prelim_drm_i915_debug_event *baseEvent) {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);

        if (baseEvent && (baseEvent->flags & PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK)) {
            prelim_drm_i915_debug_event_ack eventToAck = {};
            eventToAck.type = baseEvent->type;
            eventToAck.seqno = baseEvent->seqno;
            eventToAck.flags = 0;
            debugEvent.flags = ZET_DEBUG_EVENT_FLAG_NEED_ACK;

            eventsToAck.push_back(
                std::pair<zet_debug_event_t, prelim_drm_i915_debug_event_ack>(debugEvent, eventToAck));
        }

        apiEvents.push(debugEvent);

        apiEventCondition.notify_all();
    }

    MOCKABLE_VIRTUAL void createTileSessionsIfEnabled();
    MOCKABLE_VIRTUAL TileDebugSessionLinux *createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession);

    static void *asyncThreadFunction(void *arg);
    static void *readInternalEventsThreadFunction(void *arg);
    void startAsyncThread() override;
    void closeAsyncThread();

    void startInternalEventsThread() {
        internalEventThread.thread = NEO::Thread::create(readInternalEventsThreadFunction, reinterpret_cast<void *>(this));
    }
    void closeInternalEventsThread() {
        internalEventThread.close();
    }

    bool closeFd();

    virtual std::vector<uint64_t> getAllMemoryHandles() {
        std::vector<uint64_t> allVms;
        std::unique_lock<std::mutex> memLock(asyncThreadMutex);

        auto &vmIds = clientHandleToConnection[clientHandle]->vmIds;
        allVms.resize(vmIds.size());
        std::copy(vmIds.begin(), vmIds.end(), allVms.begin());
        return allVms;
    }

    void handleEventsAsync();
    void readInternalEventsAsync();
    MOCKABLE_VIRTUAL std::unique_ptr<uint64_t[]> getInternalEvent();

    void handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind);
    void handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam);
    void handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention);
    void handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines);
    virtual bool ackIsaEvents(uint32_t deviceIndex, uint64_t isaVa);

    void attachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void detachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override;

    void extractUuidData(uint64_t client, const UuidData &uuidData);
    uint64_t extractVaFromUuidString(std::string &uuid);

    bool readModuleDebugArea() override;
    ze_result_t readSbaBuffer(EuThread::ThreadId, NEO::SbaTrackedAddresses &sbaBuffer) override;
    void readStateSaveAreaHeader() override;

    ze_result_t readGpuMemory(uint64_t vmHandle, char *output, size_t size, uint64_t gpuVa) override;
    ze_result_t writeGpuMemory(uint64_t vmHandle, const char *input, size_t size, uint64_t gpuVa) override;
    ze_result_t getISAVMHandle(uint32_t deviceIndex, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle);
    bool getIsaInfoForAllInstances(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t vmHandles[], ze_result_t &status);

    ze_result_t getElfOffset(const zet_debug_memory_space_desc_t *desc, size_t size, const char *&elfData, uint64_t &offset);
    ze_result_t readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer,
                             const char *&elfData, const uint64_t offset);
    virtual bool tryReadElf(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status);

    bool tryWriteIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer, ze_result_t &status);
    bool tryReadIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status);
    virtual bool tryAccessIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write, ze_result_t &status);
    ze_result_t accessDefaultMemForThreadAll(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write);

    MOCKABLE_VIRTUAL int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize);

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override;
    virtual uint64_t getSbaBufferGpuVa(uint64_t memoryHandle);
    void printContextVms();

    bool isTileWithinDeviceBitfield(uint32_t tileIndex) {
        return connectedDevice->getNEODevice()->getDeviceBitfield().test(tileIndex);
    }

    bool checkAllOtherTileIsaAllocationsPresent(uint32_t tileIndex, uint64_t isaVa) {
        bool allInstancesPresent = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (i != tileIndex && connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (clientHandleToConnection[clientHandle]->isaMap[i].find(isaVa) == clientHandleToConnection[clientHandle]->isaMap[i].end()) {
                    allInstancesPresent = false;
                    break;
                }
            }
        }
        return allInstancesPresent;
    }

    bool checkAllOtherTileIsaAllocationsRemoved(uint32_t tileIndex, uint64_t isaVa) {
        bool allInstancesRemoved = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (i != tileIndex && connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (clientHandleToConnection[clientHandle]->isaMap[i].find(isaVa) != clientHandleToConnection[clientHandle]->isaMap[i].end()) {
                    allInstancesRemoved = false;
                    break;
                }
            }
        }
        return allInstancesRemoved;
    }

    ThreadHelper internalEventThread;
    std::mutex internalEventThreadMutex;
    std::condition_variable internalEventCondition;
    std::queue<std::unique_ptr<uint64_t[]>> internalEventQueue;
    std::vector<std::pair<zet_debug_event_t, prelim_drm_i915_debug_event_ack>> eventsToAck;

    int fd = 0;
    virtual int ioctl(unsigned long request, void *arg);
    std::unique_ptr<IoctlHandler> ioctlHandler;
    std::atomic<bool> detached{false};

    uint64_t clientHandle = invalidClientHandle;
    uint64_t clientHandleClosed = invalidClientHandle;
    std::unordered_map<uint64_t, uint32_t> uuidL0CommandQueueHandleToDevice;
    uint64_t euControlInterruptSeqno[NEO::EngineLimits::maxHandleCount];

    std::unordered_map<uint64_t, std::unique_ptr<ClientConnection>> clientHandleToConnection;
};

struct TileDebugSessionLinux : DebugSessionLinux {
    TileDebugSessionLinux(zet_debug_config_t config, Device *device, DebugSessionImp *rootDebugSession) : DebugSessionLinux(config, device, 0),
                                                                                                          rootDebugSession(reinterpret_cast<DebugSessionLinux *>(rootDebugSession)) {
        tileIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));
    }

    ~TileDebugSessionLinux() override = default;

    bool closeConnection() override { return true; }
    ze_result_t initialize() override { return ZE_RESULT_SUCCESS; }

    bool insertModule(zet_debug_event_info_module_t module);
    bool removeModule(zet_debug_event_info_module_t module);
    bool processEntry();
    bool processExit();
    void attachTile() override;
    void detachTile() override;

  protected:
    void startAsyncThread() override { UNRECOVERABLE_IF(true); };
    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override { UNRECOVERABLE_IF(true); };

    bool readModuleDebugArea() override { return true; };

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        return rootDebugSession->getContextStateSaveAreaGpuVa(memoryHandle);
    };

    void readStateSaveAreaHeader() override;

    uint64_t getSbaBufferGpuVa(uint64_t memoryHandle) override {
        return rootDebugSession->getSbaBufferGpuVa(memoryHandle);
    }

    int ioctl(unsigned long request, void *arg) override {
        return rootDebugSession->ioctl(request, arg);
    }

    std::vector<uint64_t> getAllMemoryHandles() override {
        return rootDebugSession->getAllMemoryHandles();
    }

    bool tryReadElf(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, ze_result_t &status) override {

        return rootDebugSession->tryReadElf(desc, size, buffer, status);
    }

    bool tryAccessIsa(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer, bool write, ze_result_t &status) override {
        return rootDebugSession->tryAccessIsa(deviceBitfield, desc, size, buffer, write, status);
    }

    bool ackIsaEvents(uint32_t deviceIndex, uint64_t isaVa) override {
        return rootDebugSession->ackIsaEvents(this->tileIndex, isaVa);
    }

    ze_result_t readGpuMemory(uint64_t vmHandle, char *output, size_t size, uint64_t gpuVa) override {
        return rootDebugSession->readGpuMemory(vmHandle, output, size, gpuVa);
    }

    ze_result_t writeGpuMemory(uint64_t vmHandle, const char *input, size_t size, uint64_t gpuVa) override {
        return rootDebugSession->writeGpuMemory(vmHandle, input, size, gpuVa);
    }

    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        return rootDebugSession->resumeImp(threads, this->tileIndex);
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        return rootDebugSession->interruptImp(this->tileIndex);
    }

    DebugSessionLinux *rootDebugSession = nullptr;
    uint32_t tileIndex = std::numeric_limits<uint32_t>::max();
    bool isAttached = false;
    bool processEntryState = false;

    std::unordered_map<uint64_t, zet_debug_event_info_module_t> modules;
};

} // namespace L0
