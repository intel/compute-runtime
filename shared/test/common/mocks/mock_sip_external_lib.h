/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/sip_external_lib/sip_external_lib.h"

#include <map>
#include <optional>

class MockSipExternalLib : public NEO::SipExternalLib {
  public:
    std::vector<char> getSipKernelBinaryRetBinary = {'t', 's', 's', 'a', 'r', 'e', 'a'};
    std::vector<char> getSipKernelBinaryStateSaveAreaHeader;
    int getSipKernelBinaryRetValue = 0;
    uint32_t getSipKernelBinaryCalled = 0;
    std::optional<NEO::SipKernelType> getSipKernelBinaryCapturedType;
    int getSipKernelBinary(NEO::Device &device, NEO::SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) override {
        ++getSipKernelBinaryCalled;
        getSipKernelBinaryCapturedType = type;
        retBinary = getSipKernelBinaryRetBinary;
        stateSaveAreaHeader = getSipKernelBinaryStateSaveAreaHeader;
        return getSipKernelBinaryRetValue;
    }

    bool createRegisterDescriptorMapRetValue = true;
    bool createRegisterDescriptorMap() override {
        return createRegisterDescriptorMapRetValue;
    }

    std::map<NEO::SipRegisterType, SIP::regset_desc *> mockRegsetDescMap;
    SIP::regset_desc *getRegsetDescFromMap(NEO::SipRegisterType type) override {
        return mockRegsetDescMap[type];
    }

    std::map<NEO::SipRegisterType, NEO::RegsetDescExt> getRegsetDescExtMap;
    std::optional<NEO::RegsetDescExt> getRegsetDescExt(NEO::SipRegisterType type) const override {
        const auto &regdesc = getRegsetDescExtMap.find(type);
        if (regdesc != getRegsetDescExtMap.end()) {
            return regdesc->second;
        }
        return std::nullopt;
    }

    size_t getStateSaveAreaSizeRetValue = 0;
    size_t getStateSaveAreaSize() const override {
        return getStateSaveAreaSizeRetValue;
    }

    std::optional<NEO::SipRegisterType> getSipLibRegisterAccessCapturedType;
    std::vector<NEO::SipRegisterType> getSipLibRegisterAccessQueriedTypes;
    std::vector<void *> getSipLibRegisterAccessQueriedHandles;
    bool getSipLibRegisterAccessRetValue = true;
    std::optional<size_t> getSipLibRegisterAccessFailingCall;
    uint32_t getSipLibRegisterAccessCount;
    std::vector<uint32_t> getSipLibRegisterAccessCountPerCall;
    uint32_t getSipLibRegisterAccessStartOffset;
    bool getSipLibRegisterAccess(void *sipHandle, NEO::SipLibThreadId sipThreadId, NEO::SipRegisterType sipRegisterType, uint32_t *registerCount, uint32_t *registerStartOffset) override {
        const auto callIndex = getSipLibRegisterAccessQueriedTypes.size();
        getSipLibRegisterAccessCapturedType = sipRegisterType;
        getSipLibRegisterAccessQueriedTypes.push_back(sipRegisterType);
        getSipLibRegisterAccessQueriedHandles.push_back(sipHandle);
        if (getSipLibRegisterAccessFailingCall == callIndex) {
            return false;
        }
        if (registerCount) {
            *registerCount = callIndex < getSipLibRegisterAccessCountPerCall.size() ? getSipLibRegisterAccessCountPerCall[callIndex] : getSipLibRegisterAccessCount;
        }
        if (registerStartOffset) {
            *registerStartOffset = getSipLibRegisterAccessStartOffset;
        }
        return getSipLibRegisterAccessRetValue;
    }

    bool getSlmStartOffsetRetValue = true;
    uint32_t getSlmStartOffsetResult = 0;
    bool getSlmStartOffset(void *sipHandle, NEO::SipLibThreadId threadId, uint32_t *startOffset) override {
        if (startOffset) {
            *startOffset = getSlmStartOffsetResult;
        }
        return getSlmStartOffsetRetValue;
    }

    bool getBarrierStartOffsetRetValue = true;
    uint32_t getBarrierStartOffsetResult = 0;
    bool getBarrierStartOffset(void *siphandle, NEO::SipLibThreadId threadId, uint32_t *startOffset) override {
        if (startOffset) {
            *startOffset = getBarrierStartOffsetResult;
        }
        return getBarrierStartOffsetRetValue;
    }

    bool openSipSessionRetValue = true;
    void *openSipSessionHandle = reinterpret_cast<void *>(0x1000);
    NEO::SipReadMemoryFn openSipSessionCapturedReadMemory = nullptr;
    NEO::SipWriteMemoryFn openSipSessionCapturedWriteMemory = nullptr;
    void *openSipSessionCapturedUserArg = nullptr;
    bool openSipSession(void **sipHandle, NEO::SipReadMemoryFn readMemory, NEO::SipWriteMemoryFn writeMemory, void *userArg) override {
        openSipSessionCapturedReadMemory = readMemory;
        openSipSessionCapturedWriteMemory = writeMemory;
        openSipSessionCapturedUserArg = userArg;
        if (openSipSessionRetValue) {
            *sipHandle = openSipSessionHandle;
        }
        return openSipSessionRetValue;
    }

    std::vector<void *> closedSipHandles;
    void closeSipSession(void *sipHandle) override {
        closedSipHandles.push_back(sipHandle);
    }
};
