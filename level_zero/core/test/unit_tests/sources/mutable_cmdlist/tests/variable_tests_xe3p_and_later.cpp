/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/variable_fixture.h"

namespace L0 {
namespace ult {

using VariableInOrderTest = Test<VariableInOrderFixture>;

HWTEST2_F(VariableInOrderTest, givenCbSignalExternalEventWhenMutatingEventThenPostSyncFieldIsUpdated, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;
    createMutableComputeWalker<FamilyType, WalkerType>(0x1000);

    auto event = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, event);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, this->mutableComputeWalker.get(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_EQ(this->externalEventCounterValue, this->variable->eventValue.inOrderExecBaseSignalValue);

    auto newEvent = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, newEvent);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_EQ(this->externalEventCounterValue * 2, newEvent->getInOrderExecBaseSignalValue());
    EXPECT_EQ(this->externalEventCounterValue * 2, this->variable->eventValue.inOrderExecBaseSignalValue);

    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    EXPECT_EQ(reinterpret_cast<uint64_t>(this->externalEventDeviceAddress), walkerCmdCpu->getPostSync().getDestinationAddress());
}

} // namespace ult
} // namespace L0
