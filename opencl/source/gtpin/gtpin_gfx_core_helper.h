/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "neo_igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace NEO {

class Kernel;
struct HardwareInfo;
class GTPinGfxCoreHelper;

using GTPinGfxCoreHelperCreateFunctionType = std::unique_ptr<GTPinGfxCoreHelper> (*)();

class GTPinGfxCoreHelper {
  public:
    static std::unique_ptr<GTPinGfxCoreHelper> create(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getGenVersion() const = 0;
    virtual void addSurfaceState(Kernel *pKernel) const = 0;
    virtual void *getSurfaceState(Kernel *pKernel, size_t bti) const = 0;
    virtual bool canUseSharedAllocation(const HardwareInfo &hwInfo) const = 0;

    virtual ~GTPinGfxCoreHelper() = default;

  protected:
    GTPinGfxCoreHelper() = default;
};

template <typename GfxFamily>
class GTPinGfxCoreHelperHw : public GTPinGfxCoreHelper {
  public:
    static std::unique_ptr<GTPinGfxCoreHelper> create() {
        auto gtpinHelper = std::unique_ptr<GTPinGfxCoreHelper>(new GTPinGfxCoreHelperHw<GfxFamily>());
        return gtpinHelper;
    }
    uint32_t getGenVersion() const override;
    void addSurfaceState(Kernel *pKernel) const override;
    void *getSurfaceState(Kernel *pKernel, size_t bti) const override;
    bool canUseSharedAllocation(const HardwareInfo &hwInfo) const override;

    ~GTPinGfxCoreHelperHw() override = default;

  protected:
    GTPinGfxCoreHelperHw() = default;
};
} // namespace NEO
