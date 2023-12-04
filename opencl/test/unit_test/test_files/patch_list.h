/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
#pragma once
#pragma pack( push, 1 )

#include <cstdint>

const uint32_t MAGIC_CL = 0x494E5443; // NOLINT(readability-identifier-naming)
struct SProgramBinaryHeader
{
	uint32_t   Magic;   // NOLINT(readability-identifier-naming)
	uint32_t   Version; // NOLINT(readability-identifier-naming)

	uint32_t   Device;                // NOLINT(readability-identifier-naming)
    uint32_t   GPUPointerSizeInBytes; // NOLINT(readability-identifier-naming)

    uint32_t   NumberOfKernels; // NOLINT(readability-identifier-naming)

    uint32_t   SteppingId; // NOLINT(readability-identifier-naming)

    uint32_t   PatchListSize; // NOLINT(readability-identifier-naming)
};
static_assert( sizeof( SProgramBinaryHeader ) == 28 , "The size of SProgramBinaryHeader is not what is expected" );

struct SKernelBinaryHeader
{
    uint32_t   CheckSum;       // NOLINT(readability-identifier-naming)
    uint64_t   ShaderHashCode; // NOLINT(readability-identifier-naming)
    uint32_t   KernelNameSize; // NOLINT(readability-identifier-naming)
    uint32_t   PatchListSize;  // NOLINT(readability-identifier-naming)
};
static_assert( sizeof( SKernelBinaryHeader ) == 20 , "The size of SKernelBinaryHeader is not what is expected" );

struct SKernelBinaryHeaderCommon :
       SKernelBinaryHeader
{
    uint32_t   KernelHeapSize;       // NOLINT(readability-identifier-naming)
    uint32_t   GeneralStateHeapSize; // NOLINT(readability-identifier-naming)
    uint32_t   DynamicStateHeapSize; // NOLINT(readability-identifier-naming)
    uint32_t   SurfaceStateHeapSize; // NOLINT(readability-identifier-naming)
    uint32_t   KernelUnpaddedSize;   // NOLINT(readability-identifier-naming)
};
static_assert( sizeof( SKernelBinaryHeaderCommon ) == ( 20 + sizeof( SKernelBinaryHeader ) ) , "The size of SKernelBinaryHeaderCommon is not what is expected" );

enum PATCH_TOKEN
{  
    PATCH_TOKEN_UNKNOWN,                                        // 0	- (Unused)
    PATCH_TOKEN_MEDIA_STATE_POINTERS,                           // 1    - (Unused)
    PATCH_TOKEN_STATE_SIP,                                      // 2	@SPatchStateSIP@
    PATCH_TOKEN_CS_URB_STATE,                                   // 3    - (Unused)
    PATCH_TOKEN_CONSTANT_BUFFER,                                // 4    - (Unused)
    PATCH_TOKEN_SAMPLER_STATE_ARRAY,                            // 5	@SPatchSamplerStateArray@
    PATCH_TOKEN_INTERFACE_DESCRIPTOR,                           // 6    - (Unused)
    PATCH_TOKEN_VFE_STATE,                                      // 7    - (Unused)
    PATCH_TOKEN_BINDING_TABLE_STATE,                            // 8	@SPatchBindingTableState@
    PATCH_TOKEN_ALLOCATE_SCRATCH_SURFACE,                       // 9	- (Unused)
    PATCH_TOKEN_ALLOCATE_SIP_SURFACE,                           // 10	@SPatchAllocateSystemThreadSurface@
    PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT,          // 11   @SPatchGlobalMemoryObjectKernelArgument@ - OpenCL
    PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT,            // 12   @SPatchImageMemoryObjectKernelArgument@	- OpenCL
    PATCH_TOKEN_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT,         // 13   - (Unused)	- OpenCL
    PATCH_TOKEN_ALLOCATE_SURFACE_WITH_INITIALIZATION,           // 14	- (Unused)
    PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE,                         // 15	@SPatchAllocateLocalSurface@
    PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT,                        // 16   @SPatchSamplerKernelArgument@	- OpenCL
    PATCH_TOKEN_DATA_PARAMETER_BUFFER,                          // 17   @SPatchDataParameterBuffer@ - OpenCL
    PATCH_TOKEN_MEDIA_VFE_STATE,                                // 18	@SPatchMediaVFEState@
    PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD,                // 19	@SPatchMediaInterfaceDescriptorLoad@
    PATCH_TOKEN_MEDIA_CURBE_LOAD,                               // 20   - (Unused)
    PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA,                      // 21	@SPatchInterfaceDescriptorData@
    PATCH_TOKEN_THREAD_PAYLOAD,                                 // 22	@SPatchThreadPayload@
    PATCH_TOKEN_EXECUTION_ENVIRONMENT,                          // 23	@SPatchExecutionEnvironment@
    PATCH_TOKEN_ALLOCATE_PRIVATE_MEMORY,                        // 24	- (Unused)
    PATCH_TOKEN_DATA_PARAMETER_STREAM,                          // 25	@SPatchDataParameterStream
    PATCH_TOKEN_KERNEL_ARGUMENT_INFO,                           // 26   @SPatchKernelArgumentInfo@ - OpenCL
    PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO,                         // 27   @SPatchKernelAttributesInfo@ - OpenCL
    PATCH_TOKEN_STRING,                                         // 28   @SPatchString@ - OpenCL
    PATCH_TOKEN_ALLOCATE_PRINTF_SURFACE,                        // 29   - (Unused)	- OpenCL
    PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT, // 30   @SPatchStatelessGlobalMemoryObjectKernelArgument@	- OpenCL
    PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT,//31   @SPatchStatelessConstantMemoryObjectKernelArgument@ - OpenCL
    PATCH_TOKEN_ALLOCATE_STATELESS_SURFACE_WITH_INITIALIZATION, // 32	- (Unused)
    PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE,              // 33	@SPatchAllocateStatelessPrintfSurface@
    PATCH_TOKEN_CB_MAPPING,                                     // 34	- (Unused)
    PATCH_TOKEN_CB2CR_GATHER_TABLE,                             // 35	- (Unused)
    PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE,          // 36	@SPatchAllocateStatelessEventPoolSurface@
    PATCH_TOKEN_NULL_SURFACE_LOCATION,                          // 37	- (Unused)
    PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY,              // 38	@SPatchAllocateStatelessPrivateSurface@
    PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION,           // 39	- (Unused)
    PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION,             // 40	- (Unused)
    PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO,             // 41	@SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo@
    PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO,           // 42	@SPatchAllocateConstantMemorySurfaceProgramBinaryInfo@
    PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION,   // 43	@SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization@
    PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION, // 44	@SPatchAllocateStatelessConstantMemorySurfaceWithInitialization@
    PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE,                // 45	@SPatchAllocateStatelessDefaultDeviceQueueSurface@
    PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT,            // 46	@SPatchStatelessDeviceQueueKernelArgument@
    PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO,                // 47	@SPatchGlobalPointerProgramBinaryInfo@
    PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO,              // 48	@SPatchConstantPointerProgramBinaryInfo@
    PATCH_TOKEN_CONSTRUCTOR_DESTRUCTOR_KERNEL_PROGRAM_BINARY_INFO, // 49	- (Unused)
    PATCH_TOKEN_INLINE_VME_SAMPLER_INFO,                           // 50	- (Unused)
    PATCH_TOKEN_GTPIN_FREE_GRF_INFO,                               // 51	@SPatchGtpinFreeGRFInfo@
    PATCH_TOKEN_GTPIN_INFO,

    NUM_PATCH_TOKENS
};

struct SPatchItemHeader
{
    uint32_t   Token; // NOLINT(readability-identifier-naming)
    uint32_t   Size;  // NOLINT(readability-identifier-naming)
};

struct SPatchDataParameterBuffer :
       SPatchItemHeader
{
    uint32_t   Type;                // NOLINT(readability-identifier-naming)
    uint32_t   ArgumentNumber;      // NOLINT(readability-identifier-naming)
    uint32_t   Offset;              // NOLINT(readability-identifier-naming)
    uint32_t   DataSize;            // NOLINT(readability-identifier-naming)
    uint32_t   SourceOffset;        // NOLINT(readability-identifier-naming)
    uint32_t   LocationIndex;       // NOLINT(readability-identifier-naming)
    uint32_t   LocationIndex2;      // NOLINT(readability-identifier-naming)
    uint32_t   IsEmulationArgument; // NOLINT(readability-identifier-naming)
};

struct SPatchMediaInterfaceDescriptorLoad :
       SPatchItemHeader
{
    uint32_t   InterfaceDescriptorDataOffset; // NOLINT(readability-identifier-naming)
};
static_assert( sizeof( SPatchMediaInterfaceDescriptorLoad ) == ( 4 + sizeof( SPatchItemHeader ) ) , "The size of SPatchMediaInterfaceDescriptorLoad is not what is expected" );
struct SPatchStateSIP :
       SPatchItemHeader
{
    uint32_t   SystemKernelOffset; // NOLINT(readability-identifier-naming)
};

struct SPatchSamplerStateArray :
       SPatchItemHeader
{
    uint32_t   Offset;            // NOLINT(readability-identifier-naming)
    uint32_t   Count;             // NOLINT(readability-identifier-naming)
    uint32_t   BorderColorOffset; // NOLINT(readability-identifier-naming)
};

struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :
    SPatchItemHeader
{
    uint32_t   ConstantBufferIndex; // NOLINT(readability-identifier-naming)
    uint32_t   InlineDataSize;      // NOLINT(readability-identifier-naming)
};
static_assert( sizeof( SPatchAllocateConstantMemorySurfaceProgramBinaryInfo ) == ( 8 + sizeof( SPatchItemHeader ) ) , "The size of SPatchAllocateConstantMemorySurfaceProgramBinaryInfo is not what is expected" );
#pragma pack( pop )
// clang-format on
