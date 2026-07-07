/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_error_mappers.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

namespace NEO {
namespace LEO {
namespace ult {

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

} // namespace ult
} // namespace LEO
} // namespace NEO
