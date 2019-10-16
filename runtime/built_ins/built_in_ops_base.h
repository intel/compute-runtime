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
constexpr Type CopyBufferToImage3d{6};
constexpr Type CopyImage3dToBuffer{7};
constexpr Type CopyImageToImage1d{8};
constexpr Type CopyImageToImage2d{9};
constexpr Type CopyImageToImage3d{10};
constexpr Type FillImage1d{11};
constexpr Type FillImage2d{12};
constexpr Type FillImage3d{13};
constexpr Type VmeBlockMotionEstimateIntel{14};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{15};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{16};
constexpr Type Scheduler{17};

constexpr uint32_t MaxBaseValue{17};
} // namespace EBuiltInOps
} // namespace NEO
