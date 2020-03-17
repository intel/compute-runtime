/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"

#include "opencl/test/unit_test/fixtures/memory_allocator_multi_device_fixture.h"
#include "opencl/test/unit_test/mock_gdi/mock_gdi.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"

using namespace NEO;

void MemoryAllocatorMultiDeviceSystemSpecificFixture::SetUp(ExecutionEnvironment &executionEnvironment) {
    static D3DDDI_OPENALLOCATIONINFO allocationInfo;
    auto gdi = new MockGdi();
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;
    auto osEnvironment = new OsEnvironmentWin();
    osEnvironment->gdi.reset(gdi);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[i]->getGmmClientContext(), nullptr, 0, false);
        auto wddm = static_cast<WddmMock *>(executionEnvironment.rootDeviceEnvironments[i]->osInterface->get()->getWddm());
        wddm->hwDeviceId = std::make_unique<HwDeviceId>(ADAPTER_HANDLE, LUID{}, osEnvironment);
        wddm->callBaseMapGpuVa = false;

        allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
        allocationInfo.hAllocation = ALLOCATION_HANDLE;
        allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);
    }
    executionEnvironment.osEnvironment.reset(osEnvironment);
}

void MemoryAllocatorMultiDeviceSystemSpecificFixture::TearDown(ExecutionEnvironment &executionEnvironment) {}
