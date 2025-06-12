/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>
    : public ::L0::ImageCoreFamily<gfxCoreFamily> {
    using BaseClass = ::L0::ImageCoreFamily<gfxCoreFamily>;

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    WhiteBox() : ::L0::ImageCoreFamily<gfxCoreFamily>() {}

    ~WhiteBox() override {}

    using BaseClass::implicitArgsSurfaceState;
    using BaseClass::packedSurfaceState;
    using BaseClass::redescribedSurfaceState;
    using BaseClass::surfaceState;
    using BaseClass::ImageImp::imageFromBuffer;
    using BaseClass::ImageImp::imgInfo;
    using BaseClass::ImageImp::sampledImage;
    using BaseClass::ImageImp::samplerDesc;
};

} // namespace ult
} // namespace L0
