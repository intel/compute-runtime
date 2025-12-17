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
    std::vector<char> getSipKernelBinaryRetBinary;
    std::vector<char> getSipKernelBinaryStateSaveAreaHeader;
    int getSipKernelBinaryRetValue = 0;
    int getSipKernelBinary(NEO::Device &device, NEO::SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) override {
        retBinary = getSipKernelBinaryRetBinary;
        stateSaveAreaHeader = getSipKernelBinaryStateSaveAreaHeader;
        return getSipKernelBinaryRetValue;
    }

    bool createRegisterDescriptorMapRetValue = true;
    bool createRegisterDescriptorMap() override {
        return createRegisterDescriptorMapRetValue;
    }

    std::map<NEO::SipRegisterType, SIP::regset_desc *> mockRegsetDescMap;
    std::optional<NEO::SipRegisterType> getRegsetDescFromMapCapturedType;
    SIP::regset_desc *getRegsetDescFromMap(NEO::SipRegisterType type) override {
        getRegsetDescFromMapCapturedType = type;
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
    bool getSipLibRegisterAccessRetValue = true;
    uint32_t getSipLibRegisterAccessCount;
    uint32_t getSipLibRegisterAccessStartOffset;
    bool getSipLibRegisterAccess(void *sipHandle, NEO::SipLibThreadId sipThreadId, NEO::SipRegisterType sipRegisterType, uint32_t *registerCount, uint32_t *registerStartOffset) override {
        getSipLibRegisterAccessCapturedType = sipRegisterType;
        if (registerCount) {
            *registerCount = getSipLibRegisterAccessCount;
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
};
