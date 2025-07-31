/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/source/device_binary_format/zebin/zeinfo.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {

struct KernelDescriptor;
struct KernelInfo;
struct ProgramInfo;

namespace Zebin::ZeInfo {
inline constexpr NEO::Zebin::ZeInfo::Types::Version zeInfoDecoderVersion{1, 59};

using KernelExecutionEnvBaseT = Types::Kernel::ExecutionEnv::ExecutionEnvBaseT;

template <typename T>
bool readZeInfoValueChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef context, std::string &outErrReason);
template <typename T>
bool readEnumChecked(ConstStringRef enumString, T &outValue, ConstStringRef kernelName, std::string &outErrReason);
template <typename T>
bool readZeInfoEnumChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef kernelName, std::string &outErrReason);
void readZeInfoValueCheckedExtra(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &execEnvMetadataNd, KernelExecutionEnvBaseT &kernelExecEnv, ConstStringRef context,
                                 ConstStringRef key, std::string &outErrReason, std::string &outWarning, bool &validExecEnv, DecodeError &error);

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

DecodeError decodeZeInfo(ProgramInfo &dst, ConstStringRef zeInfo, std::string &outErrReason, std::string &outWarning);

DecodeError decodeAndPopulateKernelMiscInfo(size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos, ConstStringRef metadataString, std::string &outErrReason, std::string &outWarning);

DecodeError extractZeInfoKernelSections(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &kernelNd, ZeInfoKernelSections &outZeInfoKernelSections, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError validateZeInfoKernelSectionsCount(const ZeInfoKernelSections &outZeInfoKernelSections, std::string &outErrReason, std::string &outWarning);

using ZeInfoGlobalHostAccessTables = StackVec<Types::GlobalHostAccessTable::GlobalHostAccessTableT, 32>;
DecodeError readZeInfoGlobalHostAceessTable(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                            ZeInfoGlobalHostAccessTables &outDeviceNameToHostTable,
                                            ConstStringRef context,
                                            std::string &outErrReason, std::string &outWarning);

DecodeError populateZeInfoVersion(Types::Version &dst, ConstStringRef &versionStr, std::string &outErrReason);
DecodeError validateZeInfoVersion(const Types::Version &receivedZeInfoVersion, std::string &outErrReason, std::string &outWarning);

DecodeError populateExternalFunctionsMetadata(NEO::ProgramInfo &dst, NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &functionNd, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoVersion(Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning, Types::Version &srcZeInfoVersion);
DecodeError readZeInfoVersionFromZeInfo(Types::Version &dst,
                                        NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &versionNd, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoGlobalHostAccessTable(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoFunctions(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning);

DecodeError decodeZeInfoKernels(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning, const Types::Version &srcZeInfoVersion);
DecodeError decodeZeInfoKernelEntry(KernelDescriptor &dst, Yaml::YamlParser &yamlParser, const Yaml::Node &kernelNd, uint32_t grfSize, uint32_t minScratchSpaceSize, uint32_t samplerStateSize, uint32_t samplerBorderColorStateSize, std::string &outErrReason, std::string &outWarning, const Types::Version &srcZeInfoVersion);

DecodeError decodeZeInfoKernelExecutionEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning, const Types::Version &srcZeInfoVersion);
DecodeError readZeInfoExecutionEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExecutionEnvBaseT &outExecEnv, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelExecutionEnvironment(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv, const Types::Version &srcZeInfoVersion);

using KernelAttributesBaseT = Types::Kernel::Attributes::AttributesBaseT;
DecodeError decodeZeInfoKernelUserAttributes(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoAttributes(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelAttributesBaseT &outAttributes, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelSourceAttributes(NEO::KernelDescriptor &dst, const KernelAttributesBaseT &attributes);

using KernelDebugEnvBaseT = Types::Kernel::DebugEnv::DebugEnvBaseT;
DecodeError decodeZeInfoKernelDebugEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoDebugEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelDebugEnvBaseT &outDebugEnv, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelDebugEnvironment(NEO::KernelDescriptor &dst, const KernelDebugEnvBaseT &debugEnv);

using KernelPerThreadPayloadArgBaseT = Types::Kernel::PerThreadPayloadArgument::PerThreadPayloadArgumentBaseT;
using KernelPerThreadPayloadArguments = StackVec<KernelPerThreadPayloadArgBaseT, 1>;
DecodeError decodeZeInfoKernelPerThreadPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoPerThreadPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadPayloadArguments &outPerThreadPayloadArguments, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPerThreadPayloadArgument(KernelDescriptor &dst, const KernelPerThreadPayloadArgBaseT &src, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning);

using KernelPayloadArgBaseT = Types::Kernel::PayloadArgument::PayloadArgumentBaseT;
using KernelPayloadArguments = StackVec<KernelPayloadArgBaseT, 32>;
DecodeError decodeZeInfoKernelPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPayloadArguments &outPayloadArguments, int32_t &outMaxPayloadArgumentIndex, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPayloadArgument(NEO::KernelDescriptor &dst, const KernelPayloadArgBaseT &src, std::string &outErrReason, std::string &outWarning);

using KernelInlineSamplerBaseT = Types::Kernel::InlineSamplers::InlineSamplerBaseT;
using KernelInlineSamplers = StackVec<KernelInlineSamplerBaseT, 4>;
DecodeError decodeZeInfoKernelInlineSamplers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoInlineSamplers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelInlineSamplers &outInlineSamplers, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelInlineSampler(KernelDescriptor &dst, const KernelInlineSamplerBaseT &src, std::string &outErrReason, std::string &outWarning);

using KernelPerThreadMemoryBufferBaseT = Types::Kernel::PerThreadMemoryBuffer::PerThreadMemoryBufferBaseT;
using KernelPerThreadMemoryBuffers = StackVec<KernelPerThreadMemoryBufferBaseT, 8>;
DecodeError decodeZeInfoKernelPerThreadMemoryBuffers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning, const Types::Version &srcZeInfoVersion);
DecodeError readZeInfoPerThreadMemoryBuffers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadMemoryBuffers &outPerThreadMemoryBuffers, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelPerThreadMemoryBuffer(KernelDescriptor &dst, const KernelPerThreadMemoryBufferBaseT &src, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning, const Types::Version &srcZeInfoVersion);

using KernelExperimentalPropertiesBaseT = Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT;
DecodeError decodeZeInfoKernelExperimentalProperties(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoExperimentalProperties(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExperimentalPropertiesBaseT &outExperimentalProperties, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
void populateKernelExperimentalProperties(KernelDescriptor &dst, const KernelExperimentalPropertiesBaseT &experimentalProperties);

using KernelBindingTableEntryBaseT = Types::Kernel::BindingTableEntry::BindingTableEntryBaseT;
using KernelBindingTableEntries = StackVec<KernelBindingTableEntryBaseT, 32>;
DecodeError decodeZeInfoKernelBindingTableEntries(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoBindingTableIndices(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelBindingTableEntries &outBindingTableIndices, ConstStringRef context, std::string &outErrReason, std::string &outWarning);
DecodeError populateKernelBindingTableIndicies(KernelDescriptor &dst, const KernelBindingTableEntries &btEntries, std::string &outErrReason);

using KernelMiscArgInfos = StackVec<Types::Miscellaneous::KernelArgMiscInfoT, 32>;
DecodeError readKernelMiscArgumentInfos(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning);
void populateKernelMiscInfo(KernelDescriptor &dst, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning);

void generateSSHWithBindingTable(KernelDescriptor &dst);
void generateDSH(KernelDescriptor &dst, uint32_t samplerStateSize, uint32_t samplerBorderColorStateSize);

inline bool isAtLeastZeInfoVersion(const Types::Version &srcVersion, const Types::Version &expectedVersion) {
    return srcVersion.minor >= expectedVersion.minor;
}

inline bool isScratchMemoryUsageDefinedInExecutionEnvironment(const Types::Version &srcVersion) { return isAtLeastZeInfoVersion(srcVersion, {1, 39}); }

void encounterUnknownZeInfoAttribute(const std::string &entryDescriptor, std::string &outErrReason, std::string &outWarning, DecodeError &errCode);

} // namespace Zebin::ZeInfo

} // namespace NEO
