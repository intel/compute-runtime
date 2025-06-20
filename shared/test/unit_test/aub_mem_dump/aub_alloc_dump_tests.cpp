/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using AubAllocDumpTests = Test<DeviceFixture>;

HWTEST_F(AubAllocDumpTests, givenBufferOrImageWhenGraphicsAllocationIsKnownThenItsTypeCanBeCheckedIfItIsWritable) {
    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    gfxAllocation->setAllocationType(AllocationType::buffer);
    EXPECT_FALSE(gfxAllocation->isMemObjectsAllocationWithWritableFlags());
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::buffer);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::bufferHostMemory);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::bufferHostMemory);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::externalHostPtr);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::externalHostPtr);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::mapAllocation);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::mapAllocation);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::image);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableImage(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::image);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableImage(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenImageResourceWhenGmmResourceInfoIsAvailableThenImageSurfaceTypeCanBeDeducedFromGmmResourceType) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_1D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_2D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_3D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_INVALID));
}
