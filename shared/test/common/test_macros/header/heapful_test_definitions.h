/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define HEAPFUL_HWTEST_F(test_fixture, test_name) \
    HWTEST2_F(test_fixture, test_name, IsHeapfulRequired)

#define HEAPFUL_HWTEST_P(test_fixture, test_name) \
    HWTEST2_P(test_fixture, test_name, IsHeapfulRequired)

#define SBA_HWTEST_F(test_fixture, test_name) \
    HWTEST2_F(test_fixture, test_name, IsSbaRequired)

#define SBA_HWTEST_P(test_fixture, test_name) \
    HWTEST2_P(test_fixture, test_name, IsSbaRequired)
