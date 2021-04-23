/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hp_core/image_xe_hp_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_XE_HP_SDV> : public ImageCoreFamily<IGFX_XE_HP_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE_HP_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_XE_HP_SDV, ImageProductFamily<IGFX_XE_HP_SDV>> populateXEHP;

} // namespace L0
