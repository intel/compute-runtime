/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/extensions/public/cl_ext_private.h"

#include "engine_group_types.h"
#include "igfxfmid.h"

#include <string>

namespace NEO {

class Context;
struct HardwareInfo;
struct KernelInfo;
struct MultiDispatchInfo;

class ClHwHelper {
  public:
    static ClHwHelper &get(GFXCORE_FAMILY gfxCore);

    virtual bool requiresAuxResolves(const KernelInfo &kernelInfo) const = 0;
    virtual bool allowRenderCompressionForContext(const HardwareInfo &hwInfo, const Context &context) const = 0;
    virtual cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const = 0;
    virtual bool getQueueFamilyName(std::string &name, EngineGroupType type) const = 0;
    virtual cl_ulong getKernelPrivateMemSize(const KernelInfo &kernelInfo) const = 0;
    virtual bool preferBlitterForLocalToLocalTransfers() const = 0;

  protected:
    virtual bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const = 0;

    ClHwHelper() = default;
};

template <typename GfxFamily>
class ClHwHelperHw : public ClHwHelper {
  public:
    static ClHwHelper &get() {
        static ClHwHelperHw<GfxFamily> clHwHelper;
        return clHwHelper;
    }

    bool requiresAuxResolves(const KernelInfo &kernelInfo) const override;
    bool allowRenderCompressionForContext(const HardwareInfo &hwInfo, const Context &context) const override;
    cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const override;
    bool getQueueFamilyName(std::string &name, EngineGroupType type) const override;
    cl_ulong getKernelPrivateMemSize(const KernelInfo &kernelInfo) const override;
    bool preferBlitterForLocalToLocalTransfers() const override;

  protected:
    bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const override;

    ClHwHelperHw() = default;
};

} // namespace NEO
