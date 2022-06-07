/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_wddm.h"

#include "KmEscape.h"

namespace NEO {

struct WddmEuDebugInterfaceMock : public WddmMock {
    WddmEuDebugInterfaceMock(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}

    bool isDebugAttachAvailable() override {
        return debugAttachAvailable;
    }

    NTSTATUS escape(D3DKMT_ESCAPE &escapeCommand) override {
        auto pEscapeInfo = static_cast<KM_ESCAPE_INFO *>(escapeCommand.pPrivateDriverData);
        if (pEscapeInfo->EscapeOperation == KM_ESCAPE_EUDBG_UMD_REGISTER_ALLOCATION_TYPE) {
            ++registerAllocationTypeCalled;
            GFX_ALLOCATION_DEBUG_DATA_INFO *allocDataInfo = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(pEscapeInfo->KmEuDbgUmdRegisterAllocationData.DebugDataBufferPtr);
            registerAllocationTypePassedParams.allocDataSize = allocDataInfo->DataSize;
            memcpy_s(registerAllocationTypePassedParams.allocData, 100, reinterpret_cast<uint8_t *>(allocDataInfo->DataPointer), allocDataInfo->DataSize);
            return ntStatus;
        }

        if (pEscapeInfo->EscapeOperation != KM_ESCAPE_EUDBG_L0_DBGUMD_HANDLER) {
            return ntStatus;
        }

        ++dbgUmdEscapeActionCalled[pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeActionType];

        pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = escapeReturnStatus;
        switch (pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeActionType) {
        case DBGUMD_ACTION_ATTACH_DEBUGGER: {
            pEscapeInfo->KmEuDbgL0EscapeInfo.AttachDebuggerParams.hDebugHandle = debugHandle;
            break;
        }
        case DBGUMD_ACTION_DETACH_DEBUGGER:
            break;
        case DBGUMD_ACTION_READ_EVENT: {
            if (curEvent >= numEvents) {
                // KMD event queue is empty
                pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED;
                break;
            }

            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = eventQueue[curEvent].escapeReturnStatus;
            pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.ReadEventType = eventQueue[curEvent].readEventType;
            auto paramBuffer = reinterpret_cast<uint8_t *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.EventParamBufferPtr);
            memcpy_s(paramBuffer, pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.EventParamsBufferSize, &eventQueue[curEvent].eventParamsBuffer, sizeof(READ_EVENT_PARAMS_BUFFER));
            return eventQueue[curEvent++].ntStatus;
        }
        case DBGUMD_ACTION_READ_ALLOCATION_DATA: {
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = readAllocationDataOutParams.escapeReturnStatus;
            if (readAllocationDataOutParams.outData != nullptr) {
                auto outBuffer = reinterpret_cast<uint8_t *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.OutputBufferPtr);
                memcpy_s(outBuffer, pEscapeInfo->KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.OutputBufferSize, readAllocationDataOutParams.outData, readAllocationDataOutParams.outDataSize);
            }
            break;
        }
        }
        return ntStatus;
    };

    uint32_t numEvents = 0;
    uint32_t curEvent = 0;
    struct {
        NTSTATUS ntStatus = STATUS_SUCCESS;
        EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
        EUDBG_DBGUMD_READ_EVENT_TYPE readEventType = DBGUMD_READ_EVENT_MAX;
        union {
            READ_EVENT_PARAMS_BUFFER eventParamsBuffer;
            uint8_t rawBytes[READ_EVENT_PARAMS_BUFFER_MIN_SIZE_BYTES] = {0};
        } eventParamsBuffer;
    } eventQueue[10] = {0};

    struct {
        EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
        void *outData = nullptr;
        size_t outDataSize = 0;
    } readAllocationDataOutParams;

    struct {
        uint32_t allocDataSize = 0;
        uint32_t allocData[100] = {0};
    } registerAllocationTypePassedParams;

    bool debugAttachAvailable = true;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    uint32_t escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;

    uint64_t debugHandle = 0x0DEB0DEB;
    uint32_t dbgUmdEscapeActionCalled[DBGUMD_ACTION_MAX] = {0};
    uint32_t registerAllocationTypeCalled = 0;
};

} // namespace NEO
