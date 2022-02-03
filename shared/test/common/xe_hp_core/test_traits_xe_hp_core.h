/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_XE_HP_CORE> {
    static constexpr bool surfaceStateCompressionParamsSupported = true;
    static constexpr bool clearColorAddressMatcher = true;
    static constexpr bool auxBuiltinsSupported = true;
    static constexpr bool localMemCompressionAubsSupported = true;
    static constexpr bool systemMemCompressionAubsSupported = false;
    static constexpr bool l3ControlSupported = true;
    static constexpr bool forceNonCoherentSupported = true;
    static constexpr bool threadPreemptionDisableBitMatcher = true;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = false;
    static constexpr bool iohInSbaSupported = false;
    static constexpr bool auxTranslationSupported = true;
    static constexpr bool isUsingNonDefaultIoctls = true;
    static constexpr bool deviceEnqueueSupport = false;
    static constexpr bool fusedEuDispatchSupported = true;
    static constexpr bool numberOfWalkersInCfeStateSupported = true;
    static constexpr bool forceGpuNonCoherent = false;
    static constexpr bool isUnTypedDataPortCacheFlushSupported = false;
    static constexpr bool imagesSupported = true;
};
