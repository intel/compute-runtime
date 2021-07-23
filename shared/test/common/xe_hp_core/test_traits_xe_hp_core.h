/*
 * Copyright (C) 2021 Intel Corporation
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
    static constexpr bool compressionAubsSupported = true;
    static constexpr bool l3ControlSupported = true;
    static constexpr bool forceNonCoherentSupported = true;
};
