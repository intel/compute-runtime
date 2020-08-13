/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

class MockKmdSysManager : public KmdSysManager {};

constexpr uint32_t mockKmdVersionMajor = 1;
constexpr uint32_t mockKmdVersionMinor = 0;
constexpr uint32_t mockKmdPatchNumber = 0;

template <>
struct Mock<MockKmdSysManager> : public MockKmdSysManager {

    ze_bool_t allowSetCalls = false;

    uint32_t mockPowerLimit1 = 2500;

    MOCK_METHOD(bool, escape, (uint32_t escapeOp, uint64_t pDataIn, uint32_t dataInSize, uint64_t pDataOut, uint32_t dataOutSize));

    MOCKABLE_VIRTUAL void getInterfaceProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setInterfaceProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getPowerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        if (pRequest->inRequestId == KmdSysman::Requests::Power::CurrentPowerLimit1) {
            uint32_t *pPl1 = reinterpret_cast<uint32_t *>(pBuffer);
            *pPl1 = mockPowerLimit1;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } else {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        }
    }

    MOCKABLE_VIRTUAL void setPowerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        if (pRequest->inRequestId == KmdSysman::Requests::Power::CurrentPowerLimit1) {
            uint32_t *pPl1 = reinterpret_cast<uint32_t *>(pBuffer);
            mockPowerLimit1 = *pPl1;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } else {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        }
    }

    MOCKABLE_VIRTUAL void getFrequencyProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setFrequencyProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getTemperatureProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setTemperatureProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getFpsProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setFpsProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getSchedulerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setSchedulerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getMemoryProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setMemoryProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void getPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    MOCKABLE_VIRTUAL void setPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    void retrieveCorrectVersion(KmdSysman::GfxSysmanMainHeaderOut *pHeaderOut) {
        pHeaderOut->outNumElements = 1;
        pHeaderOut->outTotalSize = 0;

        KmdSysman::GfxSysmanReqHeaderOut *pResponse = reinterpret_cast<KmdSysman::GfxSysmanReqHeaderOut *>(pHeaderOut->outBuffer);
        uint8_t *pBuffer = nullptr;

        pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        pResponse->outDataSize = sizeof(KmdSysman::KmdSysmanVersion);
        pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);
        pHeaderOut->outTotalSize += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::KmdSysmanVersion *pCurrentVersion = reinterpret_cast<KmdSysman::KmdSysmanVersion *>(pBuffer);
        pCurrentVersion->majorVersion = mockKmdVersionMajor;
        pCurrentVersion->minorVersion = mockKmdVersionMinor;
        pCurrentVersion->patchNumber = mockKmdPatchNumber;

        pHeaderOut->outTotalSize += sizeof(KmdSysman::KmdSysmanVersion);
    }

    bool validateInputBuffer(KmdSysman::GfxSysmanMainHeaderIn *pHeaderIn) {
        uint32_t sizeCheck = pHeaderIn->inTotalsize;
        uint8_t *pBufferPtr = pHeaderIn->inBuffer;

        for (uint32_t i = 0; i < pHeaderIn->inNumElements; i++) {
            KmdSysman::GfxSysmanReqHeaderIn *pRequest = reinterpret_cast<KmdSysman::GfxSysmanReqHeaderIn *>(pBufferPtr);
            if (pRequest->inCommand == KmdSysman::Command::Get || pRequest->inCommand == KmdSysman::Command::Set) {
                if (pRequest->inComponent >= KmdSysman::Component::InterfaceProperties && pRequest->inComponent < KmdSysman::Component::MaxComponents) {
                    pBufferPtr += sizeof(KmdSysman::GfxSysmanReqHeaderIn);
                    sizeCheck -= sizeof(KmdSysman::GfxSysmanReqHeaderIn);

                    if (pRequest->inCommand == KmdSysman::Command::Set) {
                        if (pRequest->inDataSize == 0) {
                            return false;
                        }
                        pBufferPtr += pRequest->inDataSize;
                        sizeCheck -= pRequest->inDataSize;
                    }
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        if (sizeCheck != 0) {
            return false;
        }

        return true;
    }

    void setProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        if (!allowSetCalls) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inComponent) {
        case KmdSysman::Component::InterfaceProperties: {
            setInterfaceProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::PowerComponent: {
            setPowerProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FrequencyComponent: {
            setFrequencyProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::ActivityComponent: {
            setActivityProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FanComponent: {
            setFanProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::TemperatureComponent: {
            setTemperatureProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FpsComponent: {
            setFpsProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::SchedulerComponent: {
            setSchedulerProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::MemoryComponent: {
            setMemoryProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::PciComponent: {
            setPciProperty(pRequest, pResponse);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void getProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        switch (pRequest->inComponent) {
        case KmdSysman::Component::InterfaceProperties: {
            getInterfaceProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::PowerComponent: {
            getPowerProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FrequencyComponent: {
            getFrequencyProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::ActivityComponent: {
            getActivityProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FanComponent: {
            getFanProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::TemperatureComponent: {
            getTemperatureProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::FpsComponent: {
            getFpsProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::SchedulerComponent: {
            getSchedulerProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::MemoryComponent: {
            getMemoryProperty(pRequest, pResponse);
        } break;
        case KmdSysman::Component::PciComponent: {
            getPciProperty(pRequest, pResponse);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    bool mock_escape(uint32_t escapeOp, uint64_t pInPtr, uint32_t dataInSize, uint64_t pOutPtr, uint32_t dataOutSize) {
        void *pDataIn = reinterpret_cast<void *>(pInPtr);
        void *pDataOut = reinterpret_cast<void *>(pOutPtr);

        if (pDataIn == nullptr || pDataOut == nullptr) {
            return false;
        }

        if (dataInSize != sizeof(KmdSysman::GfxSysmanMainHeaderIn) || dataOutSize != sizeof(KmdSysman::GfxSysmanMainHeaderOut)) {
            return false;
        }

        if (escapeOp != KmdSysman::PcEscapeOperation) {
            return false;
        }

        KmdSysman::GfxSysmanMainHeaderIn *pSysmanMainHeaderIn = reinterpret_cast<KmdSysman::GfxSysmanMainHeaderIn *>(pDataIn);
        KmdSysman::GfxSysmanMainHeaderOut *pSysmanMainHeaderOut = reinterpret_cast<KmdSysman::GfxSysmanMainHeaderOut *>(pDataOut);

        KmdSysman::KmdSysmanVersion versionSysman;
        versionSysman.data = pSysmanMainHeaderIn->inVersion;

        if (versionSysman.majorVersion != KmdSysman::KmdMajorVersion) {
            if (versionSysman.majorVersion == 0) {
                retrieveCorrectVersion(pSysmanMainHeaderOut);
                return true;
            }
            return false;
        }

        if (pSysmanMainHeaderIn->inTotalsize == 0) {
            return false;
        }

        if (pSysmanMainHeaderIn->inNumElements == 0) {
            return false;
        }

        if (!validateInputBuffer(pSysmanMainHeaderIn)) {
            return false;
        }

        uint8_t *pBufferIn = pSysmanMainHeaderIn->inBuffer;
        uint8_t *pBufferOut = pSysmanMainHeaderOut->outBuffer;
        uint32_t requestOffset = 0;
        uint32_t responseOffset = 0;
        pSysmanMainHeaderOut->outTotalSize = 0;

        for (uint32_t i = 0; i < pSysmanMainHeaderIn->inNumElements; i++) {
            KmdSysman::GfxSysmanReqHeaderIn *pRequest = reinterpret_cast<KmdSysman::GfxSysmanReqHeaderIn *>(pBufferIn);
            KmdSysman::GfxSysmanReqHeaderOut *pResponse = reinterpret_cast<KmdSysman::GfxSysmanReqHeaderOut *>(pBufferOut);

            switch (pRequest->inCommand) {
            case KmdSysman::Command::Get: {
                getProperty(pRequest, pResponse);
                requestOffset = sizeof(KmdSysman::GfxSysmanReqHeaderIn);
                responseOffset = sizeof(KmdSysman::GfxSysmanReqHeaderOut);
                responseOffset += pResponse->outDataSize;
            } break;
            case KmdSysman::Command::Set: {
                setProperty(pRequest, pResponse);
                requestOffset = sizeof(KmdSysman::GfxSysmanReqHeaderIn);
                requestOffset += pRequest->inDataSize;
                responseOffset = sizeof(KmdSysman::GfxSysmanReqHeaderOut);
            } break;
            default: {
                return false;
            } break;
            }

            pResponse->outRequestId = pRequest->inRequestId;
            pResponse->outComponent = pRequest->inComponent;
            pBufferIn += requestOffset;
            pBufferOut += responseOffset;
            pSysmanMainHeaderOut->outTotalSize += responseOffset;
        }

        pSysmanMainHeaderOut->outNumElements = pSysmanMainHeaderIn->inNumElements;
        pSysmanMainHeaderOut->outStatus = KmdSysman::KmdSysmanSuccess;

        return true;
    }

    Mock() = default;
    ~Mock() = default;
};

} // namespace ult
} // namespace L0