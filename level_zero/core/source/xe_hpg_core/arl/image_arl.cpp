/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/xe_hpg_core/image_xe_hpg_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_ARROWLAKE> : public ImageCoreFamily<IGFX_XE_HPG_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE_HPG_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_ARROWLAKE, ImageProductFamily<IGFX_ARROWLAKE>> populateARL;

} // namespace L0
