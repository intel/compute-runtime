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
    SipKernel(SipKernelType type, const void *binary, size_t binarySize);
    SipKernel(const SipKernel &) = delete;
    SipKernel &operator=(const SipKernel &) = delete;
    SipKernel(SipKernel &&) = default;
    SipKernel &operator=(SipKernel &&) = default;

    const char *getBinary() const {
        return binary.get();
    }

    size_t getBinarySize() const {
        return binarySize;
    }

    SipKernelType getType() const {
        return type;
    }

  protected:
    SipKernelType type = SipKernelType::COUNT;
    std::unique_ptr<char[]> binary = nullptr;
    size_t binarySize = 0;
};
}
