/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hpc_core/image_xe_hpc_core.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_PVC> : public ImageCoreFamily<IGFX_XE_HPC_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_XE_HPC_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_PVC, ImageProductFamily<IGFX_PVC>> populatePVC;

} // namespace L0
