/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

using namespace NEO;

struct RawBinarySipFixture : public DeviceFixture {
    void SetUp() {
        DebugManager.flags.LoadBinarySipFromFile.set("dummy_file.bin");

        backupSipInitType = std::make_unique<VariableBackup<bool>>(&MockSipData::useMockSip, false);
        backupSipClassType = std::make_unique<VariableBackup<SipClassType>>(&SipKernel::classType);

        backupFopenReturned = std::make_unique<VariableBackup<FILE *>>(&IoFunctions::mockFopenReturned);
        backupFtellReturned = std::make_unique<VariableBackup<long int>>(&IoFunctions::mockFtellReturn, 128);
        backupFreadReturned = std::make_unique<VariableBackup<size_t>>(&IoFunctions::mockFreadReturn, 128u);

        backupFopenCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFopenCalled, 0u);
        backupFseekCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFseekCalled, 0u);
        backupFtellCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFtellCalled, 0u);
        backupRewindCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockRewindCalled, 0u);
        backupFreadCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFreadCalled, 0u);
        backupFcloseCalled = std::make_unique<VariableBackup<uint32_t>>(&IoFunctions::mockFcloseCalled, 0u);

        DeviceFixture::SetUp();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    DebugManagerStateRestore dbgRestorer;

    std::unique_ptr<VariableBackup<bool>> backupSipInitType;
    std::unique_ptr<VariableBackup<SipClassType>> backupSipClassType;

    std::unique_ptr<VariableBackup<FILE *>> backupFopenReturned;
    std::unique_ptr<VariableBackup<long int>> backupFtellReturned;
    std::unique_ptr<VariableBackup<size_t>> backupFreadReturned;

    std::unique_ptr<VariableBackup<uint32_t>> backupFopenCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFseekCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFtellCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupRewindCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFreadCalled;
    std::unique_ptr<VariableBackup<uint32_t>> backupFcloseCalled;
};

using RawBinarySipTest = Test<RawBinarySipFixture>;

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenInitSipKernelThenSipIsLoadedFromFile) {
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto sipAllocation = SipKernel::getSipKernel(*pDevice).getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenInitSipKernelAndDebuggerActiveThenDbgSipIsLoadedFromFile) {
    pDevice->setDebuggerActive(true);
    auto currentSipKernelType = SipKernel::getSipKernelType(*pDevice);
    bool ret = SipKernel::initSipKernel(currentSipKernelType, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_LE(SipKernelType::DbgCsr, currentSipKernelType);

    uint32_t sipIndex = static_cast<uint32_t>(currentSipKernelType);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto sipAllocation = SipKernel::getSipKernel(*pDevice).getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenOpenFileFailsThenSipIsNotLoadedFromFile) {
    IoFunctions::mockFopenReturned = nullptr;
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(0u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(0u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(0u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(0u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(0u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenTellSizeDiffrentThanBytesReadThenSipIsNotCreated) {
    IoFunctions::mockFtellReturn = 28;
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenBytesReadIsZeroThenSipIsNotCreated) {
    IoFunctions::mockFreadReturn = 0u;
    IoFunctions::mockFtellReturn = 0;
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenAllocationCreationFailsThenSipIsNotCreated) {
    pDevice->executionEnvironment->memoryManager.reset(new FailMemoryManager(0, *pDevice->executionEnvironment));
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_FALSE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));
    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    EXPECT_EQ(nullptr, sipKernel);
}

TEST_F(RawBinarySipTest, givenRawBinaryFileWhenInitSipKernelTwiceThenSipIsLoadedFromFileAndCreatedOnce) {
    bool ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    EXPECT_EQ(SipKernelType::Csr, SipKernel::getSipKernelType(*pDevice));

    uint32_t sipIndex = static_cast<uint32_t>(SipKernelType::Csr);
    auto sipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, sipKernel);
    auto storedAllocation = sipKernel->getSipAllocation();

    auto &refSipKernel = SipKernel::getSipKernel(*pDevice);
    EXPECT_EQ(sipKernel, &refSipKernel);

    auto sipAllocation = refSipKernel.getSipAllocation();
    EXPECT_NE(nullptr, storedAllocation);
    EXPECT_EQ(storedAllocation, sipAllocation);

    ret = SipKernel::initSipKernel(SipKernelType::Csr, *pDevice);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(1u, IoFunctions::mockFseekCalled);
    EXPECT_EQ(1u, IoFunctions::mockFtellCalled);
    EXPECT_EQ(1u, IoFunctions::mockRewindCalled);
    EXPECT_EQ(1u, IoFunctions::mockFreadCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);

    auto secondSipKernel = pDevice->getRootDeviceEnvironment().sipKernels[sipIndex].get();
    ASSERT_NE(nullptr, secondSipKernel);
    auto secondStoredAllocation = sipKernel->getSipAllocation();
    EXPECT_NE(nullptr, secondStoredAllocation);

    EXPECT_EQ(sipKernel, secondSipKernel);
    EXPECT_EQ(storedAllocation, secondStoredAllocation);
}
