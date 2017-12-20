/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "igfxfmid.h"

#include "runtime/helpers/hw_info.h"

namespace OCLRT {

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

  protected:
    HwInfoConfigHw() {}
};

template <PRODUCT_FAMILY gfxProduct>
struct LinuxEnableGfxProductHw {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;

    LinuxEnableGfxProductHw() {
        HwInfoConfig *pHwInfoConfig = HwInfoConfigHw<gfxProduct>::get();
        hwInfoConfigFactory[gfxProduct] = pHwInfoConfig;
        pHwInfoConfig->threadsPerEu = GfxProduct::threadsPerEu;
    }
};

} // namespace OCLRT
