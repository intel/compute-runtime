/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

// Uncomment this macro to build "empty" schedulers
//#define WA_DISABLE_SCHEDULERS 1

#if !defined(__OPENCL_VERSION__)
#include <cstdint>

typedef uint32_t uint;
typedef uint64_t ulong;
#endif

#define OCLRT_SIZEOF_MEDIA_INTERFACE_DESCRIPTOR_LOAD_DEVICE_CMD (4 * sizeof(uint))
#define OCLRT_SIZEOF_MEDIA_CURBE_LOAD_DEVICE_CMD (4 * sizeof(uint))
#define OCLRT_SIZEOF_MEDIA_STATE_FLUSH (2 * sizeof(uint))
#define OCLRT_SIZEOF_MI_ATOMIC_CMD (11 * sizeof(uint))
#define OCLRT_SIZEOF_MEDIA_VFE_STATE_CMD (9 * sizeof(uint))
#define OCLRT_SIZEOF_MI_ARB_CHECK (1 * sizeof(uint))

#define OCLRT_SIZEOF_MEDIA_INTERFACE_DESCRIPTOR_LOAD_DEVICE_CMD_DWORD_OFFSET (4)
#define OCLRT_SIZEOF_MI_ATOMIC_CMD_DWORD_OFFSET (11)
#define OCLRT_SIZEOF_MEDIA_CURBE_LOAD_DEVICE_CMD_DWORD_OFFSET (4)
#define OCLRT_IMM_LOAD_REGISTER_CMD_DEVICE_CMD_DWORD_OFFSET (3)

#define OCLRT_SIZEOF_MSFLUSH_DWORD (2)
#define OCLRT_SIZEOF_MI_ARB_CHECK_DWORD (1)
#define OCLRT_SIZEOF_MEDIA_VFE_STATE_DWORD (9)

#define OCLRT_BATCH_BUFFER_END_CMD (83886080)

//Constant buffer stuff
#define COMPILER_DATA_PARAMETER_GLOBAL_SURFACE (49)

#define SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT (50)

#define SCHEDULER_DATA_PARAMETER_GLOBAL_POINTER_SHIFT (63)
#define SCHEDULER_DATA_PARAMETER_SAMPLER_SHIFT (51)
#define SCHEDULER_DATA_PARAMETER_SAMPLER_ADDED_VALUE (2 * SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT)

#define CS_PREFETCH_SIZE (8 * 64)

#define ALL_BITS_SET_DWORD_MASK (0xffffffff)
#define DWORD_SIZE_IN_BITS (32)

#define CL_sRGB 0x10BF
#define CL_sRGBX 0x10C0
#define CL_sRGBA 0x10C1
#define CL_sBGRA 0x10C2

//scheduler currently can spawn up to 8 GPGPU_WALKERS between scheduler runs, so it needs 8 * 3 HW threads for scheduling blocks + 1 HW thread to scheduler next scheduler
//each HW group consist of 3 HW threads that are capable of scheduling 1 block

//!!! Make sure value of this define equals MAX_NUMBER_OF_PARALLEL_GPGPU_WALKERS in DeviceEnqueueInternalTypes.h
#define PARALLEL_SCHEDULER_HW_GROUPS (8)
#define PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP (3)
#define PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 (3)
#define PARALLEL_SCHEDULER_HW_GROUPS_IN_THREADS (PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP * PARALLEL_SCHEDULER_HW_GROUPS)

#define PARALLEL_SCHEDULER_NUMBER_HW_THREADS (PARALLEL_SCHEDULER_HW_GROUPS_IN_THREADS + 1)

//parallel scheduler 2.0 is compiled in simd8
#define PARALLEL_SCHEDULER_COMPILATION_SIZE_20 (8)

#define HW_GROUP_ID_SHIFT(COMPILATION_SIZE) ((COMPILATION_SIZE & 0x10) ? 4 : 3)

#define GRF_SIZE (32)
#define SIZEOF_3GRFS (3 * GRF_SIZE)

//estimation for dynamic payload size
#define SCHEDULER_DYNAMIC_PAYLOAD_SIZE (PARALLEL_SCHEDULER_NUMBER_HW_THREADS * SIZEOF_3GRFS)

//assume that max DSH per walker is 9472B ( assuming registers can take up to 4KB, and max dynamic payload is around 96B * 56(HW threads) it should be fine.
#define MAX_DSH_SIZE_PER_ENQUEUE 9472

#define MAX_BINDING_TABLE_INDEX (253)
#define MAX_SSH_PER_KERNEL_SIZE (MAX_BINDING_TABLE_INDEX * 64) //max SSH that can be one kernel. It is 253 binding table entries multiplied by the Surface State size.

#define OCLRT_ARG_OFFSET_TO_SAMPLER_OBJECT_ID(ArgOffset) (ArgOffset + MAX_SSH_PER_KERNEL_SIZE)
#define OCLRT_IMAGE_MAX_OBJECT_ID (MAX_SSH_PER_KERNEL_SIZE - 1)
#define OCLRT_SAMPLER_MIN_OBJECT_ID (MAX_SSH_PER_KERNEL_SIZE)

typedef enum tagDebugDataTypes {
    DBG_DEFAULT = 0,
    DBG_COMMAND_QUEUE = 1,
    DBG_EVENTS_UPDATE = 2,
    DBG_EVENTS_NUMBER = 3,
    DBG_STACK_UPDATE = 4,
    DBG_BEFORE_PATCH = 5,
    DBG_KERNELID = 6,
    DBG_DSHOFFSET = 7,
    DBG_IDOFFSET = 8,
    DBG_AFTER_PATCH = 9,
    DBG_UNSPECIFIED = 10,
    DBG_ENQUEUES_NUMBER = 11,
    DBG_LOCAL_ID,
    DBG_WKG_ID,
    DBG_SCHEDULER_END,
    // Add here new debug enums
    DBG_MAX
} DebugDataTypes;
// Struct for debugging kernels
typedef struct
{
    DebugDataTypes m_dataType;
    uint m_dataSize;
} DebugDataInfo;
typedef struct
{
    enum DDBFlags { DDB_HAS_DATA_INFO = 1,
                    DDB_SCHEDULER_PROFILING = 2,
                    DDB_COMMAND_QUEUE_RAW = 4 } ddbFlags;
    uint m_size;
    uint m_stackTop;    //index of data stack
    uint m_dataInfoTop; //index of the top of DataInfo stack, this stacks grows with decrementing address
    uint m_stackBottom;
    uint m_dataInfoBottom; //index of the bottom of DataInfo
    uint m_dataInfoSize;
    uint m_flags;

    uint m_offset;    //current offset indicates free place
    uint m_data[100]; //buffer
} DebugDataBuffer;

#pragma pack(push)
#pragma pack(4)
#include "DeviceEnqueueInternalTypes.h"
#pragma pack(pop)
