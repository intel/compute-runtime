/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
