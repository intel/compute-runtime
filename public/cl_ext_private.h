/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

/***************************************
 * * Internal only queue properties *
 * ****************************************/
// Intel evaluation now. Remove it after approval for public release
#define CL_DEVICE_DRIVER_VERSION_INTEL 0x10010

#define CL_DEVICE_DRIVER_VERSION_INTEL_NEO1 0x454E4831 // Driver version is ENH1

/***************************************
 * * cl_intel_debug_info extension *
 * ****************************************/
#define cl_intel_debug_info 1

// New queries for clGetProgramInfo:
#define CL_PROGRAM_DEBUG_INFO_INTEL 0x4100
#define CL_PROGRAM_DEBUG_INFO_SIZES_INTEL 0x4101

// New queries for clGetKernelInfo:
#define CL_KERNEL_BINARY_PROGRAM_INTEL 0x407D
#define CL_KERNEL_BINARIES_INTEL 0x4102
#define CL_KERNEL_BINARY_SIZES_INTEL 0x4103
