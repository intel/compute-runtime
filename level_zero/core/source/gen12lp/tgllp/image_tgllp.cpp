/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/image_hw.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_TIGERLAKE_LP> : public ImageCoreFamily<IGFX_GEN12LP_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    bool initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_GEN12LP_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_TIGERLAKE_LP, ImageProductFamily<IGFX_TIGERLAKE_LP>> populateTGLLP;

} // namespace L0
