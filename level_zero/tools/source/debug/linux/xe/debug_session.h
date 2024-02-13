/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

namespace L0 {

struct DebugSessionLinuxXe : DebugSessionLinux {

    ~DebugSessionLinuxXe() override;
    DebugSessionLinuxXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params);
    static DebugSession *createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach);

    ze_result_t initialize() override;

    bool closeConnection() override;

    ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t acknowledgeEvent(const zet_debug_event_t *event) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    struct IoctlHandlerXe : DebugSessionLinux::IoctlHandler {
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

                    if (request == DRM_XE_EUDEBUG_IOCTL_EU_CONTROL) {
                        shouldRetryIoctl = (error == EINTR || error == EAGAIN);
                    }
                }
            } while (shouldRetryIoctl);
            return ret;
        }
    };

    using ContextHandle = uint64_t;
    using ExecQueueHandle = uint64_t;

    struct ContextParams {
        ContextHandle handle = 0;
        uint64_t vm = UINT64_MAX;
        std::vector<drm_xe_engine_class_instance> engines;
    };

    struct ExecQueueParams {
        uint64_t vmHandle = 0;
        uint16_t engineClass = UINT16_MAX;
    };

    uint32_t xeDebuggerVersion = 0;

    std::shared_ptr<ClientConnection> getClientConnection(uint64_t clientHandle) override {
        return clientHandleToConnection[clientHandle];
    };

  protected:
    int euControlIoctl(ThreadControlCmd threadCmd,
                       const NEO::EngineClassInstance *classInstance,
                       std::unique_ptr<uint8_t[]> &bitmask,
                       size_t bitmaskSize, uint64_t &seqnoOut, uint64_t &bitmaskSizeOut) override;

    void startAsyncThread() override;
    static void *asyncThreadFunction(void *arg);
    void handleEventsAsync();

    bool readModuleDebugArea() override {
        UNRECOVERABLE_IF(true);
        return true;
    }

    ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    void cleanRootSessionAfterDetach(uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
    }

    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        UNRECOVERABLE_IF(true);
        return ZE_RESULT_SUCCESS;
    }

    int openVmFd(uint64_t vmHandle, bool readOnly) override;
    int flushVmCache(int vmfd) override;

    void enqueueApiEvent(zet_debug_event_t &debugEvent) override {
        UNRECOVERABLE_IF(true);
    }

    uint64_t getContextStateSaveAreaGpuVa(uint64_t memoryHandle) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    size_t getContextStateSaveAreaSize(uint64_t memoryHandle) override {
        UNRECOVERABLE_IF(true);
        return 0;
    }

    void attachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void detachTile() override {
        UNRECOVERABLE_IF(true);
    }

    struct ClientConnectionXe : public ClientConnection {
        drm_xe_eudebug_event_client client = {};
        size_t getElfSize(uint64_t elfHandle) override { return 0; };
        char *getElfData(uint64_t elfHandle) override { return nullptr; };

        std::unordered_map<ExecQueueHandle, ExecQueueParams> execQueues;
        std::unordered_map<uint64_t, uint64_t> lrcHandleToVmHandle;
    };
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnectionXe>> clientHandleToConnection;

    std::vector<std::unique_ptr<uint64_t[]>> pendingVmBindEvents;
    bool checkAllEventsCollected();
    MOCKABLE_VIRTUAL void handleEvent(drm_xe_eudebug_event *event);
    void readInternalEventsAsync() override;
    void pushApiEvent(zet_debug_event_t &debugEvent);
    ze_result_t getISAVMHandle(uint32_t deviceIndex, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t &vmHandle) override { return ZE_RESULT_ERROR_UNKNOWN; };
    bool getIsaInfoForAllInstances(NEO::DeviceBitfield deviceBitfield, const zet_debug_memory_space_desc_t *desc, size_t size, uint64_t vmHandles[], ze_result_t &status) override { return false; };

    std::atomic<bool> detached{false};

    ze_result_t readEventImp(drm_xe_eudebug_event *drmDebugEvent);
    int ioctl(unsigned long request, void *arg);
    std::atomic<bool> processEntryEventGenerated = false;
};

} // namespace L0
