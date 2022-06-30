/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace NEO {
class Thread;
struct EngineClassInstance;
} // namespace NEO

namespace L0 {

struct DebugSessionLinux : DebugSessionImp {
    ~DebugSessionLinux() override;
    DebugSessionLinux(const zet_debug_config_t &config, Device *device, int debugFd);

    ze_result_t initialize() override;

    bool closeConnection() override;
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override;
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
    using ApiEventQueue = std::queue<zet_debug_event_t>;

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

        uint64_t moduleBegin;
        uint64_t moduleEnd;

        std::unordered_set<uint64_t> cookies;
        int vmBindCounter = 0;
        bool moduleLoadEventAck = false;
        std::vector<prelim_drm_i915_debug_event> ackEvents;
    };

    struct Module {
        std::unordered_set<uint64_t> loadAddresses;
        uint64_t elfUuidHandle;
        uint32_t segmentCount;
        int segmentVmBindCounter;
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

        std::unordered_map<uint64_t, std::unique_ptr<IsaAllocation>> isaMap;
        std::unordered_map<uint64_t, uint64_t> elfMap;
        std::unordered_map<uint64_t, ContextHandle> lrcToContextHandle;

        uint64_t moduleDebugAreaGpuVa = 0;
        uint64_t contextStateSaveAreaGpuVa = 0;
        uint64_t stateBaseAreaGpuVa = 0;

        ApiEventQueue apiEvents;
        std::vector<std::pair<zet_debug_event_t, prelim_drm_i915_debug_event_ack>> eventsToAck;

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
    ze_result_t resumeImp(std::vector<ze_device_thread_t> threads, uint32_t deviceIndex) override;
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

            clientHandleToConnection[clientHandle]->eventsToAck.push_back(
                std::pair<zet_debug_event_t, prelim_drm_i915_debug_event_ack>(debugEvent, eventToAck));
        }

        clientHandleToConnection[clientHandle]->apiEvents.push(debugEvent);

        apiEventCondition.notify_all();
    }

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

    void handleEventsAsync();
    void readInternalEventsAsync();
    MOCKABLE_VIRTUAL std::unique_ptr<uint64_t[]> getInternalEvent();

    void handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind);
    void handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam);
    void handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention);
    void handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines);

    void extractUuidData(uint64_t client, const UuidData &uuidData);
    uint64_t extractVaFromUuidString(std::string &uuid);

    bool readModuleDebugArea() override;
    ze_result_t readSbaBuffer(EuThread::ThreadId, NEO::SbaTrackedAddresses &sbaBuffer) override;
    void readStateSaveAreaHeader() override;

    void applyResumeWa(std::vector<ze_device_thread_t> threads, uint8_t *bitmask, size_t bitmaskSize);

    ze_result_t readGpuMemory(uint64_t vmHandle, char *output, size_t size, uint64_t gpuVa) override;
    ze_result_t writeGpuMemory(uint64_t vmHandle, const char *input, size_t size, uint64_t gpuVa) override;
    ze_result_t getISAVMHandle(const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle);
    ze_result_t getElfOffset(const zet_debug_memory_space_desc_t *desc, size_t size, const char *&elfData, uint64_t &offset);
    ze_result_t readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer,
                             const char *&elfData, const uint64_t offset);

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override;

    MOCKABLE_VIRTUAL int threadControl(std::vector<ze_device_thread_t> threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize);

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override;
    uint64_t getSbaBufferGpuVa(uint64_t memoryHandle);
    void printContextVms();

    ThreadHelper asyncThread;
    std::mutex asyncThreadMutex;
    std::condition_variable apiEventCondition;

    ThreadHelper internalEventThread;
    std::mutex internalEventThreadMutex;
    std::condition_variable internalEventCondition;
    std::queue<std::unique_ptr<uint64_t[]>> internalEventQueue;

    int fd = 0;
    int ioctl(unsigned long request, void *arg);
    std::unique_ptr<IoctlHandler> ioctlHandler;
    std::atomic<bool> detached{false};

    uint64_t clientHandle = invalidClientHandle;
    uint64_t clientHandleClosed = invalidClientHandle;
    uint64_t uuidL0CommandQueueHandle = invalidClientHandle;
    uint64_t euControlInterruptSeqno[NEO::EngineLimits::maxHandleCount];

    std::unordered_map<uint64_t, std::unique_ptr<ClientConnection>> clientHandleToConnection;
};

} // namespace L0
