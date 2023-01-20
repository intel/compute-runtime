/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/kernel_info.h"

#include <string>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t;
}

namespace NEO {

inline constexpr NEO::Elf::ZebinKernelMetadata::Types::Version zeInfoDecoderVersion{1, 26};

template <Elf::ELF_IDENTIFIER_CLASS numBits = Elf::EI_CLASS_64>
struct ZebinSections {
    using SectionHeaderData = typename NEO::Elf::Elf<numBits>::SectionHeaderAndData;
    StackVec<SectionHeaderData *, 32> textKernelSections;
    StackVec<SectionHeaderData *, 1> zeInfoSections;
    StackVec<SectionHeaderData *, 1> globalDataSections;
    StackVec<SectionHeaderData *, 1> constDataSections;
    StackVec<SectionHeaderData *, 1> constDataStringSections;
    StackVec<SectionHeaderData *, 1> symtabSections;
    StackVec<SectionHeaderData *, 1> spirvSections;
    StackVec<SectionHeaderData *, 1> noteIntelGTSections;
    StackVec<SectionHeaderData *, 1> buildOptionsSection;
};

using UniqueNode = StackVec<const NEO::Yaml::Node *, 1>;
struct ZeInfoSections {
    UniqueNode kernels;
    UniqueNode version;
    UniqueNode globalHostAccessTable;
    UniqueNode functions;
};

struct ZeInfoKernelSections {
    UniqueNode attributesNd;
    UniqueNode nameNd;
    UniqueNode executionEnvNd;
    UniqueNode debugEnvNd;
    UniqueNode payloadArgumentsNd;
    UniqueNode bindingTableIndicesNd;
    UniqueNode perThreadPayloadArgumentsNd;
    UniqueNode perThreadMemoryBuffersNd;
    UniqueNode experimentalPropertiesNd;
    UniqueNode inlineSamplersNd;
};

DecodeError validateZeInfoVersion(const Elf::ZebinKernelMetadata::Types::Version &receivedZeInfoVersion, std::string &outErrReason, std::string &outWarning);

bool validateTargetDevice(const TargetDevice &targetDevice, Elf::ELF_IDENTIFIER_CLASS numBits, PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCore, AOT::PRODUCT_CONFIG productConfig, Elf::ZebinTargetFlags targetMetadata);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
bool validateTargetDevice(const Elf::Elf<numBits> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError decodeIntelGTNoteSection(ArrayRef<const uint8_t> intelGTNotesSection, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError getIntelGTNotes(const Elf::Elf<numBits> &elf, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError extractZebinSections(NEO::Elf::Elf<numBits> &elf, ZebinSections<numBits> &out, std::string &outErrReason, std::string &outWarning);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError validateZebinSectionsCount(const ZebinSections<numBits> &sections, std::string &outErrReason, std::string &outWarning);
void extractZeInfoKernelSections(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &kernelNd, ZeInfoKernelSections &outZeInfoKernelSections, ConstStringRef context, std::string &outWarning);
DecodeError validateZeInfoKernelSectionsCount(const ZeInfoKernelSections &outZeInfoKernelSections, std::string &outErrReason, std::string &outWarning);
template <typename T>
bool readEnumChecked(ConstStringRef enumString, T &outValue, ConstStringRef kernelName, std::string &outErrReason);
template <typename T>
bool readZeInfoEnumChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef kernelName, std::string &outErrReason);

using ZeInfoGlobalHostAccessTables = StackVec<NEO::Elf::ZebinKernelMetadata::Types::GlobalHostAccessTable::globalHostAccessTableT, 32>;
DecodeError readZeInfoGlobalHostAceessTable(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                            ZeInfoGlobalHostAccessTables &outDeviceNameToHostTable,
                                            ConstStringRef context,
                                            std::string &outErrReason, std::string &outWarning);

NEO::DecodeError readZeInfoVersionFromZeInfo(NEO::Elf::ZebinKernelMetadata::Types::Version &dst,
                                             NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &versionNd, std::string &outErrReason, std::string &outWarning);

NEO::DecodeError populateZeInfoVersion(NEO::Elf::ZebinKernelMetadata::Types::Version &dst, ConstStringRef &versionStr, std::string &outErrReason);

NEO::DecodeError populateExternalFunctionsMetadata(NEO::ProgramInfo &dst, NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &functionNd, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
NEO::DecodeError decodeZebin(ProgramInfo &dst, NEO::Elf::Elf<numBits> &elf, std::string &outErrReason, std::string &outWarning);

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ArrayRef<const uint8_t> getKernelHeap(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections);

NEO::DecodeError decodeZeInfo(ProgramInfo &dst, ConstStringRef zeInfo, std::string &outErrReason, std::string &outWarning);

void setKernelMiscInfoPosition(ConstStringRef metadata, NEO::ProgramInfo &dst);

using KernelMiscArgInfos = StackVec<NEO::Elf::ZebinKernelMetadata::Types::Miscellaneous::KernelArgMiscInfoT, 32>;
NEO::DecodeError readKernelMiscArgumentInfos(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning);

void populateKernelMiscInfo(KernelDescriptor &dst, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning);

NEO::DecodeError decodeAndPopulateKernelMiscInfo(size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos, ConstStringRef metadataString, std::string &outErrReason, std::string &outWarning);

ConstStringRef extractZeInfoMetadataStringFromZebin(const ArrayRef<const uint8_t> zebin, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoVersion(Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);
DecodeError decodeZeInfoGlobalHostAccessTable(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);
DecodeError decodeZeInfoFunctions(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);
DecodeError decodeZeInfoKernels(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoKernelEntry(KernelDescriptor &dst, Yaml::YamlParser &yamlParser, const Yaml::Node &kernelNd, uint32_t grfSize, uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning);

using KernelExecutionEnvBaseT = Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT;
DecodeError decodeZeInfoKernelExecutionEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoExecutionEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExecutionEnvBaseT &outExecEnv, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelExecutionEnvironment(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv);

using KernelAttributesBaseT = Elf::ZebinKernelMetadata::Types::Kernel::Attributes::AttributesBaseT;
DecodeError decodeZeInfoKernelUserAttributes(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoAttributes(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelAttributesBaseT &outAttributes, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelSourceAttributes(NEO::KernelDescriptor &dst, const KernelAttributesBaseT &attributes);

using KernelDebugEnvBaseT = Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT;
DecodeError decodeZeInfoKernelDebugEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoDebugEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelDebugEnvBaseT &outDebugEnv, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelDebugEnvironment(NEO::KernelDescriptor &dst, const KernelDebugEnvBaseT &debugEnv);

using KernelPerThreadPayloadArgBaseT = Elf::ZebinKernelMetadata::Types::Kernel::PerThreadPayloadArgument::PerThreadPayloadArgumentBaseT;
using KernelPerThreadPayloadArguments = StackVec<KernelPerThreadPayloadArgBaseT, 1>;
DecodeError decodeZeInfoKernelPerThreadPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoPerThreadPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadPayloadArguments &outPerThreadPayloadArguments, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPerThreadPayloadArgument(KernelDescriptor &dst, const KernelPerThreadPayloadArgBaseT &src, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning);

using KernelPayloadArgBaseT = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT;
using KernelPayloadArguments = StackVec<KernelPayloadArgBaseT, 32>;
DecodeError decodeZeInfoKernelPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPayloadArguments &outPayloadArguments, int32_t &outMaxPayloadArgumentIndex, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPayloadArgument(NEO::KernelDescriptor &dst, const KernelPayloadArgBaseT &src, std::string &outErrReason, std::string &outWarning);

using KernelInlineSamplerBaseT = Elf::ZebinKernelMetadata::Types::Kernel::InlineSamplers::InlineSamplerBaseT;
using KernelInlineSamplers = StackVec<KernelInlineSamplerBaseT, 4>;
DecodeError decodeZeInfoKernelInlineSamplers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoInlineSamplers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelInlineSamplers &outInlineSamplers, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelInlineSampler(KernelDescriptor &dst, const KernelInlineSamplerBaseT &src, std::string &outErrReason, std::string &outWarning);

using KernelPerThreadMemoryBufferBaseT = Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::PerThreadMemoryBufferBaseT;
using KernelPerThreadMemoryBuffers = StackVec<KernelPerThreadMemoryBufferBaseT, 8>;
DecodeError decodeZeInfoKernelPerThreadMemoryBuffers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoPerThreadMemoryBuffers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadMemoryBuffers &outPerThreadMemoryBuffers, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPerThreadMemoryBuffer(KernelDescriptor &dst, const KernelPerThreadMemoryBufferBaseT &src, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning);

using KernelExperimentalPropertiesBaseT = Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT;
DecodeError decodeZeInfoKernelExperimentalProperties(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoExperimentalProperties(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExperimentalPropertiesBaseT &outExperimentalProperties, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelExperimentalProperties(KernelDescriptor &dst, const KernelExperimentalPropertiesBaseT &experimentalProperties);

using KernelBindingTableEntryBaseT = Elf::ZebinKernelMetadata::Types::Kernel::BindingTableEntry::BindingTableEntryBaseT;
using KernelBindingTableEntries = StackVec<KernelBindingTableEntryBaseT, 32>;
DecodeError decodeZeInfoKernelBindingTableEntries(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoBindingTableIndices(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelBindingTableEntries &outBindingTableIndices, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelBindingTableIndicies(KernelDescriptor &dst, const KernelBindingTableEntries &btEntries, std::string &outErrReason);

void generateSSHWithBindingTable(KernelDescriptor &dst);
void generateDSH(KernelDescriptor &dst);

} // namespace NEO
