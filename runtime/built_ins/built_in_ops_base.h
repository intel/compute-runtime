/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
namespace EBuiltInOps {

using Type = uint32_t;

constexpr Type AuxTranslation{0};
constexpr Type CopyBufferToBuffer{1};
constexpr Type CopyBufferToBufferStateless{2};
constexpr Type CopyBufferRect{3};
constexpr Type CopyBufferRectStateless{4};
constexpr Type FillBuffer{5};
constexpr Type FillBufferStateless{6};
constexpr Type CopyBufferToImage3d{7};
constexpr Type CopyBufferToImage3dStateless{8};
constexpr Type CopyImage3dToBuffer{9};
constexpr Type CopyImage3dToBufferStateless{10};
constexpr Type CopyImageToImage1d{11};
constexpr Type CopyImageToImage2d{12};
constexpr Type CopyImageToImage3d{13};
constexpr Type FillImage1d{14};
constexpr Type FillImage2d{15};
constexpr Type FillImage3d{16};

constexpr Type MaxBaseValue{16};
constexpr Type COUNT{64};
} // namespace EBuiltInOps
} // namespace NEO
