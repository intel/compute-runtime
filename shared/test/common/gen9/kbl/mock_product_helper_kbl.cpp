/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_product_helper_hw.h"

namespace NEO {

constexpr static auto gfxProduct = IGFX_KABYLAKE;
#include "shared/test/common/helpers/mock_product_helper_hw.inl"
template struct MockProductHelperHw<gfxProduct>;
} // namespace NEO