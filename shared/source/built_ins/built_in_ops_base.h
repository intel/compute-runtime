/*
 * Copyright (C) 2019-2024 Intel Corporation
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
inline constexpr Type copyBufferToBufferStatelessHeapless{3};
inline constexpr Type copyBufferRect{4};
inline constexpr Type copyBufferRectStateless{5};
inline constexpr Type copyBufferRectStatelessHeapless{6};
inline constexpr Type fillBuffer{7};
inline constexpr Type fillBufferStateless{8};
inline constexpr Type fillBufferStatelessHeapless{9};
inline constexpr Type copyBufferToImage3d{10};
inline constexpr Type copyBufferToImage3dStateless{11};
inline constexpr Type copyImage3dToBuffer{12};
inline constexpr Type copyImage3dToBufferStateless{13};
inline constexpr Type copyImageToImage1d{14};
inline constexpr Type copyImageToImage2d{15};
inline constexpr Type copyImageToImage3d{16};
inline constexpr Type fillImage1d{17};
inline constexpr Type fillImage2d{18};
inline constexpr Type fillImage3d{19};
inline constexpr Type queryKernelTimestamps{20};

constexpr bool isStateless(Type type) {
    constexpr std::array<Type, 8> statelessBuiltins{{copyBufferToBufferStateless, copyBufferRectStateless, fillBufferStateless, copyBufferToImage3dStateless, copyImage3dToBufferStateless, copyBufferToBufferStatelessHeapless, copyBufferRectStatelessHeapless, fillBufferStatelessHeapless}};
    for (auto builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

constexpr bool isHeapless(Type type) {
    constexpr Type statelessBuiltins[] = {copyBufferToBufferStatelessHeapless, copyBufferRectStatelessHeapless, fillBufferStatelessHeapless};
    for (auto builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

template <Type builtinType>
constexpr uint32_t adjustBuiltinType(const bool useStateless, const bool useHeapless) {
    return builtinType;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferToBuffer>(const bool useStateless, const bool useHeapless) {
    if (useHeapless) {
        return copyBufferToBufferStatelessHeapless;
    } else if (useStateless) {
        return copyBufferToBufferStateless;
    }
    return copyBufferToBuffer;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferRect>(const bool useStateless, const bool useHeapless) {
    if (useHeapless) {
        return copyBufferRectStatelessHeapless;
    } else if (useStateless) {
        return copyBufferRectStateless;
    }
    return copyBufferRect;
}

template <>
constexpr uint32_t adjustBuiltinType<fillBuffer>(const bool useStateless, const bool useHeapless) {
    if (useHeapless) {
        return fillBufferStatelessHeapless;
    } else if (useStateless) {
        return fillBufferStateless;
    }
    return fillBuffer;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferToImage3d>(const bool useStateless, const bool useHeapless) {
    if (useStateless) {
        return copyBufferToImage3dStateless;
    }
    return copyBufferToImage3d;
}

template <>
constexpr uint32_t adjustBuiltinType<copyImage3dToBuffer>(const bool useStateless, const bool useHeapless) {
    if (useStateless) {
        return copyImage3dToBufferStateless;
    }
    return copyImage3dToBuffer;
}

inline constexpr Type maxBaseValue{20};
inline constexpr Type count{64};
} // namespace EBuiltInOps
} // namespace NEO
