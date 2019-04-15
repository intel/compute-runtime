/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/enqueue_fixture.h"

#include "runtime/helpers/ptr_math.h"

// clang-format off
// EnqueueTraits
cl_uint EnqueueTraits::numEventsInWaitList   = 0;
const cl_event *EnqueueTraits::eventWaitList = nullptr;
cl_event *EnqueueTraits::event               = nullptr;

static const auto negOne = static_cast<size_t>(-1);

static int ptrOutputContent[16] = {
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1 };
static auto ptrOutput = (void *)ptrOutputContent;

// EnqueueCopyBufferTraits
const size_t EnqueueCopyBufferTraits::srcOffset = 0;
const size_t EnqueueCopyBufferTraits::dstOffset = 0;
const size_t EnqueueCopyBufferTraits::size      = negOne;
cl_command_type EnqueueCopyBufferTraits::cmdType = CL_COMMAND_COPY_BUFFER;

// EnqueueCopyBufferToImageTraits
const size_t EnqueueCopyBufferToImageTraits::srcOffset = 0;
const size_t EnqueueCopyBufferToImageTraits::dstOrigin[3] = { 0,  0,  0};
const size_t EnqueueCopyBufferToImageTraits::region[3]    = {negOne, negOne, negOne};
cl_command_type EnqueueCopyBufferToImageTraits::cmdType = CL_COMMAND_COPY_BUFFER_TO_IMAGE;

// EnqueueCopyImageToBufferTraits
const size_t EnqueueCopyImageToBufferTraits::srcOrigin[3] = { 0,  0,  0};
const size_t EnqueueCopyImageToBufferTraits::region[3]    = {negOne, negOne, negOne};
const size_t EnqueueCopyImageToBufferTraits::dstOffset = 0;
cl_command_type EnqueueCopyImageToBufferTraits::cmdType = CL_COMMAND_COPY_IMAGE_TO_BUFFER;

// EnqueueCopyImageTraits
const size_t EnqueueCopyImageTraits::region[3]    = {negOne, negOne, negOne};
const size_t EnqueueCopyImageTraits::srcOrigin[3] = { 0,  0,  0};
const size_t EnqueueCopyImageTraits::dstOrigin[3] = { 0,  0,  0};
cl_command_type EnqueueCopyImageTraits::cmdType = CL_COMMAND_COPY_IMAGE;

// EnqueueFillBufferTraits
const float EnqueueFillBufferTraits::pattern[1]   = {1.2345f};
const size_t EnqueueFillBufferTraits::patternSize = sizeof(pattern);
const size_t EnqueueFillBufferTraits::offset      = 0;
const size_t EnqueueFillBufferTraits::size        = 2 * patternSize;
cl_command_type EnqueueFillBufferTraits::cmdType = CL_COMMAND_FILL_BUFFER;

// EnqueueFillImageTraits
const float  EnqueueFillImageTraits::fillColor[4] = { 1.0f, 2.0f, 3.0f, 4.0f};
const size_t EnqueueFillImageTraits::origin[3]    = { 0, 0, 0};
const size_t EnqueueFillImageTraits::region[3]    = {negOne, negOne, negOne};
cl_command_type EnqueueFillImageTraits::cmdType = CL_COMMAND_COPY_IMAGE;

// EnqueueKernelTraits
const cl_uint EnqueueKernelTraits::workDim            = 1;
const size_t EnqueueKernelTraits::globalWorkOffset[3] = {0, 0, 0};
const size_t EnqueueKernelTraits::globalWorkSize[3]   = {1, 1, 1};
const size_t *EnqueueKernelTraits::localWorkSize      = nullptr;
cl_command_type EnqueueKernelTraits::cmdType = CL_COMMAND_NDRANGE_KERNEL;

// EnqueueMapBufferTraits
const cl_bool EnqueueMapBufferTraits::blocking   = CL_TRUE;
const cl_mem_flags EnqueueMapBufferTraits::flags = CL_MAP_WRITE;
const size_t EnqueueMapBufferTraits::offset      = 0;
const size_t EnqueueMapBufferTraits::sizeInBytes = negOne;
cl_int *EnqueueMapBufferTraits::errcodeRet       = nullptr;
cl_command_type EnqueueMapBufferTraits::cmdType = CL_COMMAND_MAP_BUFFER;

// EnqueueReadBufferTraits
const cl_bool EnqueueReadBufferTraits::blocking             = CL_TRUE;
const size_t EnqueueReadBufferTraits::offset                = 0;
const size_t EnqueueReadBufferTraits::sizeInBytes           = negOne;
void *EnqueueReadBufferTraits::hostPtr                      = ptrOutput;
cl_command_type EnqueueReadBufferTraits::cmdType            = CL_COMMAND_READ_BUFFER;
GraphicsAllocation *EnqueueReadBufferTraits::mapAllocation  = nullptr;

// EnqueueReadImageTraits
const cl_bool EnqueueReadImageTraits::blocking  = CL_TRUE;
const size_t EnqueueReadImageTraits::origin[3]  = {0, 0, 0};
const size_t EnqueueReadImageTraits::region[3]  = {2, 2, 1};
const size_t EnqueueReadImageTraits::rowPitch   = 0;
const size_t EnqueueReadImageTraits::slicePitch = 0;
void *EnqueueReadImageTraits::hostPtr           = ptrOutput;
cl_command_type EnqueueReadImageTraits::cmdType = CL_COMMAND_READ_IMAGE;
GraphicsAllocation *EnqueueReadImageTraits::mapAllocation  = nullptr;

// EnqueueWriteBufferTraits
const bool EnqueueWriteBufferTraits::zeroCopy               = true;
const cl_bool EnqueueWriteBufferTraits::blocking            = CL_TRUE;
const size_t EnqueueWriteBufferTraits::offset               = 0;
const size_t EnqueueWriteBufferTraits::sizeInBytes          = negOne;
void *EnqueueWriteBufferTraits::hostPtr                     = ptrGarbage;
cl_command_type EnqueueWriteBufferTraits::cmdType           = CL_COMMAND_WRITE_BUFFER;
GraphicsAllocation *EnqueueWriteBufferTraits::mapAllocation = nullptr;

// EnqueueWriteBufferRectTraits
const bool EnqueueWriteBufferRectTraits::zeroCopy          = true;
const cl_bool EnqueueWriteBufferRectTraits::blocking       = CL_TRUE;
const size_t EnqueueWriteBufferRectTraits::bufferOrigin[3] = { 0, 0, 0 };
const size_t EnqueueWriteBufferRectTraits::hostOrigin[3]   = { 0, 0, 0 };
const size_t EnqueueWriteBufferRectTraits::region[3]       = { 2, 2, 1 };
size_t EnqueueWriteBufferRectTraits::bufferRowPitch        = 0;
size_t EnqueueWriteBufferRectTraits::bufferSlicePitch      = 0;
size_t EnqueueWriteBufferRectTraits::hostRowPitch          = 0;
size_t EnqueueWriteBufferRectTraits::hostSlicePitch        = 0;
void *EnqueueWriteBufferRectTraits::hostPtr                = ptrGarbage;
cl_command_type EnqueueWriteBufferRectTraits::cmdType      = CL_COMMAND_WRITE_BUFFER_RECT;

// EnqueueWriteImageTraits
const cl_bool EnqueueWriteImageTraits::blocking  = CL_TRUE;
const size_t EnqueueWriteImageTraits::origin[3]  = {0, 0, 0};
const size_t EnqueueWriteImageTraits::region[3]  = {2, 2, 1};
const size_t EnqueueWriteImageTraits::rowPitch   = 0;
const size_t EnqueueWriteImageTraits::slicePitch = 0;
void *EnqueueWriteImageTraits::hostPtr           = ptrGarbage;
cl_command_type EnqueueWriteImageTraits::cmdType = CL_COMMAND_WRITE_IMAGE;
GraphicsAllocation *EnqueueWriteImageTraits::mapAllocation  = nullptr;
// clang-format on
