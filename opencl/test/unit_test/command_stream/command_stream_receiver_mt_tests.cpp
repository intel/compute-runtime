/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

struct CommandStreamReceiverMtTest : public ClDeviceFixture,
                                     public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();

        commandStreamReceiver = &pDevice->getGpgpuCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
    }

    void TearDown() override {
        ClDeviceFixture::tearDown();
    }

    CommandStreamReceiver *commandStreamReceiver;
};

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadWhenSettingFlagProgressThenFunctionAsksTwiceForConfirmation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(0);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter, &mockCSR]() {
        if (confirmationCounter == 0) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 1) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        }
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserStartConfirmation;

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    EXPECT_EQ(2u, confirmationCounter);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadBeforeWalkerOnlyWhenSettingFlagProgressThenFunctionAsksOnceForConfirmation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(0);
    DebugManager.flags.PauseOnGpuMode.set(0);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter, &mockCSR]() {
        EXPECT_EQ(0u, confirmationCounter);
        EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
        confirmationCounter++;
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserStartConfirmation;

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    EXPECT_EQ(1u, confirmationCounter);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_FALSE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadAfterWalkerOnlyWhenSettingFlagProgressThenFunctionAsksOnceForConfirmation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(0);
    DebugManager.flags.PauseOnGpuMode.set(1);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter, &mockCSR]() {
        EXPECT_EQ(0u, confirmationCounter);
        EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
        confirmationCounter++;
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserEndConfirmation;

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    EXPECT_EQ(1u, confirmationCounter);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadOnEachEnqueueWhenSettingFlagProgressThenFunctionAsksMultipleTimesForConfirmation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(-2);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter, &mockCSR]() {
        if (confirmationCounter == 0) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 1) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 2) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 3) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
            DebugManager.flags.PauseOnEnqueue.set(-1);
        }
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserStartConfirmation;

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }

    EXPECT_EQ(4u, confirmationCounter);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadOnEachBlitWhenSettingFlagProgressThenFunctionAsksMultipleTimesForConfirmation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnBlitCopy.set(-2);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter, &mockCSR]() {
        if (confirmationCounter == 0) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 1) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 2) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserStartConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
        } else if (confirmationCounter == 3) {
            {
                std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
                EXPECT_TRUE(DebugPauseState::waitingForUserEndConfirmation == *mockCSR->debugPauseStateAddress);
            }
            confirmationCounter++;
            DebugManager.flags.PauseOnBlitCopy.set(-1);
        }
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserStartConfirmation;

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserEndConfirmation;
    }

    while (currentValue != DebugPauseState::hasUserEndConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }

    EXPECT_EQ(4u, confirmationCounter);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadWhenTerminatingAtFirstStageThenFunctionEndsCorrectly) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(0);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter]() {
        confirmationCounter++;
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::terminate;
    }

    EXPECT_EQ(0u, confirmationCounter);
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.length());
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}

HWTEST_F(CommandStreamReceiverMtTest, givenDebugPauseThreadWhenTerminatingAtSecondStageThenFunctionEndsCorrectly) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PauseOnEnqueue.set(0);
    testing::internal::CaptureStdout();
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    uint32_t confirmationCounter = 0;

    mockCSR->debugConfirmationFunction = [&confirmationCounter]() {
        confirmationCounter++;
    };

    pDevice->resetCommandStreamReceiver(mockCSR);

    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::waitingForUserStartConfirmation;
    }
    auto currentValue = DebugPauseState::waitingForUserStartConfirmation;

    while (currentValue != DebugPauseState::hasUserStartConfirmation) {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        currentValue = *mockCSR->debugPauseStateAddress;
    }
    {
        std::unique_lock<SpinLock> lock{mockCSR->debugPauseStateLock};
        *mockCSR->debugPauseStateAddress = DebugPauseState::terminate;
    }

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, std::string("Debug break: Press enter to start workload")));
    EXPECT_FALSE(hasSubstr(output, std::string("Debug break: Workload ended, press enter to continue")));
    EXPECT_EQ(1u, confirmationCounter);
    mockCSR->userPauseConfirmation->join();
    mockCSR->userPauseConfirmation.reset();
}
