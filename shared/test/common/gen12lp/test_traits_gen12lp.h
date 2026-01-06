/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_GEN12LP_CORE> {
    static constexpr bool auxBuiltinsSupported = true;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = false;
    static constexpr bool iohInSbaSupported = true;
    static constexpr bool auxTranslationSupported = true;
    static constexpr bool implementsPreambleThreadArbitration = false;
    static constexpr bool forceGpuNonCoherent = true;
    static constexpr bool imagesSupported = true;
    static constexpr bool largeGrfModeInStateComputeModeSupported = true;
    static constexpr bool heaplessAllowed = false;
    static constexpr bool heaplessRequired = false;
    static constexpr bool bindingTableStateSupported = true;
};
