/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <array>
#include <cstdint>

namespace NEO {
namespace EBuiltInOps {

using Type = uint32_t;

inline constexpr Type AuxTranslation{0};
inline constexpr Type CopyBufferToBuffer{1};
inline constexpr Type CopyBufferToBufferStateless{2};
inline constexpr Type CopyBufferRect{3};
inline constexpr Type CopyBufferRectStateless{4};
inline constexpr Type FillBuffer{5};
inline constexpr Type FillBufferStateless{6};
inline constexpr Type CopyBufferToImage3d{7};
inline constexpr Type CopyBufferToImage3dStateless{8};
inline constexpr Type CopyImage3dToBuffer{9};
inline constexpr Type CopyImage3dToBufferStateless{10};
inline constexpr Type CopyImageToImage1d{11};
inline constexpr Type CopyImageToImage2d{12};
inline constexpr Type CopyImageToImage3d{13};
inline constexpr Type FillImage1d{14};
inline constexpr Type FillImage2d{15};
inline constexpr Type FillImage3d{16};
inline constexpr Type QueryKernelTimestamps{17};

constexpr bool isStateless(Type type) {
    constexpr std::array<Type, 5> statelessBuiltins{{CopyBufferToBufferStateless, CopyBufferRectStateless, FillBufferStateless, CopyBufferToImage3dStateless, CopyImage3dToBufferStateless}};
    for (auto &builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

inline constexpr Type MaxBaseValue{17};
inline constexpr Type COUNT{64};
} // namespace EBuiltInOps
} // namespace NEO
