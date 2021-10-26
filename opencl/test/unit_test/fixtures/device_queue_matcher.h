/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "test_traits_common.h"

struct DeviceEnqueueSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::deviceEnqueueSupport;
    }
};