/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe3p_core/image_xe3p_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<NEO::criProductEnumValue> : public ImageCoreFamily<NEO::xe3pCoreEnumValue> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<NEO::xe3pCoreEnumValue>::initialize(device, desc);
    };
};

static ImagePopulateFactory<NEO::criProductEnumValue, ImageProductFamily<NEO::criProductEnumValue>> populateCRI;

} // namespace L0
