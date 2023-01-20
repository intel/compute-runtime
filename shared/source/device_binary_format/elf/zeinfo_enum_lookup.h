/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/utilities/lookup_array.h"

namespace NEO::Zebin::ZeInfo::EnumLookup {
using namespace Elf::ZebinKernelMetadata;

namespace ArgType {
using namespace Tags::Kernel::PayloadArgument::ArgType;
using namespace Tags::Kernel::PerThreadPayloadArgument::ArgType;
using namespace Tags::Kernel::PayloadArgument::ArgType::Image;
using namespace Tags::Kernel::PayloadArgument::ArgType::Sampler;
using namespace Tags::Kernel::PayloadArgument::ArgType::Sampler::Vme;
using ArgType = Types::Kernel::ArgType;

inline constexpr ConstStringRef name = "argument type";
inline constexpr LookupArray<ConstStringRef, ArgType, 38> lookup({{{packedLocalIds, ArgType::ArgTypePackedLocalIds},
                                                                   {localId, ArgType::ArgTypeLocalId},
                                                                   {localSize, ArgType::ArgTypeLocalSize},
                                                                   {groupCount, ArgType::ArgTypeGroupCount},
                                                                   {globalSize, ArgType::ArgTypeGlobalSize},
                                                                   {enqueuedLocalSize, ArgType::ArgTypeEnqueuedLocalSize},
                                                                   {globalIdOffset, ArgType::ArgTypeGlobalIdOffset},
                                                                   {privateBaseStateless, ArgType::ArgTypePrivateBaseStateless},
                                                                   {argByvalue, ArgType::ArgTypeArgByvalue},
                                                                   {argBypointer, ArgType::ArgTypeArgBypointer},
                                                                   {bufferAddress, ArgType::ArgTypeBufferAddress},
                                                                   {bufferOffset, ArgType::ArgTypeBufferOffset},
                                                                   {printfBuffer, ArgType::ArgTypePrintfBuffer},
                                                                   {workDimensions, ArgType::ArgTypeWorkDimensions},
                                                                   {implicitArgBuffer, ArgType::ArgTypeImplicitArgBuffer},
                                                                   {width, ArgType::ArgTypeImageWidth},
                                                                   {height, ArgType::ArgTypeImageHeight},
                                                                   {depth, ArgType::ArgTypeImageDepth},
                                                                   {channelDataType, ArgType::ArgTypeImageChannelDataType},
                                                                   {channelOrder, ArgType::ArgTypeImageChannelOrder},
                                                                   {arraySize, ArgType::ArgTypeImageArraySize},
                                                                   {numSamples, ArgType::ArgTypeImageNumSamples},
                                                                   {numMipLevels, ArgType::ArgTypeImageMipLevels},
                                                                   {flatBaseOffset, ArgType::ArgTypeImageFlatBaseOffset},
                                                                   {flatWidth, ArgType::ArgTypeImageFlatWidth},
                                                                   {flatHeight, ArgType::ArgTypeImageFlatHeight},
                                                                   {flatPitch, ArgType::ArgTypeImageFlatPitch},
                                                                   {snapWa, ArgType::ArgTypeSamplerSnapWa},
                                                                   {normCoords, ArgType::ArgTypeSamplerNormCoords},
                                                                   {addrMode, ArgType::ArgTypeSamplerAddrMode},
                                                                   {blockType, ArgType::ArgTypeVmeMbBlockType},
                                                                   {subpixelMode, ArgType::ArgTypeVmeSubpixelMode},
                                                                   {sadAdjustMode, ArgType::ArgTypeVmeSadAdjustMode},
                                                                   {searchPathType, ArgType::ArgTypeVmeSearchPathType},
                                                                   {syncBuffer, ArgType::ArgTypeSyncBuffer},
                                                                   {rtGlobalBuffer, ArgType::ArgTypeRtGlobalBuffer},
                                                                   {dataConstBuffer, ArgType::ArgTypeDataConstBuffer},
                                                                   {dataGlobalBuffer, ArgType::ArgTypeDataGlobalBuffer}}});
static_assert(lookup.size() == ArgType::ArgTypeMax - 1, "Every enum field must be present");
} // namespace ArgType

namespace MemoryAddressingMode {
namespace AddrModeTag = Tags::Kernel::PayloadArgument::MemoryAddressingMode;
using AddrMode = Types::Kernel::PayloadArgument::MemoryAddressingMode;
inline constexpr LookupArray<ConstStringRef, AddrMode, 4> lookup({{{AddrModeTag::stateless, AddrMode::MemoryAddressingModeStateless},
                                                                   {AddrModeTag::stateful, AddrMode::MemoryAddressingModeStateful},
                                                                   {AddrModeTag::bindless, AddrMode::MemoryAddressingModeBindless},
                                                                   {AddrModeTag::sharedLocalMemory, AddrMode::MemoryAddressingModeSharedLocalMemory}}});
inline constexpr ConstStringRef name = "addressing mode";
static_assert(lookup.size() == AddrMode::MemoryAddressIngModeMax - 1, "Every enum field must be present");
} // namespace MemoryAddressingMode

namespace AddressSpace {
using namespace Tags::Kernel::PayloadArgument::AddrSpace;
using AddrSpace = Types::Kernel::PayloadArgument::AddressSpace;

inline constexpr ConstStringRef name = "address space";
inline constexpr LookupArray<ConstStringRef, AddrSpace, 5> lookup({{{global, AddrSpace::AddressSpaceGlobal},
                                                                    {local, AddrSpace::AddressSpaceLocal},
                                                                    {constant, AddrSpace::AddressSpaceConstant},
                                                                    {image, AddrSpace::AddressSpaceImage},
                                                                    {sampler, AddrSpace::AddressSpaceSampler}}});
static_assert(lookup.size() == AddrSpace::AddressSpaceMax - 1, "Every enum field must be present");
} // namespace AddressSpace

namespace AccessType {
using namespace Tags::Kernel::PayloadArgument::AccessType;
using AccessType = Types::Kernel::PayloadArgument::AccessType;

inline constexpr ConstStringRef name = "access type";
inline constexpr LookupArray<ConstStringRef, AccessType, 3> lookup({{{readonly, AccessType::AccessTypeReadonly},
                                                                     {writeonly, AccessType::AccessTypeWriteonly},
                                                                     {readwrite, AccessType::AccessTypeReadwrite}}});
static_assert(lookup.size() == AccessType::AccessTypeMax - 1, "Every enum field must be present");
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
using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;
using MemoryUsage = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
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
inline constexpr LookupArray<ConstStringRef, ImageType, 16> lookup({{{imageTypeBuffer, ImageType::ImageTypeBuffer},
                                                                     {imageType1D, ImageType::ImageType1D},
                                                                     {imageType1DArray, ImageType::ImageType1DArray},
                                                                     {imageType2D, ImageType::ImageType2D},
                                                                     {imageType2DArray, ImageType::ImageType2DArray},
                                                                     {imageType3D, ImageType::ImageType3D},
                                                                     {imageTypeCube, ImageType::ImageTypeCube},
                                                                     {imageTypeCubeArray, ImageType::ImageTypeCubeArray},
                                                                     {imageType2DDepth, ImageType::ImageType2DDepth},
                                                                     {imageType2DArrayDepth, ImageType::ImageType2DArrayDepth},
                                                                     {imageType2DMSAA, ImageType::ImageType2DMSAA},
                                                                     {imageType2DMSAADepth, ImageType::ImageType2DMSAADepth},
                                                                     {imageType2DArrayMSAA, ImageType::ImageType2DArrayMSAA},
                                                                     {imageType2DArrayMSAADepth, ImageType::ImageType2DArrayMSAADepth},
                                                                     {imageType2DMedia, ImageType::ImageType2DMedia},
                                                                     {imageType2DMediaBlock, ImageType::ImageType2DMediaBlock}}});
static_assert(lookup.size() == ImageType::ImageTypeMax - 1, "Every enum field must be present");
} // namespace ImageType

namespace SamplerType {
using namespace Tags::Kernel::PayloadArgument::SamplerType;
using SamplerType = Types::Kernel::PayloadArgument::SamplerType;
inline constexpr ConstStringRef name = "sampler type";
inline constexpr LookupArray<ConstStringRef, SamplerType, 12> lookup({{{samplerTypeTexture, SamplerType::SamplerTypeTexture},
                                                                       {samplerType8x8, SamplerType::SamplerType8x8},
                                                                       {samplerType2DConsolve8x8, SamplerType::SamplerType2DConvolve8x8},
                                                                       {samplerTypeErode8x8, SamplerType::SamplerTypeErode8x8},
                                                                       {samplerTypeDilate8x8, SamplerType::SamplerTypeDilate8x8},
                                                                       {samplerTypeMinMaxFilter8x8, SamplerType::SamplerTypeMinMaxFilter8x8},
                                                                       {samplerTypeCentroid8x8, SamplerType::SamplerTypeBoolCentroid8x8},
                                                                       {samplerTypeBoolCentroid8x8, SamplerType::SamplerTypeBoolCentroid8x8},
                                                                       {samplerTypeBoolSum8x8, SamplerType::SamplerTypeBoolSum8x8},
                                                                       {samplerTypeVME, SamplerType::SamplerTypeVME},
                                                                       {samplerTypeVE, SamplerType::SamplerTypeVE},
                                                                       {samplerTypeVD, SamplerType::SamplerTypeVD}}});
static_assert(lookup.size() == SamplerType::SamplerTypeMax - 1, "Every enum field must be present");
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
inline constexpr LookupArray<ConstStringRef, AddrMode, 5> lookup({{{none, AddrMode::None},
                                                                   {repeat, AddrMode::Repeat},
                                                                   {clamp_edge, AddrMode::ClampEdge},
                                                                   {clamp_border, AddrMode::ClampBorder},
                                                                   {mirror, AddrMode::Mirror}}});
static_assert(lookup.size() == static_cast<size_t>(AddrMode::Max) - 1, "Every enum field must be present");
} // namespace InlineSamplerAddrMode

namespace InlineSamplerFilterMode {
using namespace Tags::Kernel::InlineSamplers::FilterMode;
using FilterMode = Types::Kernel::InlineSamplers::FilterMode;
inline constexpr ConstStringRef name = "inline sampler filter mode";
inline constexpr LookupArray<ConstStringRef, FilterMode, 2> lookup({{{nearest, FilterMode::Nearest},
                                                                     {linear, FilterMode::Linear}}});
static_assert(lookup.size() == FilterMode::Max - 1, "Every enum field must be present");
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