/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/sharings/va/mock_va_sharing.h"

namespace OCLRT {
int vaDisplayIsValidCalled = 0;
int vaDeriveImageCalled = 0;
int vaDestroyImageCalled = 0;
int vaSyncSurfaceCalled = 0;
int vaGetLibFuncCalled = 0;
int vaExtGetSurfaceHandleCalled = 0;
osHandle acquiredVaHandle = 0;
VAImage mockVaImage = {};
uint16_t vaSharingFunctionsMockWidth, vaSharingFunctionsMockHeight;

void VASharingFunctionsMock::initMembers() {
    vaDisplayIsValidPFN = mockVaDisplayIsValid;
    vaDeriveImagePFN = mockVaDeriveImage;
    vaDestroyImagePFN = mockVaDestroyImage;
    vaSyncSurfacePFN = mockVaSyncSurface;
    vaGetLibFuncPFN = mockVaGetLibFunc;
    vaExtGetSurfaceHandlePFN = mockExtGetSurfaceHandle;

    vaDisplayIsValidCalled = 0;
    vaDeriveImageCalled = 0;
    vaDestroyImageCalled = 0;
    vaSyncSurfaceCalled = 0;
    vaGetLibFuncCalled = 0;
    vaExtGetSurfaceHandleCalled = 0;

    mockVaImage = {};

    acquiredVaHandle = 0;
    vaSharingFunctionsMockWidth = 256u;
    vaSharingFunctionsMockHeight = 256u;
}
} // namespace OCLRT
