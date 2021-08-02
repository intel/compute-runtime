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
#include <vector>

namespace NEO {

class Context;
class ClDevice;
struct ArgDescPointer;
struct HardwareInfo;
struct KernelInfo;
struct MultiDispatchInfo;

class ClHwHelper {
  public:
    static ClHwHelper &get(GFXCORE_FAMILY gfxCore);

    virtual bool requiresNonAuxMode(const ArgDescPointer &argAsPtr, const HardwareInfo &hwInfo) const = 0;
    virtual bool requiresAuxResolves(const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) const = 0;
    virtual bool allowRenderCompressionForContext(const ClDevice &clDevice, const Context &context) const = 0;
    virtual cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const = 0;
    virtual bool getQueueFamilyName(std::string &name, EngineGroupType type) const = 0;
    virtual cl_ulong getKernelPrivateMemSize(const KernelInfo &kernelInfo) const = 0;
    virtual bool preferBlitterForLocalToLocalTransfers() const = 0;
    virtual bool isSupportedKernelThreadArbitrationPolicy() const = 0;
    virtual std::vector<uint32_t> getSupportedThreadArbitrationPolicies() const = 0;
    virtual cl_version getDeviceIpVersion(const HardwareInfo &hwInfo) const = 0;
    virtual cl_device_feature_capabilities_intel getSupportedDeviceFeatureCapabilities() const = 0;

  protected:
    virtual bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const = 0;

    static uint8_t makeDeviceRevision(const HardwareInfo &hwInfo);
    static cl_version makeDeviceIpVersion(uint16_t major, uint8_t minor, uint8_t revision);

    ClHwHelper() = default;
};

template <typename GfxFamily>
class ClHwHelperHw : public ClHwHelper {
  public:
    static ClHwHelper &get() {
        static ClHwHelperHw<GfxFamily> clHwHelper;
        return clHwHelper;
    }

    bool requiresNonAuxMode(const ArgDescPointer &argAsPtr, const HardwareInfo &hwInfo) const override;
    bool requiresAuxResolves(const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) const override;
    bool allowRenderCompressionForContext(const ClDevice &clDevice, const Context &context) const override;
    cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const override;
    bool getQueueFamilyName(std::string &name, EngineGroupType type) const override;
    cl_ulong getKernelPrivateMemSize(const KernelInfo &kernelInfo) const override;
    bool preferBlitterForLocalToLocalTransfers() const override;
    bool isSupportedKernelThreadArbitrationPolicy() const override;
    std::vector<uint32_t> getSupportedThreadArbitrationPolicies() const override;
    cl_version getDeviceIpVersion(const HardwareInfo &hwInfo) const override;
    cl_device_feature_capabilities_intel getSupportedDeviceFeatureCapabilities() const override;

  protected:
    bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const override;
    ClHwHelperHw() = default;
};

} // namespace NEO
