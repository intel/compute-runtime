/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

#include <type_traits>

using namespace NEO;

class DispatchInfoFixture : public ContextFixture, public ClDeviceFixture {
    using ContextFixture::setUp;

  public:
    DispatchInfoFixture() {}

  protected:
    void setUp() {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        pKernelInfo = std::make_unique<MockKernelInfo>();

        pKernelInfo->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024;
        pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        pKernel->slmTotalSize = 128;
    }
    void tearDown() {
        delete pKernel;
        delete pProgram;

        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockKernelInfo> pKernelInfo;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
};

typedef Test<DispatchInfoFixture> DispatchInfoTest;

TEST_F(DispatchInfoTest, GivenNoGeometryWhenDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo);

    EXPECT_EQ(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(0u, dispatchInfo->getRequiredScratchSize(0u));
    EXPECT_FALSE(dispatchInfo->usesSlm());
    EXPECT_FALSE(dispatchInfo->usesStatelessPrintfSurface());
    EXPECT_EQ(0u, dispatchInfo->getDim());

    Vec3<size_t> vecZero({0, 0, 0});

    EXPECT_EQ(vecZero, dispatchInfo->getGWS());
    EXPECT_EQ(vecZero, dispatchInfo->getEnqueuedWorkgroupSize());
    EXPECT_EQ(vecZero, dispatchInfo->getOffset());
    EXPECT_EQ(vecZero, dispatchInfo->getActualWorkgroupSize());
    EXPECT_EQ(vecZero, dispatchInfo->getLocalWorkgroupSize());
    EXPECT_EQ(vecZero, dispatchInfo->getTotalNumberOfWorkgroups());
    EXPECT_EQ(vecZero, dispatchInfo->getNumberOfWorkgroups());
    EXPECT_EQ(vecZero, dispatchInfo->getStartOfWorkgroups());
}

TEST_F(DispatchInfoTest, GivenUserGeometryWhenDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({16, 16, 16});
    Vec3<size_t> offset({1, 2, 3});
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo(pClDevice, pKernel, 3, gws, elws, offset));

    EXPECT_NE(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(1024u, dispatchInfo->getRequiredScratchSize(0u));
    EXPECT_TRUE(dispatchInfo->usesSlm());
    EXPECT_TRUE(dispatchInfo->usesStatelessPrintfSurface());
    EXPECT_EQ(3u, dispatchInfo->getDim());

    EXPECT_EQ(gws, dispatchInfo->getGWS());
    EXPECT_EQ(elws, dispatchInfo->getEnqueuedWorkgroupSize());
    EXPECT_EQ(offset, dispatchInfo->getOffset());

    Vec3<size_t> vecZero({0, 0, 0});

    EXPECT_EQ(vecZero, dispatchInfo->getActualWorkgroupSize());
    EXPECT_EQ(vecZero, dispatchInfo->getLocalWorkgroupSize());
    EXPECT_EQ(vecZero, dispatchInfo->getTotalNumberOfWorkgroups());
    EXPECT_EQ(vecZero, dispatchInfo->getNumberOfWorkgroups());
    EXPECT_EQ(vecZero, dispatchInfo->getStartOfWorkgroups());

    dispatchInfo->setKernel(nullptr);
    EXPECT_EQ(nullptr, dispatchInfo->getKernel());
}

TEST_F(DispatchInfoTest, GivenFullGeometryWhenDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({32, 32, 32});
    Vec3<size_t> offset({1, 2, 3});
    Vec3<size_t> agws({256, 256, 256});
    Vec3<size_t> lws({32, 32, 32});
    Vec3<size_t> twgs({8, 8, 8});
    Vec3<size_t> nwgs({8, 8, 8});
    Vec3<size_t> swgs({0, 0, 0});
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo(pClDevice, pKernel, 3, gws, elws, offset, agws, lws, twgs, nwgs, swgs));

    EXPECT_NE(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(1024u, dispatchInfo->getRequiredScratchSize(0u));
    EXPECT_TRUE(dispatchInfo->usesSlm());
    EXPECT_TRUE(dispatchInfo->usesStatelessPrintfSurface());
    EXPECT_EQ(3u, dispatchInfo->getDim());

    EXPECT_EQ(gws, dispatchInfo->getGWS());
    EXPECT_EQ(elws, dispatchInfo->getEnqueuedWorkgroupSize());
    EXPECT_EQ(offset, dispatchInfo->getOffset());
    EXPECT_EQ(agws, dispatchInfo->getActualWorkgroupSize());
    EXPECT_EQ(lws, dispatchInfo->getEnqueuedWorkgroupSize());
    EXPECT_EQ(twgs, dispatchInfo->getTotalNumberOfWorkgroups());
    EXPECT_EQ(nwgs, dispatchInfo->getNumberOfWorkgroups());
    EXPECT_EQ(swgs, dispatchInfo->getStartOfWorkgroups());

    dispatchInfo->setKernel(nullptr);
    EXPECT_EQ(nullptr, dispatchInfo->getKernel());
}

TEST_F(DispatchInfoTest, WhenMultiDispatchInfoIsCreatedThenItIsNonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<MultiDispatchInfo>::value);
    EXPECT_FALSE(std::is_copy_constructible<MultiDispatchInfo>::value);
}

TEST_F(DispatchInfoTest, WhenMultiDispatchInfoIsCreatedThenItIsNonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<MultiDispatchInfo>::value);
    EXPECT_FALSE(std::is_copy_assignable<MultiDispatchInfo>::value);
}

TEST_F(DispatchInfoTest, WhenMultiDispatchInfoIsCreatedThenItIsEmpty) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_TRUE(multiDispatchInfo.empty());
    EXPECT_EQ(0u, multiDispatchInfo.getRequiredScratchSize(0u));
    EXPECT_FALSE(multiDispatchInfo.usesSlm());
    EXPECT_FALSE(multiDispatchInfo.usesStatelessPrintfSurface());
    EXPECT_EQ(0u, multiDispatchInfo.getRedescribedSurfaces().size());
}

TEST_F(DispatchInfoTest, GivenRedescribedSurfacesWhenCreatingMultiDispatchInfoThenRedescribedSurfacesSizeisOne) {
    MultiDispatchInfo multiDispatchInfo;

    auto image = std::unique_ptr<Image>(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image);

    auto imageRedescribed = image->redescribe();
    multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(imageRedescribed));

    EXPECT_EQ(1u, multiDispatchInfo.getRedescribedSurfaces().size());
}

TEST_F(DispatchInfoTest, GivenNoGeometryWhenMultiDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    DispatchInfo dispatchInfo;

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(0u, multiDispatchInfo.getRequiredScratchSize(0u));
    EXPECT_FALSE(multiDispatchInfo.usesSlm());
    EXPECT_FALSE(multiDispatchInfo.usesStatelessPrintfSurface());
}

TEST_F(DispatchInfoTest, GivenUserGeometryWhenMultiDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({16, 16, 16});
    Vec3<size_t> offset({1, 2, 3});

    DispatchInfo dispatchInfo(pClDevice, pKernel, 3, gws, elws, offset);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(1024u, multiDispatchInfo.getRequiredScratchSize(0u));
    EXPECT_TRUE(multiDispatchInfo.usesSlm());
    EXPECT_TRUE(multiDispatchInfo.usesStatelessPrintfSurface());

    EXPECT_NE(nullptr, multiDispatchInfo.begin()->getKernel());

    EXPECT_EQ(gws, multiDispatchInfo.begin()->getGWS());
    EXPECT_EQ(elws, multiDispatchInfo.begin()->getEnqueuedWorkgroupSize());
    EXPECT_EQ(offset, multiDispatchInfo.begin()->getOffset());

    Vec3<size_t> vecZero({0, 0, 0});

    EXPECT_EQ(vecZero, multiDispatchInfo.begin()->getLocalWorkgroupSize());
    EXPECT_EQ(vecZero, multiDispatchInfo.begin()->getTotalNumberOfWorkgroups());
    EXPECT_EQ(vecZero, multiDispatchInfo.begin()->getNumberOfWorkgroups());
    EXPECT_EQ(vecZero, multiDispatchInfo.begin()->getStartOfWorkgroups());
}

TEST_F(DispatchInfoTest, GivenFullGeometryWhenMultiDispatchInfoIsCreatedThenValuesAreSetCorrectly) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({32, 32, 32});
    Vec3<size_t> offset({1, 2, 3});
    Vec3<size_t> agws({256, 256, 256});
    Vec3<size_t> lws({32, 32, 32});
    Vec3<size_t> twgs({8, 8, 8});
    Vec3<size_t> nwgs({8, 8, 8});
    Vec3<size_t> swgs({0, 0, 0});

    DispatchInfo dispatchInfo(pClDevice, pKernel, 3, gws, elws, offset, agws, lws, twgs, nwgs, swgs);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(1024u, multiDispatchInfo.getRequiredScratchSize(0u));
    EXPECT_TRUE(multiDispatchInfo.usesSlm());
    EXPECT_TRUE(multiDispatchInfo.usesStatelessPrintfSurface());

    EXPECT_NE(nullptr, multiDispatchInfo.begin()->getKernel());

    EXPECT_EQ(gws, multiDispatchInfo.begin()->getGWS());
    EXPECT_EQ(elws, multiDispatchInfo.begin()->getEnqueuedWorkgroupSize());
    EXPECT_EQ(offset, multiDispatchInfo.begin()->getOffset());
    EXPECT_EQ(agws, multiDispatchInfo.begin()->getActualWorkgroupSize());
    EXPECT_EQ(lws, multiDispatchInfo.begin()->getLocalWorkgroupSize());
    EXPECT_EQ(twgs, multiDispatchInfo.begin()->getTotalNumberOfWorkgroups());
    EXPECT_EQ(nwgs, multiDispatchInfo.begin()->getNumberOfWorkgroups());
    EXPECT_EQ(swgs, multiDispatchInfo.begin()->getStartOfWorkgroups());
}

TEST_F(DispatchInfoTest, WhenSettingValuesInDispatchInfoThenThoseValuesAreSet) {
    DispatchInfo dispatchInfo;

    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({16, 16, 16});
    Vec3<size_t> offset({1, 2, 3});
    Vec3<size_t> agws({256, 256, 256});
    Vec3<size_t> lws({4, 4, 4});
    Vec3<size_t> twgs({64, 64, 64});
    Vec3<size_t> nwgs({64, 64, 64});
    Vec3<size_t> swgs({8, 8, 8});

    dispatchInfo.setGWS(gws);
    dispatchInfo.setEnqueuedWorkgroupSize(elws);
    dispatchInfo.setOffsets(offset);
    dispatchInfo.setActualGlobalWorkgroupSize(agws);
    dispatchInfo.setLWS(lws);
    dispatchInfo.setTotalNumberOfWorkgroups(twgs);
    dispatchInfo.setNumberOfWorkgroups(nwgs);
    dispatchInfo.setStartOfWorkgroups(swgs);

    EXPECT_EQ(gws, dispatchInfo.getGWS());
    EXPECT_EQ(elws, dispatchInfo.getEnqueuedWorkgroupSize());
    EXPECT_EQ(offset, dispatchInfo.getOffset());
    EXPECT_EQ(agws, dispatchInfo.getActualWorkgroupSize());
    EXPECT_EQ(lws, dispatchInfo.getLocalWorkgroupSize());
    EXPECT_EQ(twgs, dispatchInfo.getTotalNumberOfWorkgroups());
    EXPECT_EQ(nwgs, dispatchInfo.getNumberOfWorkgroups());
    EXPECT_EQ(swgs, dispatchInfo.getStartOfWorkgroups());
}

TEST_F(DispatchInfoTest, givenKernelWhenMultiDispatchInfoIsCreatedThenQueryMainKernel) {
    std::unique_ptr<MockKernel> baseKernel(MockKernel::create(*pDevice, pProgram));
    std::unique_ptr<MockKernel> builtInKernel(MockKernel::create(*pDevice, pProgram));
    builtInKernel->isBuiltIn = true;
    DispatchInfo baseDispatchInfo(pClDevice, baseKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo builtInDispatchInfo(pClDevice, builtInKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});

    {
        MultiDispatchInfo multiDispatchInfo(baseKernel.get());
        multiDispatchInfo.push(builtInDispatchInfo);
        EXPECT_EQ(baseKernel.get(), multiDispatchInfo.peekMainKernel()); // dont pick builtin kernel

        multiDispatchInfo.push(baseDispatchInfo);
        EXPECT_EQ(baseKernel.get(), multiDispatchInfo.peekMainKernel());
    }

    {
        MultiDispatchInfo multiDispatchInfo;
        EXPECT_EQ(nullptr, multiDispatchInfo.peekMainKernel());

        multiDispatchInfo.push(builtInDispatchInfo);
        EXPECT_EQ(builtInKernel.get(), multiDispatchInfo.peekMainKernel());
    }

    {
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(baseDispatchInfo);
        multiDispatchInfo.push(builtInDispatchInfo);

        std::reverse_iterator<DispatchInfo *> rend = multiDispatchInfo.rend();
        std::reverse_iterator<const DispatchInfo *> crend = multiDispatchInfo.crend();
        std::reverse_iterator<DispatchInfo *> rbegin = multiDispatchInfo.rbegin();
        std::reverse_iterator<const DispatchInfo *> crbegin = multiDispatchInfo.crbegin();

        EXPECT_EQ(rbegin.base(), multiDispatchInfo.end());
        EXPECT_EQ(crbegin.base(), multiDispatchInfo.end());
        EXPECT_EQ(rend.base(), multiDispatchInfo.begin());
        EXPECT_EQ(crend.base(), multiDispatchInfo.begin());
    }
}

TEST(DispatchInfoBasicTests, givenDispatchInfoWhenCreatedThenDefaultValueOfPartitionIsFalse) {
    DispatchInfo dispatchInfo;
    EXPECT_FALSE(dispatchInfo.peekCanBePartitioned());
}

TEST(DispatchInfoBasicTests, givenDispatchInfoWhenSetCanBePartitionIsCalledThenStateIsChangedAccordingly) {
    DispatchInfo dispatchInfo;
    dispatchInfo.setCanBePartitioned(true);
    EXPECT_TRUE(dispatchInfo.peekCanBePartitioned());
}
