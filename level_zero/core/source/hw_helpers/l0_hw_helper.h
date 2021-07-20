/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include "igfxfmid.h"

namespace L0 {

struct HardwareInfo;
struct Event;
struct Device;
struct EventPool;

class L0HwHelper {
  public:
    static L0HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual void setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, uint32_t groupType) const = 0;
    virtual L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const = 0;

    virtual bool isResumeWARequired() = 0;

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
    L0::Event *createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const override;
    L0HwHelperHw() = default;

    bool isResumeWARequired() override;
};

} // namespace L0
