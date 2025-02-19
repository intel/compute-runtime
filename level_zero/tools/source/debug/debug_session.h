/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/os_thread.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"
#include "level_zero/tools/source/debug/eu_thread.h"

#include <memory>

struct _zet_debug_session_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_zet_debug_session_handle_t>);

namespace L0 {

struct Device;

struct DebugSession : _zet_debug_session_handle_t {
    virtual ~DebugSession() = default;
    DebugSession() = delete;

    static DebugSession *create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach);

    static DebugSession *fromHandle(zet_debug_session_handle_t handle) { return static_cast<DebugSession *>(handle); }
    inline zet_debug_session_handle_t toHandle() { return this; }

    virtual bool closeConnection() = 0;
    virtual ze_result_t initialize() = 0;

    virtual ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) = 0;
    virtual ze_result_t interrupt(ze_device_thread_t thread) = 0;
    virtual ze_result_t resume(ze_device_thread_t thread) = 0;
    virtual ze_result_t readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) = 0;
    virtual ze_result_t writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) = 0;
    virtual ze_result_t acknowledgeEvent(const zet_debug_event_t *event) = 0;
    virtual ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) = 0;
    virtual ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) = 0;
    static ze_result_t getRegisterSetProperties(Device *device, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties);
    virtual ze_result_t getThreadRegisterSetProperties(ze_device_thread_t thread, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties);
    MOCKABLE_VIRTUAL bool areRequestedThreadsStopped(ze_device_thread_t thread);

    Device *getConnectedDevice() { return connectedDevice; }
    zet_debug_config_t getDebugConfig() { return config; }

    static bool isThreadAll(ze_device_thread_t thread) {
        return thread.slice == UINT32_MAX && thread.subslice == UINT32_MAX && thread.eu == UINT32_MAX && thread.thread == UINT32_MAX;
    }

    static bool isSingleThread(ze_device_thread_t thread) {
        return thread.slice != UINT32_MAX && thread.subslice != UINT32_MAX && thread.eu != UINT32_MAX && thread.thread != UINT32_MAX;
    }

    static bool areThreadsEqual(ze_device_thread_t thread, ze_device_thread_t thread2) {
        return thread.slice == thread2.slice && thread.subslice == thread2.subslice && thread.eu == thread2.eu && thread.thread == thread2.thread;
    }

    static bool checkSingleThreadWithinDeviceThread(ze_device_thread_t checkedThread, ze_device_thread_t thread) {
        if (DebugSession::isThreadAll(thread)) {
            return true;
        }

        bool threadMatch = (thread.thread == checkedThread.thread) || thread.thread == UINT32_MAX;
        bool euMatch = (thread.eu == checkedThread.eu) || thread.eu == UINT32_MAX;
        bool subsliceMatch = (thread.subslice == checkedThread.subslice) || thread.subslice == UINT32_MAX;
        bool sliceMatch = (thread.slice == checkedThread.slice) || thread.slice == UINT32_MAX;

        return threadMatch && euMatch && subsliceMatch && sliceMatch;
    }

    static void printBitmask(uint8_t *bitmask, size_t bitmaskSize);

    MOCKABLE_VIRTUAL const NEO::TopologyMap &getTopologyMap();

    virtual uint32_t getDeviceIndexFromApiThread(ze_device_thread_t thread);
    virtual ze_device_thread_t convertToPhysicalWithinDevice(ze_device_thread_t thread, uint32_t deviceIndex);
    virtual EuThread::ThreadId convertToThreadId(ze_device_thread_t thread);
    virtual ze_device_thread_t convertToApi(EuThread::ThreadId threadId);

    ze_result_t sanityMemAccessThreadCheck(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc);

    virtual DebugSession *attachTileDebugSession(Device *device) = 0;
    virtual void detachTileDebugSession(DebugSession *tileSession) = 0;
    virtual bool areAllTileDebugSessionDetached() = 0;

    virtual void setAttachMode(bool isRootAttach) = 0;
    void setAttached() { attached = true; }
    void setDetached() { attached = false; }
    bool isAttached() { return attached; }

    struct ThreadHelper {
        void close() {
            threadActive.store(false);

            if (thread) {
                thread->join();
                thread.reset();
            }
        }
        std::unique_ptr<NEO::Thread> thread;
        std::atomic<bool> threadActive{true};
    };

  protected:
    DebugSession(const zet_debug_config_t &config, Device *device);
    void createEuThreads();
    void updateGrfRegisterSetProperties(EuThread::ThreadId thread, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties);
    virtual uint32_t getRegisterSize(uint32_t type) = 0;
    virtual ze_result_t readRegistersImp(EuThread::ThreadId thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) = 0;

    virtual void startAsyncThread() = 0;

    virtual bool isBindlessSystemRoutine();
    virtual bool readModuleDebugArea() = 0;

    struct DebugAreaInfo {
        uint64_t gpuVa;
        uint64_t size;
    };
    virtual DebugAreaInfo getModuleDebugAreaInfo() { return {0, 0}; }
    virtual ze_result_t readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) = 0;

    void fillDevicesFromThread(ze_device_thread_t thread, std::vector<uint8_t> &devices);

    std::vector<EuThread::ThreadId> getSingleThreadsForDevice(uint32_t deviceIndex, ze_device_thread_t physicalThread, const NEO::HardwareInfo &hwInfo);

    size_t getPerThreadScratchOffset(size_t ptss, EuThread::ThreadId threadId);

    NEO::DebugAreaHeader debugArea;

    Device *connectedDevice = nullptr;
    const zet_debug_config_t config;
    std::map<uint64_t, std::unique_ptr<EuThread>> allThreads;
    bool attached = false;
};

} // namespace L0
