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
constexpr Type CopyBufferRect{2};
constexpr Type FillBuffer{3};
constexpr Type CopyBufferToImage3d{4};
constexpr Type CopyImage3dToBuffer{5};
constexpr Type CopyImageToImage1d{6};
constexpr Type CopyImageToImage2d{7};
constexpr Type CopyImageToImage3d{8};
constexpr Type FillImage1d{9};
constexpr Type FillImage2d{10};
constexpr Type FillImage3d{11};
constexpr Type VmeBlockMotionEstimateIntel{12};
constexpr Type VmeBlockAdvancedMotionEstimateCheckIntel{13};
constexpr Type VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel{14};
constexpr Type Scheduler{15};
constexpr uint32_t MaxBaseValue{15};
} // namespace EBuiltInOps
} // namespace NEO
