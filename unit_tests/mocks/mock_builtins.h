/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/program/program.h"

#include <memory>

class MockBuiltins : public OCLRT::BuiltIns {
  public:
    const OCLRT::SipKernel &getSipKernel(OCLRT::SipKernelType type, OCLRT::Device &device) override {
        if (sipKernelsOverride.find(type) != sipKernelsOverride.end()) {
            return *sipKernelsOverride[type];
        }
        getSipKernelCalled = true;
        getSipKernelType = type;
        return BuiltIns::getSipKernel(type, device);
    }

    void overrideSipKernel(std::unique_ptr<OCLRT::SipKernel> kernel) {
        sipKernelsOverride[kernel->getType()] = std::move(kernel);
    }

    OCLRT::BuiltIns *originalGlobalBuiltins = nullptr;
    std::map<OCLRT::SipKernelType, std::unique_ptr<OCLRT::SipKernel>> sipKernelsOverride;
    bool getSipKernelCalled = false;
    OCLRT::SipKernelType getSipKernelType = OCLRT::SipKernelType::COUNT;
};
