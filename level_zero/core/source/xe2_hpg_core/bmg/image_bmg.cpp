/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe2_hpg_core/image_xe2_hpg_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_BMG> : public ImageCoreFamily<IGFX_XE2_HPG_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE2_HPG_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_BMG, ImageProductFamily<IGFX_BMG>> populateBMG;

} // namespace L0
