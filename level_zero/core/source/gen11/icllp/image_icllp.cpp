/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen11/hw_info.h"

#include "level_zero/core/source/image/image_hw.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_ICELAKE_LP> : public ImageCoreFamily<IGFX_GEN11_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    bool initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_GEN11_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_ICELAKE_LP, ImageProductFamily<IGFX_ICELAKE_LP>> populateICLLP;

} // namespace L0
