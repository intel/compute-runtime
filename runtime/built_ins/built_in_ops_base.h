/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
constexpr Type CopyImageToImage1d{10};
constexpr Type CopyImageToImage2d{11};
constexpr Type CopyImageToImage3d{12};
constexpr Type FillImage1d{13};
constexpr Type FillImage2d{14};
constexpr Type FillImage3d{15};
constexpr Type VmeBlockMotionEstimateIntel{16};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{17};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{18};
constexpr Type Scheduler{19};

constexpr uint32_t MaxBaseValue{19};
} // namespace EBuiltInOps
} // namespace NEO
