/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/hw_cmds.h"

namespace NEO {
class Kernel;

class GTPinHwHelper {
  public:
    static GTPinHwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getGenVersion() = 0;
    virtual bool addSurfaceState(Kernel *pKernel) = 0;
    virtual void *getSurfaceState(Kernel *pKernel, size_t bti) = 0;

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

  private:
    GTPinHwHelperHw(){};
};
} // namespace NEO
