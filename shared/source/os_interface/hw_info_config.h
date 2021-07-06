/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/unified_memory/usm_memory_support.h"

#include "igfxfmid.h"

namespace NEO {

struct HardwareInfo;
struct StateComputeModeProperties;
class OSInterface;
class HwInfoConfig;

extern HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT];

class HwInfoConfig {
  public:
    static HwInfoConfig *get(PRODUCT_FAMILY product) {
        return hwInfoConfigFactory[product];
    }
    int configureHwInfoWddm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface);
    int configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface);
    virtual int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) = 0;
    virtual void adjustPlatformForProductFamily(HardwareInfo *hwInfo) = 0;
    virtual void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) = 0;
    virtual uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) = 0;
    virtual uint64_t getDeviceMemCapabilities() = 0;
    virtual uint64_t getSingleDeviceSharedMemCapabilities() = 0;
    virtual uint64_t getCrossDeviceSharedMemCapabilities() = 0;
    virtual void getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) = 0;
    virtual uint64_t getSharedSystemMemCapabilities() = 0;
    virtual void convertTimestampsFromOaToCsDomain(uint64_t &timestampData) = 0;
    virtual uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) = 0;
    virtual bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const = 0;
    virtual uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const = 0;
    virtual void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) = 0;
    uint32_t threadsPerEu;
};

template <PRODUCT_FAMILY gfxProduct>
class HwInfoConfigHw : public HwInfoConfig {
  public:
    static HwInfoConfig *get() {
        static HwInfoConfigHw<gfxProduct> instance;
        return &instance;
    }
    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) override;
    void adjustPlatformForProductFamily(HardwareInfo *hwInfo) override;
    void adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) override;
    uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) override;
    uint64_t getDeviceMemCapabilities() override;
    uint64_t getSingleDeviceSharedMemCapabilities() override;
    uint64_t getCrossDeviceSharedMemCapabilities() override;
    void getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) override;
    uint64_t getSharedSystemMemCapabilities() override;
    void convertTimestampsFromOaToCsDomain(uint64_t &timestampData) override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) override;
    bool isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const override;
    bool isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const override;
    uint32_t getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const override;
    uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const override;
    void setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) override;

  protected:
    HwInfoConfigHw() = default;

    void enableRenderCompression(HardwareInfo *hwInfo);
    void enableBlitterOperationsSupport(HardwareInfo *hwInfo);
    uint64_t getHostMemCapabilitiesValue();
    bool getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo);
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableProductHwInfoConfig {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;

    EnableProductHwInfoConfig() {
        HwInfoConfig *pHwInfoConfig = HwInfoConfigHw<gfxProduct>::get();
        hwInfoConfigFactory[gfxProduct] = pHwInfoConfig;
        pHwInfoConfig->threadsPerEu = GfxProduct::threadsPerEu;
    }
};

} // namespace NEO
