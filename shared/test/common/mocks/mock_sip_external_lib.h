/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/sip_external_lib/sip_external_lib.h"

class MockSipExternalLib : public NEO::SipExternalLib {
  public:
    std::vector<char> getSipKernelBinaryRetBinary;
    std::vector<char> getSipKernelBinaryStateSaveAreaHeader;
    int getSipKernelBinaryRetValue = 0;
    int getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) override {
        retBinary = getSipKernelBinaryRetBinary;
        stateSaveAreaHeader = getSipKernelBinaryStateSaveAreaHeader;
        return getSipKernelBinaryRetValue;
    }

    bool createRegisterDescriptorMapRetValue = true;
    bool createRegisterDescriptorMap() override {
        return createRegisterDescriptorMapRetValue;
    }

    SIP::regset_desc *getRegsetDescFromMapRetValue = nullptr;
    SIP::regset_desc *getRegsetDescFromMap(uint32_t type) override {
        return getRegsetDescFromMapRetValue;
    }

    size_t getStateSaveAreaSizeRetValue = 0;
    size_t getStateSaveAreaSize() const override {
        return getStateSaveAreaSizeRetValue;
    }

    bool getSipLibRegisterAccessRetValue = true;
    bool getSipLibRegisterAccess(void *sipHandle, SipLibThreadId &sipThreadId, uint32_t sipRegisterType, uint32_t *registerCount, uint32_t *registerStartOffset) override {
        return getSipLibRegisterAccessRetValue;
    }
    uint32_t getSipLibCommandRegisterTypeRetValue = 0;
    uint32_t getSipLibCommandRegisterType() override {
        return getSipLibCommandRegisterTypeRetValue;
    }
};
