/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off

struct SProgramBinaryHeader
{
	uint32_t   Magic;
	uint32_t   Version;

	uint32_t   Device;
    uint32_t   GPUPointerSizeInBytes;

    uint32_t   NumberOfKernels;

    uint32_t   SteppingId;

    uint32_t   PatchListSize;
};

struct SKernelBinaryHeader
{
    uint32_t   CheckSum;
    uint64_t   ShaderHashCode;
    uint32_t   KernelNameSize;
    uint32_t   PatchListSize;
};

struct SKernelBinaryHeaderCommon :
       SKernelBinaryHeader
{
    uint32_t   KernelHeapSize;
    uint32_t   GeneralStateHeapSize;
    uint32_t   DynamicStateHeapSize;
    uint32_t   SurfaceStateHeapSize;
    uint32_t   KernelUnpaddedSize;
};

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
    PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT,           // 11   @SPatchGlobalMemoryObjectKernelArgument@ - OpenCL
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
    uint32_t   Token;
    uint32_t   Size;
};

struct SPatchDataParameterBuffer :
       SPatchItemHeader
{
    uint32_t   Type;
    uint32_t   ArgumentNumber;
    uint32_t   Offset;
    uint32_t   DataSize;
    uint32_t   SourceOffset;
    uint32_t   LocationIndex;
    uint32_t   LocationIndex2;
    uint32_t   IsEmulationArgument;
};

struct SPatchMediaInterfaceDescriptorLoad :
       SPatchItemHeader
{
    uint32_t   InterfaceDescriptorDataOffset;
};

struct SPatchStateSIP :
       SPatchItemHeader
{
    uint32_t   SystemKernelOffset;
};

struct SPatchSamplerStateArray :
       SPatchItemHeader
{
    uint32_t   Offset;
    uint32_t   Count;
    uint32_t   BorderColorOffset;
};

struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :
    SPatchItemHeader
{
    uint32_t   ConstantBufferIndex;
    uint32_t   InlineDataSize;
};
// clang-format on