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
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = DBGUMD_ACTION_ESCAPE_SUCCESS;
            pEscapeInfo->KmEuDbgL0EscapeInfo.AttachDebuggerParams.hDebugHandle = debugHandle;
            break;
        }
        case DBGUMD_ACTION_DETACH_DEBUGGER:
            break;
        }
        return escapeStatus;
    };

    bool debugAttachAvailable = true;
    NTSTATUS escapeStatus = STATUS_SUCCESS;
    uint64_t debugHandle = 0x0DEB0DEB;
    uint32_t dbgUmdEscapeActionCalled[DBGUMD_ACTION_MAX] = {0};
};

} // namespace NEO