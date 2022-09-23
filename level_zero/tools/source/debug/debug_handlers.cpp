/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_handlers.h"

#include "shared/source/os_interface/debug_env_reader.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {
namespace DebugApiHandlers {

std::mutex debugSessionMutex;

ze_result_t debugAttach(zet_device_handle_t hDevice, const zet_debug_config_t *config, zet_debug_session_handle_t *phDebug) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    if (L0::Device::fromHandle(hDevice)->getNEODevice()->isSubDevice() && !NEO::DebugManager.flags.ExperimentalEnableTileAttach.get()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    NEO::EnvironmentVariableReader envReader;
    auto affinityMask = envReader.getSetting("ZE_AFFINITY_MASK", std::string(""));

    if (!affinityMask.empty()) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stdout,
                              "%s", "ZE_AFFINITY_MASK is not recommended while using program debug API\n");
    }

    auto session = L0::Device::fromHandle(hDevice)->getDebugSession(*config);

    std::unique_lock<std::mutex> lock(debugSessionMutex);

    auto rootSession = L0::Device::fromHandle(hDevice)->getNEODevice()->getRootDevice()->getSpecializedDevice<DeviceImp>()->getDebugSession(*config);

    // If root device with active TileSessions or
    // subdevice with root device attached - fail
    if ((!L0::Device::fromHandle(hDevice)->getNEODevice()->isSubDevice() && session && !session->areAllTileDebugSessionDetached()) ||
        (L0::Device::fromHandle(hDevice)->getNEODevice()->isSubDevice() && rootSession && rootSession->isAttached())) {
        result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        *phDebug = nullptr;
        return result;
    }

    if (!session) {
        bool isRootAttach = !L0::Device::fromHandle(hDevice)->getNEODevice()->isSubDevice();
        session = L0::Device::fromHandle(hDevice)->createDebugSession(*config, result, isRootAttach);
    }

    if (session) {
        session->setAttached();
        *phDebug = session->toHandle();
    }
    return result;
}

ze_result_t debugDetach(zet_debug_session_handle_t hDebug) {
    auto session = L0::DebugSession::fromHandle(hDebug);
    if (session) {
        auto device = session->getConnectedDevice();
        device->removeDebugSession();

        session->closeConnection();
        if (!device->getNEODevice()->isSubDevice()) {
            delete session;
        } else {
            std::unique_lock<std::mutex> lock(debugSessionMutex);
            auto rootL0Device = device->getNEODevice()->getRootDevice()->getSpecializedDevice<DeviceImp>();
            zet_debug_config_t dummy = {};
            auto rootSession = rootL0Device->getDebugSession(dummy);
            session->setDetached();
            rootSession->detachTileDebugSession(session);

            if (rootSession->areAllTileDebugSessionDetached()) {

                rootL0Device->removeDebugSession();
                delete rootSession;
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t debugReadEvent(zet_debug_session_handle_t hDebug, uint64_t timeout, zet_debug_event_t *event) { return L0::DebugSession::fromHandle(hDebug)->readEvent(timeout, event); }

ze_result_t debugInterrupt(zet_debug_session_handle_t hDebug, ze_device_thread_t thread) { return L0::DebugSession::fromHandle(hDebug)->interrupt(thread); }

ze_result_t debugResume(zet_debug_session_handle_t hDebug, ze_device_thread_t thread) { return L0::DebugSession::fromHandle(hDebug)->resume(thread); }

ze_result_t debugReadMemory(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    return L0::DebugSession::fromHandle(hDebug)->readMemory(thread, desc, size, buffer);
}

ze_result_t debugWriteMemory(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    return L0::DebugSession::fromHandle(hDebug)->writeMemory(thread, desc, size, buffer);
}

ze_result_t debugAcknowledgeEvent(zet_debug_session_handle_t hDebug, const zet_debug_event_t *event) {
    return L0::DebugSession::fromHandle(hDebug)->acknowledgeEvent(event);
}

ze_result_t debugGetRegisterSetProperties(zet_device_handle_t hDevice, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    return L0::DebugSession::getRegisterSetProperties(L0::Device::fromHandle(hDevice), pCount, pRegisterSetProperties);
}

ze_result_t debugReadRegisters(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    return L0::DebugSession::fromHandle(hDebug)->readRegisters(thread, type, start, count, pRegisterValues);
}

ze_result_t debugWriteRegisters(zet_debug_session_handle_t hDebug, ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) {
    return L0::DebugSession::fromHandle(hDebug)->writeRegisters(thread, type, start, count, pRegisterValues);
}

} // namespace DebugApiHandlers
} // namespace L0
