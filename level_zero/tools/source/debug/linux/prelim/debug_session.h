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
    ze_result_t initialize() override;

    bool closeConnection() override;
    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override;

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
        uint64_t moduleUuidHandle;
        uint64_t elfUuidHandle;
        uint32_t segmentCount;
        NEO::DeviceBitfield deviceBitfield;
        int segmentVmBindCounter[NEO::EngineLimits::maxHandleCount];

        std::vector<prelim_drm_i915_debug_event> ackEvents[NEO::EngineLimits::maxHandleCount];
        bool moduleLoadEventAcked[NEO::EngineLimits::maxHandleCount];
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

        size_t contextStateSaveAreaSize = 0;

        std::unordered_map<uint64_t, Module> uuidToModule;
    };

  protected:
    MOCKABLE_VIRTUAL void handleEvent(prelim_drm_i915_debug_event *event);
    bool checkAllEventsCollected();
    std::unordered_map<uint64_t, std::unique_ptr<ClientConnection>> clientHandleToConnection;
    ze_result_t readEventImp(prelim_drm_i915_debug_event *drmDebugEvent);

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        pushApiEvent(debugEvent);
    }

    void pushApiEvent(zet_debug_event_t &debugEvent) {
        return pushApiEvent(debugEvent, invalidHandle);
    }

    void pushApiEvent(zet_debug_event_t &debugEvent, uint64_t moduleUuidHandle) {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);

        if (moduleUuidHandle != invalidHandle && (debugEvent.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK)) {
            eventsToAck.push_back(
                std::pair<zet_debug_event_t, uint64_t>(debugEvent, moduleUuidHandle));
        }

        apiEvents.push(debugEvent);

        apiEventCondition.notify_all();
    }

    MOCKABLE_VIRTUAL void createTileSessionsIfEnabled();
    MOCKABLE_VIRTUAL TileDebugSessionLinuxi915 *createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession);

    static void *asyncThreadFunction(void *arg);
    void startAsyncThread() override;

    std::vector<uint64_t> getAllMemoryHandles() override {
        std::vector<uint64_t> allVms;
        std::unique_lock<std::mutex> memLock(asyncThreadMutex);

        auto &vmIds = clientHandleToConnection[clientHandle]->vmIds;
        allVms.resize(vmIds.size());
        std::copy(vmIds.begin(), vmIds.end(), allVms.begin());
        return allVms;
    }

    void handleEventsAsync();

    uint64_t getVmHandleFromClientAndlrcHandle(uint64_t clientHandle, uint64_t lrcHandle);
    bool handleVmBindEvent(prelim_drm_i915_debug_event_vm_bind *vmBind);
    void handleContextParamEvent(prelim_drm_i915_debug_event_context_param *contextParam);
    void handleAttentionEvent(prelim_drm_i915_debug_event_eu_attention *attention);
    void handleEnginesEvent(prelim_drm_i915_debug_event_engines *engines);
    void handlePageFaultEvent(prelim_drm_i915_debug_event_page_fault *pf);
    virtual bool ackIsaEvents(uint32_t deviceIndex, uint64_t isaVa);
    virtual bool ackModuleEvents(uint32_t deviceIndex, uint64_t moduleUuidHandle);
    ze_result_t getElfOffset(const zet_debug_memory_space_desc_t *desc, size_t size, const char *&elfData, uint64_t &offset) override;

    MOCKABLE_VIRTUAL void processPendingVmBindEvents();

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
    int openVmFd(uint64_t vmHandle, bool readOnly) override;

    ze_result_t getISAVMHandle(uint32_t deviceIndex, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle) override;
    bool getIsaInfoForAllInstances(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t vmHandles[], ze_result_t &status) override;

    int euControlIoctl(ThreadControlCmd threadCmd,
                       const NEO::EngineClassInstance *classInstance,
                       std::unique_ptr<uint8_t[]> &bitmask,
                       size_t bitmaskSize, uint64_t &seqnoOut, uint64_t &bitmaskSizeOut) override;
    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override;
    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override;
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

    bool checkAllOtherTileModuleSegmentsPresent(uint32_t tileIndex, const Module &module) {
        bool allInstancesPresent = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (i != tileIndex && connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (module.loadAddresses[i].size() != module.segmentCount) {
                    allInstancesPresent = false;
                    break;
                }
            }
        }
        return allInstancesPresent;
    }

    bool checkAllOtherTileModuleSegmentsRemoved(uint32_t tileIndex, const Module &module) {
        bool allInstancesRemoved = true;
        for (uint32_t i = 0; i < NEO::EngineLimits::maxHandleCount; i++) {
            if (i != tileIndex && connectedDevice->getNEODevice()->getDeviceBitfield().test(i)) {
                if (module.loadAddresses[i].size() != 0) {
                    allInstancesRemoved = false;
                    break;
                }
            }
        }
        return allInstancesRemoved;
    }

    std::vector<std::pair<zet_debug_event_t, uint64_t>> eventsToAck; // debug event, uuid handle to module
    std::vector<std::unique_ptr<uint64_t[]>> pendingVmBindEvents;

    uint32_t i915DebuggerVersion = 0;
    virtual int ioctl(unsigned long request, void *arg);
    std::atomic<bool> detached{false};

    std::unordered_map<uint64_t, uint32_t> uuidL0CommandQueueHandleToDevice;
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
