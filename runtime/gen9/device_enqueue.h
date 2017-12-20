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
 *
 * File Name: Device_Enqueue.h
 */

#pragma once
#include "../execution_model/device_enqueue.h"

#define WA_PROFILING_PREEMPTION 1
#define WA_SCHEDULER_PREEMPTION 1
#define WA_KERNEL_PREEMPTION 1
#define WA_ARB_CHECK_AFTER_MSF 1
#define WA_MI_ATOMIC_BEFORE_MEDIA_ID_LOAD 1

#define OCLRT_GPGPU_WALKER_CMD_DEVICE_CMD_G9 (15 * sizeof(uint))
#define OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9 (6 * sizeof(uint))
#define OCLRT_LOAD_REGISTER_IMM_CMD_G9 (3 * sizeof(uint))

#define OCLRT_GPGPU_WALKER_CMD_DEVICE_CMD_G9_DWORD_OFFSET (15)
#define OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9_DWORD_OFFSET (6)

// Changed: MediaVFE state cmd size removal
#define SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE_GEN9 (OCLRT_SIZEOF_MEDIA_STATE_FLUSH + OCLRT_SIZEOF_MI_ARB_CHECK + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9 + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9 + OCLRT_SIZEOF_MI_ATOMIC_CMD + OCLRT_SIZEOF_MEDIA_INTERFACE_DESCRIPTOR_LOAD_DEVICE_CMD + OCLRT_GPGPU_WALKER_CMD_DEVICE_CMD_G9 + OCLRT_SIZEOF_MEDIA_STATE_FLUSH + OCLRT_SIZEOF_MI_ARB_CHECK + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9 + OCLRT_PIPE_CONTROL_CMD_DEVICE_CMD_G9 + CS_PREFETCH_SIZE)
#define SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES_GEN9 (128)