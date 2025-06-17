/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeStatesTest = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTest, givenCommandContainerWhenEncodingMediaDescriptorThenUsedSizeDidNotIncreased, IsAtLeastXeCore) {
    auto sizeBefore = cmdContainer->getCommandStream()->getUsed();
    EncodeMediaInterfaceDescriptorLoad<FamilyType>::encode(*cmdContainer.get(), nullptr);
    auto sizeAfter = cmdContainer->getCommandStream()->getUsed();
    EXPECT_EQ(sizeBefore, sizeAfter);
}
