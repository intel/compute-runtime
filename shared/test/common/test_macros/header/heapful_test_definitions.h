/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define HEAPFUL_HWTEST_F(test_fixture, test_name) \
    HWTEST2_F(test_fixture, test_name, IsHeapfulSupported)

#define HEAPFUL_HWTEST_P(test_fixture, test_name) \
    HWTEST2_P(test_fixture, test_name, IsHeapfulSupported)
