/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_GEN12LP_CORE> {
    static constexpr bool auxBuiltinsSupported = true;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = true;
    static constexpr bool iohInSbaSupported = true;
    static constexpr bool auxTranslationSupported = true;
    static constexpr bool isUsingNonDefaultIoctls = false;
};
