/*
 * Copyright (C) 2019-2025 Intel Corporation
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
inline constexpr Type copyBufferToImage3dHeapless{12};
inline constexpr Type copyImage3dToBuffer{13};
inline constexpr Type copyImage3dToBufferStateless{14};
inline constexpr Type copyImage3dToBufferHeapless{15};
inline constexpr Type copyImageToImage1d{16};
inline constexpr Type copyImageToImage1dHeapless{17};
inline constexpr Type copyImageToImage2d{18};
inline constexpr Type copyImageToImage2dHeapless{19};
inline constexpr Type copyImageToImage3d{20};
inline constexpr Type copyImageToImage3dHeapless{21};
inline constexpr Type fillImage1d{22};
inline constexpr Type fillImage1dHeapless{23};
inline constexpr Type fillImage2d{24};
inline constexpr Type fillImage2dHeapless{25};
inline constexpr Type fillImage3d{26};
inline constexpr Type fillImage3dHeapless{27};
inline constexpr Type queryKernelTimestamps{28};
inline constexpr Type fillImage1dBuffer{29};
inline constexpr Type fillImage1dBufferHeapless{30};

constexpr bool isStateless(Type type) {
    constexpr std::array<Type, 10> statelessBuiltins{{copyBufferToBufferStateless, copyBufferRectStateless, fillBufferStateless, copyBufferToImage3dStateless,
                                                      copyImage3dToBufferStateless, copyBufferToBufferStatelessHeapless, copyBufferRectStatelessHeapless, fillBufferStatelessHeapless,
                                                      copyBufferToImage3dHeapless, copyImage3dToBufferHeapless}};
    for (auto builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

constexpr bool isHeapless(Type type) {
    constexpr Type statelessBuiltins[] = {copyBufferToBufferStatelessHeapless, copyBufferRectStatelessHeapless, fillBufferStatelessHeapless,
                                          copyBufferToImage3dHeapless, copyImage3dToBufferHeapless, copyImageToImage1dHeapless, copyImageToImage2dHeapless, copyImageToImage3dHeapless,
                                          fillImage1dHeapless, fillImage2dHeapless, fillImage3dHeapless, fillImage1dBufferHeapless};

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
    if (useHeapless) {
        return copyBufferToImage3dHeapless;
    } else if (useStateless) {
        return copyBufferToImage3dStateless;
    }
    return copyBufferToImage3d;
}

template <>
constexpr uint32_t adjustBuiltinType<copyImage3dToBuffer>(const bool useStateless, const bool useHeapless) {
    if (useHeapless) {
        return copyImage3dToBufferHeapless;
    } else if (useStateless) {
        return copyImage3dToBufferStateless;
    }
    return copyImage3dToBuffer;
}

template <Type builtinType>
constexpr Type adjustImageBuiltinType(const bool useHeapless) {
    return builtinType;
}

#define DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(type)                            \
    template <>                                                           \
    constexpr Type adjustImageBuiltinType<type>(const bool useHeapless) { \
        return useHeapless ? type##Heapless : type;                       \
    }

DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(copyImageToImage1d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(copyImageToImage2d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(copyImageToImage3d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(fillImage1d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(fillImage2d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(fillImage3d);
DEFINE_ADJUST_BUILTIN_TYPE_IMAGE(fillImage1dBuffer);

inline constexpr Type maxBaseValue{fillImage1dBufferHeapless};
inline constexpr Type count{64};
} // namespace EBuiltInOps
} // namespace NEO
