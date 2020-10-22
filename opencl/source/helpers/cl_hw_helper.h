/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

namespace NEO {

struct HardwareInfo;
struct KernelInfo;
struct MultiDispatchInfo;

class ClHwHelper {
  public:
    static ClHwHelper &get(GFXCORE_FAMILY gfxCore);

    virtual bool requiresAuxResolves(const KernelInfo &kernelInfo) const = 0;

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

    static bool isBlitAuxTranslationRequired(const HardwareInfo &hwInfo, const MultiDispatchInfo &multiDispatchInfo);
    bool requiresAuxResolves(const KernelInfo &kernelInfo) const override;

  protected:
    bool hasStatelessAccessToBuffer(const KernelInfo &kernelInfo) const override;

    ClHwHelperHw() = default;
};

} // namespace NEO
