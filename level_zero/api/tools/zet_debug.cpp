/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/zet_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDeviceGetDebugProperties(
    zet_device_handle_t hDevice,
    zet_device_debug_properties_t *pDebugProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugAttach(
    zet_device_handle_t hDevice,
    const zet_debug_config_t *config,
    zet_debug_session_handle_t *phDebug) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugDetach(
    zet_debug_session_handle_t hDebug) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugReadEvent(
    zet_debug_session_handle_t hDebug,
    uint64_t timeout,
    zet_debug_event_t *event) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugInterrupt(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugResume(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugReadMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    void *buffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugWriteMemory(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    const zet_debug_memory_space_desc_t *desc,
    size_t size,
    const void *buffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugAcknowledgeEvent(
    zet_debug_session_handle_t hDebug,
    const zet_debug_event_t *event) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugGetRegisterSetProperties(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_debug_regset_properties_t *pRegisterSetProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugReadRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    zet_debug_regset_type_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDebugWriteRegisters(
    zet_debug_session_handle_t hDebug,
    ze_device_thread_t thread,
    zet_debug_regset_type_t type,
    uint32_t start,
    uint32_t count,
    void *pRegisterValues) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}