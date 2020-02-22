/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define EXPECT_EQ_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_EQ(evalA, evalB); \
    }

#define EXPECT_NE_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_NE(evalA, evalB); \
    }

#define EXPECT_GT_VAL(a, b)      \
    {                            \
        auto evalA = (a);        \
        auto evalB = (b);        \
        EXPECT_GT(evalA, evalB); \
    }

#define EXPECT_EQ_CONST(a, b)       \
    {                               \
        decltype(b) expected = (a); \
        EXPECT_EQ_VAL(expected, b); \
    }
