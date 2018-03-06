/*
 * Copyright (c) 2017, Intel Corporation
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

#include <cinttypes>
#include <memory>
#include "runtime/helpers/ptr_math.h"
#include "runtime/program/program.h"

namespace OCLRT {

class Device;

enum class SipKernelType : std::uint32_t {
    Csr = 0,
    DbgCsr,
    DbgCsrLocal,
    COUNT
};

const char *getSipKernelCompilerInternalOptions(SipKernelType kernel);

const char *getSipLlSrc(const Device &device);

class SipKernel {
  public:
    SipKernel(SipKernelType type, Program *sipProgram);
    SipKernel(const SipKernel &) = delete;
    SipKernel &operator=(const SipKernel &) = delete;
    SipKernel(SipKernel &&) = default;
    SipKernel &operator=(SipKernel &&) = default;

    const char *getBinary() const {
        auto kernelInfo = program->getKernelInfo(size_t{0});
        return reinterpret_cast<const char *>(ptrOffset(kernelInfo->heapInfo.pKernelHeap, kernelInfo->systemKernelOffset));
    }

    size_t getBinarySize() const {
        auto kernelInfo = program->getKernelInfo(size_t{0});
        return kernelInfo->heapInfo.pKernelHeader->KernelHeapSize - kernelInfo->systemKernelOffset;
    }

    SipKernelType getType() const {
        return type;
    }

  protected:
    SipKernelType type = SipKernelType::COUNT;
    std::unique_ptr<Program> program;
};
}
