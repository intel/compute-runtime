/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/test_traits_platforms.h"

template <>
struct TestTraitsPlatforms<IGFX_METEORLAKE> {
    static constexpr bool programOnlyChangedFieldsInComputeStateMode = false;
};
