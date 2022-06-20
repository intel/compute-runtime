/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
const char *CompilerHwInfoConfigHw<IGFX_DG2>::getCachingPolicyOptions() const {
    static constexpr const char *cachingPolicy = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    return cachingPolicy;
};
