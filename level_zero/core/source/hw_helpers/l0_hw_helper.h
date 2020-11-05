/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"
#include "ze_api.h"

namespace L0 {

struct HardwareInfo;

class L0HwHelper {
  public:
    static L0HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, uint32_t groupType) const = 0;

  protected:
    L0HwHelper() = default;
};

template <typename GfxFamily>
class L0HwHelperHw : public L0HwHelper {
  public:
    static L0HwHelper &get() {
        static L0HwHelperHw<GfxFamily> l0HwHelper;
        return l0HwHelper;
    }
    void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, uint32_t groupType) const override;
    L0HwHelperHw() = default;
};

} // namespace L0
