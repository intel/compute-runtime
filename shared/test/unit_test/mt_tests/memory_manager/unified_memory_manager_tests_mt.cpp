/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

#include <memory>
#include <thread>

using namespace NEO;

TEST(SvmDeviceAllocationTest, givenGivenSvmAllocsManagerWhenObtainOwnershipCalledThenLockedUniqueLockReturned) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    auto lock = svmManager->obtainOwnership();
    std::thread th1([&] {
        EXPECT_FALSE(svmManager->mtxForIndirectAccess.try_lock());
    });
    th1.join();
    lock.unlock();
    std::thread th2([&] {
        EXPECT_TRUE(svmManager->mtxForIndirectAccess.try_lock());
        svmManager->mtxForIndirectAccess.unlock();
    });
    th2.join();
}
