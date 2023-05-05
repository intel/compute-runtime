/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

namespace L0 {
namespace DebugApiHandlers {
ze_result_t debugAttach(zet_device_handle_t hDevice, const zet_debug_config_t *config, zet_debug_session_handle_t *phDebug);
ze_result_t debugDetach(zet_debug_session_handle_t hDebug);
ze_result_t debugReadEvent(zet_debug_session_handle_t hDebug, uint64_t timeout, zet_debug_event_t *event);
ze_result_t debugInterrupt(zet_debug_session_handle_t hDebug, ze_device_thread_t thread);
ze_result_t debugResume(zet_debug_session_handle_t hDebug, ze_device_thread_t thread);
ze_result_t debugReadMemory(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer);
ze_result_t debugWriteMemory(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer);
ze_result_t debugAcknowledgeEvent(zet_debug_session_handle_t hDebug, const zet_debug_event_t *event);
ze_result_t debugGetRegisterSetProperties(zet_device_handle_t hDevice, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties);
ze_result_t debugGetThreadRegisterSetProperties(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties);
ze_result_t debugReadRegisters(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
ze_result_t debugWriteRegisters(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues);
} // namespace DebugApiHandlers
} // namespace L0