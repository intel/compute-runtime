/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "gfxEscape.h"

namespace L0 {
struct PcEscapeInfo {
    GFX_ESCAPE_HEADER_T headerGfx;
    uint32_t escapeOpInput;
    uint32_t escapeErrorType;

    uint32_t dataInSize;
    uint64_t pDataIn;

    uint32_t dataOutSize;
    uint64_t pDataOut;
};

uint32_t sumOfBufferData(void *pBuffer, uint32_t bufferSize) {
    uint32_t index;
    uint32_t ulCheckSum;
    uint32_t numOfUnsignedLongs = bufferSize / sizeof(uint32_t);
    uint32_t *pElement = static_cast<uint32_t *>(pBuffer);

    ulCheckSum = 0;
    for (index = 0; index < numOfUnsignedLongs; index++) {
        ulCheckSum += *pElement;
        pElement++;
    }

    return ulCheckSum;
}

KmdSysManager::KmdSysManager(NEO::Wddm *pWddm) {
    pWddmAccess = pWddm;
}

KmdSysManager *KmdSysManager::create(NEO::Wddm *pWddm) {
    return new KmdSysManager(pWddm);
}
ze_result_t KmdSysManager::requestSingle(KmdSysman::RequestProperty &inputRequest, KmdSysman::ResponseProperty &outputResponse) {
    KmdSysman::GfxSysmanMainHeaderIn inMainHeader;
    KmdSysman::GfxSysmanMainHeaderOut outMainHeader;

    memset(&inMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderIn));
    memset(&outMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderOut));

    std::vector<KmdSysman::RequestProperty> vectorInput;
    vectorInput.push_back(inputRequest);

    if (!parseBufferIn(&inMainHeader, vectorInput)) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    KmdSysman::KmdSysmanVersion versionKmd;
    versionKmd.data = 0;
    versionKmd.majorVersion = KmdSysman::KmdMajorVersion;
    versionKmd.minorVersion = KmdSysman::KmdMinorVersion;
    versionKmd.patchNumber = KmdSysman::KmdPatchNumber;

    inMainHeader.inVersion = versionKmd.data;
    uint64_t inPointerToLongInt = reinterpret_cast<uint64_t>(&inMainHeader);
    uint64_t outPointerToLongInt = reinterpret_cast<uint64_t>(&outMainHeader);
    auto status = escape(KmdSysman::PcEscapeOperation, inPointerToLongInt, sizeof(KmdSysman::GfxSysmanMainHeaderIn),
                         outPointerToLongInt, sizeof(KmdSysman::GfxSysmanMainHeaderOut));

    if (status == STATUS_SUCCESS) {
        std::vector<KmdSysman::ResponseProperty> vecOutput;
        if (!parseBufferOut(&outMainHeader, vecOutput)) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        if (vecOutput.size() > 0) {
            outputResponse = vecOutput[0];
        } else {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        if ((outputResponse.returnCode == KmdSysman::ReturnCodes::DomainServiceNotSupported) ||
            (outputResponse.returnCode == KmdSysman::ReturnCodes::GetNotSupported) ||
            (outputResponse.returnCode == KmdSysman::ReturnCodes::SetNotSupported)) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        return (outputResponse.returnCode == KmdSysman::KmdSysmanSuccess) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;

    } else if (status == STATUS_DEVICE_REMOVED) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t KmdSysManager::requestMultiple(std::vector<KmdSysman::RequestProperty> &inputRequest, std::vector<KmdSysman::ResponseProperty> &outputResponse) {
    KmdSysman::GfxSysmanMainHeaderIn inMainHeader;
    KmdSysman::GfxSysmanMainHeaderOut outMainHeader;

    if (inputRequest.size() == 0) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    memset(&inMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderIn));
    memset(&outMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderOut));

    if (!parseBufferIn(&inMainHeader, inputRequest)) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    KmdSysman::KmdSysmanVersion versionKmd;
    versionKmd.data = 0;
    versionKmd.majorVersion = 1;
    versionKmd.minorVersion = 0;
    versionKmd.patchNumber = 0;

    inMainHeader.inVersion = versionKmd.data;
    uint64_t inPointerToLongInt = reinterpret_cast<uint64_t>(&inMainHeader);
    uint64_t outPointerToLongInt = reinterpret_cast<uint64_t>(&outMainHeader);
    auto status = escape(KmdSysman::PcEscapeOperation, inPointerToLongInt, sizeof(KmdSysman::GfxSysmanMainHeaderIn),
                         outPointerToLongInt, sizeof(KmdSysman::GfxSysmanMainHeaderOut));

    if (status == STATUS_SUCCESS) {
        if (!parseBufferOut(&outMainHeader, outputResponse)) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        if (outputResponse.size() == 0) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }
        return ZE_RESULT_SUCCESS;
    } else if (status == STATUS_DEVICE_REMOVED) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

NTSTATUS KmdSysManager::escape(uint32_t escapeOp, uint64_t pDataIn, uint32_t dataInSize, uint64_t pDataOut, uint32_t dataOutSize) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if (pWddmAccess) {
        D3DKMT_ESCAPE escapeCommand = {0};
        PcEscapeInfo pcEscape = {};
        escapeCommand.Flags.HardwareAccess = 0;
        escapeCommand.Flags.Reserved = 0;
        escapeCommand.hAdapter = (D3DKMT_HANDLE)0;
        escapeCommand.hContext = (D3DKMT_HANDLE)0;
        escapeCommand.hDevice = (D3DKMT_HANDLE)pWddmAccess->getDeviceHandle();
        escapeCommand.pPrivateDriverData = &pcEscape;
        escapeCommand.PrivateDriverDataSize = sizeof(pcEscape);
        escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

        pcEscape.headerGfx.EscapeCode = GFX_ESCAPE_PWRCONS_CONTROL;
        pcEscape.escapeErrorType = 0;
        pcEscape.escapeOpInput = escapeOp;
        pcEscape.headerGfx.Size = sizeof(pcEscape) - sizeof(pcEscape.headerGfx);

        pcEscape.pDataIn = pDataIn;
        pcEscape.pDataOut = pDataOut;

        pcEscape.dataInSize = dataInSize;
        pcEscape.dataOutSize = dataOutSize;
        void *pBuffer = &pcEscape;
        pBuffer = reinterpret_cast<uint8_t *>(pBuffer) + sizeof(pcEscape.headerGfx);
        pcEscape.headerGfx.CheckSum = sumOfBufferData(pBuffer, pcEscape.headerGfx.Size);

        status = pWddmAccess->escape(escapeCommand);
    }

    return status;
}

bool KmdSysManager::parseBufferIn(KmdSysman::GfxSysmanMainHeaderIn *pInMainHeader, std::vector<KmdSysman::RequestProperty> &vectorInput) {
    memset(pInMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderIn));
    for (uint32_t i = 0; i < vectorInput.size(); i++) {
        KmdSysman::GfxSysmanReqHeaderIn headerIn;
        headerIn.inRequestId = vectorInput[i].requestId;
        headerIn.inCommand = vectorInput[i].commandId;
        headerIn.inComponent = vectorInput[i].componentId;
        headerIn.inCommandParam = vectorInput[i].paramInfo;
        headerIn.inDataSize = vectorInput[i].dataSize;

        if ((pInMainHeader->inTotalsize + sizeof(KmdSysman::GfxSysmanReqHeaderIn)) >= KmdSysman::KmdMaxBufferSize) {
            memset(pInMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderIn));
            return false;
        }

        if ((pInMainHeader->inTotalsize + sizeof(KmdSysman::GfxSysmanReqHeaderIn) + headerIn.inDataSize) >= KmdSysman::KmdMaxBufferSize) {
            memset(pInMainHeader, 0, sizeof(KmdSysman::GfxSysmanMainHeaderIn));
            return false;
        }

        uint8_t *pBuff = reinterpret_cast<uint8_t *>(&pInMainHeader->inBuffer[pInMainHeader->inTotalsize]);
        memcpy_s(pBuff, sizeof(KmdSysman::GfxSysmanReqHeaderIn), &headerIn, sizeof(KmdSysman::GfxSysmanReqHeaderIn));

        pBuff += sizeof(KmdSysman::GfxSysmanReqHeaderIn);
        pInMainHeader->inTotalsize += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        if (headerIn.inDataSize > 0) {
            memcpy_s(pBuff, headerIn.inDataSize, vectorInput[i].dataBuffer, headerIn.inDataSize);
            pInMainHeader->inTotalsize += headerIn.inDataSize;
        }

        pInMainHeader->inNumElements++;
    }

    return true;
}

bool KmdSysManager::parseBufferOut(KmdSysman::GfxSysmanMainHeaderOut *pOutMainHeader, std::vector<KmdSysman::ResponseProperty> &vectorOutput) {
    uint8_t *pBuff = reinterpret_cast<uint8_t *>(pOutMainHeader->outBuffer);
    uint32_t totalSize = 0;
    vectorOutput.clear();
    for (uint32_t i = 0; i < pOutMainHeader->outNumElements; i++) {
        KmdSysman::ResponseProperty propertyResponse;
        KmdSysman::GfxSysmanReqHeaderOut headerOut;

        memcpy_s(&headerOut, sizeof(KmdSysman::GfxSysmanReqHeaderOut), pBuff, sizeof(KmdSysman::GfxSysmanReqHeaderOut));

        propertyResponse.requestId = headerOut.outRequestId;
        propertyResponse.returnCode = headerOut.outReturnCode;
        propertyResponse.componentId = headerOut.outComponent;
        propertyResponse.dataSize = headerOut.outDataSize;

        pBuff += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        if (headerOut.outDataSize > 0) {
            memcpy_s(propertyResponse.dataBuffer, headerOut.outDataSize, pBuff, headerOut.outDataSize);
            pBuff += headerOut.outDataSize;
        }

        vectorOutput.push_back(propertyResponse);

        totalSize += sizeof(KmdSysman::GfxSysmanReqHeaderOut);
        totalSize += headerOut.outDataSize;
    }

    if (totalSize != pOutMainHeader->outTotalSize) {
        vectorOutput.clear();
        return false;
    }

    return true;
}

} // namespace L0
