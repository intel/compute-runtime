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
constexpr Type CopyImage3dToBuffer{8};
constexpr Type CopyImageToImage1d{9};
constexpr Type CopyImageToImage2d{10};
constexpr Type CopyImageToImage3d{11};
constexpr Type FillImage1d{12};
constexpr Type FillImage2d{13};
constexpr Type FillImage3d{14};
constexpr Type VmeBlockMotionEstimateIntel{15};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{16};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{17};
constexpr Type Scheduler{18};

constexpr uint32_t MaxBaseValue{18};
} // namespace EBuiltInOps
} // namespace NEO
