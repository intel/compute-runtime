/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/debug/debug_handlers.h"
#include <level_zero/zet_api.h>

namespace L0 {
ze_result_t zetDeviceGetDebugProperties(
    zet_device_handle_t hDevice,
    zet_device_debug_properties_t *pDebugProperties) {
    return L0::Device::fromHandle(hDevice)->getDebugProperties(pDebugProperties);
}

ze_result_t zetDebugAttach(
    zet_device_handle_t hDevice,
    const zet_debug_config_t *config,
    zet_debug_session_handle_t *phDebug) {
    return L0::DebugApiHandlers::debugAttach(hDevice, config, phDebug);
}

ze_result_t zetDebugDetach(
    zet_debug_session_handle_t hDebug) {
    return L0::DebugApiHandlers::debugDetach(hDebug);
}

ze_result_t zetDebugReadEvent(
    zet_debug_session_handle_t hDebug,
    uint64_t timeout,
    zet_debug_event_t *event) {
    return L0::DebugApiHandlers::debugReadEvent(hDebug, timeout, event);
}

ze_result_t zetDebugInterrupt(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return L0::DebugApiHandlers::debugInterrupt(hDebug, thread);
}

ze_result_t zetDebugResume(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return L0::DebugApiHandlers::debugResume(hDebug, thread);
}

ze_result_t zetDebugReadMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    void *buffer) {
    return L0::DebugApiHandlers::debugReadMemory(hDebug, thread, desc, size, buffer);
}

ze_result_t zetDebugWriteMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    const void *buffer) {
    return L0::DebugApiHandlers::debugWriteMemory(hDebug, thread, desc, size, buffer);
}

ze_result_t zetDebugAcknowledgeEvent(
    zet_debug_session_handle_t hDebug,
    const zet_debug_event_t *event) {
    return L0::DebugApiHandlers::debugAcknowledgeEvent(hDebug, event);
}

ze_result_t zetDebugGetRegisterSetProperties(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_debug_regset_properties_t *pRegisterSetProperties) {
    return L0::DebugApiHandlers::debugGetRegisterSetProperties(hDevice, pCount, pRegisterSetProperties);
}

ze_result_t zetDebugReadRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    uint32_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return L0::DebugApiHandlers::debugReadRegisters(hDebug, thread, type, start, count, pRegisterValues);
}

ze_result_t zetDebugWriteRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    uint32_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return L0::DebugApiHandlers::debugWriteRegisters(hDebug, thread, type, start, count, pRegisterValues);
}
} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zetDeviceGetDebugProperties(
    zet_device_handle_t hDevice,
    zet_device_debug_properties_t *pDebugProperties) {
    return L0::zetDeviceGetDebugProperties(
        hDevice,
        pDebugProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugAttach(
    zet_device_handle_t hDevice,
    const zet_debug_config_t *config,
    zet_debug_session_handle_t *phDebug) {
    return L0::zetDebugAttach(
        hDevice,
        config,
        phDebug);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugDetach(
    zet_debug_session_handle_t hDebug) {
    return L0::zetDebugDetach(
        hDebug);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugReadEvent(
    zet_debug_session_handle_t hDebug,
    uint64_t timeout,
    zet_debug_event_t *event) {
    return L0::zetDebugReadEvent(
        hDebug,
        timeout,
        event);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugAcknowledgeEvent(
    zet_debug_session_handle_t hDebug,
    const zet_debug_event_t *event) {
    return L0::zetDebugAcknowledgeEvent(
        hDebug,
        event);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugInterrupt(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return L0::zetDebugInterrupt(
        hDebug,
        thread);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugResume(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return L0::zetDebugResume(
        hDebug,
        thread);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugReadMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    void *buffer) {
    return L0::zetDebugReadMemory(
        hDebug,
        thread,
        desc,
        size,
        buffer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugWriteMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    const void *buffer) {
    return L0::zetDebugWriteMemory(
        hDebug,
        thread,
        desc,
        size,
        buffer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugGetRegisterSetProperties(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_debug_regset_properties_t *pRegisterSetProperties) {
    return L0::zetDebugGetRegisterSetProperties(
        hDevice,
        pCount,
        pRegisterSetProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugReadRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    uint32_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return L0::zetDebugReadRegisters(
        hDebug,
        thread,
        type,
        start,
        count,
        pRegisterValues);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetDebugWriteRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    uint32_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return L0::zetDebugWriteRegisters(
        hDebug,
        thread,
        type,
        start,
        count,
        pRegisterValues);
}
}
