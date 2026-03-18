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
namespace BuiltIn {

// Each value maps to precompiled binary (.bin or .spv) or source (.cl) resource,
// which contains one or more kernel functions.
// Variants of the same builtIn group (e.g. stateless, wideStateless, heapless, wideHeapless) select
// alternative compilations of the same kernel group with different addressing modes or compilation options
enum class Group : uint32_t {
    auxTranslation = 0,
    copyBufferToBuffer,
    copyBufferToBufferStateless,
    copyBufferToBufferWideStateless,
    copyBufferToBufferStatelessHeapless,
    copyBufferToBufferWideStatelessHeapless,
    copyBufferRect,
    copyBufferRectStateless,
    copyBufferRectWideStateless,
    copyBufferRectStatelessHeapless,
    copyBufferRectWideStatelessHeapless,
    fillBuffer,
    fillBufferStateless,
    fillBufferWideStateless,
    fillBufferStatelessHeapless,
    fillBufferWideStatelessHeapless,
    copyBufferToImage3d,
    copyBufferToImage3dStateless,
    copyBufferToImage3dWideStateless,
    copyBufferToImage3dStatelessHeapless,
    copyBufferToImage3dWideStatelessHeapless,
    copyImage3dToBuffer,
    copyImage3dToBufferStateless,
    copyImage3dToBufferWideStateless,
    copyImage3dToBufferStatelessHeapless,
    copyImage3dToBufferWideStatelessHeapless,
    copyImageToImage1d,
    copyImageToImage1dHeapless,
    copyImageToImage2d,
    copyImageToImage2dHeapless,
    copyImageToImage3d,
    copyImageToImage3dHeapless,
    fillImage1d,
    fillImage1dHeapless,
    fillImage2d,
    fillImage2dHeapless,
    fillImage3d,
    fillImage3dHeapless,
    queryKernelTimestamps,
    fillImage1dBuffer,
    fillImage1dBufferHeapless,
    queryKernelTimestampsStateless,
    queryKernelTimestampsStatelessHeapless,

    count
};

constexpr uint32_t toIndex(Group id) { return static_cast<uint32_t>(id); }

constexpr bool isStateless(Group group) {
    constexpr std::array<Group, 12> statelessBuiltins{{Group::copyBufferToBufferStateless, Group::copyBufferRectStateless, Group::fillBufferStateless, Group::copyBufferToImage3dStateless,
                                                       Group::copyImage3dToBufferStateless, Group::copyBufferToBufferStatelessHeapless, Group::copyBufferRectStatelessHeapless, Group::fillBufferStatelessHeapless,
                                                       Group::copyBufferToImage3dStatelessHeapless, Group::copyImage3dToBufferStatelessHeapless, Group::queryKernelTimestampsStateless, Group::queryKernelTimestampsStatelessHeapless}};
    for (auto builtinGroup : statelessBuiltins) {
        if (group == builtinGroup) {
            return true;
        }
    }
    return false;
}

constexpr bool isWideStateless(Group group) {
    constexpr std::array<Group, 10> wideStatelessBuiltins{{Group::copyBufferToBufferWideStateless, Group::copyBufferRectWideStateless, Group::fillBufferWideStateless, Group::copyBufferToImage3dWideStateless,
                                                           Group::copyImage3dToBufferWideStateless, Group::copyBufferToBufferWideStatelessHeapless, Group::copyBufferRectWideStatelessHeapless, Group::fillBufferWideStatelessHeapless,
                                                           Group::copyBufferToImage3dWideStatelessHeapless, Group::copyImage3dToBufferWideStatelessHeapless}};
    for (auto builtinGroup : wideStatelessBuiltins) {
        if (group == builtinGroup) {
            return true;
        }
    }
    return false;
}

constexpr bool isHeapless(Group group) {
    constexpr Group heaplessBuiltins[] = {Group::copyBufferToBufferStatelessHeapless, Group::copyBufferToBufferWideStatelessHeapless, Group::copyBufferRectStatelessHeapless, Group::copyBufferRectWideStatelessHeapless, Group::fillBufferStatelessHeapless, Group::fillBufferWideStatelessHeapless,
                                          Group::copyBufferToImage3dStatelessHeapless, Group::copyBufferToImage3dWideStatelessHeapless, Group::copyImage3dToBufferStatelessHeapless, Group::copyImage3dToBufferWideStatelessHeapless, Group::copyImageToImage1dHeapless, Group::copyImageToImage2dHeapless, Group::copyImageToImage3dHeapless,
                                          Group::fillImage1dHeapless, Group::fillImage2dHeapless, Group::fillImage3dHeapless, Group::fillImage1dBufferHeapless, Group::queryKernelTimestampsStatelessHeapless};

    for (auto builtinGroup : heaplessBuiltins) {
        if (group == builtinGroup) {
            return true;
        }
    }
    return false;
}

template <Group id>
constexpr Group adjustBuiltinGroup(const bool useStateless, const bool useHeapless, const bool useWideness = false) {
    return id;
}

template <>
constexpr Group adjustBuiltinGroup<Group::copyBufferToBuffer>(const bool useStateless,
                                                              const bool useHeapless,
                                                              const bool useWideness) {
    if (useHeapless) {
        return useWideness ? Group::copyBufferToBufferWideStatelessHeapless
                           : Group::copyBufferToBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? Group::copyBufferToBufferWideStateless
                           : Group::copyBufferToBufferStateless;
    }
    return Group::copyBufferToBuffer;
}

template <>
constexpr Group adjustBuiltinGroup<Group::copyBufferRect>(const bool useStateless,
                                                          const bool useHeapless,
                                                          const bool useWideness) {
    if (useHeapless) {
        return useWideness ? Group::copyBufferRectWideStatelessHeapless
                           : Group::copyBufferRectStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? Group::copyBufferRectWideStateless
                           : Group::copyBufferRectStateless;
    }
    return Group::copyBufferRect;
}

template <>
constexpr Group adjustBuiltinGroup<Group::fillBuffer>(const bool useStateless,
                                                      const bool useHeapless,
                                                      const bool useWideness) {
    if (useHeapless) {
        return useWideness ? Group::fillBufferWideStatelessHeapless
                           : Group::fillBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? Group::fillBufferWideStateless
                           : Group::fillBufferStateless;
    }
    return Group::fillBuffer;
}

template <>
constexpr Group adjustBuiltinGroup<Group::copyBufferToImage3d>(const bool useStateless,
                                                               const bool useHeapless,
                                                               const bool useWideness) {
    if (useHeapless) {
        return useWideness ? Group::copyBufferToImage3dWideStatelessHeapless
                           : Group::copyBufferToImage3dStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? Group::copyBufferToImage3dWideStateless
                           : Group::copyBufferToImage3dStateless;
    }
    return Group::copyBufferToImage3d;
}

template <>
constexpr Group adjustBuiltinGroup<Group::copyImage3dToBuffer>(const bool useStateless,
                                                               const bool useHeapless,
                                                               const bool useWideness) {
    if (useHeapless) {
        return useWideness ? Group::copyImage3dToBufferWideStatelessHeapless
                           : Group::copyImage3dToBufferStatelessHeapless;
    } else if (useStateless) {
        return useWideness ? Group::copyImage3dToBufferWideStateless
                           : Group::copyImage3dToBufferStateless;
    }
    return Group::copyImage3dToBuffer;
}

template <Group id>
constexpr Group adjustImageBuiltinGroup(const bool useHeapless) {
    return id;
}

#define DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(type)                                    \
    template <>                                                                    \
    constexpr Group adjustImageBuiltinGroup<Group::type>(const bool useHeapless) { \
        return useHeapless ? Group::type##Heapless : Group::type;                  \
    }

DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(copyImageToImage1d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(copyImageToImage2d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(copyImageToImage3d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(fillImage1d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(fillImage2d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(fillImage3d);
DEFINE_ADJUST_BUILTIN_GROUP_IMAGE(fillImage1dBuffer);

} // namespace BuiltIn
} // namespace NEO
