/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/context/gl/context_gl_tests.h"

namespace NEO {

TEST_F(GlContextTest, GivenClGlContextWhenContextIsCreatedThenSuccess) {
    testContextCreation(CL_GL_CONTEXT_KHR);
}

TEST_F(GlContextTest, GivenEglContextWhenContextIsCreatedThenSuccess) {
    testContextCreation(CL_EGL_DISPLAY_KHR);
}

TEST_F(GlContextTest, GivenGlxContextWhenContextIsCreatedThenSuccess) {
    testContextCreation(CL_GLX_DISPLAY_KHR);
}

TEST_F(GlContextTest, GivenWglContextWhenContextIsCreatedThenSuccess) {
    testContextCreation(CL_WGL_HDC_KHR);
}

} // namespace NEO
