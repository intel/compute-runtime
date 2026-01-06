/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe3p_core/image_xe3p_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_CRI> : public ImageCoreFamily<IGFX_XE3P_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE3P_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_CRI, ImageProductFamily<IGFX_CRI>> populateCRI;

} // namespace L0
