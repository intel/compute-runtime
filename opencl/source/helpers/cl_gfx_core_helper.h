/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/api_gfx_core_helper.h"
#include "shared/source/helpers/definitions/engine_group_types.h"

#include "opencl/extensions/public/cl_ext_private.h"

#include "neo_igfxfmid.h"

#include <memory>
#include <string>
#include <vector>

namespace NEO {

class Context;
class ClDevice;
struct ArgDescPointer;
struct HardwareInfo;
struct KernelInfo;
struct MultiDispatchInfo;
struct RootDeviceEnvironment;

class ClGfxCoreHelper;
using createClGfxCoreHelperFunctionType = std::unique_ptr<ClGfxCoreHelper> (*)();

class ClGfxCoreHelper : public ApiGfxCoreHelper {
  public:
    static std::unique_ptr<ClGfxCoreHelper> create(GFXCORE_FAMILY gfxCore);

    ~ClGfxCoreHelper() override = default;
    virtual bool requiresNonAuxMode(const ArgDescPointer &argAsPtr) const = 0;
    virtual bool requiresAuxResolves(const KernelInfo &kernelInfo) const = 0;
    virtual cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const = 0;
    virtual bool getQueueFamilyName(std::string &name, EngineGroupType type) const = 0;
    virtual bool preferBlitterForLocalToLocalTransfers() const = 0;
    virtual bool isSupportedKernelThreadArbitrationPolicy() const = 0;
    virtual std::vector<uint32_t> getSupportedThreadArbitrationPolicies() const = 0;
    virtual cl_device_feature_capabilities_intel getSupportedDeviceFeatureCapabilities(const RootDeviceEnvironment &rootDeviceEnvironment) const = 0;
    virtual bool allowImageCompression(cl_image_format format) const = 0;
    virtual bool isFormatRedescribable(cl_image_format format) const = 0;
    virtual bool isLimitationForPreemptionNeeded() const = 0;

  protected:
    virtual bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const = 0;

    static uint8_t makeDeviceRevision(const HardwareInfo &hwInfo);

    ClGfxCoreHelper() = default;
};

template <typename GfxFamily>
class ClGfxCoreHelperHw : public ClGfxCoreHelper {
  public:
    ~ClGfxCoreHelperHw() override = default;
    static std::unique_ptr<ClGfxCoreHelper> create() {
        return std::unique_ptr<ClGfxCoreHelper>(new ClGfxCoreHelperHw<GfxFamily>());
    }

    bool requiresNonAuxMode(const ArgDescPointer &argAsPtr) const override;
    bool requiresAuxResolves(const KernelInfo &kernelInfo) const override;
    cl_command_queue_capabilities_intel getAdditionalDisabledQueueFamilyCapabilities(EngineGroupType type) const override;
    bool getQueueFamilyName(std::string &name, EngineGroupType type) const override;
    bool preferBlitterForLocalToLocalTransfers() const override;
    bool isSupportedKernelThreadArbitrationPolicy() const override;
    std::vector<uint32_t> getSupportedThreadArbitrationPolicies() const override;
    cl_device_feature_capabilities_intel getSupportedDeviceFeatureCapabilities(const RootDeviceEnvironment &rootDeviceEnvironment) const override;
    bool allowImageCompression(cl_image_format format) const override;
    bool isFormatRedescribable(cl_image_format format) const override;
    bool isLimitationForPreemptionNeeded() const override;

  protected:
    bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const override;
    ClGfxCoreHelperHw() = default;
};

extern createClGfxCoreHelperFunctionType clGfxCoreHelperFactory[IGFX_MAX_CORE];

} // namespace NEO
