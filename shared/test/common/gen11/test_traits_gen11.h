/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits.h"

template <>
struct TestTraits<IGFX_GEN11_CORE> {
    static constexpr bool auxBuiltinsSupported = false;
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = true;
    static constexpr bool iohInSbaSupported = true;
    static constexpr bool auxTranslationSupported = false;
    static constexpr bool isUsingNonDefaultIoctls = false;
};
