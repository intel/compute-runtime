/*
 * Copyright (C) 2019-2026 Intel Corporation
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
inline constexpr Type copyBufferToBufferWideStateless{3};
inline constexpr Type copyBufferToBufferStatelessHeapless{4};
inline constexpr Type copyBufferToBufferWideStatelessHeapless{5};
inline constexpr Type copyBufferRect{6};
inline constexpr Type copyBufferRectStateless{7};
inline constexpr Type copyBufferRectWideStateless{8};
inline constexpr Type copyBufferRectStatelessHeapless{9};
inline constexpr Type copyBufferRectWideStatelessHeapless{10};
inline constexpr Type fillBuffer{11};
inline constexpr Type fillBufferStateless{12};
inline constexpr Type fillBufferWideStateless{13};
inline constexpr Type fillBufferStatelessHeapless{14};
inline constexpr Type fillBufferWideStatelessHeapless{15};
inline constexpr Type copyBufferToImage3d{16};
inline constexpr Type copyBufferToImage3dStateless{17};
inline constexpr Type copyBufferToImage3dWideStateless{18};
inline constexpr Type copyBufferToImage3dStatelessHeapless{19};
inline constexpr Type copyBufferToImage3dWideStatelessHeapless{20};
inline constexpr Type copyImage3dToBuffer{21};
inline constexpr Type copyImage3dToBufferStateless{22};
inline constexpr Type copyImage3dToBufferWideStateless{23};
inline constexpr Type copyImage3dToBufferStatelessHeapless{24};
inline constexpr Type copyImage3dToBufferWideStatelessHeapless{25};
inline constexpr Type copyImageToImage1d{26};
inline constexpr Type copyImageToImage1dHeapless{27};
inline constexpr Type copyImageToImage2d{28};
inline constexpr Type copyImageToImage2dHeapless{29};
inline constexpr Type copyImageToImage3d{30};
inline constexpr Type copyImageToImage3dHeapless{31};
inline constexpr Type fillImage1d{32};
inline constexpr Type fillImage1dHeapless{33};
inline constexpr Type fillImage2d{34};
inline constexpr Type fillImage2dHeapless{35};
inline constexpr Type fillImage3d{36};
inline constexpr Type fillImage3dHeapless{37};
inline constexpr Type queryKernelTimestamps{38};
inline constexpr Type fillImage1dBuffer{39};
inline constexpr Type fillImage1dBufferHeapless{40};

constexpr bool isStateless(Type type) {
    constexpr std::array<Type, 10> statelessBuiltins{{copyBufferToBufferStateless, copyBufferRectStateless, fillBufferStateless, copyBufferToImage3dStateless,
                                                      copyImage3dToBufferStateless, copyBufferToBufferStatelessHeapless, copyBufferRectStatelessHeapless, fillBufferStatelessHeapless,
                                                      copyBufferToImage3dStatelessHeapless, copyImage3dToBufferStatelessHeapless}};
    for (auto builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

constexpr bool isWideStateless(Type type) {
    constexpr std::array<Type, 10> statelessBuiltins{{copyBufferToBufferWideStateless, copyBufferRectWideStateless, fillBufferWideStateless, copyBufferToImage3dWideStateless,
                                                      copyImage3dToBufferWideStateless, copyBufferToBufferWideStatelessHeapless, copyBufferRectWideStatelessHeapless, fillBufferWideStatelessHeapless,
                                                      copyBufferToImage3dWideStatelessHeapless, copyImage3dToBufferWideStatelessHeapless}};
    for (auto builtinType : statelessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

constexpr bool isHeapless(Type type) {
    constexpr Type heaplessBuiltins[] = {copyBufferToBufferStatelessHeapless, copyBufferToBufferWideStatelessHeapless, copyBufferRectStatelessHeapless, copyBufferRectWideStatelessHeapless, fillBufferStatelessHeapless, fillBufferWideStatelessHeapless,
                                         copyBufferToImage3dStatelessHeapless, copyBufferToImage3dWideStatelessHeapless, copyImage3dToBufferStatelessHeapless, copyImage3dToBufferWideStatelessHeapless, copyImageToImage1dHeapless, copyImageToImage2dHeapless, copyImageToImage3dHeapless,
                                         fillImage1dHeapless, fillImage2dHeapless, fillImage3dHeapless, fillImage1dBufferHeapless};

    for (auto builtinType : heaplessBuiltins) {
        if (type == builtinType) {
            return true;
        }
    }
    return false;
}

template <Type builtinType>
constexpr uint32_t adjustBuiltinType(const bool useStateless, const bool useHeapless, const bool useWideness = false) {
    return builtinType;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferToBuffer>(const bool useStateless,
                                                         const bool useHeapless,
                                                         const bool useWideness) {
    if (useHeapless) {
        return useWideness ? copyBufferToBufferWideStatelessHeapless
                           : copyBufferToBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? copyBufferToBufferWideStateless
                           : copyBufferToBufferStateless;
    }
    return copyBufferToBuffer;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferRect>(const bool useStateless,
                                                     const bool useHeapless,
                                                     const bool useWideness) {
    if (useHeapless) {
        return useWideness ? copyBufferRectWideStatelessHeapless
                           : copyBufferRectStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? copyBufferRectWideStateless
                           : copyBufferRectStateless;
    }
    return copyBufferRect;
}

template <>
constexpr uint32_t adjustBuiltinType<fillBuffer>(const bool useStateless,
                                                 const bool useHeapless,
                                                 const bool useWideness) {
    if (useHeapless) {
        return useWideness ? fillBufferWideStatelessHeapless
                           : fillBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? fillBufferWideStateless
                           : fillBufferStateless;
    }
    return fillBuffer;
}

template <>
constexpr uint32_t adjustBuiltinType<copyBufferToImage3d>(const bool useStateless,
                                                          const bool useHeapless,
                                                          const bool useWideness) {
    if (useHeapless) {
        return useWideness ? copyBufferToImage3dWideStatelessHeapless
                           : copyBufferToImage3dStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? copyBufferToImage3dWideStateless
                           : copyBufferToImage3dStateless;
    }
    return copyBufferToImage3d;
}

template <>
constexpr uint32_t adjustBuiltinType<copyImage3dToBuffer>(const bool useStateless,
                                                          const bool useHeapless,
                                                          const bool useWideness) {
    if (useHeapless) {
        return useWideness ? copyImage3dToBufferWideStatelessHeapless
                           : copyImage3dToBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? copyImage3dToBufferWideStateless
                           : copyImage3dToBufferStateless;
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
