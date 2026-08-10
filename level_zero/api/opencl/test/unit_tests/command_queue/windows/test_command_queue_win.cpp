/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(InPlaceSharingAcquireReleaseWindowsTests, givenDefaultDebugFlagWhenCheckingInPlaceModeThenD3DSharingsUseItInPlace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.LeoInPlaceSharingAcquireRelease.set(-1);

    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_D3D10_OBJECTS_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_D3D10_OBJECTS_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_D3D11_OBJECTS_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_D3D11_OBJECTS_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_DX9_MEDIA_SURFACES_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_DX9_MEDIA_SURFACES_KHR));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_DX9_OBJECTS_INTEL));
    EXPECT_TRUE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_DX9_OBJECTS_INTEL));
}

TEST(InPlaceSharingAcquireReleaseWindowsTests, givenDebugFlagSetToZeroWhenCheckingInPlaceModeThenD3DSharingsDoNotUseItInPlace) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.LeoInPlaceSharingAcquireRelease.set(0);

    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_D3D10_OBJECTS_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_D3D10_OBJECTS_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_D3D11_OBJECTS_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_D3D11_OBJECTS_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_DX9_MEDIA_SURFACES_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_DX9_MEDIA_SURFACES_KHR));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_ACQUIRE_DX9_OBJECTS_INTEL));
    EXPECT_FALSE(CommandQueue::isInPlaceSharingAcquireReleaseEnabled(CL_COMMAND_RELEASE_DX9_OBJECTS_INTEL));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
