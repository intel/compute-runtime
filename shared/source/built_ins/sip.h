/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/program/program_info.h"

#include <memory>

namespace NEO {

class Device;
class GraphicsAllocation;

const char *getSipKernelCompilerInternalOptions(SipKernelType kernel);

const char *getSipLlSrc(const Device &device);

class SipKernel {
  public:
    SipKernel(SipKernelType type, ProgramInfo &&sipProgramInfo);
    SipKernel(const SipKernel &) = delete;
    SipKernel &operator=(const SipKernel &) = delete;
    SipKernel(SipKernel &&) = delete;
    SipKernel &operator=(SipKernel &&) = delete;
    virtual ~SipKernel();

    const char *getBinary() const;

    size_t getBinarySize() const;

    SipKernelType getType() const {
        return type;
    }

    static const size_t maxDbgSurfaceSize;

    MOCKABLE_VIRTUAL GraphicsAllocation *getSipAllocation() const;
    static SipKernelType getSipKernelType(GFXCORE_FAMILY family, bool debuggingActive);
    static GraphicsAllocation *getSipKernelAllocation(Device &device);

  protected:
    SipKernelType type = SipKernelType::COUNT;
    const ProgramInfo programInfo;
};
} // namespace NEO
