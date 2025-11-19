/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe3_core/image_xe3_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<NEO::nvlsProductEnumValue> : public ImageCoreFamily<IGFX_XE3_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE3_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<NEO::nvlsProductEnumValue, ImageProductFamily<NEO::nvlsProductEnumValue>> populateNVLS;

} // namespace L0
