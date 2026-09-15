/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/d3d/leo_d3d_sharing.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

template <typename D3D>
class MockD3DSharingFunctions : public D3DSharingFunctions<D3D> {
    typedef typename D3D::D3DDevice D3DDevice;
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DFence D3DFence;
    typedef typename D3D::D3DResource D3DResource;

  public:
    MockD3DSharingFunctions() : D3DSharingFunctions<D3D>(reinterpret_cast<D3DDevice *>(0x1)) {}

    void createQuery(D3DQuery **query) override {
        createQueryCalled++;
        *query = queryToReturn;
    }

    void createFence(D3DFence **fence) override {
        createFenceCalled++;
        *fence = fenceToReturn;
    }

    void addRef(D3DResource *resource) override {
        addRefCalled++;
    }

    void release(IUnknown *resource) override {
        releaseCalled++;
        releaseParamsPassed.push_back(resource);
    }

    void copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource, D3DResource *src, cl_uint srcSubresource) override {
        copySubresourceRegionCalled++;
    }

    ADDMETHOD_NOBASE_VOIDRETURN(flushAndWait, (D3DQuery * query));
    ADDMETHOD_NOBASE_VOIDRETURN(signalAndWait, (D3DFence * fence));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceContext, (D3DQuery * query));
    ADDMETHOD_NOBASE_VOIDRETURN(releaseDeviceContext, (D3DQuery * query));

    D3DQuery *queryToReturn = nullptr;
    D3DFence *fenceToReturn = nullptr;

    uint32_t createQueryCalled = 0u;
    uint32_t createFenceCalled = 0u;
    uint32_t addRefCalled = 0u;
    uint32_t releaseCalled = 0u;
    uint32_t copySubresourceRegionCalled = 0u;
    StackVec<IUnknown *, 4> releaseParamsPassed{};
};

struct MockD3DContext : public Context {
    MockD3DContext() : Context(nullptr, nullptr, 0, nullptr, false) {
        this->sharingFunctions.resize(SharingType::MAX_SHARING_VALUE);
    }

    using Context::setInteropUserSyncEnabled;

    template <typename Sharing>
    void setSharingFunctions(Sharing *sharing) {
        this->sharingFunctions[Sharing::sharingId].reset(sharing);
    }
};

template <typename D3D>
struct D3DSharingTest : public ::testing::Test {
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DFence D3DFence;
    typedef typename D3D::D3DResource D3DResource;

    void SetUp() override {
        mockSharingFcns = new MockD3DSharingFunctions<D3D>();
        mockSharingFcns->queryToReturn = reinterpret_cast<D3DQuery *>(0x10);
        mockSharingFcns->fenceToReturn = reinterpret_cast<D3DFence *>(0x20);
        context.setSharingFunctions(mockSharingFcns);
    }

    std::unique_ptr<D3DSharing<D3D>> createSharing(bool sharedResource) {
        return std::make_unique<D3DSharing<D3D>>(&context, reinterpret_cast<D3DResource *>(&dummyResource),
                                                 reinterpret_cast<D3DResource *>(&dummyResourceStaging), 0u, sharedResource);
    }

    MockD3DContext context{};
    MockD3DSharingFunctions<D3D> *mockSharingFcns = nullptr;
    uint64_t dummyResource{};
    uint64_t dummyResourceStaging{};
};

using D3D9SharingTest = D3DSharingTest<D3DTypesHelper::D3D9>;
using D3D10SharingTest = D3DSharingTest<D3DTypesHelper::D3D10>;
using D3D11SharingTest = D3DSharingTest<D3DTypesHelper::D3D11>;

TEST_F(D3D11SharingTest, givenD3D11SharingWhenCreatedThenFenceIsCreatedAlongsideQuery) {
    auto sharing = createSharing(false);

    EXPECT_EQ(1u, mockSharingFcns->createQueryCalled);
    EXPECT_EQ(1u, mockSharingFcns->createFenceCalled);
    EXPECT_EQ(mockSharingFcns->queryToReturn, sharing->getQuery());
    EXPECT_EQ(mockSharingFcns->fenceToReturn, sharing->getFence());
}

TEST_F(D3D11SharingTest, givenD3DFenceAndInteropUserSyncNotSetAndNonSharedResourceWhenSynchronizingObjectThenCopySubregionAndSignalAndWait) {
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(false);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(1u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(1u, mockSharingFcns->getDeviceContextCalled);
    EXPECT_EQ(1u, mockSharingFcns->releaseDeviceContextCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D11SharingTest, givenD3DFenceAndInteropUserSyncSetAndNonSharedResourceWhenSynchronizingObjectThenCopySubregionAndSignalAndWait) {
    context.setInteropUserSyncEnabled(true);
    auto sharing = createSharing(false);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(1u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D11SharingTest, givenD3DFenceAndInteropUserSyncNotSetAndSharedResourceWhenSynchronizingObjectThenDoNotCopySubregionAndSignalAndWait) {
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(true);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(0u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D11SharingTest, givenD3DFenceAndInteropUserSyncSetAndSharedResourceWhenSynchronizingObjectThenDoNotCopySubregionAndDoNotSignalAndWait) {
    context.setInteropUserSyncEnabled(true);
    auto sharing = createSharing(true);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(0u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D11SharingTest, givenNoD3DFenceAndInteropUserSyncNotSetAndNonSharedResourceWhenSynchronizingObjectThenCopySubregionAndFlushAndWait) {
    mockSharingFcns->fenceToReturn = nullptr;
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(false);
    ASSERT_EQ(nullptr, sharing->getFence());

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(1u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D11SharingTest, givenNoD3DFenceAndInteropUserSyncNotSetAndSharedResourceWhenSynchronizingObjectThenDoNotCopySubregionAndFlushAndWait) {
    mockSharingFcns->fenceToReturn = nullptr;
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(true);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(0u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
}

TEST_F(D3D11SharingTest, givenNoD3DFenceAndInteropUserSyncSetAndSharedResourceWhenSynchronizingObjectThenDoNotCopySubregionAndDoNotFlushAndWait) {
    mockSharingFcns->fenceToReturn = nullptr;
    context.setInteropUserSyncEnabled(true);
    auto sharing = createSharing(true);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(0u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
}

TEST_F(D3D11SharingTest, givenNonSharedResourceWhenSharingIsDestroyedThenStagingResourceQueryAndFenceAreReleased) {
    {
        auto sharing = createSharing(false);
    }

    ASSERT_EQ(4u, mockSharingFcns->releaseCalled);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(&dummyResourceStaging), mockSharingFcns->releaseParamsPassed[0]);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(&dummyResource), mockSharingFcns->releaseParamsPassed[1]);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(mockSharingFcns->queryToReturn), mockSharingFcns->releaseParamsPassed[2]);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(mockSharingFcns->fenceToReturn), mockSharingFcns->releaseParamsPassed[3]);
}

TEST_F(D3D11SharingTest, givenSharedResourceWhenSharingIsDestroyedThenResourceQueryAndFenceAreReleased) {
    {
        auto sharing = createSharing(true);
    }

    ASSERT_EQ(3u, mockSharingFcns->releaseCalled);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(&dummyResource), mockSharingFcns->releaseParamsPassed[0]);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(mockSharingFcns->queryToReturn), mockSharingFcns->releaseParamsPassed[1]);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(mockSharingFcns->fenceToReturn), mockSharingFcns->releaseParamsPassed[2]);
}

TEST_F(D3D11SharingTest, givenNoD3DFenceWhenSharingIsDestroyedThenNullFenceIsPassedToRelease) {
    mockSharingFcns->fenceToReturn = nullptr;

    {
        auto sharing = createSharing(true);
    }

    ASSERT_EQ(3u, mockSharingFcns->releaseCalled);
    EXPECT_EQ(nullptr, mockSharingFcns->releaseParamsPassed[2]);
}

TEST_F(D3D10SharingTest, givenD3D10SharingWhenSynchronizingObjectThenFlushAndWaitIsUsedInsteadOfSignalAndWait) {
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(false);
    ASSERT_NE(nullptr, sharing->getFence());

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(1u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST_F(D3D10SharingTest, givenD3D10SharingWhenDestroyedThenFenceIsReleased) {
    {
        auto sharing = createSharing(true);
    }

    EXPECT_EQ(1u, mockSharingFcns->createFenceCalled);
    ASSERT_EQ(3u, mockSharingFcns->releaseCalled);
    EXPECT_EQ(reinterpret_cast<IUnknown *>(mockSharingFcns->fenceToReturn), mockSharingFcns->releaseParamsPassed[2]);
}

TEST_F(D3D9SharingTest, givenD3D9SharingWhenSynchronizingObjectThenFlushAndWaitIsUsedInsteadOfSignalAndWait) {
    context.setInteropUserSyncEnabled(false);
    auto sharing = createSharing(false);

    UpdateData updateData{0u};
    sharing->synchronizeObject(updateData);

    EXPECT_EQ(1u, mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->signalAndWaitCalled);
    EXPECT_EQ(SynchronizeStatus::ACQUIRE_SUCCESFUL, updateData.synchronizationStatus);
}

TEST(D3DSharingFunctionsTest, givenD3D9SharingFunctionsWhenCreateFenceIsCalledThenNoFenceIsCreatedAndSignalAndWaitIsNoOp) {
    D3DSharingFunctions<D3DTypesHelper::D3D9> sharingFunctions{nullptr};

    D3DTypesHelper::D3D9::D3DFence *fence = nullptr;
    sharingFunctions.createFence(&fence);
    EXPECT_EQ(nullptr, fence);

    sharingFunctions.signalAndWait(fence);
    sharingFunctions.release(nullptr);
}

TEST(D3DSharingFunctionsTest, givenD3D10SharingFunctionsWhenCreateFenceIsCalledThenNoFenceIsCreatedAndSignalAndWaitIsNoOp) {
    D3DSharingFunctions<D3DTypesHelper::D3D10> sharingFunctions{nullptr};

    D3DTypesHelper::D3D10::D3DFence *fence = nullptr;
    sharingFunctions.createFence(&fence);
    EXPECT_EQ(nullptr, fence);

    sharingFunctions.signalAndWait(fence);
    sharingFunctions.release(nullptr);
}

TEST(D3DSharingFunctionsTest, givenNullResourceWhenReleaseIsCalledThenResourceIsNotDereferenced) {
    D3DSharingFunctions<D3DTypesHelper::D3D11> sharingFunctions{nullptr};

    sharingFunctions.release(nullptr);
    EXPECT_EQ(nullptr, sharingFunctions.getDevice());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
