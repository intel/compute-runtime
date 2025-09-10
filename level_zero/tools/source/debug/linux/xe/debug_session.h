/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"

#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/source/debug/linux/debug_session_factory.h"

namespace L0 {

struct DebugSessionLinuxXe : DebugSessionLinux {

    ~DebugSessionLinuxXe() override;
    DebugSessionLinuxXe(const zet_debug_config_t &config, Device *device, int debugFd, void *params);
    static DebugSession *createLinuxSession(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach);

    struct IoctlHandlerXe : DebugSessionLinux::IoctlHandler {
        IoctlHandlerXe(const NEO::EuDebugInterface &euDebugInterface) : euDebugInterface(euDebugInterface){};
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

                    if (request == euDebugInterface.getParamValue(NEO::EuDebugParam::ioctlEuControl)) {
                        shouldRetryIoctl = (error == EINTR || error == EAGAIN);
                    }
                }
            } while (shouldRetryIoctl);
            return ret;
        }
        const NEO::EuDebugInterface &euDebugInterface;
    };

    using ExecQueueHandle = uint64_t;
    using LrcHandle = uint64_t;

    struct ExecQueueParams {
        uint64_t vmHandle = 0;
        uint16_t engineClass = UINT16_MAX;
        std::vector<LrcHandle> lrcHandles;
    };

    uint32_t xeDebuggerVersion = 0;

    std::shared_ptr<ClientConnection> getClientConnection(uint64_t clientHandle) override {
        return clientHandleToConnection[clientHandle];
    };

  protected:
    int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) override;
    int threadControlInterruptAll();
    int threadControlResume(const std::vector<EuThread::ThreadId> &threads);
    int threadControlStopped(std::unique_ptr<uint8_t[]> &bitmaskOut, size_t &bitmaskSizeOut);
    MOCKABLE_VIRTUAL void handleAttentionEvent(NEO::EuDebugEventEuAttention *attention);
    void handleMetadataEvent(NEO::EuDebugEventMetadata *pMetaData);
    bool handleMetadataOpEvent(NEO::EuDebugEventVmBindOpMetadata *vmBindOpMetadata);
    void updateContextAndLrcHandlesForThreadsWithAttention(EuThread::ThreadId threadId, const AttentionEventFields &attention) override;
    int eventAckIoctl(EventToAck &event) override;
    MOCKABLE_VIRTUAL int getEuControlCmdUnlock() const;
    Module &getModule(uint64_t moduleHandle) override {
        auto connection = clientHandleToConnection[clientHandle].get();
        DEBUG_BREAK_IF(connection->metaDataToModule.find(moduleHandle) == connection->metaDataToModule.end());
        return connection->metaDataToModule[moduleHandle];
    }

    void startAsyncThread() override;
    static void *asyncThreadFunction(void *arg);
    bool handleInternalEvent() override;
    MOCKABLE_VIRTUAL void processPendingVmBindEvents();
    DebugSessionImp *createTileSession(const zet_debug_config_t &config, Device *device, DebugSessionImp *rootDebugSession) override {
        return nullptr;
    }

    int openVmFd(uint64_t vmHandle, bool readOnly) override;
    int flushVmCache(int vmfd) override;

    void attachTile() override {
        UNRECOVERABLE_IF(true);
    }
    void detachTile() override {
        UNRECOVERABLE_IF(true);
    }

    struct MetaData {
        NEO::EuDebugEventMetadata metadata;
        std::unique_ptr<char[]> data;
    };

    using VmBindOpSeqNo = uint64_t;
    using VmBindSeqNo = uint64_t;
    struct VmBindOpData {
        uint64_t pendingNumExtensions = 0;
        NEO::EuDebugEventVmBindOp vmBindOp;
        std::vector<NEO::EuDebugEventVmBindOpMetadata> vmBindOpMetadataVec;
    };

    struct VmBindData {
        uint64_t pendingNumBinds = 0;
        NEO::EuDebugEventVmBind vmBind;
        bool uFenceReceived = false;
        NEO::EuDebugEventVmBindUfence vmBindUfence;
        std::unordered_map<VmBindOpSeqNo, VmBindOpData> vmBindOpMap;
    };

    struct ClientConnectionXe : public ClientConnection {
        NEO::EuDebugEventClient client = {};
        size_t getElfSize(uint64_t elfHandle) override { return metaDataMap[elfHandle].metadata.len; };
        char *getElfData(uint64_t elfHandle) override { return metaDataMap[elfHandle].data.get(); };

        std::unordered_map<ExecQueueHandle, ExecQueueParams> execQueues;
        std::unordered_map<uint64_t, uint64_t> lrcHandleToVmHandle;
        std::unordered_map<uint64_t, MetaData> metaDataMap;
        std::unordered_map<uint64_t, Module> metaDataToModule;
        std::unordered_map<VmBindSeqNo, VmBindData> vmBindMap;
        std::unordered_map<VmBindOpSeqNo, VmBindSeqNo> vmBindIdentifierMap;
    };
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnectionXe>> clientHandleToConnection;
    bool canHandleVmBind(VmBindData &vmBindData) const;
    MOCKABLE_VIRTUAL bool handleVmBind(VmBindData &vmBindData);
    void handleVmBindWithoutUfence(VmBindData &vmBindData, VmBindOpData &vmBindOpData);

    void extractMetaData(uint64_t client, const MetaData &metaData);
    bool checkAllEventsCollected();
    MOCKABLE_VIRTUAL void handleEvent(NEO::EuDebugEvent *event);
    void additionalEvents(NEO::EuDebugEvent *event);
    MOCKABLE_VIRTUAL bool eventTypeIsAttention(uint16_t eventType);
    void readInternalEventsAsync() override;
    std::atomic<bool> detached{false};

    uint64_t getVmHandleFromClientAndlrcHandle(uint64_t clientHandle, uint64_t lrcHandle) override;
    void checkTriggerEventsForAttentionForTileSession(uint32_t tileIndex) override {}
    std::unique_lock<std::mutex> getThreadStateMutexForTileSession(uint32_t tileIndex) override {
        std::mutex m;
        std::unique_lock<std::mutex> lock(m);
        lock.release();
        return lock;
    }
    void addThreadToNewlyStoppedFromRaisedAttentionForTileSession(EuThread::ThreadId threadId,
                                                                  uint64_t memoryHandle,
                                                                  const void *stateSaveArea,
                                                                  uint32_t tileIndex) override {}

    void pushApiEventForTileSession(uint32_t tileIndex, zet_debug_event_t &debugEvent) override { UNRECOVERABLE_IF(true) }
    void setPageFaultForTileSession(uint32_t tileIndex, EuThread::ThreadId threadId, bool hasPageFault) override{UNRECOVERABLE_IF(true)}

    uint64_t euControlInterruptSeqno = 0;

    ze_result_t readEventImp(NEO::EuDebugEvent *drmDebugEvent);
    bool openSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle, uint64_t gpuVa);
    bool closeSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle);
    void closeExternalSipHandles();
    int ioctl(unsigned long request, void *arg);
    std::atomic<bool> processEntryEventGenerated = false;
    std::atomic<uint64_t> newestAttSeqNo = 0;
    std::unique_ptr<NEO::EuDebugInterface> euDebugInterface;
};

} // namespace L0
