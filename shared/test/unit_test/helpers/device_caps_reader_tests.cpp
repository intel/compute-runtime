/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/device_caps_reader.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(DeviceCapsReaderTbxTest, givenTbxReaderWhenCreateIsCalledThenAReaderIsReturned) {
    auto aubManager = std::make_unique<MockAubManager>();

    auto capsReader = DeviceCapsReaderTbx::create(*aubManager, 0);
    EXPECT_NE(nullptr, capsReader);
}

TEST(DeviceCapsReaderTbxTest, givenTbxReaderWhenIndexOperatorIsUsedThenReadMMIO) {
    struct MyMockAubManager : MockAubManager {
        uint32_t readMMIO(uint32_t offset) override {
            return 123;
        }
    };

    MyMockAubManager aubManager;

    auto capsReader = DeviceCapsReaderTbx::create(aubManager, 0);
    EXPECT_NE(nullptr, capsReader);

    auto val = (*capsReader)[0];
    EXPECT_EQ(123u, val);
}

TEST(DeviceCapsReaderTbxTest, givenTbxReaderWhenGettingOffsetThenOffsetIsReturned) {
    const auto offset = 123u;
    auto aubManager = std::make_unique<MockAubManager>();

    auto capsReader = DeviceCapsReaderTbx::create(*aubManager, offset);
    EXPECT_NE(nullptr, capsReader);

    auto val = capsReader->getOffset();
    EXPECT_EQ(offset, val);
}
