/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/iga_wrapper.h"

#include <map>
#include <string>

struct MockIgaWrapper : public IgaWrapper {
    bool tryDisassembleGenISA(const void *kernelPtr, uint32_t kernelSize, std::string &out) override {
        out = asmToReturn;
        disasmWasCalled = true;
        receivedBinary.assign(reinterpret_cast<const char *>(kernelPtr), kernelSize);
        return asmToReturn.size() != 0;
    }

    bool tryAssembleGenISA(const std::string &inAsm, std::string &outBinary) override {
        outBinary = binaryToReturn;
        asmWasCalled = true;
        receivedAsm = inAsm;
        return outBinary.size() != 0;
    }

    void setGfxCore(GFXCORE_FAMILY core) override {
    }

    void setProductFamily(PRODUCT_FAMILY product) override {
    }

    bool isKnownPlatform() const override {
        return false;
    }

    bool tryLoadIga() override {
        return true;
    }

    std::string asmToReturn;
    std::string binaryToReturn;
    std::string receivedAsm;
    std::string receivedBinary;

    bool disasmWasCalled = false;
    bool asmWasCalled = false;
};
