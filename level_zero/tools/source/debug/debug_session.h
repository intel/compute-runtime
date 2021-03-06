/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/debugger/debugger_l0.h"
#include <level_zero/zet_api.h>

struct _zet_debug_session_handle_t {};

namespace L0 {

struct Device;

struct DebugSession : _zet_debug_session_handle_t {
    virtual ~DebugSession() = default;
    DebugSession() = delete;

    static DebugSession *create(const zet_debug_config_t &config, Device *device, ze_result_t &result);

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

    Device *getConnectedDevice() { return connectedDevice; }

  protected:
    DebugSession(const zet_debug_config_t &config, Device *device) : connectedDevice(device){};
    virtual void startAsyncThread() = 0;

    Device *connectedDevice = nullptr;
};

struct RootDebugSession : DebugSession {
    virtual ~RootDebugSession() = default;
    RootDebugSession() = delete;

  protected:
    RootDebugSession(const zet_debug_config_t &config, Device *device) : DebugSession(config, device){};

    virtual bool readModuleDebugArea() = 0;
    DebugAreaHeader debugArea;
};

} // namespace L0
