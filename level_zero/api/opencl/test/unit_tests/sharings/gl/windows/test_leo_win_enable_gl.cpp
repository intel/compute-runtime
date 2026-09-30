/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/sharings/gl/windows/leo_win_enable_gl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(LeoGlSharingBuilderFactoryWindowsTest, givenGlSharingDisabledByDebugFlagWhenGettingExtensionsThenNoGlExtensionIsReported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AddClGlSharing.set(0);

    GlSharingBuilderFactory factory;
    EXPECT_EQ(std::string(""), factory.getExtensions(nullptr));
}

TEST(LeoGlSharingBuilderFactoryWindowsTest, givenGlSharingEnabledByDebugFlagWhenGettingExtensionsThenGlExtensionsAreReported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AddClGlSharing.set(1);

    GlSharingBuilderFactory factory;
    EXPECT_EQ(std::string("cl_khr_gl_sharing "
                          "cl_khr_gl_depth_images "
                          "cl_khr_gl_event "
                          "cl_khr_gl_msaa_sharing "),
              factory.getExtensions(nullptr));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
