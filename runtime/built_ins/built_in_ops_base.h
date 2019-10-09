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
constexpr Type FillBuffer{4};
constexpr Type CopyBufferToImage3d{5};
constexpr Type CopyImage3dToBuffer{6};
constexpr Type CopyImageToImage1d{7};
constexpr Type CopyImageToImage2d{8};
constexpr Type CopyImageToImage3d{9};
constexpr Type FillImage1d{10};
constexpr Type FillImage2d{11};
constexpr Type FillImage3d{12};
constexpr Type VmeBlockMotionEstimateIntel{13};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{14};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{15};
constexpr Type Scheduler{16};

constexpr uint32_t MaxBaseValue{16};
} // namespace EBuiltInOps
} // namespace NEO
