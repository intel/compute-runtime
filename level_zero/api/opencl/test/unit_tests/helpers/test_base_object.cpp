/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_error_mappers.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct MockPlatform : public Platform {
    using Platform::magic;

    MockPlatform(ze_driver_handle_t driverHandle) : Platform(driverHandle) {}
};

using BaseObjectTests = Test<OclFixture>;

TEST_F(BaseObjectTests, givenValidPlatformWhenCastToObjectThenReturnsCorrectPointer) {
    auto castResult = castToObject<Platform>(static_cast<cl_platform_id>(platform));
    EXPECT_EQ(platform, castResult);
}

TEST_F(BaseObjectTests, givenNullPlatformWhenCastToObjectThenReturnsNull) {
    cl_platform_id nullHandle = nullptr;
    auto castResult = castToObject<Platform>(nullHandle);
    EXPECT_EQ(nullptr, castResult);
}

TEST_F(BaseObjectTests, givenValidPlatformWhenGetMagicThenReturnsObjectMagic) {
    EXPECT_EQ(Platform::objectMagic & Platform::maskMagic, platform->getMagic() & Platform::maskMagic);
}

TEST_F(BaseObjectTests, givenValidPlatformWhenRetainThenRefCountIncreases) {
    auto refCountBefore = platform->getReference();
    platform->retain();
    EXPECT_EQ(refCountBefore + 1, platform->getReference());
    platform->release();
}

TEST_F(BaseObjectTests, givenValidPlatformInitiallyThenRefCountIsOne) {
    EXPECT_EQ(1, platform->getReference());
}

TEST_F(BaseObjectTests, givenConstHandleWhenCastToObjectThenReturnsCorrectPointer) {
    const _cl_platform_id *constHandle = static_cast<cl_platform_id>(platform);
    EXPECT_EQ(platform, castToObject<Platform>(constHandle));
}

TEST_F(BaseObjectTests, givenNullConstHandleWhenCastToObjectThenReturnsNull) {
    const _cl_platform_id *constHandle = nullptr;
    EXPECT_EQ(nullptr, castToObject<Platform>(constHandle));
}

TEST_F(BaseObjectTests, givenCorruptedMagicWhenCastToObjectThenReturnsNull) {
    auto mockPlatform = std::make_unique<MockPlatform>(driverHandle->toHandle());
    cl_platform_id handle = mockPlatform.get();
    ASSERT_EQ(mockPlatform.get(), castToObject<Platform>(handle));

    mockPlatform->magic = static_cast<cl_long>(Platform::objectMagic + 1);
    EXPECT_EQ(nullptr, castToObject<Platform>(handle));

    mockPlatform->magic = static_cast<cl_long>(Platform::objectMagic);
    EXPECT_EQ(mockPlatform.get(), castToObject<Platform>(handle));
}

TEST_F(BaseObjectTests, givenDeadMagicWhenCastToObjectThenReturnsNull) {
    auto mockPlatform = std::make_unique<MockPlatform>(driverHandle->toHandle());
    cl_platform_id handle = mockPlatform.get();

    mockPlatform->magic = static_cast<cl_long>(Platform::deadMagic);
    EXPECT_EQ(nullptr, castToObject<Platform>(handle));

    mockPlatform->magic = static_cast<cl_long>(Platform::objectMagic);
    EXPECT_EQ(mockPlatform.get(), castToObject<Platform>(handle));
}

TEST_F(BaseObjectTests, givenZeroedMagicWhenCastToObjectThenReturnsNull) {
    auto mockPlatform = std::make_unique<MockPlatform>(driverHandle->toHandle());
    cl_platform_id handle = mockPlatform.get();

    mockPlatform->magic = 0;
    EXPECT_EQ(nullptr, castToObject<Platform>(handle));

    mockPlatform->magic = static_cast<cl_long>(Platform::objectMagic);
    EXPECT_EQ(mockPlatform.get(), castToObject<Platform>(handle));
}

TEST_F(BaseObjectTests, givenBaseObjectConstantsThenDeadMagicIsTheFullMaskAndDiffersFromObjectMagic) {
    EXPECT_EQ(static_cast<cl_ulong>(Platform::maskMagic), static_cast<cl_ulong>(Platform::deadMagic));
    EXPECT_NE(static_cast<cl_ulong>(Platform::objectMagic), static_cast<cl_ulong>(Platform::deadMagic));
    EXPECT_NE(static_cast<cl_ulong>(Context::objectMagic), static_cast<cl_ulong>(Platform::objectMagic));
}

TEST_F(BaseObjectTests, givenRetainWhenQueryingCountsThenApiAndInternalCountsBothGrow) {
    const auto apiBefore = platform->getRefApiCount();
    const auto internalBefore = platform->getRefInternalCount();

    platform->retain();

    EXPECT_EQ(apiBefore + 1, platform->getRefApiCount());
    EXPECT_EQ(internalBefore + 1, platform->getRefInternalCount());

    platform->release();
}

TEST_F(BaseObjectTests, givenIncRefInternalWhenQueryingCountsThenApiCountIsUnchanged) {
    const auto apiBefore = platform->getRefApiCount();
    const auto internalBefore = platform->getRefInternalCount();

    platform->incRefInternal();

    EXPECT_EQ(apiBefore, platform->getRefApiCount());
    EXPECT_EQ(apiBefore, platform->getReference());
    EXPECT_EQ(internalBefore + 1, platform->getRefInternalCount());

    platform->decRefInternal();
}

TEST_F(BaseObjectTests, givenStillReferencedObjectWhenReleasingThenReturnedPointerIsNotMarkedUnused) {
    platform->retain();

    auto maybeUnused = platform->release();

    EXPECT_FALSE(maybeUnused.isUnused());
}

TEST_F(BaseObjectTests, givenObjectWhenTakingOwnershipTwiceFromTheSameThreadThenBothLocksAreHeld) {
    auto firstLock = platform->takeOwnership();
    auto secondLock = platform->takeOwnership();

    EXPECT_TRUE(firstLock.owns_lock());
    EXPECT_TRUE(secondLock.owns_lock());
}

TEST_F(BaseObjectTests, givenObjectWhenOwnershipScopeEndsThenTheLockCanBeTakenAgain) {
    {
        auto lock = platform->takeOwnership();
        EXPECT_TRUE(lock.owns_lock());
    }

    auto lock = platform->takeOwnership();
    EXPECT_TRUE(lock.owns_lock());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
