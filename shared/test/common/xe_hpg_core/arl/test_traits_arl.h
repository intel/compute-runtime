/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits_platforms.h"

template <>
struct TestTraitsPlatforms<IGFX_ARROWLAKE> {
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = true;
};
