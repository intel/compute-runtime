/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/os_interface/windows/d3d_sharing_functions.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

template <typename D3D>
class MockD3DSharingFunctions : public D3DSharingFunctions<D3D> {
  public:
    typedef typename D3D::D3DDevice D3DDevice;
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DResource D3DResource;
    MockD3DSharingFunctions() : D3DSharingFunctions<D3D>((D3DDevice *)1) {
    }
    void getDeviceContext(D3DQuery *query) override {
        signalDeviceContextCalled = true;
        while (!signalLockChecked)
            ;
    }
    void copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource,
                               D3DResource *src, cl_uint srcSubresource) override {
    }
    void flushAndWait(D3DQuery *query) override {
    }
    void releaseDeviceContext(D3DQuery *query) override {
    }
    void addRef(D3DResource *resource) override {
    }
    void createQuery(D3DQuery **query) override {
    }
    void release(IUnknown *resource) override {
    }

    std::atomic_bool signalDeviceContextCalled = false;
    std::atomic_bool signalLockChecked = false;
};

template <typename D3D>
class MockD3DSharingBase : public D3DSharing<D3D> {
  public:
    using D3DSharing<D3D>::sharingFunctions;
    MockD3DSharingBase(Context *ctx) : D3DSharing<D3D>(ctx, nullptr, nullptr, 0, false) {
    }
    void checkIfMutexWasLocked() {
        isLocked = !this->mtx.try_lock();
        reinterpret_cast<MockD3DSharingFunctions<D3D> *>(this->sharingFunctions)->signalLockChecked = true;
    }
    bool isLocked = false;
};

TEST(SharingD3DMT, givenD3DSharingWhenSynchroniceObjectIsCalledThenMtxIsLockedBeforeAccessingDevice) {
    auto mockCtx = std::make_unique<MockContext>();
    mockCtx->sharingFunctions[MockD3DSharingFunctions<D3DTypesHelper::D3D11>::sharingId] = std::make_unique<MockD3DSharingFunctions<D3DTypesHelper::D3D11>>();
    auto mockD3DSharing = std::make_unique<MockD3DSharingBase<D3DTypesHelper::D3D11>>(mockCtx.get());
    UpdateData updateData(0);
    std::thread t1(&MockD3DSharingBase<D3DTypesHelper::D3D11>::synchronizeObject, mockD3DSharing.get(), updateData);
    while (!reinterpret_cast<MockD3DSharingFunctions<D3DTypesHelper::D3D11> *>(mockD3DSharing->sharingFunctions)->signalDeviceContextCalled)
        ;
    std::thread t2(&MockD3DSharingBase<D3DTypesHelper::D3D11>::checkIfMutexWasLocked, mockD3DSharing.get());
    t1.join();
    t2.join();
    EXPECT_TRUE(mockD3DSharing->isLocked);
}

TEST(SharingD3DMT, givenD3DSharingWhenReleaseResourceIsCalledThenMtxIsLockedBeforeAccessingDevice) {
    auto mockCtx = std::make_unique<MockContext>();
    mockCtx->sharingFunctions[MockD3DSharingFunctions<D3DTypesHelper::D3D11>::sharingId] = std::make_unique<MockD3DSharingFunctions<D3DTypesHelper::D3D11>>();
    auto mockD3DSharing = std::make_unique<MockD3DSharingBase<D3DTypesHelper::D3D11>>(mockCtx.get());
    UpdateData updateData(0);
    std::thread t1(&MockD3DSharingBase<D3DTypesHelper::D3D11>::releaseResource, mockD3DSharing.get(), nullptr, 0);
    while (!reinterpret_cast<MockD3DSharingFunctions<D3DTypesHelper::D3D11> *>(mockD3DSharing->sharingFunctions)->signalDeviceContextCalled)
        ;
    std::thread t2(&MockD3DSharingBase<D3DTypesHelper::D3D11>::checkIfMutexWasLocked, mockD3DSharing.get());
    t1.join();
    t2.join();
    EXPECT_TRUE(mockD3DSharing->isLocked);
}
