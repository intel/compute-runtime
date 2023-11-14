/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/test/common/fixtures/memory_allocator_multi_device_fixture.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"

using namespace NEO;

void MemoryAllocatorMultiDeviceSystemSpecificFixture::setUp(ExecutionEnvironment &executionEnvironment) {
    static D3DDDI_OPENALLOCATIONINFO allocationInfo;
    auto gdi = new MockGdi();
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;
    auto osEnvironment = new OsEnvironmentWin();
    osEnvironment->gdi.reset(gdi);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    for (const auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
        gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
        auto wddm = static_cast<WddmMock *>(rootDeviceEnvironment->osInterface->getDriverModel()->as<Wddm>());
        wddm->hwDeviceId = std::make_unique<HwDeviceIdWddm>(ADAPTER_HANDLE, LUID{}, 1u, osEnvironment, std::make_unique<UmKmDataTranslator>());
        wddm->callBaseMapGpuVa = false;

        allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
        allocationInfo.hAllocation = ALLOCATION_HANDLE;
        allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);
    }
    executionEnvironment.osEnvironment.reset(osEnvironment);
}

void MemoryAllocatorMultiDeviceSystemSpecificFixture::tearDown(ExecutionEnvironment &executionEnvironment) {}
