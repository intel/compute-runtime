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
using ArgType = Types::Kernel::ArgType;

static constexpr ConstStringRef name = "argument type";
static constexpr LookupArray<ConstStringRef, ArgType, 27> lookup({{{packedLocalIds, ArgType::ArgTypePackedLocalIds},
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
                                                                   {flatPitch, ArgType::ArgTypeImageFlatPitch}}});
static_assert(lookup.size() == ArgType::ArgTypeMax - 1, "Every enum field must be present");
} // namespace ArgType
namespace MemoryAddressingMode {
namespace AddrModeTag = Tags::Kernel::PayloadArgument::MemoryAddressingMode;
using AddrMode = Types::Kernel::PayloadArgument::MemoryAddressingMode;
static constexpr LookupArray<ConstStringRef, AddrMode, 4> lookup({{{AddrModeTag::stateless, AddrMode::MemoryAddressingModeStateless},
                                                                   {AddrModeTag::stateful, AddrMode::MemoryAddressingModeStateful},
                                                                   {AddrModeTag::bindless, AddrMode::MemoryAddressingModeBindless},
                                                                   {AddrModeTag::sharedLocalMemory, AddrMode::MemoryAddressingModeSharedLocalMemory}}});
static constexpr ConstStringRef name = "addressing mode";
static_assert(lookup.size() == AddrMode::MemoryAddressIngModeMax - 1, "Every enum field must be present");
} // namespace MemoryAddressingMode
namespace AddressSpace {
using namespace Tags::Kernel::PayloadArgument::AddrSpace;
using AddrSpace = Types::Kernel::PayloadArgument::AddressSpace;

static constexpr ConstStringRef name = "address space";
static constexpr LookupArray<ConstStringRef, AddrSpace, 5> lookup({{{global, AddrSpace::AddressSpaceGlobal},
                                                                    {local, AddrSpace::AddressSpaceLocal},
                                                                    {constant, AddrSpace::AddressSpaceConstant},
                                                                    {image, AddrSpace::AddressSpaceImage},
                                                                    {sampler, AddrSpace::AddressSpaceSampler}}});
static_assert(lookup.size() == AddrSpace::AddressSpaceMax - 1, "Every enum field must be present");
} // namespace AddressSpace
namespace AccessType {
using namespace Tags::Kernel::PayloadArgument::AccessType;
using AccessType = Types::Kernel::PayloadArgument::AccessType;

static constexpr ConstStringRef name = "access type";
static constexpr LookupArray<ConstStringRef, AccessType, 3> lookup({{{readonly, AccessType::AccessTypeReadonly},
                                                                     {writeonly, AccessType::AccessTypeWriteonly},
                                                                     {readwrite, AccessType::AccessTypeReadwrite}}});
static_assert(lookup.size() == AccessType::AccessTypeMax - 1, "Every enum field must be present");
} // namespace AccessType
namespace AllocationType {
using namespace Tags::Kernel::PerThreadMemoryBuffer::AllocationType;
using AllocType = Types::Kernel::PerThreadMemoryBuffer::AllocationType;
static constexpr ConstStringRef name = "allocation type";
static constexpr LookupArray<ConstStringRef, AllocType, 3> lookup({{{global, AllocType::AllocationTypeGlobal},
                                                                    {scratch, AllocType::AllocationTypeScratch},
                                                                    {slm, AllocType::AllocationTypeSlm}}});
static_assert(lookup.size() == AllocType::AllocationTypeMax - 1, "Every enum field must be present");
} // namespace AllocationType
namespace MemoryUsage {
using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;
using MemoryUsage = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
static constexpr ConstStringRef name = "memory usage";
static constexpr LookupArray<ConstStringRef, MemoryUsage, 3> lookup({{{privateSpace, MemoryUsage::MemoryUsagePrivateSpace},
                                                                      {spillFillSpace, MemoryUsage::MemoryUsageSpillFillSpace},
                                                                      {singleSpace, MemoryUsage::MemoryUsageSingleSpace}}});
static_assert(lookup.size() == MemoryUsage::MemoryUsageMax - 1, "Every enum field must be present");
} // namespace MemoryUsage
namespace ImageType {
using namespace Tags::Kernel::PayloadArgument::ImageType;
using ImageType = Types::Kernel::PayloadArgument::ImageType;
static constexpr ConstStringRef name = "image type";
static constexpr LookupArray<ConstStringRef, ImageType, 2> lookup({{{imageTypeMedia, ImageType::ImageTypeMedia},
                                                                    {imageTypeBlock, ImageType::ImageTypeMediaBlock}}});
static_assert(lookup.size() == ImageType::ImageTypeMax - 1, "Every enum field must be present");

} // namespace ImageType

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
} // namespace NEO::Zebin::ZeInfo::EnumLookup