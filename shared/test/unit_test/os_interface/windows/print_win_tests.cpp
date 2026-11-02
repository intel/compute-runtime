/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/print.h"

#include "gtest/gtest.h"

#include <cstdlib>

namespace {
void customInvalidParameterHandler(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t) {}
} // namespace

TEST(SimpleSprintfWinTest, GivenInvalidConversionSpecifierWhenSimpleSprintfIsCalledThenZeroIsReturned) {
    char out[64] = {};

    EXPECT_EQ(0U, simpleSprintf<int32_t>(out, sizeof(out), "%y", 1));
    EXPECT_EQ(0U, simpleSprintf(out, sizeof(out), "%y", "value"));
}

TEST(SimpleSprintfWinTest, GivenThreadLocalInvalidParameterHandlerWhenSimpleSprintfIsCalledThenTheHandlerIsRestored) {
    char out[64] = {};
    auto previousHandler = _set_thread_local_invalid_parameter_handler(customInvalidParameterHandler);

    simpleSprintf<int32_t>(out, sizeof(out), "%y", 1);
    EXPECT_TRUE(customInvalidParameterHandler == _get_thread_local_invalid_parameter_handler());

    simpleSprintf(out, sizeof(out), "%y", "value");
    EXPECT_TRUE(customInvalidParameterHandler == _get_thread_local_invalid_parameter_handler());

    _set_thread_local_invalid_parameter_handler(previousHandler);
}
