/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class Kernel;
struct HardwareInfo;

class GTPinGfxCoreHelper {
  public:
    static GTPinGfxCoreHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getGenVersion() = 0;
    virtual bool addSurfaceState(Kernel *pKernel) = 0;
    virtual void *getSurfaceState(Kernel *pKernel, size_t bti) = 0;
    virtual bool canUseSharedAllocation(const HardwareInfo &hwInfo) const = 0;

  protected:
    GTPinGfxCoreHelper(){};
};

template <typename GfxFamily>
class GTPinGfxCoreHelperHw : public GTPinGfxCoreHelper {
  public:
    static GTPinGfxCoreHelper &get() {
        static GTPinGfxCoreHelperHw<GfxFamily> gtpinGfxCoreHelper;
        return gtpinGfxCoreHelper;
    }
    uint32_t getGenVersion() override;
    bool addSurfaceState(Kernel *pKernel) override;
    void *getSurfaceState(Kernel *pKernel, size_t bti) override;
    bool canUseSharedAllocation(const HardwareInfo &hwInfo) const override;

  protected:
    GTPinGfxCoreHelperHw(){};
};
} // namespace NEO
