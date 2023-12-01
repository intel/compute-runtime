/*
 * Copyright (C) 2019-2023 Intel Corporation
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

inline constexpr Type auxTranslation{0};
inline constexpr Type copyBufferToBuffer{1};
inline constexpr Type copyBufferToBufferStateless{2};
inline constexpr Type copyBufferRect{3};
inline constexpr Type copyBufferRectStateless{4};
inline constexpr Type fillBuffer{5};
inline constexpr Type fillBufferStateless{6};
inline constexpr Type copyBufferToImage3d{7};
inline constexpr Type copyBufferToImage3dStateless{8};
inline constexpr Type copyImage3dToBuffer{9};
inline constexpr Type copyImage3dToBufferStateless{10};
inline constexpr Type copyImageToImage1d{11};
inline constexpr Type copyImageToImage2d{12};
inline constexpr Type copyImageToImage3d{13};
inline constexpr Type fillImage1d{14};
inline constexpr Type fillImage2d{15};
inline constexpr Type fillImage3d{16};
inline constexpr Type queryKernelTimestamps{17};

constexpr bool isStateless(Type type) {
    constexpr std::array<Type, 5> statelessBuiltins{{copyBufferToBufferStateless, copyBufferRectStateless, fillBufferStateless, copyBufferToImage3dStateless, copyImage3dToBufferStateless}};
    for (auto &builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

inline constexpr Type maxBaseValue{17};
inline constexpr Type count{64};
} // namespace EBuiltInOps
} // namespace NEO
