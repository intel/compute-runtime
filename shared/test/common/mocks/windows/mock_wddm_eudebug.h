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

        if (pEscapeInfo->EscapeOperation == KM_ESCAPE_EUDBG_UMD_CREATE_DEBUG_DATA) {
            ++createDebugDataCalled;
            if (STATUS_SUCCESS == createDebugDataPassedParam.ntStatus) {
                createDebugDataPassedParam.param = pEscapeInfo->KmEuDbgUmdCreateDebugData;
            } else {
                createDebugDataPassedParam.param.hElfAddressPtr = 0xDEADDEAD;
            }
            return createDebugDataPassedParam.ntStatus;
        }

        if (pEscapeInfo->EscapeOperation == KM_ESCAPE_EUDBG_UMD_MODULE_CREATE_NOTIFY) {
            ++moduleCreateNotifyCalled;
            if (STATUS_SUCCESS == moduleCreateNotificationPassedParam.ntStatus) {
                moduleCreateNotificationPassedParam.param = pEscapeInfo->KmEuDbgUmdCreateModuleNotification;
            } else {
                moduleCreateNotificationPassedParam.param.hElfAddressPtr = 0xDEADDEAD;
            }

            return moduleCreateNotificationPassedParam.ntStatus;
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
            pEscapeInfo->KmEuDbgL0EscapeInfo.ReadEventParams.EventSeqNo = eventQueue[curEvent].seqNo;
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
        case DBGUMD_ACTION_READ_GFX_MEMORY: {
            void *dst = reinterpret_cast<void *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferPtr);
            size_t size = pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferSize;
            if (srcReadBuffer) {
                auto offsetInMemory = pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.GpuVirtualAddr - srcReadBufferBaseAddress;
                memcpy(dst, reinterpret_cast<char *>(srcReadBuffer) + offsetInMemory, size);
            } else {
                memset(dst, 0xaa, size);
            }
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = escapeReturnStatus;
            break;
        }
        case DBGUMD_ACTION_WRITE_GFX_MEMORY: {
            void *src = reinterpret_cast<void *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferPtr);
            size_t size = pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferSize;
            if (dstWriteBuffer) {
                auto offsetInMemory = pEscapeInfo->KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.GpuVirtualAddr - dstWriteBufferBaseAddress;
                memcpy(reinterpret_cast<char *>(dstWriteBuffer) + offsetInMemory, src, size);
            } else {
                memcpy(testBuffer, src, size);
            }
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = escapeReturnStatus;
            break;
        }
        case DBGUMD_ACTION_READ_MMIO: {
            uint64_t *ptr = reinterpret_cast<uint64_t *>(pEscapeInfo->KmEuDbgL0EscapeInfo.MmioReadParams.RegisterOutBufferPtr);
            *ptr = mockGpuVa;
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
            break;
        }
        case DBGUMD_ACTION_ACKNOWLEDGE_EVENT: {
            acknowledgeEventPassedParam = pEscapeInfo->KmEuDbgL0EscapeInfo.AckEventParams;
            break;
        }
        case DBGUMD_ACTION_READ_UMD_MEMORY: {
            if (elfData != nullptr && escapeReturnStatus == DBGUMD_RETURN_ESCAPE_SUCCESS) {
                memcpy(reinterpret_cast<void *>(pEscapeInfo->KmEuDbgL0EscapeInfo.ReadUmdMemoryParams.BufferPtr), elfData, pEscapeInfo->KmEuDbgL0EscapeInfo.ReadUmdMemoryParams.BufferSize);
            }
            pEscapeInfo->KmEuDbgL0EscapeInfo.EscapeReturnStatus = escapeReturnStatus;
            break;
        }
        case DBGUMD_ACTION_EU_CONTROL_CLR_ATT_BIT: {
            if (pEscapeInfo->KmEuDbgL0EscapeInfo.EuControlClrAttBitParams.BitMaskSizeInBytes != 0) {
                euControlBitmaskSize = pEscapeInfo->KmEuDbgL0EscapeInfo.EuControlClrAttBitParams.BitMaskSizeInBytes;
                euControlBitmask = std::make_unique<uint8_t[]>(euControlBitmaskSize);
                memcpy(euControlBitmask.get(), reinterpret_cast<void *>(pEscapeInfo->KmEuDbgL0EscapeInfo.EuControlClrAttBitParams.BitmaskArrayPtr), euControlBitmaskSize);
            }
            break;
        }
        case DBGUMD_ACTION_EU_CONTROL_INT_ALL: {
            break;
        }
        }

        return ntStatus;
    };

    void *elfData = nullptr;
    uint32_t numEvents = 0;
    uint32_t curEvent = 0;
    struct {
        NTSTATUS ntStatus = STATUS_SUCCESS;
        EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;
        EUDBG_DBGUMD_READ_EVENT_TYPE readEventType = DBGUMD_READ_EVENT_MAX;
        uint32_t seqNo = 0;
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

    struct {
        EUDBG_UMD_CREATE_DEBUG_DATA param;
        NTSTATUS ntStatus = STATUS_SUCCESS;
    } createDebugDataPassedParam;

    struct {
        EUDBG_UMD_MODULE_NOTIFICATION param;
        NTSTATUS ntStatus = STATUS_SUCCESS;
    } moduleCreateNotificationPassedParam;

    DBGUMD_ACTION_ACKNOWLEDGE_EVENT_PARAMS acknowledgeEventPassedParam = {0};

    bool debugAttachAvailable = true;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    EUDBG_L0DBGUMD_ESCAPE_RETURN_TYPE escapeReturnStatus = DBGUMD_RETURN_ESCAPE_SUCCESS;

    uint64_t debugHandle = 0x0DEB0DEB;
    uint32_t dbgUmdEscapeActionCalled[DBGUMD_ACTION_MAX] = {0};
    uint32_t registerAllocationTypeCalled = 0;
    uint32_t createDebugDataCalled = 0;
    uint32_t moduleCreateNotifyCalled = 0;
    static constexpr size_t bufferSize = 16;
    uint8_t testBuffer[bufferSize] = {0};
    uint64_t mockGpuVa = 0x12345678;
    void *srcReadBuffer = nullptr;
    uint64_t srcReadBufferBaseAddress = 0;
    void *dstWriteBuffer = nullptr;
    uint64_t dstWriteBufferBaseAddress = 0;

    std::unique_ptr<uint8_t[]> euControlBitmask;
    size_t euControlBitmaskSize = 0;
};

} // namespace NEO
