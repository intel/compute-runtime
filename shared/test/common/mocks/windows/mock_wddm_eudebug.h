/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_wddm.h"

#include "KmEscape.h"

#pragma once

namespace NEO {

struct WddmEuDebugInterfaceMock : public WddmMock {
    WddmEuDebugInterfaceMock(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}

    bool isDebugAttachAvailable() override {
        return debugAttachAvailable;
    }

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand) override {
        auto pEscapeInfo = static_cast<KM_ESCAPE_INFO *>(escapeCommand.pPrivateDriverData);
        if (pEscapeInfo->EscapeOperation != KM_ESCAPE_EUDBG_L0_DBGUMD_HANDLER) {
            return escapeStatus;
        }

        ++dbgUmdEscapeActionCalled[pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeActionType];

        switch (pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeActionType) {
        case DBGUMD_ACTION_ATTACH_DEBUGGER: {
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
            pEscapeInfo->KmEuDbgL0EscapeInfo.AttachDebuggerParams.hDebugHandle = debugHandle;
            break;
        }
        case DBGUMD_ACTION_DETACH_DEBUGGER:
            break;
        case DBGUMD_ACTION_READ_EVENT: {
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = readEventOutParams.escapeReturnStatus;
            if (DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED == pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
                // KMD event queue is empty
                break;
            }
            pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.ReadEventType = readEventOutParams.readEventType;
            auto paramBuffer = reinterpret_cast<uint8_t *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.EventParamBufferPtr);
            memcpy_s(paramBuffer, pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.EventParamsBufferSize, &readEventOutParams.eventParamsBuffer, sizeof(READ_EVENT_PARAMS_BUFFER));
            break;
        }
        }
        return escapeStatus;
    };

    struct {
        EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
        EUDBG_DBGUMD_READ_EVENT_TYPE readEventType = DBGUMD_READ_EVENT_MAX;
        READ_EVENT_PARAMS_BUFFER eventParamsBuffer = {0};
    } readEventOutParams;

    bool debugAttachAvailable = true;
    NTSTATUS escapeStatus = STATUS_SUCCESS;
    uint64_t debugHandle = 0x0DEB0DEB;
    uint32_t dbgUmdEscapeActionCalled[DBGUMD_ACTION_MAX] = {0};
};

} // namespace NEO
