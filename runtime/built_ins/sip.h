/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"

#include <cinttypes>
#include <memory>

namespace NEO {

class Device;
class Program;
class GraphicsAllocation;

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
    ~SipKernel();

    const char *getBinary() const;

    size_t getBinarySize() const;

    SipKernelType getType() const {
        return type;
    }

    static const size_t maxDbgSurfaceSize;

    GraphicsAllocation *getSipAllocation() const;
    static SipKernelType getSipKernelType(GFXCORE_FAMILY family, bool debuggingActive);

  protected:
    SipKernelType type = SipKernelType::COUNT;
    Program *program = nullptr;
};
} // namespace NEO
