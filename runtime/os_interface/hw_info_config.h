/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_info.h"
#include "public/cl_ext_private.h"

#include "igfxfmid.h"

namespace NEO {

struct HardwareInfo;
class OSInterface;
class HwInfoConfig;

extern HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT];

class HwInfoConfig {
  public:
    static HwInfoConfig *get(PRODUCT_FAMILY product) {
        return hwInfoConfigFactory[product];
    }
    int configureHwInfo(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface);
    virtual int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) = 0;
    virtual void adjustPlatformForProductFamily(HardwareInfo *hwInfo) = 0;
    virtual cl_unified_shared_memory_capabilities_intel getHostMemCapabilities() = 0;
    virtual cl_unified_shared_memory_capabilities_intel getDeviceMemCapabilities() = 0;
    virtual cl_unified_shared_memory_capabilities_intel getSingleDeviceSharedMemCapabilities() = 0;
    virtual cl_unified_shared_memory_capabilities_intel getCrossDeviceSharedMemCapabilities() = 0;
    virtual cl_unified_shared_memory_capabilities_intel getSharedSystemMemCapabilities() = 0;
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
    cl_unified_shared_memory_capabilities_intel getHostMemCapabilities() override;
    cl_unified_shared_memory_capabilities_intel getDeviceMemCapabilities() override;
    cl_unified_shared_memory_capabilities_intel getSingleDeviceSharedMemCapabilities() override;
    cl_unified_shared_memory_capabilities_intel getCrossDeviceSharedMemCapabilities() override;
    cl_unified_shared_memory_capabilities_intel getSharedSystemMemCapabilities() override;

  protected:
    HwInfoConfigHw() {}
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
