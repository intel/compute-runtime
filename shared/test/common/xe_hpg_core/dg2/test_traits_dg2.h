/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits_platforms.h"

template <>
struct TestTraitsPlatforms<IGFX_DG2> {
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = false;
};
