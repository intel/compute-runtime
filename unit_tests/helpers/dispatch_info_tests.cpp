/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/dispatch_info.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"

#include <type_traits>

using namespace NEO;

class DispatchInfoFixture : public ContextFixture, public DeviceFixture {
    using ContextFixture::SetUp;

  public:
    DispatchInfoFixture() {}

  protected:
    void SetUp() {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        pKernelInfo = std::make_unique<KernelInfo>();

        pMediaVFEstate = new SPatchMediaVFEState();
        pMediaVFEstate->PerThreadScratchSpace = 1024;
        pMediaVFEstate->ScratchSpaceOffset = 0;
        pKernelInfo->patchInfo.mediavfestate = pMediaVFEstate;
        pPrintfSurface = new SPatchAllocateStatelessPrintfSurface();
        pKernelInfo->patchInfo.pAllocateStatelessPrintfSurface = pPrintfSurface;
        pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pDevice);
        pKernel->slmTotalSize = 128;
    }
    void TearDown() override {
        delete pKernel;
        delete pPrintfSurface;
        delete pMediaVFEstate;
        delete pProgram;

        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    std::unique_ptr<KernelInfo> pKernelInfo;
    SPatchMediaVFEState *pMediaVFEstate = nullptr;
    SPatchAllocateStatelessPrintfSurface *pPrintfSurface = nullptr;
    MockProgram *pProgram = nullptr;
    Kernel *pKernel = nullptr;
};

typedef Test<DispatchInfoFixture> DispatchInfoTest;

TEST_F(DispatchInfoTest, DispatchInfoWithNoGeometry) {
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo);

    EXPECT_EQ(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(0u, dispatchInfo->getRequiredScratchSize());
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

TEST_F(DispatchInfoTest, DispatchInfoWithUserGeometry) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({16, 16, 16});
    Vec3<size_t> offset({1, 2, 3});
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo(pKernel, 3, gws, elws, offset));

    EXPECT_NE(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(1024u, dispatchInfo->getRequiredScratchSize());
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

TEST_F(DispatchInfoTest, DispatchInfoWithFullGeometry) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({32, 32, 32});
    Vec3<size_t> offset({1, 2, 3});
    Vec3<size_t> agws({256, 256, 256});
    Vec3<size_t> lws({32, 32, 32});
    Vec3<size_t> twgs({8, 8, 8});
    Vec3<size_t> nwgs({8, 8, 8});
    Vec3<size_t> swgs({0, 0, 0});
    std::unique_ptr<DispatchInfo> dispatchInfo(new DispatchInfo(pKernel, 3, gws, elws, offset, agws, lws, twgs, nwgs, swgs));

    EXPECT_NE(nullptr, dispatchInfo->getKernel());
    EXPECT_EQ(1024u, dispatchInfo->getRequiredScratchSize());
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

TEST_F(DispatchInfoTest, MultiDispatchInfoNonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<MultiDispatchInfo>::value);
    EXPECT_FALSE(std::is_copy_constructible<MultiDispatchInfo>::value);
}

TEST_F(DispatchInfoTest, MultiDispatchInfoNonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<MultiDispatchInfo>::value);
    EXPECT_FALSE(std::is_copy_assignable<MultiDispatchInfo>::value);
}

TEST_F(DispatchInfoTest, MultiDispatchInfoEmpty) {
    MultiDispatchInfo multiDispatchInfo;
    EXPECT_TRUE(multiDispatchInfo.empty());
    EXPECT_EQ(0u, multiDispatchInfo.getRequiredScratchSize());
    EXPECT_FALSE(multiDispatchInfo.usesSlm());
    EXPECT_FALSE(multiDispatchInfo.usesStatelessPrintfSurface());
    EXPECT_EQ(0u, multiDispatchInfo.getRedescribedSurfaces().size());
}

TEST_F(DispatchInfoTest, MultiDispatchInfoWithRedescribedSurfaces) {
    MultiDispatchInfo multiDispatchInfo;

    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, image);

    auto imageRedescribed = image->redescribe();
    multiDispatchInfo.pushRedescribedMemObj(std::unique_ptr<MemObj>(imageRedescribed));

    EXPECT_EQ(1u, multiDispatchInfo.getRedescribedSurfaces().size());
}

TEST_F(DispatchInfoTest, MultiDispatchInfoWithNoGeometry) {
    DispatchInfo dispatchInfo;

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(0u, multiDispatchInfo.getRequiredScratchSize());
    EXPECT_FALSE(multiDispatchInfo.usesSlm());
    EXPECT_FALSE(multiDispatchInfo.usesStatelessPrintfSurface());
}

TEST_F(DispatchInfoTest, MultiDispatchInfoWithUserGeometry) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({16, 16, 16});
    Vec3<size_t> offset({1, 2, 3});

    DispatchInfo dispatchInfo(pKernel, 3, gws, elws, offset);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(1024u, multiDispatchInfo.getRequiredScratchSize());
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

TEST_F(DispatchInfoTest, MultiDispatchInfoWithFullGeometry) {
    Vec3<size_t> gws({256, 256, 256});
    Vec3<size_t> elws({32, 32, 32});
    Vec3<size_t> offset({1, 2, 3});
    Vec3<size_t> agws({256, 256, 256});
    Vec3<size_t> lws({32, 32, 32});
    Vec3<size_t> twgs({8, 8, 8});
    Vec3<size_t> nwgs({8, 8, 8});
    Vec3<size_t> swgs({0, 0, 0});

    DispatchInfo dispatchInfo(pKernel, 3, gws, elws, offset, agws, lws, twgs, nwgs, swgs);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    EXPECT_FALSE(multiDispatchInfo.empty());
    EXPECT_EQ(1024u, multiDispatchInfo.getRequiredScratchSize());
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

TEST_F(DispatchInfoTest, WorkGroupSetGet) {
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

TEST_F(DispatchInfoTest, givenKernelWhenMultiDispatchInfoIsCreatedThenQueryParentAndMainKernel) {
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(*pContext));
    std::unique_ptr<MockKernel> baseKernel(MockKernel::create(*pDevice, pProgram));
    std::unique_ptr<MockKernel> builtInKernel(MockKernel::create(*pDevice, pProgram));
    builtInKernel->isBuiltIn = true;
    DispatchInfo parentKernelDispatchInfo(parentKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo baseDispatchInfo(baseKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});
    DispatchInfo builtInDispatchInfo(builtInKernel.get(), 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1});

    {
        MultiDispatchInfo multiDispatchInfo(parentKernel.get());
        multiDispatchInfo.push(parentKernelDispatchInfo);
        EXPECT_EQ(parentKernel.get(), multiDispatchInfo.peekParentKernel());
        EXPECT_EQ(parentKernel.get(), multiDispatchInfo.peekMainKernel());
    }

    {
        MultiDispatchInfo multiDispatchInfo(baseKernel.get());
        multiDispatchInfo.push(builtInDispatchInfo);
        EXPECT_EQ(nullptr, multiDispatchInfo.peekParentKernel());
        EXPECT_EQ(baseKernel.get(), multiDispatchInfo.peekMainKernel()); // dont pick bultin kernel

        multiDispatchInfo.push(baseDispatchInfo);
        EXPECT_EQ(nullptr, multiDispatchInfo.peekParentKernel());
        EXPECT_EQ(baseKernel.get(), multiDispatchInfo.peekMainKernel());
    }

    {
        MultiDispatchInfo multiDispatchInfo;
        EXPECT_EQ(nullptr, multiDispatchInfo.peekParentKernel());
        EXPECT_EQ(nullptr, multiDispatchInfo.peekMainKernel());

        multiDispatchInfo.push(builtInDispatchInfo);
        EXPECT_EQ(nullptr, multiDispatchInfo.peekParentKernel());
        EXPECT_EQ(builtInKernel.get(), multiDispatchInfo.peekMainKernel());
    }

    {
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(parentKernelDispatchInfo);
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

TEST(DispatchInfoBasicTests, givenDispatchInfoWithoutKernelWhenGettingSizeForPrivateScratchThenZeroIsReturned) {
    DispatchInfo dispatchInfo;
    EXPECT_EQ(nullptr, dispatchInfo.getKernel());
    EXPECT_EQ(0u, dispatchInfo.getRequiredPrivateScratchSize());
}
