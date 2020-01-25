/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "patchtokens_dumper.h"

#include "patchtokens_decoder.h"

#include <sstream>

namespace NEO {

namespace PatchTokenBinary {

#define CASE_TOK_STR(TOK)                              \
    case TOK:                                          \
        return std::to_string(TOK) + "(" + #TOK + ")"; \
        break;

std::string asString(PATCH_TOKEN token) {
    switch (token) {
    default:
        return std::to_string(token);
        CASE_TOK_STR(PATCH_TOKEN_UNKNOWN);
        CASE_TOK_STR(PATCH_TOKEN_MEDIA_STATE_POINTERS);
        CASE_TOK_STR(PATCH_TOKEN_STATE_SIP);
        CASE_TOK_STR(PATCH_TOKEN_CS_URB_STATE);
        CASE_TOK_STR(PATCH_TOKEN_CONSTANT_BUFFER);
        CASE_TOK_STR(PATCH_TOKEN_SAMPLER_STATE_ARRAY);
        CASE_TOK_STR(PATCH_TOKEN_INTERFACE_DESCRIPTOR);
        CASE_TOK_STR(PATCH_TOKEN_VFE_STATE);
        CASE_TOK_STR(PATCH_TOKEN_BINDING_TABLE_STATE);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_SCRATCH_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_SIP_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_DATA_PARAMETER_BUFFER);
        CASE_TOK_STR(PATCH_TOKEN_MEDIA_VFE_STATE);
        CASE_TOK_STR(PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD);
        CASE_TOK_STR(PATCH_TOKEN_MEDIA_CURBE_LOAD);
        CASE_TOK_STR(PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA);
        CASE_TOK_STR(PATCH_TOKEN_THREAD_PAYLOAD);
        CASE_TOK_STR(PATCH_TOKEN_EXECUTION_ENVIRONMENT);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_PRIVATE_MEMORY);
        CASE_TOK_STR(PATCH_TOKEN_DATA_PARAMETER_STREAM);
        CASE_TOK_STR(PATCH_TOKEN_KERNEL_ARGUMENT_INFO);
        CASE_TOK_STR(PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO);
        CASE_TOK_STR(PATCH_TOKEN_STRING);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_PRINTF_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_CB_MAPPING);
        CASE_TOK_STR(PATCH_TOKEN_CB2CR_GATHER_TABLE);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_NULL_SURFACE_LOCATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION);
        CASE_TOK_STR(PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE);
        CASE_TOK_STR(PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT);
        CASE_TOK_STR(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO);
        CASE_TOK_STR(PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO);
        CASE_TOK_STR(PATCH_TOKEN_CONSTRUCTOR_DESTRUCTOR_KERNEL_PROGRAM_BINARY_INFO);
        CASE_TOK_STR(PATCH_TOKEN_INLINE_VME_SAMPLER_INFO);
        CASE_TOK_STR(PATCH_TOKEN_GTPIN_FREE_GRF_INFO);
        CASE_TOK_STR(PATCH_TOKEN_GTPIN_INFO);
        CASE_TOK_STR(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE);
        CASE_TOK_STR(PATCH_TOKEN_PROGRAM_RELOCATION_TABLE);
        CASE_TOK_STR(PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1);
    }
}

std::string asString(DATA_PARAMETER_TOKEN dataParameter) {
    switch (dataParameter) {
    default:
        return std::to_string(dataParameter);
        CASE_TOK_STR(DATA_PARAMETER_TOKEN_UNKNOWN);
        CASE_TOK_STR(DATA_PARAMETER_KERNEL_ARGUMENT);
        CASE_TOK_STR(DATA_PARAMETER_LOCAL_WORK_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_GLOBAL_WORK_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_NUM_WORK_GROUPS);
        CASE_TOK_STR(DATA_PARAMETER_WORK_DIMENSIONS);
        CASE_TOK_STR(DATA_PARAMETER_LOCAL_ID);
        CASE_TOK_STR(DATA_PARAMETER_EXECUTION_MASK);
        CASE_TOK_STR(DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_WIDTH);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_HEIGHT);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_DEPTH);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_CHANNEL_ORDER);
        CASE_TOK_STR(DATA_PARAMETER_FLAT_IMAGE_BASEOFFSET);
        CASE_TOK_STR(DATA_PARAMETER_FLAT_IMAGE_WIDTH);
        CASE_TOK_STR(DATA_PARAMETER_FLAT_IMAGE_HEIGHT);
        CASE_TOK_STR(DATA_PARAMETER_FLAT_IMAGE_PITCH);
        CASE_TOK_STR(DATA_PARAMETER_SAMPLER_ADDRESS_MODE);
        CASE_TOK_STR(DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS);
        CASE_TOK_STR(DATA_PARAMETER_GLOBAL_WORK_OFFSET);
        CASE_TOK_STR(DATA_PARAMETER_NUM_HARDWARE_THREADS);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_ARRAY_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_PRINTF_SURFACE_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_NUM_SAMPLES);
        CASE_TOK_STR(DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED);
        CASE_TOK_STR(DATA_PARAMETER_PARENT_EVENT);
        CASE_TOK_STR(DATA_PARAMETER_VME_MB_BLOCK_TYPE);
        CASE_TOK_STR(DATA_PARAMETER_VME_SUBPIXEL_MODE);
        CASE_TOK_STR(DATA_PARAMETER_VME_SAD_ADJUST_MODE);
        CASE_TOK_STR(DATA_PARAMETER_VME_SEARCH_PATH_TYPE);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS);
        CASE_TOK_STR(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_MAX_WORKGROUP_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE);
        CASE_TOK_STR(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS);
        CASE_TOK_STR(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_SIMD_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_OBJECT_ID);
        CASE_TOK_STR(DATA_PARAMETER_VME_IMAGE_TYPE);
        CASE_TOK_STR(DATA_PARAMETER_VME_MB_SKIP_BLOCK_TYPE);
        CASE_TOK_STR(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_IMAGE_SRGB_CHANNEL_ORDER);
        CASE_TOK_STR(DATA_PARAMETER_STAGE_IN_GRID_ORIGIN);
        CASE_TOK_STR(DATA_PARAMETER_STAGE_IN_GRID_SIZE);
        CASE_TOK_STR(DATA_PARAMETER_BUFFER_OFFSET);
        CASE_TOK_STR(DATA_PARAMETER_BUFFER_STATEFUL);
    }
}
#undef CASE_TOK_STR

void dump(const SProgramBinaryHeader &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SProgramBinaryHeader {\n";
    out << indent << "    uint32_t   Magic; // = " << value.Magic << "\n";
    out << indent << "    uint32_t   Version; // = " << value.Version << "\n";
    out << indent << "\n";
    out << indent << "    uint32_t   Device; // = " << value.Device << "\n";
    out << indent << "    uint32_t   GPUPointerSizeInBytes; // = " << value.GPUPointerSizeInBytes << "\n";
    out << indent << "\n";
    out << indent << "    uint32_t   NumberOfKernels; // = " << value.NumberOfKernels << "\n";
    out << indent << "\n";
    out << indent << "    uint32_t   SteppingId; // = " << value.SteppingId << "\n";
    out << indent << "\n";
    out << indent << "    uint32_t   PatchListSize; // = " << value.PatchListSize << "\n";
    out << indent << "};\n";
}

void dump(const SKernelBinaryHeader &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SKernelBinaryHeader {\n";
    out << indent << "    uint32_t   CheckSum;// = " << value.CheckSum << "\n";
    out << indent << "    uint64_t   ShaderHashCode;// = " << value.ShaderHashCode << "\n";
    out << indent << "    uint32_t   KernelNameSize;// = " << value.KernelNameSize << "\n";
    out << indent << "    uint32_t   PatchListSize;// = " << value.PatchListSize << "\n";
    out << indent << "};\n";
}

void dump(const SPatchDataParameterBuffer &value, std::stringstream &out, const std::string &indent);
void dump(const SPatchItemHeader &value, std::stringstream &out, const std::string &indent) {
    if (value.Token == iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER) {
        dump(static_cast<const SPatchDataParameterBuffer &>(value), out, indent);
        return;
    }
    out << indent << "struct SPatchItemHeader {\n";
    out << indent << "    uint32_t   Token;// = " << asString(static_cast<PATCH_TOKEN>(value.Token)) << "\n";
    out << indent << "    uint32_t   Size;// = " << value.Size << "\n";
    out << indent << "};\n";
}

void dumpPatchItemHeaderInline(const SPatchItemHeader &value, std::stringstream &out, const std::string &indent) {
    out << "Token=" << asString(static_cast<PATCH_TOKEN>(value.Token)) << ", Size=" << value.Size;
}

void dump(const SPatchGlobalMemoryObjectKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchGlobalMemoryObjectKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "}\n";
}

void dump(const SPatchImageMemoryObjectKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchImageMemoryObjectKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t  ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t  Type;// = " << value.Type << "\n";
    out << indent << "    uint32_t  Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t  LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t  LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t  Writeable;// = " << value.Writeable << "\n";
    out << indent << "    uint32_t  Transformable;// = " << value.Transformable << "\n";
    out << indent << "    uint32_t  needBindlessHandle;// = " << value.needBindlessHandle << "\n";
    out << indent << "    uint32_t  IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "    uint32_t  btiOffset;// = " << value.btiOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchSamplerKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchSamplerKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   Type;// = " << value.Type << "\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   needBindlessHandle;// = " << value.needBindlessHandle << "\n";
    out << indent << "    uint32_t   TextureMask;// = " << value.TextureMask << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "    uint32_t   btiOffset;// = " << value.btiOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchDataParameterBuffer &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchDataParameterBuffer :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Type;// = " << asString(static_cast<DATA_PARAMETER_TOKEN>(value.Type)) << "\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   DataSize;// = " << value.DataSize << "\n";
    out << indent << "    uint32_t   SourceOffset;// = " << value.SourceOffset << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "}\n";
}

void dump(const SPatchKernelArgumentInfo &value, std::stringstream &out, const std::string &indent) {
    auto toStr = [](ArrayRef<const char> &src) { return std::string(src.begin(), src.end()); };
    auto inlineData = getInlineData(&value);
    out << indent << "struct SPatchKernelArgumentInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t AddressQualifierSize;// = " << value.AddressQualifierSize << " : [" << toStr(inlineData.addressQualifier) << "]\n";
    out << indent << "    uint32_t AccessQualifierSize;// = " << value.AccessQualifierSize << " : [" << toStr(inlineData.accessQualifier) << "]\n";
    out << indent << "    uint32_t ArgumentNameSize;// = " << value.ArgumentNameSize << " : [" << toStr(inlineData.argName) << "]\n";
    out << indent << "    uint32_t TypeNameSize;// = " << value.TypeNameSize << " : [" << toStr(inlineData.typeName) << "]\n";
    out << indent << "    uint32_t TypeQualifierSize;// = " << value.TypeQualifierSize << " : [" << toStr(inlineData.typeQualifiers) << "]\n";
    out << indent << "}\n";
}

void dump(const SPatchKernelAttributesInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchKernelAttributesInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t AttributesSize;// = " << value.AttributesSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchMediaInterfaceDescriptorLoad &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchMediaInterfaceDescriptorLoad :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   InterfaceDescriptorDataOffset;// = " << value.InterfaceDescriptorDataOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchInterfaceDescriptorData &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchInterfaceDescriptorData :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   SamplerStateOffset;// = " << value.SamplerStateOffset << "\n";
    out << indent << "    uint32_t   KernelOffset;// = " << value.KernelOffset << "\n";
    out << indent << "    uint32_t   BindingTableOffset;// = " << value.BindingTableOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchDataParameterStream &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchDataParameterStream :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   DataParameterStreamSize;// = " << value.DataParameterStreamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchStateSIP &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchStateSIP :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   SystemKernelOffset;// = " << value.SystemKernelOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchSamplerStateArray &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchSamplerStateArray :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   Count;// = " << value.Count << "\n";
    out << indent << "    uint32_t   BorderColorOffset;// = " << value.BorderColorOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchBindingTableState &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchBindingTableState :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   Count;// = " << value.Count << "\n";
    out << indent << "    uint32_t   SurfaceStateOffset;// = " << value.SurfaceStateOffset << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateSystemThreadSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateSystemThreadSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   PerThreadSystemThreadSurfaceSize;// = " << value.PerThreadSystemThreadSurfaceSize << "\n";
    out << indent << "    uint32_t   BTI;// = " << value.BTI << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateLocalSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateLocalSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Offset;// = " << value.Offset << "\n";
    out << indent << "    uint32_t   TotalInlineLocalMemorySize;// = " << value.TotalInlineLocalMemorySize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchThreadPayload &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchThreadPayload :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t    HeaderPresent;// = " << value.HeaderPresent << "\n";
    out << indent << "    uint32_t    LocalIDXPresent;// = " << value.LocalIDXPresent << "\n";
    out << indent << "    uint32_t    LocalIDYPresent;// = " << value.LocalIDYPresent << "\n";
    out << indent << "    uint32_t    LocalIDZPresent;// = " << value.LocalIDZPresent << "\n";
    out << indent << "    uint32_t    LocalIDFlattenedPresent;// = " << value.LocalIDFlattenedPresent << "\n";
    out << indent << "    uint32_t    IndirectPayloadStorage;// = " << value.IndirectPayloadStorage << "\n";
    out << indent << "    uint32_t    UnusedPerThreadConstantPresent;// = " << value.UnusedPerThreadConstantPresent << "\n";
    out << indent << "    uint32_t    GetLocalIDPresent;// = " << value.GetLocalIDPresent << "\n";
    out << indent << "    uint32_t    GetGroupIDPresent;// = " << value.GetGroupIDPresent << "\n";
    out << indent << "    uint32_t    GetGlobalOffsetPresent;// = " << value.GetGlobalOffsetPresent << "\n";
    out << indent << "    uint32_t    StageInGridOriginPresent;// = " << value.StageInGridOriginPresent << "\n";
    out << indent << "    uint32_t    StageInGridSizePresent;// = " << value.StageInGridSizePresent << "\n";
    out << indent << "    uint32_t    OffsetToSkipPerThreadDataLoad;// = " << value.OffsetToSkipPerThreadDataLoad << "\n";
    out << indent << "    uint32_t    OffsetToSkipSetFFIDGP;// = " << value.OffsetToSkipSetFFIDGP << "\n";
    out << indent << "    uint32_t    PassInlineData;// = " << value.PassInlineData << "\n";
    out << indent << "}\n";
}

void dump(const SPatchExecutionEnvironment &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchExecutionEnvironment :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t    RequiredWorkGroupSizeX;// = " << value.RequiredWorkGroupSizeX << "\n";
    out << indent << "    uint32_t    RequiredWorkGroupSizeY;// = " << value.RequiredWorkGroupSizeY << "\n";
    out << indent << "    uint32_t    RequiredWorkGroupSizeZ;// = " << value.RequiredWorkGroupSizeZ << "\n";
    out << indent << "    uint32_t    LargestCompiledSIMDSize;// = " << value.LargestCompiledSIMDSize << "\n";
    out << indent << "    uint32_t    CompiledSubGroupsNumber;// = " << value.CompiledSubGroupsNumber << "\n";
    out << indent << "    uint32_t    HasBarriers;// = " << value.HasBarriers << "\n";
    out << indent << "    uint32_t    DisableMidThreadPreemption;// = " << value.DisableMidThreadPreemption << "\n";
    out << indent << "    uint32_t    CompiledSIMD8;// = " << value.CompiledSIMD8 << "\n";
    out << indent << "    uint32_t    CompiledSIMD16;// = " << value.CompiledSIMD16 << "\n";
    out << indent << "    uint32_t    CompiledSIMD32;// = " << value.CompiledSIMD32 << "\n";
    out << indent << "    uint32_t    HasDeviceEnqueue;// = " << value.HasDeviceEnqueue << "\n";
    out << indent << "    uint32_t    MayAccessUndeclaredResource;// = " << value.MayAccessUndeclaredResource << "\n";
    out << indent << "    uint32_t    UsesFencesForReadWriteImages;// = " << value.UsesFencesForReadWriteImages << "\n";
    out << indent << "    uint32_t    UsesStatelessSpillFill;// = " << value.UsesStatelessSpillFill << "\n";
    out << indent << "    uint32_t    UsesMultiScratchSpaces;// = " << value.UsesMultiScratchSpaces << "\n";
    out << indent << "    uint32_t    IsCoherent;// = " << value.IsCoherent << "\n";
    out << indent << "    uint32_t    IsInitializer;// = " << value.IsInitializer << "\n";
    out << indent << "    uint32_t    IsFinalizer;// = " << value.IsFinalizer << "\n";
    out << indent << "    uint32_t    SubgroupIndependentForwardProgressRequired;// = " << value.SubgroupIndependentForwardProgressRequired << "\n";
    out << indent << "    uint32_t    CompiledForGreaterThan4GBBuffers;// = " << value.CompiledForGreaterThan4GBBuffers << "\n";
    out << indent << "    uint32_t    NumGRFRequired;// = " << value.NumGRFRequired << "\n";
    out << indent << "    uint32_t    WorkgroupWalkOrderDims;// = " << value.WorkgroupWalkOrderDims << "\n";
    out << indent << "    uint32_t    HasGlobalAtomics;// = " << value.HasGlobalAtomics << "\n";
    out << indent << "}\n";
}

void dump(const SPatchString &value, std::stringstream &out, const std::string &indent) {
    const char *strBeg = reinterpret_cast<const char *>((&value) + 1);
    std::string strValue = std::string(strBeg, strBeg + value.StringSize);
    out << indent << "struct SPatchString :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Index;// = " << value.Index << "\n";
    out << indent << "    uint32_t   StringSize;// = " << value.StringSize << " : [" << strValue << "]"
        << "\n";
    out << indent << "}\n";
}

void dump(const SPatchStatelessGlobalMemoryObjectKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchStatelessGlobalMemoryObjectKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "}\n";
}

void dump(const SPatchStatelessConstantMemoryObjectKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchStatelessConstantMemoryObjectKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   GlobalBufferIndex;// = " << value.GlobalBufferIndex << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessConstantMemorySurfaceWithInitialization :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ConstantBufferIndex;// = " << value.ConstantBufferIndex << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   Type;// = " << value.Type << "\n";
    out << indent << "    uint32_t   GlobalBufferIndex;// = " << value.GlobalBufferIndex << "\n";
    out << indent << "    uint32_t   InlineDataSize;// = " << value.InlineDataSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateConstantMemorySurfaceProgramBinaryInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateConstantMemorySurfaceProgramBinaryInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ConstantBufferIndex;// = " << value.ConstantBufferIndex << "\n";
    out << indent << "    uint32_t   InlineDataSize;// = " << value.InlineDataSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchGlobalPointerProgramBinaryInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchGlobalPointerProgramBinaryInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   GlobalBufferIndex;// = " << value.GlobalBufferIndex << "\n";
    out << indent << "    uint64_t   GlobalPointerOffset;// = " << value.GlobalPointerOffset << "\n";
    out << indent << "    uint32_t   BufferType;// = " << value.BufferType << "\n";
    out << indent << "    uint32_t   BufferIndex;// = " << value.BufferIndex << "\n";
    out << indent << "}\n";
}

void dump(const SPatchConstantPointerProgramBinaryInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchConstantPointerProgramBinaryInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ConstantBufferIndex;// = " << value.ConstantBufferIndex << "\n";
    out << indent << "    uint64_t   ConstantPointerOffset;// = " << value.ConstantPointerOffset << "\n";
    out << indent << "    uint32_t   BufferType;// = " << value.BufferType << "\n";
    out << indent << "    uint32_t   BufferIndex;// = " << value.BufferIndex << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessPrintfSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessPrintfSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   PrintfSurfaceIndex;// = " << value.PrintfSurfaceIndex << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessPrivateSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessPrivateSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "    uint32_t   PerThreadPrivateMemorySize;// = " << value.PerThreadPrivateMemorySize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchMediaVFEState &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchMediaVFEState :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ScratchSpaceOffset;// = " << value.ScratchSpaceOffset << "\n";
    out << indent << "    uint32_t   PerThreadScratchSpace;// = " << value.PerThreadScratchSpace << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessEventPoolSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessEventPoolSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   EventPoolSurfaceIndex;// = " << value.EventPoolSurfaceIndex << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchAllocateStatelessDefaultDeviceQueueSurface &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchAllocateStatelessDefaultDeviceQueueSurface :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchStatelessDeviceQueueKernelArgument &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchStatelessDeviceQueueKernelArgument :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   ArgumentNumber;// = " << value.ArgumentNumber << "\n";
    out << indent << "    uint32_t   SurfaceStateHeapOffset;// = " << value.SurfaceStateHeapOffset << "\n";
    out << indent << "    uint32_t   DataParamOffset;// = " << value.DataParamOffset << "\n";
    out << indent << "    uint32_t   DataParamSize;// = " << value.DataParamSize << "\n";
    out << indent << "    uint32_t   LocationIndex;// = " << value.LocationIndex << "\n";
    out << indent << "    uint32_t   LocationIndex2;// = " << value.LocationIndex2 << "\n";
    out << indent << "    uint32_t   IsEmulationArgument;// = " << value.IsEmulationArgument << "\n";
    out << indent << "}\n";
}

void dump(const SPatchGtpinFreeGRFInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchGtpinFreeGRFInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   BufferSize;// = " << value.BufferSize << "\n";
    out << indent << "}\n";
}

void dump(const SPatchFunctionTableInfo &value, std::stringstream &out, const std::string &indent) {
    out << indent << "struct SPatchFunctionTableInfo :\n";
    out << indent << "       SPatchItemHeader (";
    dumpPatchItemHeaderInline(value, out, "");
    out << ")\n"
        << indent << "{\n";
    out << indent << "    uint32_t   NumEntries;// = " << value.NumEntries << "\n";
    out << indent << "}\n";
}

template <typename T>
void dumpOrNull(const T *value, const std::string &messageIfNull, std::stringstream &out, const std::string &indent) {
    if (value == nullptr) {
        if (messageIfNull.empty() == false) {
            out << indent << messageIfNull;
        }
        return;
    }
    dump(*value, out, indent);
}

template <typename T>
void dumpOrNullObjArg(const T *value, std::stringstream &out, const std::string &indent) {
    if (value == nullptr) {
        return;
    }
    switch (value->Token) {
    default:
        UNRECOVERABLE_IF(value->Token != PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT);
        dumpOrNull(reinterpret_cast<const SPatchSamplerKernelArgument *>(value), "", out, indent);
        break;
    case PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT:
        dumpOrNull(reinterpret_cast<const SPatchImageMemoryObjectKernelArgument *>(value), "", out, indent);
        break;
    case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        dumpOrNull(reinterpret_cast<const SPatchGlobalMemoryObjectKernelArgument *>(value), "", out, indent);
        break;
    case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
        dumpOrNull(reinterpret_cast<const SPatchStatelessGlobalMemoryObjectKernelArgument *>(value), "", out, indent);
        break;
    case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT:
        dumpOrNull(reinterpret_cast<const SPatchStatelessConstantMemoryObjectKernelArgument *>(value), "", out, indent);
        break;
    case PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT:
        dumpOrNull(reinterpret_cast<const SPatchStatelessDeviceQueueKernelArgument *>(value), "", out, indent);
        break;
    }
}

template <typename T, size_t Size>
void dumpOrNullArrayIfNotEmpty(T (&value)[Size], const std::string &arrayName, std::stringstream &out, const std::string &indent) {
    bool allEmpty = true;
    for (size_t i = 0; i < Size; ++i) {
        allEmpty = allEmpty && (value[i] == nullptr);
    }
    if (allEmpty) {
        return;
    }
    out << indent << arrayName << " [" << Size << "] :\n";
    for (size_t i = 0; i < Size; ++i) {
        if (value[i] == nullptr) {
            continue;
        }
        out << indent << " + [" << i << "]:\n";
        dump(*value[i], out, indent + " |  ");
    }
}

template <typename T>
void dumpVecIfNotEmpty(const T &vector, const std::string &vectorName, std::stringstream &out, const std::string &indent) {
    if (vector.size() == 0) {
        return;
    }
    out << indent << vectorName << " [" << vector.size() << "] :\n";
    for (size_t i = 0; i < vector.size(); ++i) {
        out << indent << " + [" << i << "]:\n";
        dumpOrNull(vector[i], "DECODER INTERNAL ERROR\n", out, indent + " |  ");
    }
}

std::string asString(const ProgramFromPatchtokens &prog) {
    std::stringstream stream;
    stream << "Program of size : " << prog.blobs.programInfo.size()
           << " " << asString(prog.decodeStatus) << "\n";
    dumpOrNull(prog.header, "WARNING : Program header is missing\n", stream, "");
    stream << "Program-scope tokens section size : " << prog.blobs.patchList.size() << "\n";
    dumpVecIfNotEmpty(prog.unhandledTokens, "WARNING : Unhandled program-scope tokens detected", stream, "  ");
    dumpVecIfNotEmpty(prog.programScopeTokens.allocateConstantMemorySurface, "Inline Costant Surface(s)", stream, "  ");
    dumpVecIfNotEmpty(prog.programScopeTokens.constantPointer, "Inline Costant Surface - self relocations", stream, "  ");
    dumpVecIfNotEmpty(prog.programScopeTokens.allocateGlobalMemorySurface, "Inline Global Variable Surface(s)", stream, "  ");
    dumpVecIfNotEmpty(prog.programScopeTokens.globalPointer, "Inline Global Variable Surface - self relocations", stream, "  ");
    dumpOrNull(prog.programScopeTokens.symbolTable, "", stream, "  ");
    stream << "Kernels section size : " << prog.blobs.kernelsInfo.size() << "\n";
    for (size_t i = 0; i < prog.kernels.size(); ++i) {
        stream << "kernel[" << i << "] " << (prog.kernels[i].name.size() > 0 ? std::string(prog.kernels[i].name.begin(), prog.kernels[i].name.end()).c_str() : "<UNNAMED>") << ":\n";
        stream << asString(prog.kernels[i]);
    }
    return stream.str();
}

std::string asString(const KernelFromPatchtokens &kern) {
    std::stringstream stream;
    std::string indentLevel1 = "  ";
    stream << "Kernel of size : " << kern.blobs.kernelInfo.size() << " "
           << " " << asString(kern.decodeStatus) << "\n";
    dumpOrNull(kern.header, "WARNING : Kernel header is missing\n", stream, "");
    stream << "Kernel-scope tokens section size : " << kern.blobs.patchList.size() << "\n";
    dumpVecIfNotEmpty(kern.unhandledTokens, "WARNING : Unhandled kernel-scope tokens detected", stream, indentLevel1);
    dumpOrNull(kern.tokens.executionEnvironment, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.threadPayload, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.samplerStateArray, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.bindingTableState, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateLocalSurface, "", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.mediaVfeState, "mediaVfeState", stream, indentLevel1);
    dumpOrNull(kern.tokens.mediaInterfaceDescriptorLoad, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.interfaceDescriptorData, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.kernelAttributesInfo, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessPrivateSurface, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessConstantMemorySurfaceWithInitialization, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessPrintfSurface, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessEventPoolSurface, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateStatelessDefaultDeviceQueueSurface, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.inlineVmeSamplerInfo, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.gtpinFreeGrfInfo, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.stateSip, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.allocateSystemThreadSurface, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.gtpinInfo, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.programSymbolTable, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.programRelocationTable, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.dataParameterStream, "", stream, indentLevel1);
    dumpVecIfNotEmpty(kern.tokens.strings, "String literals", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.localWorkSize, "localWorkSize", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.localWorkSize2, "localWorkSize2", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize, "enqueuedLocalWorkSize", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.numWorkGroups, "numWorkGroups", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.globalWorkOffset, "globalWorkOffset", stream, indentLevel1);
    dumpOrNullArrayIfNotEmpty(kern.tokens.crossThreadPayloadArgs.globalWorkSize, "globalWorkSize", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.maxWorkGroupSize, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.workDimensions, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.simdSize, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.parentEvent, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress, "", stream, indentLevel1);
    dumpOrNull(kern.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple, "", stream, indentLevel1);
    dumpVecIfNotEmpty(kern.tokens.crossThreadPayloadArgs.childBlockSimdSize, "Child block simd size(s)", stream, indentLevel1);

    if (kern.tokens.kernelArgs.size() != 0) {
        stream << "Kernel arguments [" << kern.tokens.kernelArgs.size() << "] :\n";
        for (size_t i = 0; i < kern.tokens.kernelArgs.size(); ++i) {
            stream << "  + kernelArg[" << i << "]:\n";
            stream << asString(kern.tokens.kernelArgs[i], indentLevel1 + "| ");
        }
    }
    return stream.str();
}

std::string asString(ArgObjectType type, ArgObjectTypeSpecialized typeSpecialized) {
    std::string typeAsStr;
    switch (type) {
    default:
        UNRECOVERABLE_IF(ArgObjectType::None != type);
        return "unspecified";
    case ArgObjectType::Buffer:
        typeAsStr = "BUFFER";
        break;
    case ArgObjectType::Image:
        typeAsStr = "IMAGE";
        break;
    case ArgObjectType::Sampler:
        typeAsStr = "SAMPLER";
        break;
    case ArgObjectType::Slm:
        typeAsStr = "SLM";
        break;
    }

    switch (typeSpecialized) {
    default:
        UNRECOVERABLE_IF(ArgObjectTypeSpecialized::None != typeSpecialized);
        break;
    case ArgObjectTypeSpecialized::Vme:
        typeAsStr += " [ VME ]";
    }

    return typeAsStr;
}

std::string asString(const KernelArgFromPatchtokens &arg, const std::string &indent) {
    std::stringstream stream;
    stream << indent << "Kernel argument of type " << asString(arg.objectType, arg.objectTypeSpecialized) << "\n";
    std::string indentLevel1 = indent + "  ";
    std::string indentLevel2 = indentLevel1 + "  ";
    dumpOrNull(arg.argInfo, "", stream, indentLevel1);
    dumpOrNullObjArg(arg.objectArg, stream, indentLevel1);
    dumpOrNull(arg.objectId, "", stream, indentLevel1);
    switch (arg.objectType) {
    default:
        break;
    case ArgObjectType::Buffer:
        stream << indentLevel1 << "Buffer Metadata:\n";
        dumpOrNull(arg.metadata.buffer.bufferOffset, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.buffer.pureStateful, "", stream, indentLevel2);
        break;
    case ArgObjectType::Image:
        stream << indentLevel1 << "Image Metadata:\n";
        dumpOrNull(arg.metadata.image.width, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.height, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.depth, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.channelDataType, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.channelOrder, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.arraySize, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.numSamples, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.numMipLevels, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.flatBaseOffset, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.flatWidth, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.flatHeight, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.image.flatPitch, "", stream, indentLevel2);
        break;
    case ArgObjectType::Sampler:
        stream << indentLevel1 << "Sampler Metadata:\n";
        dumpOrNull(arg.metadata.sampler.addressMode, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.sampler.coordinateSnapWaRequired, "", stream, indentLevel2);
        dumpOrNull(arg.metadata.sampler.normalizedCoords, "", stream, indentLevel2);
        break;
    case ArgObjectType::Slm:
        stream << indentLevel1 << "Slm Metadata:\n";
        dumpOrNull(arg.metadata.slm.token, "", stream, indentLevel2);
        break;
    }
    switch (arg.objectTypeSpecialized) {
    default:
        break;
    case ArgObjectTypeSpecialized::Vme:
        stream << indentLevel1 << "Vme Metadata:\n";
        dumpOrNull(arg.metadataSpecialized.vme.mbBlockType, "", stream, indentLevel2);
        dumpOrNull(arg.metadataSpecialized.vme.sadAdjustMode, "", stream, indentLevel2);
        dumpOrNull(arg.metadataSpecialized.vme.searchPathType, "", stream, indentLevel2);
        dumpOrNull(arg.metadataSpecialized.vme.subpixelMode, "", stream, indentLevel2);
        break;
    }

    dumpVecIfNotEmpty(arg.byValMap, "  Data passed by value ", stream, indentLevel1);
    return stream.str();
}

} // namespace PatchTokenBinary

} // namespace NEO
