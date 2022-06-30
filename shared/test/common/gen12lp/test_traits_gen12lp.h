/*
 * Copyright (C) 2021-2022 Intel Corporation
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
    static constexpr bool isUsingNonDefaultIoctls = false;
    static constexpr bool deviceEnqueueSupport = false;
    static constexpr bool implementsPreambleThreadArbitration = false;
    static constexpr bool forceGpuNonCoherent = true;
    static constexpr bool imagesSupported = true;
};
