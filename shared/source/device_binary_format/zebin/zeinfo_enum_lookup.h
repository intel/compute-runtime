/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/zebin/zeinfo.h"
#include "shared/source/utilities/lookup_array.h"

namespace NEO::Zebin::ZeInfo::EnumLookup {
using namespace NEO::Zebin::ZeInfo;

namespace ArgType {
using namespace Tags::Kernel::PayloadArgument::ArgType;
using namespace Tags::Kernel::PerThreadPayloadArgument::ArgType;
using namespace Tags::Kernel::PayloadArgument::ArgType::Image;
using namespace Tags::Kernel::PayloadArgument::ArgType::Sampler;
using namespace Tags::Kernel::PayloadArgument::ArgType::Sampler::Vme;
using ArgType = Types::Kernel::ArgType;

inline constexpr ConstStringRef name = "argument type";
inline constexpr LookupArray<ConstStringRef, ArgType, 47> lookup({{
    {packedLocalIds, ArgType::argTypePackedLocalIds},
    {localId, ArgType::argTypeLocalId},
    {localSize, ArgType::argTypeLocalSize},
    {groupCount, ArgType::argTypeGroupCount},
    {globalSize, ArgType::argTypeGlobalSize},
    {enqueuedLocalSize, ArgType::argTypeEnqueuedLocalSize},
    {globalIdOffset, ArgType::argTypeGlobalIdOffset},
    {privateBaseStateless, ArgType::argTypePrivateBaseStateless},
    {argByvalue, ArgType::argTypeArgByvalue},
    {argBypointer, ArgType::argTypeArgBypointer},
    {bufferAddress, ArgType::argTypeBufferAddress},
    {bufferOffset, ArgType::argTypeBufferOffset},
    {printfBuffer, ArgType::argTypePrintfBuffer},
    {workDimensions, ArgType::argTypeWorkDimensions},
    {implicitArgBuffer, ArgType::argTypeImplicitArgBuffer},
    {width, ArgType::argTypeImageWidth},
    {height, ArgType::argTypeImageHeight},
    {depth, ArgType::argTypeImageDepth},
    {channelDataType, ArgType::argTypeImageChannelDataType},
    {channelOrder, ArgType::argTypeImageChannelOrder},
    {arraySize, ArgType::argTypeImageArraySize},
    {numSamples, ArgType::argTypeImageNumSamples},
    {numMipLevels, ArgType::argTypeImageMipLevels},
    {flatBaseOffset, ArgType::argTypeImageFlatBaseOffset},
    {flatWidth, ArgType::argTypeImageFlatWidth},
    {flatHeight, ArgType::argTypeImageFlatHeight},
    {flatPitch, ArgType::argTypeImageFlatPitch},
    {snapWa, ArgType::argTypeSamplerSnapWa},
    {normCoords, ArgType::argTypeSamplerNormCoords},
    {addrMode, ArgType::argTypeSamplerAddrMode},
    {blockType, ArgType::argTypeVmeMbBlockType},
    {subpixelMode, ArgType::argTypeVmeSubpixelMode},
    {sadAdjustMode, ArgType::argTypeVmeSadAdjustMode},
    {searchPathType, ArgType::argTypeVmeSearchPathType},
    {syncBuffer, ArgType::argTypeSyncBuffer},
    {rtGlobalBuffer, ArgType::argTypeRtGlobalBuffer},
    {dataConstBuffer, ArgType::argTypeDataConstBuffer},
    {dataGlobalBuffer, ArgType::argTypeDataGlobalBuffer},
    {assertBuffer, ArgType::argTypeAssertBuffer},
    {indirectDataPointer, ArgType::argTypeIndirectDataPointer},
    {scratchPointer, ArgType::argTypeScratchPointer},
    {regionGroupSize, ArgType::argTypeRegionGroupSize},
    {regionGroupDimension, ArgType::argTypeRegionGroupDimension},
    {regionGroupWgCount, ArgType::argTypeRegionGroupWgCount},
    {regionGroupBarrierBuffer, ArgType::argTypeRegionGroupBarrierBuffer},
    {inlineSampler, ArgType::argTypeInlineSampler},
    {bufferSize, ArgType::argTypeBufferSize},
}});
static_assert(lookup.size() == ArgType::argTypeMax - 1, "Every enum field must be present");
} // namespace ArgType

namespace MemoryAddressingMode {
namespace AddrModeTag = Tags::Kernel::PayloadArgument::MemoryAddressingMode;
using AddrMode = Types::Kernel::PayloadArgument::MemoryAddressingMode;
inline constexpr LookupArray<ConstStringRef, AddrMode, 4> lookup({{{AddrModeTag::stateless, AddrMode::memoryAddressingModeStateless},
                                                                   {AddrModeTag::stateful, AddrMode::memoryAddressingModeStateful},
                                                                   {AddrModeTag::bindless, AddrMode::memoryAddressingModeBindless},
                                                                   {AddrModeTag::sharedLocalMemory, AddrMode::memoryAddressingModeSharedLocalMemory}}});
inline constexpr ConstStringRef name = "addressing mode";
static_assert(lookup.size() == AddrMode::memoryAddressIngModeMax - 1, "Every enum field must be present");
} // namespace MemoryAddressingMode

namespace AddressSpace {
using namespace Tags::Kernel::PayloadArgument::AddrSpace;
using AddrSpace = Types::Kernel::PayloadArgument::AddressSpace;

inline constexpr ConstStringRef name = "address space";
inline constexpr LookupArray<ConstStringRef, AddrSpace, 5> lookup({{{global, AddrSpace::addressSpaceGlobal},
                                                                    {local, AddrSpace::addressSpaceLocal},
                                                                    {constant, AddrSpace::addressSpaceConstant},
                                                                    {image, AddrSpace::addressSpaceImage},
                                                                    {sampler, AddrSpace::addressSpaceSampler}}});
static_assert(lookup.size() == AddrSpace::addressSpaceMax - 1, "Every enum field must be present");
} // namespace AddressSpace

namespace AccessType {
using namespace Tags::Kernel::PayloadArgument::AccessType;
using AccessType = Types::Kernel::PayloadArgument::AccessType;

inline constexpr ConstStringRef name = "access type";
inline constexpr LookupArray<ConstStringRef, AccessType, 3> lookup({{{readonly, AccessType::accessTypeReadonly},
                                                                     {writeonly, AccessType::accessTypeWriteonly},
                                                                     {readwrite, AccessType::accessTypeReadwrite}}});
static_assert(lookup.size() == AccessType::accessTypeMax - 1, "Every enum field must be present");
} // namespace AccessType

namespace AllocationType {
using namespace Tags::Kernel::PerThreadMemoryBuffer::AllocationType;
using AllocType = Types::Kernel::PerThreadMemoryBuffer::AllocationType;
inline constexpr ConstStringRef name = "allocation type";
inline constexpr LookupArray<ConstStringRef, AllocType, 3> lookup({{{global, AllocType::AllocationTypeGlobal},
                                                                    {scratch, AllocType::AllocationTypeScratch},
                                                                    {slm, AllocType::AllocationTypeSlm}}});
static_assert(lookup.size() == AllocType::AllocationTypeMax - 1, "Every enum field must be present");
} // namespace AllocationType

namespace MemoryUsage {
using namespace Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;
using MemoryUsage = Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
inline constexpr ConstStringRef name = "memory usage";
inline constexpr LookupArray<ConstStringRef, MemoryUsage, 3> lookup({{{privateSpace, MemoryUsage::MemoryUsagePrivateSpace},
                                                                      {spillFillSpace, MemoryUsage::MemoryUsageSpillFillSpace},
                                                                      {singleSpace, MemoryUsage::MemoryUsageSingleSpace}}});
static_assert(lookup.size() == MemoryUsage::MemoryUsageMax - 1, "Every enum field must be present");
} // namespace MemoryUsage

namespace ImageType {
using namespace Tags::Kernel::PayloadArgument::ImageType;
using ImageType = Types::Kernel::PayloadArgument::ImageType;
inline constexpr ConstStringRef name = "image type";
inline constexpr LookupArray<ConstStringRef, ImageType, 16> lookup({{{imageTypeBuffer, ImageType::imageTypeBuffer},
                                                                     {imageType1D, ImageType::imageType1D},
                                                                     {imageType1DArray, ImageType::imageType1DArray},
                                                                     {imageType2D, ImageType::imageType2D},
                                                                     {imageType2DArray, ImageType::imageType2DArray},
                                                                     {imageType3D, ImageType::imageType3D},
                                                                     {imageTypeCube, ImageType::imageTypeCube},
                                                                     {imageTypeCubeArray, ImageType::imageTypeCubeArray},
                                                                     {imageType2DDepth, ImageType::imageType2DDepth},
                                                                     {imageType2DArrayDepth, ImageType::imageType2DArrayDepth},
                                                                     {imageType2DMSAA, ImageType::imageType2DMSAA},
                                                                     {imageType2DMSAADepth, ImageType::imageType2DMSAADepth},
                                                                     {imageType2DArrayMSAA, ImageType::imageType2DArrayMSAA},
                                                                     {imageType2DArrayMSAADepth, ImageType::imageType2DArrayMSAADepth},
                                                                     {imageType2DMedia, ImageType::imageType2DMedia},
                                                                     {imageType2DMediaBlock, ImageType::imageType2DMediaBlock}}});
static_assert(lookup.size() == ImageType::imageTypeMax - 1, "Every enum field must be present");
} // namespace ImageType

namespace SamplerType {
using namespace Tags::Kernel::PayloadArgument::SamplerType;
using SamplerType = Types::Kernel::PayloadArgument::SamplerType;
inline constexpr ConstStringRef name = "sampler type";
inline constexpr LookupArray<ConstStringRef, SamplerType, 12> lookup({{{samplerTypeTexture, SamplerType::samplerTypeTexture},
                                                                       {samplerType8x8, SamplerType::samplerType8x8},
                                                                       {samplerType2DConsolve8x8, SamplerType::samplerType2DConvolve8x8},
                                                                       {samplerTypeErode8x8, SamplerType::samplerTypeErode8x8},
                                                                       {samplerTypeDilate8x8, SamplerType::samplerTypeDilate8x8},
                                                                       {samplerTypeMinMaxFilter8x8, SamplerType::samplerTypeMinMaxFilter8x8},
                                                                       {samplerTypeCentroid8x8, SamplerType::samplerTypeBoolCentroid8x8},
                                                                       {samplerTypeBoolCentroid8x8, SamplerType::samplerTypeBoolCentroid8x8},
                                                                       {samplerTypeBoolSum8x8, SamplerType::samplerTypeBoolSum8x8},
                                                                       {samplerTypeVME, SamplerType::samplerTypeVME},
                                                                       {samplerTypeVE, SamplerType::samplerTypeVE},
                                                                       {samplerTypeVD, SamplerType::samplerTypeVD}}});
static_assert(lookup.size() == SamplerType::samplerTypeMax - 1, "Every enum field must be present");
} // namespace SamplerType

namespace ThreadSchedulingMode {
using namespace Tags::Kernel::ExecutionEnv::ThreadSchedulingMode;
using ThreadSchedulingMode = Types::Kernel::ExecutionEnv::ThreadSchedulingMode;
inline constexpr ConstStringRef name = "thread scheduling mode";
inline constexpr LookupArray<ConstStringRef, ThreadSchedulingMode, 3> lookup({{{ageBased, ThreadSchedulingMode::ThreadSchedulingModeAgeBased},
                                                                               {roundRobin, ThreadSchedulingMode::ThreadSchedulingModeRoundRobin},
                                                                               {roundRobinStall, ThreadSchedulingMode::ThreadSchedulingModeRoundRobinStall}}});
static_assert(lookup.size() == ThreadSchedulingMode::ThreadSchedulingModeMax - 1, "Every enum field must be present");
} // namespace ThreadSchedulingMode

namespace InlineSamplerAddrMode {
using namespace Tags::Kernel::InlineSamplers::AddrMode;
using AddrMode = Types::Kernel::InlineSamplers::AddrMode;
inline constexpr ConstStringRef name = "inline sampler addressing mode";
inline constexpr LookupArray<ConstStringRef, AddrMode, 5> lookup({{{none, AddrMode::none},
                                                                   {repeat, AddrMode::repeat},
                                                                   {clampEdge, AddrMode::clampEdge},
                                                                   {clampBorder, AddrMode::clampBorder},
                                                                   {mirror, AddrMode::mirror}}});
static_assert(lookup.size() == static_cast<size_t>(AddrMode::max) - 1, "Every enum field must be present");
} // namespace InlineSamplerAddrMode

namespace InlineSamplerFilterMode {
using namespace Tags::Kernel::InlineSamplers::FilterMode;
using FilterMode = Types::Kernel::InlineSamplers::FilterMode;
inline constexpr ConstStringRef name = "inline sampler filter mode";
inline constexpr LookupArray<ConstStringRef, FilterMode, 2> lookup({{{nearest, FilterMode::nearest},
                                                                     {linear, FilterMode::linear}}});
static_assert(lookup.size() == FilterMode::max - 1, "Every enum field must be present");
} // namespace InlineSamplerFilterMode

template <typename T>
struct EnumLooker {};

template <>
struct EnumLooker<Types::Kernel::ArgType> {
    static constexpr ConstStringRef name = ArgType::name;
    static constexpr auto members = ArgType::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PayloadArgument::MemoryAddressingMode> {
    static constexpr ConstStringRef name = MemoryAddressingMode::name;
    static constexpr auto members = MemoryAddressingMode::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PayloadArgument::AddressSpace> {
    static constexpr ConstStringRef name = AddressSpace::name;
    static constexpr auto members = AddressSpace::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PayloadArgument::AccessType> {
    static constexpr ConstStringRef name = AccessType::name;
    static constexpr auto members = AccessType::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PerThreadMemoryBuffer::AllocationType> {
    static constexpr ConstStringRef name = AllocationType::name;
    static constexpr auto members = AllocationType::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PerThreadMemoryBuffer::MemoryUsage> {
    static constexpr ConstStringRef name = MemoryUsage::name;
    static constexpr auto members = MemoryUsage::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PayloadArgument::ImageType> {
    static constexpr ConstStringRef name = ImageType::name;
    static constexpr auto members = ImageType::lookup;
};

template <>
struct EnumLooker<Types::Kernel::PayloadArgument::SamplerType> {
    static constexpr ConstStringRef name = SamplerType::name;
    static constexpr auto members = SamplerType::lookup;
};

template <>
struct EnumLooker<Types::Kernel::ExecutionEnv::ThreadSchedulingMode> {
    static constexpr ConstStringRef name = ThreadSchedulingMode::name;
    static constexpr auto members = ThreadSchedulingMode::lookup;
};

template <>
struct EnumLooker<Types::Kernel::InlineSamplers::AddrMode> {
    static constexpr ConstStringRef name = InlineSamplerAddrMode::name;
    static constexpr auto members = InlineSamplerAddrMode::lookup;
};

template <>
struct EnumLooker<Types::Kernel::InlineSamplers::FilterMode> {
    static constexpr ConstStringRef name = InlineSamplerFilterMode::name;
    static constexpr auto members = InlineSamplerFilterMode::lookup;
};
} // namespace NEO::Zebin::ZeInfo::EnumLookup
