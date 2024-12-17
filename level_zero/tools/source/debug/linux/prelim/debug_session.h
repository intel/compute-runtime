/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace NEO {
class Thread;
struct EngineClassInstance;
} // namespace NEO

namespace L0 {
struct TileDebugSessionLinuxi915;

struct DebugSessionLinuxi915 : DebugSessionLinux {

    friend struct TileDebugSessionLinuxi915;

    ~DebugSessionLinuxi915() override;
    DebugSessionLinuxi915(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

    static DebugSession *createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach);

    struct IoctlHandleri915 : DebugSessionLinux::IoctlHandler {
        int ioctl(int fd, unsigned long request, void *arg) override {
            int ret = 0;
            int error = 0;
            bool shouldRetryIoctl = false;
            do {
                shouldRetryIoctl = false;
                ret = NEO::SysCalls::ioctl(fd, request, arg);
                error = errno;

                if (ret == -1) {
                    shouldRetryIoctl = (error == EINTR || error == EAGAIN || error == EBUSY);

                    if (request == PRELIM_I915_DEBUG_IOCTL_EU_CONTROL) {
                        shouldRetryIoctl = (error == EINTR || error == EAGAIN);
                    }
                }
            } while (shouldRetryIoctl);
            return ret;
        }
    };

    using ContextHandle = uint64_t;

    struct ContextParams {
        ContextHandle handle = 0;
        uint64_t vm = UINT64_MAX;
        std::vector<i915_engine_class_instance> engines;
    };

    struct UuidData {
        uint64_t handle = 0;
        uint64_t classHandle = 0;
        NEO::DrmResourceClass classIndex = NEO::DrmResourceClass::maxSize;
        std::unique_ptr<char[]> data;
        size_t dataSize = 0;

        uint64_t ptr = 0;
    };

    struct ClientConnectioni915 : public ClientConnection {
        prelim_drm_i915_debug_event_client client = {};

        size_t getElfSize(uint64_t elfHandle) override { return uuidMap[elfHandle].dataSize; };
        char *getElfData(uint64_t elfHandle) override { return uuidMap[elfHandle].data.get(); };

        std::unordered_map<ContextHandle, ContextParams> contextsCreated;
        std::unordered_map<uint64_t, std::pair<std::string, uint32_t>> classHandleToIndex;
        std::unordered_map<uint64_t, UuidData> uuidMap;

        std::unordered_map<uint64_t, ContextHandle> lrcToContextHandle;

        std::unordered_map<uint64_t, Module> uuidToModule;
    };

    std::shared_ptr<ClientConnection> getClientConnection(uint64_t clientHandle) override {
        return clientHandleToConnection[clientHandle];
    };

  protected:
    MOCKABLE_VIRTUAL void handleEvent(prelim_drm_i915_debug_event *event);
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnectioni915>> clientHandleToConnection;

    ze_result_t readEventImp(prelim_drm_i915_debug_event *drmDebugEvent);
    DebugSessionImp *createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession) override;

    static void *asyncThreadFunction(void *arg);
    void startAsyncThread() override;

    bool handleInternalEvent() override;

    void updateContextAndLrcHandlesForThreadsWithAttention(EuThread::ThreadId threadId, const AttentionEventFields &attention) override {}
    uint64_t getVmHandleFromClientAndlrcHandle(uint64_t clientHandle, uint64_t lrcHandle) override;
    bool handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind);
    void handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam);
    void handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention);
    void handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines);
    int eventAckIoctl(EventToAck &event) override;
    Module &getModule(uint64_t moduleHandle) override {
        auto connection = clientHandleToConnection[clientHandle].get();
        DEBUG_BREAK_IF(connection->uuidToModule.find(moduleHandle) == connection->uuidToModule.end());
        return connection->uuidToModule[moduleHandle];
    }

    MOCKABLE_VIRTUAL void processPendingVmBindEvents();

    std::unique_lock<std::mutex> getThreadStateMutexForTileSession(uint32_t tileIndex) override;
    void checkTriggerEventsForAttentionForTileSession(uint32_t tileIndex) override;
    void addThreadToNewlyStoppedFromRaisedAttentionForTileSession(EuThread::ThreadId threadId,
                                                                  uint64_t memoryHandle,
                                                                  const void *stateSaveArea,
                                                                  uint32_t tileIndex) override;
    void pushApiEventForTileSession(uint32_t tileIndex, zet_debug_event_t &debugEvent) override;
    void setPageFaultForTileSession(uint32_t tileIndex, EuThread::ThreadId threadId, bool hasPageFault) override;

    void attachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void detachTile() override {
        UNRECOVERABLE_IF(true);
    }

    void extractUuidData(uint64_t client, const UuidData &uuidData);
    uint64_t extractVaFromUuidString(std::string &uuid);

    int openVmFd(uint64_t vmHandle, bool readOnly) override;

    int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) override;
    void printContextVms();

    std::vector<std::unique_ptr<uint64_t[]>> pendingVmBindEvents;

    uint32_t i915DebuggerVersion = 0;
    virtual int ioctl(unsigned long request, void *arg);
    std::atomic<bool> detached{false};

    std::unordered_map<uint64_t, uint32_t> uuidL0CommandQueueHandleToDevice;
    uint64_t euControlInterruptSeqno[NEO::EngineLimits::maxHandleCount];
    void readInternalEventsAsync() override;

    bool blockOnFenceMode = false; // false - blocking VM_BIND on CPU - autoack events until last blocking event
                                   // true - blocking on fence - do not auto-ack events
};

struct TileDebugSessionLinuxi915 : DebugSessionLinuxi915 {
    TileDebugSessionLinuxi915(zet_debug_config_t config, Device *device, DebugSessionImp *rootDebugSession) : DebugSessionLinuxi915(config, device, 0, nullptr),
                                                                                                              rootDebugSession(reinterpret_cast<DebugSessionLinuxi915 *>(rootDebugSession)) {
        tileIndex = Math::log2(static_cast<uint32_t>(connectedDevice->getNEODevice()->getDeviceBitfield().to_ulong()));
    }

    ~TileDebugSessionLinuxi915() override = default;

    bool closeConnection() override { return true; }
    ze_result_t initialize() override {
        createEuThreads();
        return ZE_RESULT_SUCCESS;
    }

    bool insertModule(zet_debug_event_info_module_t module);
    bool removeModule(zet_debug_event_info_module_t module);
    bool processEntry();
    bool processExit();
    void attachTile() override;
    void detachTile() override;

    bool isAttached = false;

  protected:
    void startAsyncThread() override { UNRECOVERABLE_IF(true); };
    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override { UNRECOVERABLE_IF(true); };

    bool readModuleDebugArea() override { return true; };

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        return rootDebugSession->getContextStateSaveAreaGpuVa(memoryHandle);
    };

    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override {
        return rootDebugSession->getContextStateSaveAreaSize(memoryHandle);
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

    bool ackModuleEvents(uint32_t deviceIndex, uint64_t moduleUuidHandle) override {
        return rootDebugSession->ackModuleEvents(this->tileIndex, moduleUuidHandle);
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

    DebugSessionLinuxi915 *rootDebugSession = nullptr;
    uint32_t tileIndex = std::numeric_limits<uint32_t>::max();
    bool processEntryState = false;

    std::unordered_map<uint64_t, zet_debug_event_info_module_t> modules;
};

} // namespace L0
