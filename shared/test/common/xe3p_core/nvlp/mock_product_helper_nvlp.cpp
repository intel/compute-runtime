/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_NVL;

#include "shared/test/common/helpers/mock_product_helper_hw.inl"

template struct NEO::MockProductHelperHw<gfxProduct>;
