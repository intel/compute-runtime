/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/api/leo_dispatch.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#define EXPECT_DISPATCH_ENTRY(name) EXPECT_TRUE(icdGlobalDispatchTable.name == &::name) << #name
#define EXPECT_CRT_DISPATCH_ENTRY(name) EXPECT_TRUE(crtGlobalDispatchTable.name == &::name) << #name

namespace NEO {
namespace LEO {
namespace ult {

TEST(ClDispatchTableTests, givenGlobalDispatchTableThenItPointsAtTheIcdAndCrtTables) {
    EXPECT_EQ(&icdGlobalDispatchTable, globalDispatchTable.icdDispatch);
    EXPECT_EQ(&crtGlobalDispatchTable, globalDispatchTable.crtDispatch);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenCoreOnePointZeroEntriesPointAtTheirApiFunctions) {
    EXPECT_DISPATCH_ENTRY(clGetPlatformIDs);
    EXPECT_DISPATCH_ENTRY(clGetPlatformInfo);
    EXPECT_DISPATCH_ENTRY(clGetDeviceIDs);
    EXPECT_DISPATCH_ENTRY(clGetDeviceInfo);
    EXPECT_DISPATCH_ENTRY(clCreateContext);
    EXPECT_DISPATCH_ENTRY(clCreateContextFromType);
    EXPECT_DISPATCH_ENTRY(clRetainContext);
    EXPECT_DISPATCH_ENTRY(clReleaseContext);
    EXPECT_DISPATCH_ENTRY(clGetContextInfo);
    EXPECT_DISPATCH_ENTRY(clCreateCommandQueue);
    EXPECT_DISPATCH_ENTRY(clRetainCommandQueue);
    EXPECT_DISPATCH_ENTRY(clReleaseCommandQueue);
    EXPECT_DISPATCH_ENTRY(clGetCommandQueueInfo);
    EXPECT_DISPATCH_ENTRY(clSetCommandQueueProperty);
    EXPECT_DISPATCH_ENTRY(clCreateBuffer);
    EXPECT_DISPATCH_ENTRY(clCreateImage2D);
    EXPECT_DISPATCH_ENTRY(clCreateImage3D);
    EXPECT_DISPATCH_ENTRY(clRetainMemObject);
    EXPECT_DISPATCH_ENTRY(clReleaseMemObject);
    EXPECT_DISPATCH_ENTRY(clGetSupportedImageFormats);
    EXPECT_DISPATCH_ENTRY(clGetMemObjectInfo);
    EXPECT_DISPATCH_ENTRY(clGetImageInfo);
    EXPECT_DISPATCH_ENTRY(clCreateSampler);
    EXPECT_DISPATCH_ENTRY(clRetainSampler);
    EXPECT_DISPATCH_ENTRY(clReleaseSampler);
    EXPECT_DISPATCH_ENTRY(clGetSamplerInfo);
    EXPECT_DISPATCH_ENTRY(clCreateProgramWithSource);
    EXPECT_DISPATCH_ENTRY(clCreateProgramWithBinary);
    EXPECT_DISPATCH_ENTRY(clRetainProgram);
    EXPECT_DISPATCH_ENTRY(clReleaseProgram);
    EXPECT_DISPATCH_ENTRY(clBuildProgram);
    EXPECT_DISPATCH_ENTRY(clUnloadCompiler);
    EXPECT_DISPATCH_ENTRY(clGetProgramInfo);
    EXPECT_DISPATCH_ENTRY(clGetProgramBuildInfo);
    EXPECT_DISPATCH_ENTRY(clCreateKernel);
    EXPECT_DISPATCH_ENTRY(clCreateKernelsInProgram);
    EXPECT_DISPATCH_ENTRY(clRetainKernel);
    EXPECT_DISPATCH_ENTRY(clReleaseKernel);
    EXPECT_DISPATCH_ENTRY(clSetKernelArg);
    EXPECT_DISPATCH_ENTRY(clGetKernelInfo);
    EXPECT_DISPATCH_ENTRY(clGetKernelWorkGroupInfo);
    EXPECT_DISPATCH_ENTRY(clWaitForEvents);
    EXPECT_DISPATCH_ENTRY(clGetEventInfo);
    EXPECT_DISPATCH_ENTRY(clRetainEvent);
    EXPECT_DISPATCH_ENTRY(clReleaseEvent);
    EXPECT_DISPATCH_ENTRY(clGetEventProfilingInfo);
    EXPECT_DISPATCH_ENTRY(clFlush);
    EXPECT_DISPATCH_ENTRY(clFinish);
    EXPECT_DISPATCH_ENTRY(clEnqueueReadBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueWriteBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueCopyBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueReadImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueWriteImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueCopyImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueCopyImageToBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueCopyBufferToImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueMapBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueMapImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueUnmapMemObject);
    EXPECT_DISPATCH_ENTRY(clEnqueueNDRangeKernel);
    EXPECT_DISPATCH_ENTRY(clEnqueueTask);
    EXPECT_DISPATCH_ENTRY(clEnqueueNativeKernel);
    EXPECT_DISPATCH_ENTRY(clEnqueueMarker);
    EXPECT_DISPATCH_ENTRY(clEnqueueWaitForEvents);
    EXPECT_DISPATCH_ENTRY(clEnqueueBarrier);
    EXPECT_DISPATCH_ENTRY(clGetExtensionFunctionAddress);
    EXPECT_DISPATCH_ENTRY(clSetEventCallback);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenCoreOnePointOneAndOnePointTwoEntriesPointAtTheirApiFunctions) {
    EXPECT_DISPATCH_ENTRY(clCreateSubBuffer);
    EXPECT_DISPATCH_ENTRY(clSetMemObjectDestructorCallback);
    EXPECT_DISPATCH_ENTRY(clCreateUserEvent);
    EXPECT_DISPATCH_ENTRY(clSetUserEventStatus);
    EXPECT_DISPATCH_ENTRY(clEnqueueReadBufferRect);
    EXPECT_DISPATCH_ENTRY(clEnqueueWriteBufferRect);
    EXPECT_DISPATCH_ENTRY(clEnqueueCopyBufferRect);
    EXPECT_DISPATCH_ENTRY(clCreateSubDevices);
    EXPECT_DISPATCH_ENTRY(clRetainDevice);
    EXPECT_DISPATCH_ENTRY(clReleaseDevice);
    EXPECT_DISPATCH_ENTRY(clCreateImage);
    EXPECT_DISPATCH_ENTRY(clCreateProgramWithBuiltInKernels);
    EXPECT_DISPATCH_ENTRY(clCompileProgram);
    EXPECT_DISPATCH_ENTRY(clLinkProgram);
    EXPECT_DISPATCH_ENTRY(clUnloadPlatformCompiler);
    EXPECT_DISPATCH_ENTRY(clGetKernelArgInfo);
    EXPECT_DISPATCH_ENTRY(clEnqueueFillBuffer);
    EXPECT_DISPATCH_ENTRY(clEnqueueFillImage);
    EXPECT_DISPATCH_ENTRY(clEnqueueMigrateMemObjects);
    EXPECT_DISPATCH_ENTRY(clEnqueueMarkerWithWaitList);
    EXPECT_DISPATCH_ENTRY(clEnqueueBarrierWithWaitList);
    EXPECT_DISPATCH_ENTRY(clGetExtensionFunctionAddressForPlatform);
    EXPECT_DISPATCH_ENTRY(clCreateCommandQueueWithProperties);
    EXPECT_DISPATCH_ENTRY(clCreatePipe);
    EXPECT_DISPATCH_ENTRY(clGetPipeInfo);
    EXPECT_DISPATCH_ENTRY(clSVMAlloc);
    EXPECT_DISPATCH_ENTRY(clSVMFree);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenCoreTwoPointZeroAndLaterEntriesPointAtTheirApiFunctions) {
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMFree);
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMMemcpy);
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMMemFill);
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMMap);
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMUnmap);
    EXPECT_DISPATCH_ENTRY(clCreateSamplerWithProperties);
    EXPECT_DISPATCH_ENTRY(clSetKernelArgSVMPointer);
    EXPECT_DISPATCH_ENTRY(clSetKernelExecInfo);
    EXPECT_DISPATCH_ENTRY(clGetKernelSubGroupInfoKHR);
    EXPECT_DISPATCH_ENTRY(clCloneKernel);
    EXPECT_DISPATCH_ENTRY(clCreateProgramWithIL);
    EXPECT_DISPATCH_ENTRY(clEnqueueSVMMigrateMem);
    EXPECT_DISPATCH_ENTRY(clGetDeviceAndHostTimer);
    EXPECT_DISPATCH_ENTRY(clGetHostTimer);
    EXPECT_DISPATCH_ENTRY(clGetKernelSubGroupInfo);
    EXPECT_DISPATCH_ENTRY(clSetDefaultDeviceCommandQueue);
    EXPECT_DISPATCH_ENTRY(clSetProgramReleaseCallback);
    EXPECT_DISPATCH_ENTRY(clSetProgramSpecializationConstant);
    EXPECT_DISPATCH_ENTRY(clCreateBufferWithProperties);
    EXPECT_DISPATCH_ENTRY(clCreateImageWithProperties);
    EXPECT_DISPATCH_ENTRY(clSetContextDestructorCallback);
    EXPECT_DISPATCH_ENTRY(clGetKernelSuggestedLocalWorkSize);
}
TEST(ClDispatchTableTests, givenCrtDispatchTableThenPopulatedEntriesPointAtTheirApiFunctions) {
    EXPECT_CRT_DISPATCH_ENTRY(clGetKernelArgInfo);
    EXPECT_CRT_DISPATCH_ENTRY(clGetImageParamsINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clCreatePerfCountersCommandQueueINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clCreateAcceleratorINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clGetAcceleratorInfoINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clRetainAcceleratorINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clReleaseAcceleratorINTEL);
    EXPECT_CRT_DISPATCH_ENTRY(clSetPerformanceConfigurationINTEL);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenRetainAndReleaseSlotsHoldDifferentFunctions) {
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainContext == icdGlobalDispatchTable.clReleaseContext);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainCommandQueue == icdGlobalDispatchTable.clReleaseCommandQueue);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainMemObject == icdGlobalDispatchTable.clReleaseMemObject);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainProgram == icdGlobalDispatchTable.clReleaseProgram);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainKernel == icdGlobalDispatchTable.clReleaseKernel);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainEvent == icdGlobalDispatchTable.clReleaseEvent);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainSampler == icdGlobalDispatchTable.clReleaseSampler);
    EXPECT_FALSE(icdGlobalDispatchTable.clRetainDevice == icdGlobalDispatchTable.clReleaseDevice);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenIdenticallyTypedQueueEntriesAreNotCrossWired) {
    EXPECT_FALSE(icdGlobalDispatchTable.clFlush == icdGlobalDispatchTable.clFinish);
    EXPECT_FALSE(icdGlobalDispatchTable.clFlush == icdGlobalDispatchTable.clRetainCommandQueue);
    EXPECT_FALSE(icdGlobalDispatchTable.clFinish == icdGlobalDispatchTable.clReleaseCommandQueue);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenDeprecatedSubDeviceEntriesStayEmpty) {
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateSubDevices == &::clCreateSubDevices);
    EXPECT_TRUE(icdGlobalDispatchTable.clRetainDevice == &::clRetainDevice);
    EXPECT_TRUE(icdGlobalDispatchTable.clReleaseDevice == &::clReleaseDevice);
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clCreateSubDevicesEXT);
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clRetainDeviceEXT);
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clReleaseDeviceEXT);
}

TEST(ClDispatchTableTests, givenIcdDispatchTableThenSubGroupInfoAliasesResolveToTheirOwnFunctions) {
    EXPECT_TRUE(icdGlobalDispatchTable.clGetKernelSubGroupInfo == &::clGetKernelSubGroupInfo);
    EXPECT_TRUE(icdGlobalDispatchTable.clGetKernelSubGroupInfoKHR == &::clGetKernelSubGroupInfoKHR);
}

using ClDispatchTableFixture = Test<OclFixture>;

TEST_F(ClDispatchTableFixture, givenPlatformWhenCreatedThenItsDispatchPointsAtTheGlobalTables) {
    EXPECT_EQ(&icdGlobalDispatchTable, platform->dispatch.icdDispatch);
    EXPECT_EQ(&crtGlobalDispatchTable, platform->dispatch.crtDispatch);
    EXPECT_TRUE(isValidObject(static_cast<cl_platform_id>(platform)));
}

TEST_F(ClDispatchTableFixture, givenContextCreatedThroughApiThenItsDispatchPointsAtTheGlobalTables) {
    cl_device_id clDeviceId = platform->getDevices()[0].get();
    cl_int errcode = CL_SUCCESS;
    auto clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    EXPECT_EQ(&icdGlobalDispatchTable, clContext->dispatch.icdDispatch);
    EXPECT_TRUE(isValidObject(clContext));
    EXPECT_NE(nullptr, castToObject<Context>(clContext));

    clReleaseContext(clContext);
}

TEST_F(ClDispatchTableFixture, givenDeviceFromPlatformThenItsDispatchPointsAtTheGlobalTables) {
    cl_device_id clDeviceId = platform->getDevices()[0].get();

    EXPECT_EQ(&icdGlobalDispatchTable, clDeviceId->dispatch.icdDispatch);
    EXPECT_TRUE(isValidObject(clDeviceId));
}

TEST_F(ClDispatchTableFixture, givenForeignDispatchInObjectWhenCheckingValidityThenItIsRejected) {
    cl_platform_id clPlatform = platform;
    auto savedDispatch = clPlatform->dispatch.icdDispatch;

    clPlatform->dispatch.icdDispatch = nullptr;
    EXPECT_FALSE(isValidObject(clPlatform));
    EXPECT_EQ(nullptr, castToObject<Platform>(clPlatform));

    clPlatform->dispatch.icdDispatch = savedDispatch;
    EXPECT_TRUE(isValidObject(clPlatform));
    EXPECT_EQ(platform, castToObject<Platform>(clPlatform));
}

TEST_F(ClDispatchTableFixture, givenNullObjectWhenCheckingValidityThenItIsRejected) {
    EXPECT_FALSE(isValidObject(static_cast<cl_platform_id>(nullptr)));
}

TEST_F(ClDispatchTableFixture, givenPlatformWhenCreatedThenGlSharingEntriesAreWiredIntoTheIcdTable) {
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateFromGLBuffer == &::clCreateFromGLBuffer);
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateFromGLTexture == &::clCreateFromGLTexture);
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateFromGLTexture2D == &::clCreateFromGLTexture2D);
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateFromGLTexture3D == &::clCreateFromGLTexture3D);
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateFromGLRenderbuffer == &::clCreateFromGLRenderbuffer);
    EXPECT_TRUE(icdGlobalDispatchTable.clGetGLObjectInfo == &::clGetGLObjectInfo);
    EXPECT_TRUE(icdGlobalDispatchTable.clGetGLTextureInfo == &::clGetGLTextureInfo);
    EXPECT_TRUE(icdGlobalDispatchTable.clEnqueueAcquireGLObjects == &::clEnqueueAcquireGLObjects);
    EXPECT_TRUE(icdGlobalDispatchTable.clEnqueueReleaseGLObjects == &::clEnqueueReleaseGLObjects);
    EXPECT_TRUE(icdGlobalDispatchTable.clCreateEventFromGLsyncKHR == &::clCreateEventFromGLsyncKHR);
    EXPECT_TRUE(icdGlobalDispatchTable.clGetGLContextInfoKHR == &::clGetGLContextInfoKHR);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
