/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include "gtest/gtest.h"

struct MockTypeForTest {
    const uint64_t objMagic = objMagicValue;
    static const zel_handle_type_t handleType;
};
const zel_handle_type_t MockTypeForTest::handleType = ZEL_HANDLE_DRIVER;
using MockPointerTypeForTest = MockTypeForTest *;

TEST(ApiHandleHelperTest, givenNullptrWhenTranslatingToInternalTypeThenNullptrIsReturned) {
    MockPointerTypeForTest input = nullptr;
    EXPECT_EQ(nullptr, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenValidObjectWhenTranslatingToInternalTypeThenInputIsRetuned) {
    MockTypeForTest dummy{};
    MockPointerTypeForTest input = &dummy;
    EXPECT_EQ(input, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenPointerToTranslateFuncWhenTranslateFuncPassesThenValidObjectIsReturned) {
    char dummy[sizeof(MockTypeForTest)]{};
    MockPointerTypeForTest input = reinterpret_cast<MockPointerTypeForTest>(dummy);

    static MockTypeForTest expectedOutputObject;

    VariableBackup<decltype(L0::loaderTranslateHandleFunc)> translateFuncBackup{&L0::loaderTranslateHandleFunc};
    L0::loaderTranslateHandleFunc = [](zel_handle_type_t handleType, void *input, void **output) -> ze_result_t {
        EXPECT_EQ(MockTypeForTest::handleType, handleType);
        *reinterpret_cast<MockPointerTypeForTest *>(output) = &expectedOutputObject;
        return ZE_RESULT_SUCCESS;
    };

    EXPECT_EQ(&expectedOutputObject, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenPointerToTranslateFuncWhenTranslateFuncFailsThenNullptrIsReturned) {
    char dummy[sizeof(MockTypeForTest)]{};
    MockPointerTypeForTest input = reinterpret_cast<MockPointerTypeForTest>(dummy);

    VariableBackup<decltype(L0::loaderTranslateHandleFunc)> translateFuncBackup{&L0::loaderTranslateHandleFunc};
    L0::loaderTranslateHandleFunc = [](zel_handle_type_t handleType, void *input, void **output) -> ze_result_t {
        EXPECT_EQ(MockTypeForTest::handleType, handleType);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    };
    EXPECT_EQ(nullptr, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenUnknownObjectAndNoPointerToTranslateFuncWhenTranslatingToInternalTypeThenNullptrIsRetuned) {
    char dummy[sizeof(MockTypeForTest)]{};
    MockPointerTypeForTest input = reinterpret_cast<MockPointerTypeForTest>(dummy);

    VariableBackup<decltype(L0::loaderTranslateHandleFunc)> translateFuncBackup{&L0::loaderTranslateHandleFunc};
    L0::loaderTranslateHandleFunc = nullptr;

    EXPECT_EQ(nullptr, toInternalType(input));
}
