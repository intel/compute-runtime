/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"

namespace NEO {
class Kernel;

class GTPinHwHelper {
  public:
    static GTPinHwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getGenVersion() = 0;
    virtual bool addSurfaceState(Kernel *pKernel) = 0;
    virtual void *getSurfaceState(Kernel *pKernel, size_t bti) = 0;
    virtual bool canUseSharedAllocation(const HardwareInfo &hwInfo) const = 0;

  protected:
    GTPinHwHelper(){};
};

template <typename GfxFamily>
class GTPinHwHelperHw : public GTPinHwHelper {
  public:
    static GTPinHwHelper &get() {
        static GTPinHwHelperHw<GfxFamily> gtpinHwHelper;
        return gtpinHwHelper;
    }
    uint32_t getGenVersion() override;
    bool addSurfaceState(Kernel *pKernel) override;
    void *getSurfaceState(Kernel *pKernel, size_t bti) override;
    bool canUseSharedAllocation(const HardwareInfo &hwInfo) const override;

  protected:
    GTPinHwHelperHw(){};
};
} // namespace NEO
