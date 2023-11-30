/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/zebin/zeinfo_enum_lookup.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_vme.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/source/utilities/const_stringref.h"

namespace NEO::Zebin::ZeInfo {

template <typename ContainerT>
bool validateCountAtMost(const ContainerT &sectionsContainer, size_t max, std::string &outErrReason, ConstStringRef name, ConstStringRef context) {
    if (sectionsContainer.size() <= max) {
        return true;
    }
    outErrReason.append(context.str() + " : Expected at most " + std::to_string(max) + " of " + name.str() + ", got : " + std::to_string(sectionsContainer.size()) + "\n");
    return false;
}

template <typename ContainerT>
bool validateCountExactly(const ContainerT &sectionsContainer, size_t num, std::string &outErrReason, ConstStringRef name, ConstStringRef context) {
    if (sectionsContainer.size() == num) {
        return true;
    }
    outErrReason.append(context.str() + " : Expected exactly " + std::to_string(num) + " of " + name.str() + ", got : " + std::to_string(sectionsContainer.size()) + "\n");
    return false;
}

DecodeError validateZeInfoVersion(const Types::Version &receivedZeInfoVersion, std::string &outErrReason, std::string &outWarning) {
    if (receivedZeInfoVersion.major != zeInfoDecoderVersion.major) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Unhandled major version : " + std::to_string(receivedZeInfoVersion.major) + ", decoder is at : " + std::to_string(zeInfoDecoderVersion.major) + "\n");
        return DecodeError::UnhandledBinary;
    }
    if (receivedZeInfoVersion.minor > zeInfoDecoderVersion.minor) {
        outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Minor version : " + std::to_string(receivedZeInfoVersion.minor) + " is newer than available in decoder : " + std::to_string(zeInfoDecoderVersion.minor) + " - some features may be skipped\n");
    }
    return DecodeError::Success;
}

void extractZeInfoSections(const Yaml::YamlParser &parser, ZeInfoSections &outZeInfoSections, std::string &outWarning) {
    for (const auto &globalScopeNd : parser.createChildrenRange(*parser.getRoot())) {
        auto key = parser.readKey(globalScopeNd);
        if (Tags::kernels == key) {
            outZeInfoSections.kernels.push_back(&globalScopeNd);
        } else if (Tags::version == key) {
            outZeInfoSections.version.push_back(&globalScopeNd);
        } else if (Tags::globalHostAccessTable == key) {
            outZeInfoSections.globalHostAccessTable.push_back(&globalScopeNd);
        } else if (Tags::functions == key) {
            outZeInfoSections.functions.push_back(&globalScopeNd);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + parser.readKey(globalScopeNd).str() + "\" in global scope of .ze_info\n");
        }
    }
}

void extractZeInfoKernelSections(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &kernelNd, ZeInfoKernelSections &outZeInfoKernelSections, ConstStringRef context, std::string &outWarning) {
    for (const auto &kernelMetadataNd : parser.createChildrenRange(kernelNd)) {
        auto key = parser.readKey(kernelMetadataNd);
        if (Tags::Kernel::name == key) {
            outZeInfoKernelSections.nameNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::attributes == key) {
            outZeInfoKernelSections.attributesNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::executionEnv == key) {
            outZeInfoKernelSections.executionEnvNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::debugEnv == key) {
            outZeInfoKernelSections.debugEnvNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::payloadArguments == key) {
            outZeInfoKernelSections.payloadArgumentsNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::perThreadPayloadArguments == key) {
            outZeInfoKernelSections.perThreadPayloadArgumentsNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::bindingTableIndices == key) {
            outZeInfoKernelSections.bindingTableIndicesNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::perThreadMemoryBuffers == key) {
            outZeInfoKernelSections.perThreadMemoryBuffersNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::experimentalProperties == key) {
            outZeInfoKernelSections.experimentalPropertiesNd.push_back(&kernelMetadataNd);
        } else if (Tags::Kernel::inlineSamplers == key) {
            outZeInfoKernelSections.inlineSamplersNd.push_back(&kernelMetadataNd);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + parser.readKey(kernelMetadataNd).str() + "\" in context of : " + context.str() + "\n");
        }
    }
}

bool validateZeInfoSectionsCount(const ZeInfoSections &zeInfoSections, std::string &outErrReason) {
    ConstStringRef context = "DeviceBinaryFormat::Zebin::ZeInfo";
    bool valid = validateCountExactly(zeInfoSections.kernels, 1U, outErrReason, "kernels", context);
    valid &= validateCountAtMost(zeInfoSections.version, 1U, outErrReason, "version", context);
    valid &= validateCountAtMost(zeInfoSections.globalHostAccessTable, 1U, outErrReason, "global host access table", context);
    valid &= validateCountAtMost(zeInfoSections.functions, 1U, outErrReason, "functions", context);
    return valid;
}

DecodeError validateZeInfoKernelSectionsCount(const ZeInfoKernelSections &outZeInfoKernelSections, std::string &outErrReason, std::string &outWarning) {
    ConstStringRef context = "DeviceBinaryFormat::Zebin::ZeInfo::Kernel";
    bool valid = validateCountExactly(outZeInfoKernelSections.nameNd, 1U, outErrReason, Tags::Kernel::name, context);
    valid &= validateCountExactly(outZeInfoKernelSections.executionEnvNd, 1U, outErrReason, Tags::Kernel::executionEnv, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.attributesNd, 1U, outErrReason, Tags::Kernel::attributes, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.debugEnvNd, 1U, outErrReason, Tags::Kernel::debugEnv, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.payloadArgumentsNd, 1U, outErrReason, Tags::Kernel::payloadArguments, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.perThreadPayloadArgumentsNd, 1U, outErrReason, Tags::Kernel::perThreadPayloadArguments, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.bindingTableIndicesNd, 1U, outErrReason, Tags::Kernel::bindingTableIndices, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.perThreadMemoryBuffersNd, 1U, outErrReason, Tags::Kernel::perThreadMemoryBuffers, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.experimentalPropertiesNd, 1U, outErrReason, Tags::Kernel::experimentalProperties, context);
    valid &= validateCountAtMost(outZeInfoKernelSections.inlineSamplersNd, 1U, outErrReason, Tags::Kernel::inlineSamplers, context);
    return valid ? DecodeError::Success : DecodeError::InvalidBinary;
}

template <typename T>
bool readZeInfoValueChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef context, std::string &outErrReason) {
    if (parser.readValueChecked(node, outValue)) {
        return true;
    }
    outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : could not read " + parser.readKey(node).str() + " from : [" + parser.readValue(node).str() + "] in context of : " + context.str() + "\n");
    return false;
}

template <typename DestinationT, size_t len>
bool readZeInfoValueCollectionCheckedArr(std::array<DestinationT, len> &vec, const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, ConstStringRef context, std::string &outErrReason) {
    auto collectionNodes = parser.createChildrenRange(node);
    size_t index = 0U;
    bool isValid = true;

    for (const auto &elementNd : collectionNodes) {
        isValid &= readZeInfoValueChecked(parser, elementNd, vec[index++], context, outErrReason);
    }

    if (index != len) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : wrong size of collection " + parser.readKey(node).str() + " in context of : " + context.str() + ". Got : " + std::to_string(index) + " expected : " + std::to_string(len) + "\n");
        isValid = false;
    }
    return isValid;
}

template <typename DestinationT, size_t len>
bool readZeInfoValueCollectionChecked(DestinationT (&vec)[len], const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, ConstStringRef context, std::string &outErrReason) {
    auto &array = reinterpret_cast<std::array<DestinationT, len> &>(vec);
    return readZeInfoValueCollectionCheckedArr(array, parser, node, context, outErrReason);
}

template <typename T>
bool readEnumChecked(ConstStringRef enumString, T &outValue, ConstStringRef kernelName, std::string &outErrReason) {
    using EnumLooker = NEO::Zebin::ZeInfo::EnumLookup::EnumLooker<T>;
    auto enumVal = EnumLooker::members.find(enumString);
    outValue = enumVal.value_or(static_cast<T>(0));

    if (false == enumVal.has_value()) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"" + enumString.str() + "\" " + EnumLooker::name.str() + " in context of " + kernelName.str() + "\n");
    }

    return enumVal.has_value();
}

template <typename T>
bool readZeInfoEnumChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef kernelName, std::string &outErrReason) {
    auto token = parser.getValueToken(node);
    if (nullptr == token) {
        return false;
    }
    auto tokenValue = token->cstrref();
    return readEnumChecked(tokenValue, outValue, kernelName, outErrReason);
}
template bool readZeInfoEnumChecked<Types::Kernel::PayloadArgument::ArgTypeT>(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, Types::Kernel::PayloadArgument::ArgTypeT &outValue, ConstStringRef kernelName, std::string &outErrReason);

DecodeError readZeInfoGlobalHostAceessTable(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                            ZeInfoGlobalHostAccessTables &outDeviceNameToHostTable,
                                            ConstStringRef context,
                                            std::string &outErrReason, std::string &outWarning) {
    bool validTable = true;
    for (const auto &globalHostAccessNameNd : parser.createChildrenRange(node)) {
        outDeviceNameToHostTable.resize(outDeviceNameToHostTable.size() + 1);
        for (const auto &globalHostAccessNameMemberNd : parser.createChildrenRange(globalHostAccessNameNd)) {
            auto &globalHostAccessMetadata = *outDeviceNameToHostTable.rbegin();
            auto key = parser.readKey(globalHostAccessNameMemberNd);
            if (Tags::GlobalHostAccessTable::deviceName == key) {
                validTable &= readZeInfoValueChecked(parser, globalHostAccessNameMemberNd, globalHostAccessMetadata.deviceName, context, outErrReason);
            } else if (Tags::GlobalHostAccessTable::hostName == key) {
                validTable &= readZeInfoValueChecked(parser, globalHostAccessNameMemberNd, globalHostAccessMetadata.hostName, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for payload argument in context of " + context.str() + "\n");
            }
        }
    }
    return validTable ? DecodeError::Success : DecodeError::InvalidBinary;
}

template <typename ElSize, size_t len>
bool setVecArgIndicesBasedOnSize(CrossThreadDataOffset (&vec)[len], size_t vecSize, CrossThreadDataOffset baseOffset) {
    switch (vecSize) {
    default:
        return false;
    case sizeof(ElSize) * 3:
        vec[2] = static_cast<CrossThreadDataOffset>(baseOffset + 2 * sizeof(ElSize));
        [[fallthrough]];
    case sizeof(ElSize) * 2:
        vec[1] = static_cast<CrossThreadDataOffset>(baseOffset + 1 * sizeof(ElSize));
        [[fallthrough]];
    case sizeof(ElSize) * 1:
        vec[0] = static_cast<CrossThreadDataOffset>(baseOffset + 0 * sizeof(ElSize));
        break;
    }
    return true;
}

void setSSHOffsetBasedOnBti(SurfaceStateHeapOffset &sshOffset, int32_t bti, uint8_t &outNumBtEntries) {
    if (bti == -1) {
        return;
    }

    constexpr auto surfaceStateSize = 64U;
    sshOffset = surfaceStateSize * bti;
    outNumBtEntries = std::max<uint8_t>(outNumBtEntries, static_cast<uint8_t>(bti + 1));
}

DecodeError readZeInfoVersionFromZeInfo(Types::Version &dst,
                                        NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &versionNd, std::string &outErrReason, std::string &outWarning) {
    if (nullptr == yamlParser.getValueToken(versionNd)) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected \'MAJOR.MINOR\' string\n");
        return DecodeError::InvalidBinary;
    }
    auto versionStr = yamlParser.readValueNoQuotes(versionNd);
    return populateZeInfoVersion(dst, versionStr, outErrReason);
}

DecodeError populateZeInfoVersion(Types::Version &dst, ConstStringRef &versionStr, std::string &outErrReason) {
    StackVec<char, 32> nullTerminated{versionStr.begin(), versionStr.end()};
    nullTerminated.push_back('\0');
    auto separator = std::find(nullTerminated.begin(), nullTerminated.end(), '.');
    if ((nullTerminated.end() == separator) || (nullTerminated.begin() == separator) || (&*nullTerminated.rbegin() == separator + 1)) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : " + std::string{versionStr} + "\n");
        return DecodeError::InvalidBinary;
    }
    *separator = 0;
    dst.major = atoi(nullTerminated.begin());
    dst.minor = atoi(separator + 1);
    return DecodeError::Success;
}

DecodeError populateExternalFunctionsMetadata(NEO::ProgramInfo &dst, NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &functionNd, std::string &outErrReason, std::string &outWarning) {
    ConstStringRef functionName;
    Types::Function::ExecutionEnv::ExecutionEnvBaseT execEnv = {};
    bool isValid = true;

    for (const auto &functionMetadataNd : yamlParser.createChildrenRange(functionNd)) {
        auto key = yamlParser.readKey(functionMetadataNd);
        if (Tags::Function::name == key) {
            functionName = yamlParser.readValueNoQuotes(functionMetadataNd);
        } else if (Tags::Function::executionEnv == key) {
            auto execEnvErr = readZeInfoExecutionEnvironment(yamlParser, functionMetadataNd, execEnv, "external functions", outErrReason, outWarning);
            if (execEnvErr != DecodeError::Success) {
                isValid = false;
            }
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + yamlParser.readKey(functionMetadataNd).str() + "\" in context of : external functions\n");
        }
    }

    if (isValid) {
        NEO::ExternalFunctionInfo extFunInfo{};
        extFunInfo.functionName = functionName.str();
        extFunInfo.barrierCount = static_cast<uint8_t>(execEnv.barrierCount);
        extFunInfo.numGrfRequired = static_cast<uint16_t>(execEnv.grfCount);
        extFunInfo.simdSize = static_cast<uint8_t>(execEnv.simdSize);
        extFunInfo.hasRTCalls = execEnv.hasRTCalls;
        dst.externalFunctions.push_back(extFunInfo);
        return DecodeError::Success;
    } else {
        return DecodeError::InvalidBinary;
    }
}

DecodeError readKernelMiscArgumentInfos(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning) {
    bool validArgInfo = true;
    for (const auto &argInfoMemberNode : parser.createChildrenRange(node)) {
        kernelMiscArgInfosVec.resize(kernelMiscArgInfosVec.size() + 1);
        auto &metadataExtended = *kernelMiscArgInfosVec.rbegin();
        for (const auto &singleArgInfoMember : parser.createChildrenRange(argInfoMemberNode)) {
            auto key = parser.readKey(singleArgInfoMember);
            if (key == Tags::KernelMiscInfo::ArgsInfo::name) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.argName, Tags::kernelMiscInfo, outErrReason);
            } else if (key == Tags::KernelMiscInfo::ArgsInfo::accessQualifier) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.accessQualifier, Tags::kernelMiscInfo, outErrReason);
            } else if (key == Tags::KernelMiscInfo::ArgsInfo::addressQualifier) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.addressQualifier, Tags::kernelMiscInfo, outErrReason);
            } else if (key == Tags::KernelMiscInfo::ArgsInfo::index) {
                validArgInfo &= parser.readValueChecked(singleArgInfoMember, metadataExtended.index);
            } else if (key == Tags::KernelMiscInfo::ArgsInfo::typeName) {
                metadataExtended.typeName = parser.readValueNoQuotes(singleArgInfoMember).str();
                validArgInfo &= (false == metadataExtended.typeName.empty());
            } else if (key == Tags::KernelMiscInfo::ArgsInfo::typeQualifiers) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.typeQualifiers, Tags::kernelMiscInfo, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : KernelMiscInfo : Unrecognized argsInfo member " + key.str() + "\n");
            }
        }
        if (-1 == metadataExtended.index) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Error : KernelMiscInfo : ArgInfo index missing (has default value -1)");
            return DecodeError::InvalidBinary;
        }
    }
    return validArgInfo ? DecodeError::Success : DecodeError::InvalidBinary;
}

void populateKernelMiscInfo(KernelDescriptor &dst, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning) {
    auto populateIfNotEmpty = [](std::string &src, std::string &dst, ConstStringRef context, std::string &warnings) {
        if (false == src.empty()) {
            dst = std::move(src);
        } else {
            warnings.append("DeviceBinaryFormat::Zebin : KernelMiscInfo : ArgInfo member \"" + context.str() + "\" missing. Ignoring.\n");
        }
    };

    dst.explicitArgsExtendedMetadata.resize(kernelMiscArgInfosVec.size());
    for (auto &srcMetadata : kernelMiscArgInfosVec) {
        ArgTypeMetadataExtended dstMetadata;
        populateIfNotEmpty(srcMetadata.argName, dstMetadata.argName, Tags::KernelMiscInfo::ArgsInfo::name, outWarning);
        populateIfNotEmpty(srcMetadata.accessQualifier, dstMetadata.accessQualifier, Tags::KernelMiscInfo::ArgsInfo::accessQualifier, outWarning);
        populateIfNotEmpty(srcMetadata.addressQualifier, dstMetadata.addressQualifier, Tags::KernelMiscInfo::ArgsInfo::addressQualifier, outWarning);
        populateIfNotEmpty(srcMetadata.typeName, dstMetadata.type, Tags::KernelMiscInfo::ArgsInfo::typeName, outWarning);
        populateIfNotEmpty(srcMetadata.typeQualifiers, dstMetadata.typeQualifiers, Tags::KernelMiscInfo::ArgsInfo::typeQualifiers, outWarning);

        ArgTypeTraits dstTypeTraits = {};
        dstTypeTraits.accessQualifier = KernelArgMetadata::parseAccessQualifier(dstMetadata.accessQualifier);
        dstTypeTraits.addressQualifier = KernelArgMetadata::parseAddressSpace(dstMetadata.addressQualifier);
        dstTypeTraits.typeQualifiers = KernelArgMetadata::parseTypeQualifiers(dstMetadata.typeQualifiers);
        dst.payloadMappings.explicitArgs.at(srcMetadata.index).getTraits() = std::move(dstTypeTraits);

        dstMetadata.type = dstMetadata.type.substr(0U, dstMetadata.type.find(";"));
        dst.explicitArgsExtendedMetadata.at(srcMetadata.index) = std::move(dstMetadata);
    }
}

DecodeError decodeAndPopulateKernelMiscInfo(size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos, ConstStringRef metadataString, std::string &outErrReason, std::string &outWarning) {
    if (std::string::npos == kernelMiscInfoOffset) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Position of " + Tags::kernelMiscInfo.str() + " not set - may be missing in zeInfo.\n");
        return DecodeError::InvalidBinary;
    }
    ConstStringRef kernelMiscInfoString(reinterpret_cast<const char *>(metadataString.begin() + kernelMiscInfoOffset), metadataString.size() - kernelMiscInfoOffset);
    NEO::KernelInfo *kernelInfo = nullptr;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(kernelMiscInfoString, outErrReason, outWarning);
    if (false == parseSuccess) {
        return DecodeError::InvalidBinary;
    }

    auto kernelMiscInfoSectionNode = parser.createChildrenRange(*parser.getRoot());
    auto validMetadata = true;
    using KernelArgsMiscInfoVec = std::vector<std::pair<std::string, KernelMiscArgInfos>>;
    KernelArgsMiscInfoVec kernelArgsMiscInfoVec;

    for (const auto &kernelMiscInfoNode : parser.createChildrenRange(*kernelMiscInfoSectionNode.begin())) {
        std::string kernelName{};
        KernelMiscArgInfos miscArgInfosVec;
        for (const auto &kernelMiscInfoNodeMetadata : parser.createChildrenRange(kernelMiscInfoNode)) {
            auto key = parser.readKey(kernelMiscInfoNodeMetadata);
            if (key == Tags::KernelMiscInfo::name) {
                validMetadata &= readZeInfoValueChecked(parser, kernelMiscInfoNodeMetadata, kernelName, Tags::kernelMiscInfo, outErrReason);
            } else if (key == Tags::KernelMiscInfo::argsInfo) {
                validMetadata &= (DecodeError::Success == readKernelMiscArgumentInfos(parser, kernelMiscInfoNodeMetadata, miscArgInfosVec, outErrReason, outWarning));
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : Unrecognized entry: " + key.str() + " in " + Tags::kernelMiscInfo.str() + " zeInfo's section.\n");
            }
        }
        if (kernelName.empty()) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Error : Missing kernel name in " + Tags::kernelMiscInfo.str() + " section.\n");
            validMetadata = false;
        }
        kernelArgsMiscInfoVec.emplace_back(std::make_pair(std::move(kernelName), miscArgInfosVec));
    }
    if (false == validMetadata) {
        return DecodeError::InvalidBinary;
    }
    for (auto &[kName, miscInfos] : kernelArgsMiscInfoVec) {
        for (auto dstKernelInfo : kernelInfos) {
            if (dstKernelInfo->kernelDescriptor.kernelMetadata.kernelName == kName) {
                kernelInfo = dstKernelInfo;
                break;
            }
        }
        if (nullptr == kernelInfo) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Error : Cannot find kernel info for kernel " + kName + ".\n");
            return DecodeError::InvalidBinary;
        }
        populateKernelMiscInfo(kernelInfo->kernelDescriptor, miscInfos, outErrReason, outWarning);
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfo(ProgramInfo &dst, ConstStringRef zeInfo, std::string &outErrReason, std::string &outWarning) {
    Yaml::YamlParser yamlParser;
    bool parseSuccess = yamlParser.parse(zeInfo, outErrReason, outWarning);
    if (false == parseSuccess) {
        return DecodeError::InvalidBinary;
    }

    if (yamlParser.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin : Empty kernels metadata section (.ze_info)\n");
        return DecodeError::Success;
    }

    ZeInfoSections zeInfoSections{};
    extractZeInfoSections(yamlParser, zeInfoSections, outWarning);
    if (false == validateZeInfoSectionsCount(zeInfoSections, outErrReason)) {
        return DecodeError::InvalidBinary;
    }

    auto zeInfoDecodeError = decodeZeInfoVersion(yamlParser, zeInfoSections, outErrReason, outWarning);
    if (DecodeError::Success != zeInfoDecodeError) {
        return zeInfoDecodeError;
    }

    zeInfoDecodeError = decodeZeInfoGlobalHostAccessTable(dst, yamlParser, zeInfoSections, outErrReason, outWarning);
    if (DecodeError::Success != zeInfoDecodeError) {
        return zeInfoDecodeError;
    }

    zeInfoDecodeError = decodeZeInfoFunctions(dst, yamlParser, zeInfoSections, outErrReason, outWarning);
    if (DecodeError::Success != zeInfoDecodeError) {
        return zeInfoDecodeError;
    }

    zeInfoDecodeError = decodeZeInfoKernels(dst, yamlParser, zeInfoSections, outErrReason, outWarning);
    if (DecodeError::Success != zeInfoDecodeError) {
        return zeInfoDecodeError;
    }

    return DecodeError::Success;
}

DecodeError decodeZeInfoVersion(Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning) {
    if (false == zeInfoSections.version.empty()) {
        Types::Version zeInfoVersion;
        auto err = readZeInfoVersionFromZeInfo(zeInfoVersion, parser, *zeInfoSections.version[0], outErrReason, outWarning);
        if (DecodeError::Success != err) {
            return err;
        }
        err = validateZeInfoVersion(zeInfoVersion, outErrReason, outWarning);
        if (DecodeError::Success != err) {
            return err;
        }
    } else {
        outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : No version info provided (i.e. no " + Tags::version.str() + " entry in global scope of DeviceBinaryFormat::Zebin::.ze_info) - will use decoder's default : \'" + std::to_string(zeInfoDecoderVersion.major) + "." + std::to_string(zeInfoDecoderVersion.minor) + "\'\n");
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoGlobalHostAccessTable(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning) {
    if (false == zeInfoSections.globalHostAccessTable.empty()) {
        ZeInfoGlobalHostAccessTables globalHostAccessMapping;
        auto zeInfoErr = readZeInfoGlobalHostAceessTable(parser, *zeInfoSections.globalHostAccessTable[0], globalHostAccessMapping, "globalHostAccessTable", outErrReason, outWarning);
        if (DecodeError::Success != zeInfoErr) {
            return zeInfoErr;
        }
        dst.globalsDeviceToHostNameMap.reserve(globalHostAccessMapping.size());
        for (auto it = globalHostAccessMapping.begin(); it != globalHostAccessMapping.end(); it++) {
            dst.globalsDeviceToHostNameMap[it->deviceName] = it->hostName;
        }
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoFunctions(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning) {
    if (false == zeInfoSections.functions.empty()) {
        for (const auto &functionNd : parser.createChildrenRange(*zeInfoSections.functions[0])) {
            auto zeInfoErr = populateExternalFunctionsMetadata(dst, parser, functionNd, outErrReason, outWarning);
            if (DecodeError::Success != zeInfoErr) {
                return zeInfoErr;
            }
        }
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoKernels(ProgramInfo &dst, Yaml::YamlParser &parser, const ZeInfoSections &zeInfoSections, std::string &outErrReason, std::string &outWarning) {
    UNRECOVERABLE_IF(zeInfoSections.kernels.size() != 1U);
    for (const auto &kernelNd : parser.createChildrenRange(*zeInfoSections.kernels[0])) {
        auto kernelInfo = std::make_unique<KernelInfo>();
        auto zeInfoErr = decodeZeInfoKernelEntry(kernelInfo->kernelDescriptor, parser, kernelNd, dst.grfSize, dst.minScratchSpaceSize, outErrReason, outWarning);
        if (DecodeError::Success != zeInfoErr) {
            return zeInfoErr;
        }

        dst.kernelInfos.push_back(kernelInfo.release());
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoKernelEntry(NEO::KernelDescriptor &dst, NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &kernelNd, uint32_t grfSize, uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning) {
    ZeInfoKernelSections zeInfokernelSections;
    extractZeInfoKernelSections(yamlParser, kernelNd, zeInfokernelSections, ".ze_info", outWarning);
    auto extractError = validateZeInfoKernelSectionsCount(zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != extractError) {
        return extractError;
    }

    dst.kernelAttributes.binaryFormat = DeviceBinaryFormat::Zebin;
    dst.kernelMetadata.kernelName = yamlParser.readValueNoQuotes(*zeInfokernelSections.nameNd[0]).str();

    auto decodeError = decodeZeInfoKernelExecutionEnvironment(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelUserAttributes(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelDebugEnvironment(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelPerThreadPayloadArguments(dst, yamlParser, zeInfokernelSections, grfSize, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelPayloadArguments(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelInlineSamplers(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelPerThreadMemoryBuffers(dst, yamlParser, zeInfokernelSections, minScratchSpaceSize, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelExperimentalProperties(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    decodeError = decodeZeInfoKernelBindingTableEntries(dst, yamlParser, zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return decodeError;
    }

    if (dst.payloadMappings.bindingTable.numEntries > 0U) {
        generateSSHWithBindingTable(dst);
        DEBUG_BREAK_IF(dst.kernelAttributes.numArgsStateful > dst.payloadMappings.bindingTable.numEntries);
        dst.kernelAttributes.numArgsStateful = std::max(dst.kernelAttributes.numArgsStateful, static_cast<uint16_t>(dst.payloadMappings.bindingTable.numEntries));
    }

    if (dst.payloadMappings.samplerTable.numSamplers > 0U) {
        generateDSH(dst);
    }

    if (NEO::debugManager.flags.ZebinAppendElws.get()) {
        dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0] = dst.kernelAttributes.crossThreadDataSize;
        dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1] = dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0] + 4;
        dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2] = dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1] + 4;
        dst.kernelAttributes.crossThreadDataSize = alignUp(dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2] + 4, 32);
    }

    return DecodeError::Success;
}

DecodeError decodeZeInfoKernelExecutionEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    KernelExecutionEnvBaseT execEnv;
    auto execEnvErr = readZeInfoExecutionEnvironment(parser, *kernelSections.executionEnvNd[0], execEnv, dst.kernelMetadata.kernelName, outErrReason, outWarning);
    if (DecodeError::Success != execEnvErr) {
        return execEnvErr;
    }
    populateKernelExecutionEnvironment(dst, execEnv);
    return DecodeError::Success;
}

DecodeError readZeInfoExecutionEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExecutionEnvBaseT &outExecEnv, ConstStringRef context,
                                           std::string &outErrReason, std::string &outWarning) {
    bool validExecEnv = true;
    for (const auto &execEnvMetadataNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(execEnvMetadataNd);
        if (Tags::Kernel::ExecutionEnv::barrierCount == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.barrierCount, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::disableMidThreadPreemption == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.disableMidThreadPreemption, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::euThreadCount == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.euThreadCount, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::grfCount == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.grfCount, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::has4gbBuffers == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.has4GBBuffers, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasDpas == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasDpas, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasFenceForImageAccess == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasFenceForImageAccess, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasGlobalAtomics == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasGlobalAtomics, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasMultiScratchSpaces == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasMultiScratchSpaces, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasNoStatelessWrite == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasNoStatelessWrite, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasStackCalls == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasStackCalls, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasRTCalls == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasRTCalls, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hwPreemptionMode == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hwPreemptionMode, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::inlineDataPayloadSize == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.inlineDataPayloadSize, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::offsetToSkipPerThreadDataLoad == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.offsetToSkipPerThreadDataLoad, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::offsetToSkipSetFfidGp == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.offsetToSkipSetFfidGp, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::requiredSubGroupSize == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.requiredSubGroupSize, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::requiredWorkGroupSize == key) {
            validExecEnv &= readZeInfoValueCollectionChecked(outExecEnv.requiredWorkGroupSize, parser, execEnvMetadataNd, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::requireDisableEUFusion == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.requireDisableEUFusion, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::simdSize == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.simdSize, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::slmSize == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.slmSize, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::subgroupIndependentForwardProgress == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.subgroupIndependentForwardProgress, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::workGroupWalkOrderDimensions == key) {
            validExecEnv &= readZeInfoValueCollectionChecked(outExecEnv.workgroupWalkOrderDimensions, parser, execEnvMetadataNd, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::threadSchedulingMode == key) {
            validExecEnv &= readZeInfoEnumChecked(parser, execEnvMetadataNd, outExecEnv.threadSchedulingMode, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::indirectStatelessCount == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.indirectStatelessCount, context, outErrReason);
        } else if (Tags::Kernel::ExecutionEnv::hasSample == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasSample, context, outErrReason);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
        }
    }

    if (false == validExecEnv) {
        return DecodeError::InvalidBinary;
    }

    if ((outExecEnv.simdSize != 1) && (outExecEnv.simdSize != 8) && (outExecEnv.simdSize != 16) && (outExecEnv.simdSize != 32)) {
        outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Invalid simd size : " + std::to_string(outExecEnv.simdSize) + " in context of : " + context.str() + ". Expected 1, 8, 16 or 32. Got : " + std::to_string(outExecEnv.simdSize) + "\n");
        return DecodeError::InvalidBinary;
    }

    return DecodeError::Success;
}

void populateKernelExecutionEnvironment(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv) {
    dst.entryPoints.skipPerThreadDataLoad = execEnv.offsetToSkipPerThreadDataLoad;
    dst.entryPoints.skipSetFFIDGP = execEnv.offsetToSkipSetFfidGp;
    dst.kernelAttributes.flags.passInlineData = (execEnv.inlineDataPayloadSize != 0);
    dst.kernelAttributes.flags.requiresDisabledMidThreadPreemption = execEnv.disableMidThreadPreemption;
    dst.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress = execEnv.subgroupIndependentForwardProgress;
    dst.kernelAttributes.flags.requiresDisabledEUFusion = execEnv.requireDisableEUFusion;
    dst.kernelAttributes.flags.useGlobalAtomics = execEnv.hasGlobalAtomics;
    dst.kernelAttributes.flags.useStackCalls = execEnv.hasStackCalls;
    dst.kernelAttributes.flags.hasRTCalls = execEnv.hasRTCalls;
    dst.kernelAttributes.flags.usesFencesForReadWriteImages = execEnv.hasFenceForImageAccess;
    dst.kernelAttributes.flags.usesSystolicPipelineSelectMode = execEnv.hasDpas;
    dst.kernelAttributes.flags.usesStatelessWrites = (false == execEnv.hasNoStatelessWrite);
    dst.kernelAttributes.flags.hasSample = execEnv.hasSample;
    dst.kernelAttributes.barrierCount = execEnv.barrierCount;
    dst.kernelAttributes.bufferAddressingMode = (execEnv.has4GBBuffers) ? KernelDescriptor::Stateless : KernelDescriptor::BindfulAndStateless;
    dst.kernelAttributes.inlineDataPayloadSize = static_cast<uint16_t>(execEnv.inlineDataPayloadSize);
    dst.kernelAttributes.numGrfRequired = execEnv.grfCount;
    dst.kernelAttributes.requiredWorkgroupSize[0] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[0]);
    dst.kernelAttributes.requiredWorkgroupSize[1] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[1]);
    dst.kernelAttributes.requiredWorkgroupSize[2] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[2]);
    dst.kernelAttributes.simdSize = execEnv.simdSize;
    dst.kernelAttributes.slmInlineSize = execEnv.slmSize;
    dst.kernelAttributes.workgroupWalkOrder[0] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[0]);
    dst.kernelAttributes.workgroupWalkOrder[1] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[1]);
    dst.kernelAttributes.workgroupWalkOrder[2] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[2]);
    dst.kernelAttributes.hasIndirectStatelessAccess = (execEnv.indirectStatelessCount > 0);
    dst.kernelAttributes.numThreadsRequired = static_cast<uint32_t>(execEnv.euThreadCount);

    using ThreadSchedulingMode = Types::Kernel::ExecutionEnv::ThreadSchedulingMode;
    switch (execEnv.threadSchedulingMode) {
    default:
        dst.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeAgeBased:
        dst.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeRoundRobin:
        dst.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeRoundRobinStall:
        dst.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        break;
    }
}

DecodeError decodeZeInfoKernelUserAttributes(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.attributesNd.empty()) {
        KernelAttributesBaseT attributes;
        auto attributeErr = readZeInfoAttributes(parser, *kernelSections.attributesNd[0], attributes, dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != attributeErr) {
            return attributeErr;
        }
        populateKernelSourceAttributes(dst, attributes);
    }
    return DecodeError::Success;
}

DecodeError readZeInfoAttributes(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelAttributesBaseT &outAttributes, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    namespace AttributeTypes = Types::Kernel::Attributes;
    bool validAttributes = true;
    for (const auto &attributesMetadataNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(attributesMetadataNd);
        if (key == Tags::Kernel::Attributes::intelReqdSubgroupSize) {
            outAttributes.intelReqdSubgroupSize = AttributeTypes::Defaults::intelReqdSubgroupSize;
            validAttributes &= readZeInfoValueChecked(parser, attributesMetadataNd, *outAttributes.intelReqdSubgroupSize, context, outErrReason);
        } else if (key == Tags::Kernel::Attributes::intelReqdWorkgroupWalkOrder) {
            outAttributes.intelReqdWorkgroupWalkOrder = AttributeTypes::Defaults::intelReqdWorkgroupWalkOrder;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.intelReqdWorkgroupWalkOrder, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == Tags::Kernel::Attributes::reqdWorkgroupSize) {
            outAttributes.reqdWorkgroupSize = AttributeTypes::Defaults::reqdWorkgroupSize;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.reqdWorkgroupSize, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == Tags::Kernel::Attributes::workgroupSizeHint) {
            outAttributes.workgroupSizeHint = AttributeTypes::Defaults::workgroupSizeHint;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.workgroupSizeHint, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == Tags::Kernel::Attributes::invalidKernel) {
            outAttributes.invalidKernel = parser.readValue(attributesMetadataNd);
        } else if (key == Tags::Kernel::Attributes::vecTypeHint) {
            outAttributes.vecTypeHint = parser.readValue(attributesMetadataNd);
        } else if (key.contains(Tags::Kernel::Attributes::hintSuffix.data())) {
            outAttributes.otherHints.push_back({key, parser.readValue(attributesMetadataNd)});
        } else {
            outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown attribute entry \"" + key.str() + "\" in context of " + context.str() + "\n");
            validAttributes = false;
        }
    }
    return validAttributes ? DecodeError::Success : DecodeError::InvalidBinary;
}

std::string attributeToString(const int32_t &attribute) {
    return std::to_string(attribute);
}
std::string attributeToString(const std::array<int32_t, 3> &attribute) {
    return std::to_string(attribute[0]) + "," + std::to_string(attribute[1]) + "," + std::to_string(attribute[2]);
}
std::string attributeToString(ConstStringRef attribute) {
    return attribute.str();
}
void appendAttribute(std::string &dst, const std::string &attributeName, const std::string &attributeValue) {
    if (dst.empty() == false) {
        dst.append(" ");
    }
    dst.append(attributeName + "(" + attributeValue + ")");
}
template <typename T>
void appendAttributeIfSet(std::string &dst, ConstStringRef attributeName, const std::optional<T> &attributeValue) {
    if (attributeValue) {
        appendAttribute(dst, attributeName.str(), attributeToString(*attributeValue));
    }
}

void populateKernelSourceAttributes(NEO::KernelDescriptor &dst, const KernelAttributesBaseT &attributes) {
    namespace AttributeTags = Tags::Kernel::Attributes;
    namespace AttributeTypes = Types::Kernel::Attributes;
    auto &languageAttributes = dst.kernelMetadata.kernelLanguageAttributes;

    for (auto &hint : attributes.otherHints) {
        appendAttribute(languageAttributes, hint.first.str(), hint.second.str());
    }

    appendAttributeIfSet(languageAttributes, AttributeTags::intelReqdSubgroupSize, attributes.intelReqdSubgroupSize);
    appendAttributeIfSet(languageAttributes, AttributeTags::intelReqdWorkgroupWalkOrder, attributes.intelReqdWorkgroupWalkOrder);
    appendAttributeIfSet(languageAttributes, AttributeTags::reqdWorkgroupSize, attributes.reqdWorkgroupSize);
    appendAttributeIfSet(languageAttributes, AttributeTags::workgroupSizeHint, attributes.workgroupSizeHint);
    appendAttributeIfSet(languageAttributes, AttributeTags::vecTypeHint, attributes.vecTypeHint);
    appendAttributeIfSet(languageAttributes, AttributeTags::invalidKernel, attributes.invalidKernel);
    dst.kernelAttributes.flags.isInvalid = attributes.invalidKernel.has_value();

    dst.kernelMetadata.requiredSubGroupSize = static_cast<uint8_t>(attributes.intelReqdSubgroupSize.value_or(0U));
}

DecodeError decodeZeInfoKernelDebugEnvironment(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.debugEnvNd.empty()) {
        KernelDebugEnvBaseT debugEnv;
        auto debugEnvErr = readZeInfoDebugEnvironment(parser, *kernelSections.debugEnvNd[0], debugEnv, dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != debugEnvErr) {
            return debugEnvErr;
        }
        populateKernelDebugEnvironment(dst, debugEnv);
    }
    return DecodeError::Success;
}

DecodeError readZeInfoDebugEnvironment(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelDebugEnvBaseT &outDebugEnv, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validDebugEnv = true;
    for (const auto &debugEnvNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(debugEnvNd);
        if (Tags::Kernel::DebugEnv::debugSurfaceBTI == key) {
            validDebugEnv &= readZeInfoValueChecked(parser, debugEnvNd, outDebugEnv.debugSurfaceBTI, context, outErrReason);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
        }
    }
    return validDebugEnv ? DecodeError::Success : DecodeError::InvalidBinary;
}

void populateKernelDebugEnvironment(NEO::KernelDescriptor &dst, const KernelDebugEnvBaseT &debugEnv) {
    if (debugEnv.debugSurfaceBTI == 0) {
        setSSHOffsetBasedOnBti(dst.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful, 0U, dst.payloadMappings.bindingTable.numEntries);
    }
}

DecodeError decodeZeInfoKernelPerThreadPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.perThreadPayloadArgumentsNd.empty()) {
        KernelPerThreadPayloadArguments perThreadPayloadArguments;
        auto perThreadPayloadArgsErr = readZeInfoPerThreadPayloadArguments(parser, *kernelSections.perThreadPayloadArgumentsNd[0], perThreadPayloadArguments,
                                                                           dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != perThreadPayloadArgsErr) {
            return perThreadPayloadArgsErr;
        }
        for (const auto &arg : perThreadPayloadArguments) {
            auto decodeErr = populateKernelPerThreadPayloadArgument(dst, arg, grfSize, outErrReason, outWarning);
            if (DecodeError::Success != decodeErr) {
                return decodeErr;
            }
        }
    }
    return DecodeError::Success;
}

DecodeError readZeInfoPerThreadPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadPayloadArguments &outPerThreadPayloadArguments, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validPerThreadPayload = true;
    for (const auto &perThreadPayloadArgumentNd : parser.createChildrenRange(node)) {
        outPerThreadPayloadArguments.resize(outPerThreadPayloadArguments.size() + 1);
        auto &perThreadPayloadArgMetadata = *outPerThreadPayloadArguments.rbegin();
        ConstStringRef argTypeStr;
        for (const auto &perThreadPayloadArgumentMemberNd : parser.createChildrenRange(perThreadPayloadArgumentNd)) {
            auto key = parser.readKey(perThreadPayloadArgumentMemberNd);
            if (Tags::Kernel::PerThreadPayloadArgument::argType == key) {
                argTypeStr = parser.readValue(perThreadPayloadArgumentMemberNd);
                validPerThreadPayload &= readZeInfoEnumChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.argType, context, outErrReason);
            } else if (Tags::Kernel::PerThreadPayloadArgument::size == key) {
                validPerThreadPayload &= readZeInfoValueChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.size, context, outErrReason);
            } else if (Tags::Kernel::PerThreadPayloadArgument::offset == key) {
                validPerThreadPayload &= readZeInfoValueChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.offset, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for per-thread payload argument in context of " + context.str() + "\n");
            }
        }
        if (0 == perThreadPayloadArgMetadata.size) {
            outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Skippinig 0-size per-thread argument of type : " + argTypeStr.str() + " in context of " + context.str() + "\n");
            outPerThreadPayloadArguments.pop_back();
        }
    }

    return validPerThreadPayload ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError populateKernelPerThreadPayloadArgument(KernelDescriptor &dst, const KernelPerThreadPayloadArgBaseT &src, const uint32_t grfSize, std::string &outErrReason, std::string &outWarning) {
    switch (src.argType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid arg type in per-thread data section in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    case Types::Kernel::ArgTypeLocalId: {
        if (src.offset != 0) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid offset for argument of type " + Tags::Kernel::PerThreadPayloadArgument::ArgType::localId.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0.\n");
            return DecodeError::InvalidBinary;
        }
        using LocalIdT = uint16_t;

        uint32_t singleChannelIndicesCount = (dst.kernelAttributes.simdSize == 32 ? 32 : 16);
        uint32_t singleChannelBytes = singleChannelIndicesCount * sizeof(LocalIdT);
        UNRECOVERABLE_IF(0 == grfSize);
        singleChannelBytes = alignUp(singleChannelBytes, grfSize);
        auto tupleSize = (src.size / singleChannelBytes);
        switch (tupleSize) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + Tags::Kernel::PerThreadPayloadArgument::ArgType::localId.str() + " in context of : " + dst.kernelMetadata.kernelName + ". For simd=" + std::to_string(dst.kernelAttributes.simdSize) + " expected : " + std::to_string(singleChannelBytes) + " or " + std::to_string(singleChannelBytes * 2) + " or " + std::to_string(singleChannelBytes * 3) + ". Got : " + std::to_string(src.size) + " \n");
            return DecodeError::InvalidBinary;
        case 1:
        case 2:
        case 3:
            dst.kernelAttributes.numLocalIdChannels = static_cast<uint8_t>(tupleSize);
            break;
        }
        dst.kernelAttributes.localId[0] = tupleSize > 0;
        dst.kernelAttributes.localId[1] = tupleSize > 1;
        dst.kernelAttributes.localId[2] = tupleSize > 2;
        dst.kernelAttributes.perThreadDataSize = dst.kernelAttributes.simdSize;
        dst.kernelAttributes.perThreadDataSize *= sizeof(LocalIdT);
        dst.kernelAttributes.perThreadDataSize = alignUp(dst.kernelAttributes.perThreadDataSize, grfSize);
        dst.kernelAttributes.perThreadDataSize *= dst.kernelAttributes.numLocalIdChannels;
        break;
    }
    case Types::Kernel::ArgTypePackedLocalIds: {
        if (src.offset != 0) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Unhandled offset for argument of type " + Tags::Kernel::PerThreadPayloadArgument::ArgType::packedLocalIds.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0.\n");
            return DecodeError::InvalidBinary;
        }
        using LocalIdT = uint16_t;
        auto tupleSize = src.size / sizeof(LocalIdT);
        switch (tupleSize) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + Tags::Kernel::PerThreadPayloadArgument::ArgType::packedLocalIds.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected : " + std::to_string(sizeof(LocalIdT)) + " or " + std::to_string(sizeof(LocalIdT) * 2) + " or " + std::to_string(sizeof(LocalIdT) * 3) + ". Got : " + std::to_string(src.size) + " \n");
            return DecodeError::InvalidBinary;

        case 1:
        case 2:
        case 3:
            dst.kernelAttributes.numLocalIdChannels = static_cast<uint8_t>(tupleSize);
            break;
        }
        dst.kernelAttributes.localId[0] = tupleSize > 0;
        dst.kernelAttributes.localId[1] = tupleSize > 1;
        dst.kernelAttributes.localId[2] = tupleSize > 2;
        dst.kernelAttributes.simdSize = 1;
        dst.kernelAttributes.perThreadDataSize = dst.kernelAttributes.simdSize;
        dst.kernelAttributes.perThreadDataSize *= dst.kernelAttributes.numLocalIdChannels;
        dst.kernelAttributes.perThreadDataSize *= sizeof(LocalIdT);
        break;
    }
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoKernelPayloadArguments(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.payloadArgumentsNd.empty()) {
        int32_t maxArgumentIndex = -1;
        KernelPayloadArguments payloadArguments;
        auto payloadArgsErr = readZeInfoPayloadArguments(parser, *kernelSections.payloadArgumentsNd[0], payloadArguments, maxArgumentIndex,
                                                         dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != payloadArgsErr) {
            return payloadArgsErr;
        }

        dst.payloadMappings.explicitArgs.resize(maxArgumentIndex + 1);
        dst.kernelAttributes.numArgsToPatch = maxArgumentIndex + 1;

        bool bindlessBufferAccess = false;
        bool bindlessImageAccess = false;
        bool bindfulBufferAccess = false;
        bool bindfulImageAccess = false;

        for (const auto &arg : payloadArguments) {
            auto decodeErr = populateKernelPayloadArgument(dst, arg, outErrReason, outWarning);
            if (DecodeError::Success != decodeErr) {
                return decodeErr;
            }

            if (arg.addrmode == Types::Kernel::PayloadArgument::MemoryAddressingModeBindless) {
                if (dst.payloadMappings.explicitArgs[arg.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                    bindlessBufferAccess = true;
                } else if (dst.payloadMappings.explicitArgs[arg.argIndex].is<NEO::ArgDescriptor::ArgTImage>()) {
                    bindlessImageAccess = true;
                }
            } else if (arg.addrmode == Types::Kernel::PayloadArgument::MemoryAddressingModeStateful) {
                if (dst.payloadMappings.explicitArgs[arg.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                    bindfulBufferAccess = true;
                } else if (dst.payloadMappings.explicitArgs[arg.argIndex].is<NEO::ArgDescriptor::ArgTImage>()) {
                    bindfulImageAccess = true;
                }
            }
        }

        if ((bindlessBufferAccess && bindfulBufferAccess) ||
            (bindlessImageAccess && bindfulImageAccess)) {
            outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : bindless and bindful addressing modes must not be mixed.\n");
            return DecodeError::InvalidBinary;
        }

        if (bindlessBufferAccess) {
            dst.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;
        }
        if (bindlessImageAccess) {
            dst.kernelAttributes.imageAddressingMode = KernelDescriptor::Bindless;
        }
        dst.kernelAttributes.crossThreadDataSize = static_cast<uint16_t>(alignUp(dst.kernelAttributes.crossThreadDataSize, 32));
    }
    return DecodeError::Success;
}

DecodeError readZeInfoPayloadArguments(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPayloadArguments &outPayloadArguments, int32_t &outMaxPayloadArgumentIndex, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validPayload = true;
    for (const auto &payloadArgumentNd : parser.createChildrenRange(node)) {
        outPayloadArguments.resize(outPayloadArguments.size() + 1);
        auto &payloadArgMetadata = *outPayloadArguments.rbegin();
        for (const auto &payloadArgumentMemberNd : parser.createChildrenRange(payloadArgumentNd)) {
            auto key = parser.readKey(payloadArgumentMemberNd);
            if (Tags::Kernel::PayloadArgument::argType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.argType, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::argIndex == key) {
                validPayload &= parser.readValueChecked(payloadArgumentMemberNd, payloadArgMetadata.argIndex);
                outMaxPayloadArgumentIndex = std::max<int32_t>(outMaxPayloadArgumentIndex, payloadArgMetadata.argIndex);
            } else if (Tags::Kernel::PayloadArgument::offset == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.offset, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::size == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.size, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::addrmode == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.addrmode, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::addrspace == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.addrspace, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::accessType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.accessType, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::samplerIndex == key) {
                validPayload &= parser.readValueChecked(payloadArgumentMemberNd, payloadArgMetadata.samplerIndex);
            } else if (Tags::Kernel::PayloadArgument::sourceOffset == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.sourceOffset, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::slmArgAlignment == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.slmArgAlignment, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::imageType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.imageType, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::imageTransformable == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.imageTransformable, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::samplerType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.samplerType, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::isPipe == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.isPipe, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::isPtr == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.isPtr, context, outErrReason);
            } else if (Tags::Kernel::PayloadArgument::btiValue == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.btiValue, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for payload argument in context of " + context.str() + "\n");
            }
        }
    }
    return validPayload ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError populateKernelPayloadArgument(NEO::KernelDescriptor &dst, const KernelPayloadArgBaseT &src, std::string &outErrReason, std::string &outWarning) {
    if (src.offset != Types::Kernel::PayloadArgument::Defaults::offset) {
        dst.kernelAttributes.crossThreadDataSize = std::max<uint16_t>(dst.kernelAttributes.crossThreadDataSize, static_cast<uint16_t>(src.offset + src.size));
    }

    auto &explicitArgs = dst.payloadMappings.explicitArgs;
    auto getVmeDescriptor = [&src, &dst]() {
        auto &argsExt = dst.payloadMappings.explicitArgsExtendedDescriptors;
        argsExt.resize(dst.payloadMappings.explicitArgs.size());
        if (argsExt[src.argIndex] == nullptr) {
            argsExt[src.argIndex] = std::make_unique<ArgDescVme>();
        }
        return static_cast<ArgDescVme *>(argsExt[src.argIndex].get());
    };

    const auto &kernelName = dst.kernelMetadata.kernelName;
    auto populateArgPointerStateless = [&src](auto &arg) {
        arg.stateless = src.offset;
        arg.pointerSize = src.size;
        return DecodeError::Success;
    };
    auto populateArgToInlineData = [&src](auto &arg) {
        arg.offset = src.offset;
        arg.pointerSize = src.size;
        return DecodeError::Success;
    };
    auto populateWithOffset = [&src](auto &dst) {
        dst = src.offset;
        return DecodeError::Success;
    };
    auto populateWithOffsetChecked = [&src, &kernelName, &outErrReason](auto &dst, size_t size, ConstStringRef typeName) {
        if (size != static_cast<size_t>(src.size)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + typeName.str() + " in context of : " + kernelName + ". Expected 4. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        dst = src.offset;
        return DecodeError::Success;
    };
    auto populateArgVec = [&src, &outErrReason, &kernelName](auto &dst, ConstStringRef typeName) {
        if (false == setVecArgIndicesBasedOnSize<uint32_t>(dst, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + typeName.str() + " in context of : " + kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        return DecodeError::Success;
    };

    switch (src.argType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid arg type in cross thread data section in context of : " + kernelName + ".\n");
        return DecodeError::InvalidBinary; // unsupported

    case Types::Kernel::ArgTypeArgBypointer: {
        auto &arg = dst.payloadMappings.explicitArgs[src.argIndex];
        auto &argTraits = arg.getTraits();
        switch (src.addrspace) {
        default:
            UNRECOVERABLE_IF(Types::Kernel::PayloadArgument::AddressSpaceUnknown != src.addrspace);
            argTraits.addressQualifier = KernelArgMetadata::AddrUnknown;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case Types::Kernel::PayloadArgument::AddressSpaceGlobal:
            argTraits.addressQualifier = KernelArgMetadata::AddrGlobal;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case Types::Kernel::PayloadArgument::AddressSpaceLocal:
            argTraits.addressQualifier = KernelArgMetadata::AddrLocal;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case Types::Kernel::PayloadArgument::AddressSpaceConstant:
            argTraits.addressQualifier = KernelArgMetadata::AddrConstant;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case Types::Kernel::PayloadArgument::AddressSpaceImage: {
            auto &argAsImage = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescImage>(true);
            if (src.imageType != Types::Kernel::PayloadArgument::ImageTypeMax) {
                argAsImage.imageType = static_cast<NEOImageType>(src.imageType);
            }

            auto &extendedInfo = dst.payloadMappings.explicitArgs[src.argIndex].getExtendedTypeInfo();
            extendedInfo.isMediaImage = (src.imageType == Types::Kernel::PayloadArgument::ImageType::ImageType2DMedia);
            extendedInfo.isMediaBlockImage = (src.imageType == Types::Kernel::PayloadArgument::ImageType::ImageType2DMediaBlock);
            extendedInfo.isTransformable = src.imageTransformable;
            dst.kernelAttributes.flags.usesImages = true;
        } break;
        case Types::Kernel::PayloadArgument::AddressSpaceSampler: {
            using SamplerType = Types::Kernel::PayloadArgument::SamplerType;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>(true);
            auto &extendedInfo = arg.getExtendedTypeInfo();
            extendedInfo.isAccelerator = (src.samplerType == SamplerType::SamplerTypeVME) ||
                                         (src.samplerType == SamplerType::SamplerTypeVE) ||
                                         (src.samplerType == SamplerType::SamplerTypeVD);
            const bool usesVme = src.samplerType == SamplerType::SamplerTypeVME;
            extendedInfo.hasVmeExtendedDescriptor = usesVme;
            dst.kernelAttributes.flags.usesVme = usesVme;
            dst.kernelAttributes.flags.usesSamplers = true;
        } break;
        }

        switch (src.accessType) {
        default:
            UNRECOVERABLE_IF(argTraits.accessQualifier != Types::Kernel::PayloadArgument::AccessTypeUnknown);
            argTraits.accessQualifier = KernelArgMetadata::AccessUnknown;
            break;
        case Types::Kernel::PayloadArgument::AccessTypeReadonly:
            argTraits.accessQualifier = KernelArgMetadata::AccessReadOnly;
            break;
        case Types::Kernel::PayloadArgument::AccessTypeReadwrite:
            argTraits.accessQualifier = KernelArgMetadata::AccessReadWrite;
            break;
        case Types::Kernel::PayloadArgument::AccessTypeWriteonly:
            argTraits.accessQualifier = KernelArgMetadata::AccessWriteOnly;
            break;
        }

        argTraits.argByValSize = sizeof(void *);
        if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>().accessedUsingStatelessAddressingMode = false;
            if (src.isPipe) {
                argTraits.typeQualifiers.pipeQual = true;
            }
        }
        switch (src.addrmode) {
        default:
            outErrReason.append("Invalid or missing memory addressing mode for arg idx : " + std::to_string(src.argIndex) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
            return DecodeError::InvalidBinary;
        case Types::Kernel::PayloadArgument::MemoryAddressingModeStateful:
            if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTSampler>()) {
                static constexpr auto maxSamplerStateSize = 16U;
                static constexpr auto maxIndirectSamplerStateSize = 64U;
                auto &sampler = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>();
                sampler.bindful = maxIndirectSamplerStateSize + maxSamplerStateSize * src.samplerIndex;
                dst.payloadMappings.samplerTable.numSamplers = std::max<uint8_t>(dst.payloadMappings.samplerTable.numSamplers, static_cast<uint8_t>(src.samplerIndex + 1));
            } else {
                dst.kernelAttributes.numArgsStateful++;
            }
            break;
        case Types::Kernel::PayloadArgument::MemoryAddressingModeStateless:
            if (false == dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                outErrReason.append("Invalid or missing memory addressing " + Tags::Kernel::PayloadArgument::MemoryAddressingMode::stateless.str() + " for arg idx : " + std::to_string(src.argIndex) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
                return DecodeError::InvalidBinary;
            }
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).stateless = src.offset;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).pointerSize = src.size;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).accessedUsingStatelessAddressingMode = true;
            break;
        case Types::Kernel::PayloadArgument::MemoryAddressingModeBindless:
            if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).bindless = src.offset;
            } else if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTImage>()) {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescImage>(false).bindless = src.offset;
            } else {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>(false).bindless = src.offset;
            }
            dst.kernelAttributes.numArgsStateful++;
            break;
        case Types::Kernel::PayloadArgument::MemoryAddressingModeSharedLocalMemory:
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).slmOffset = src.offset;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).requiredSlmAlignment = src.slmArgAlignment;
            break;
        }

        return DecodeError::Success;
    }

    case Types::Kernel::ArgTypeArgByvalue: {
        auto &argAsValue = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescValue>(true);
        ArgDescValue::Element valueElement;
        valueElement.sourceOffset = 0;
        valueElement.isPtr = src.isPtr;
        if (src.sourceOffset != -1) {
            valueElement.sourceOffset = src.sourceOffset;
        } else if (argAsValue.elements.empty() == false) {
            outErrReason.append("Missing source offset value for element in argByValue\n");
            return DecodeError::InvalidBinary;
        }
        valueElement.offset = src.offset;
        valueElement.size = src.size;
        argAsValue.elements.push_back(valueElement);
        return DecodeError::Success;
    }

    case Types::Kernel::ArgTypeBufferAddress:
        return populateArgPointerStateless(dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true));

    case Types::Kernel::ArgTypeBufferOffset:
        return populateWithOffsetChecked(dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true).bufferOffset, 4U, Tags::Kernel::PayloadArgument::ArgType::bufferOffset);

    case Types::Kernel::ArgTypeLocalSize:
        return populateArgVec(dst.payloadMappings.dispatchTraits.localWorkSize, Tags::Kernel::PayloadArgument::ArgType::localSize);

    case Types::Kernel::ArgTypeGlobalIdOffset:
        return populateArgVec(dst.payloadMappings.dispatchTraits.globalWorkOffset, Tags::Kernel::PayloadArgument::ArgType::globalIdOffset);

    case Types::Kernel::ArgTypeGroupCount:
        return populateArgVec(dst.payloadMappings.dispatchTraits.numWorkGroups, Tags::Kernel::PayloadArgument::ArgType::groupCount);

    case Types::Kernel::ArgTypeGlobalSize:
        return populateArgVec(dst.payloadMappings.dispatchTraits.globalWorkSize, Tags::Kernel::PayloadArgument::ArgType::globalSize);

    case Types::Kernel::ArgTypeEnqueuedLocalSize:
        return populateArgVec(dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize, Tags::Kernel::PayloadArgument::ArgType::enqueuedLocalSize);

    case Types::Kernel::ArgTypeWorkDimensions:
        return populateWithOffsetChecked(dst.payloadMappings.dispatchTraits.workDim, 4U, Tags::Kernel::PayloadArgument::ArgType::workDimensions);

    case Types::Kernel::ArgTypePrivateBaseStateless:
        return populateArgPointerStateless(dst.payloadMappings.implicitArgs.privateMemoryAddress);

    case Types::Kernel::ArgTypeScratchPointer:
        return populateArgToInlineData(dst.payloadMappings.implicitArgs.scratchPointerAddress);

    case Types::Kernel::ArgTypeIndirectDataPointer:
        return populateArgToInlineData(dst.payloadMappings.implicitArgs.indirectDataPointerAddress);

    case Types::Kernel::ArgTypePrintfBuffer:
        dst.kernelAttributes.flags.usesPrintf = true;
        return populateArgPointerStateless(dst.payloadMappings.implicitArgs.printfSurfaceAddress);

    case Types::Kernel::ArgTypeAssertBuffer:
        dst.kernelAttributes.flags.usesAssert = true;
        return populateArgPointerStateless(dst.payloadMappings.implicitArgs.assertBufferAddress);

    case Types::Kernel::ArgTypeSyncBuffer:
        dst.kernelAttributes.flags.usesSyncBuffer = true;
        return populateArgPointerStateless(dst.payloadMappings.implicitArgs.syncBufferAddress);

    case Types::Kernel::ArgTypeImplicitArgBuffer:
        dst.kernelAttributes.flags.requiresImplicitArgs = true;
        return populateWithOffset(dst.payloadMappings.implicitArgs.implicitArgsBuffer);

    case Types::Kernel::ArgTypeRtGlobalBuffer:
        dst.kernelAttributes.flags.hasRTCalls = true;
        return populateArgPointerStateless(dst.payloadMappings.implicitArgs.rtDispatchGlobals);

    case Types::Kernel::ArgTypeDataConstBuffer:
        if (src.offset != Types::Kernel::PayloadArgument::Defaults::offset) {
            populateArgPointerStateless(dst.payloadMappings.implicitArgs.globalConstantsSurfaceAddress);
        }
        setSSHOffsetBasedOnBti(dst.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful, src.btiValue, dst.payloadMappings.bindingTable.numEntries);
        return DecodeError::Success;

    case Types::Kernel::ArgTypeDataGlobalBuffer:
        if (src.offset != Types::Kernel::PayloadArgument::Defaults::offset) {
            populateArgPointerStateless(dst.payloadMappings.implicitArgs.globalVariablesSurfaceAddress);
        }
        setSSHOffsetBasedOnBti(dst.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful, src.btiValue, dst.payloadMappings.bindingTable.numEntries);
        return DecodeError::Success;

    case Types::Kernel::ArgTypeImageHeight:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgHeight);

    case Types::Kernel::ArgTypeImageWidth:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgWidth);

    case Types::Kernel::ArgTypeImageDepth:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgDepth);

    case Types::Kernel::ArgTypeImageChannelDataType:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.channelDataType);

    case Types::Kernel::ArgTypeImageChannelOrder:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.channelOrder);

    case Types::Kernel::ArgTypeImageArraySize:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.arraySize);

    case Types::Kernel::ArgTypeImageNumSamples:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.numSamples);

    case Types::Kernel::ArgTypeImageMipLevels:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.numMipLevels);

    case Types::Kernel::ArgTypeImageFlatBaseOffset:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatBaseOffset);

    case Types::Kernel::ArgTypeImageFlatWidth:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatWidth);

    case Types::Kernel::ArgTypeImageFlatHeight:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatHeight);

    case Types::Kernel::ArgTypeImageFlatPitch:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatPitch);

    case Types::Kernel::ArgTypeSamplerAddrMode:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerAddressingMode);

    case Types::Kernel::ArgTypeSamplerNormCoords:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerNormalizedCoords);

    case Types::Kernel::ArgTypeSamplerSnapWa:
        return populateWithOffset(explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerSnapWa);

    case Types::Kernel::ArgTypeVmeMbBlockType:
        return populateWithOffset(getVmeDescriptor()->mbBlockType);

    case Types::Kernel::ArgTypeVmeSubpixelMode:
        return populateWithOffset(getVmeDescriptor()->subpixelMode);

    case Types::Kernel::ArgTypeVmeSadAdjustMode:
        return populateWithOffset(getVmeDescriptor()->sadAdjustMode);

    case Types::Kernel::ArgTypeVmeSearchPathType:
        return populateWithOffset(getVmeDescriptor()->searchPathType);
    }

    UNREACHABLE();
    return DecodeError::InvalidBinary;
}

DecodeError decodeZeInfoKernelInlineSamplers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.inlineSamplersNd.empty()) {
        KernelInlineSamplers inlineSamplers{};
        auto decodeErr = readZeInfoInlineSamplers(parser, *kernelSections.inlineSamplersNd[0], inlineSamplers,
                                                  dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }

        for (const auto &inlineSampler : inlineSamplers) {
            auto decodeErr = populateKernelInlineSampler(dst, inlineSampler, outErrReason, outWarning);
            if (DecodeError::Success != decodeErr) {
                return decodeErr;
            }
        }
    }
    return DecodeError::Success;
}

DecodeError readZeInfoInlineSamplers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelInlineSamplers &outInlineSamplers, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validInlineSamplers = true;
    for (const auto &inlineSamplerNd : parser.createChildrenRange(node)) {
        outInlineSamplers.resize(outInlineSamplers.size() + 1);
        auto &inlineSampler = *outInlineSamplers.rbegin();
        for (const auto &inlineSamplerMemberNd : parser.createChildrenRange(inlineSamplerNd)) {
            namespace Tags = Tags::Kernel::InlineSamplers;
            auto key = parser.readKey(inlineSamplerMemberNd);
            if (Tags::samplerIndex == key) {
                validInlineSamplers &= readZeInfoValueChecked(parser, inlineSamplerMemberNd, inlineSampler.samplerIndex, context, outErrReason);
            } else if (Tags::addrMode == key) {
                validInlineSamplers &= readZeInfoEnumChecked(parser, inlineSamplerMemberNd, inlineSampler.addrMode, context, outErrReason);
            } else if (Tags::filterMode == key) {
                validInlineSamplers &= readZeInfoEnumChecked(parser, inlineSamplerMemberNd, inlineSampler.filterMode, context, outErrReason);
            } else if (Tags::normalized == key) {
                validInlineSamplers &= readZeInfoValueChecked(parser, inlineSamplerMemberNd, inlineSampler.normalized, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for inline sampler in context of " + context.str() + "\n");
            }
        }
    }

    return validInlineSamplers ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError populateKernelInlineSampler(KernelDescriptor &dst, const KernelInlineSamplerBaseT &src, std::string &outErrReason, std::string &outWarning) {
    NEO::KernelDescriptor::InlineSampler inlineSampler = {};

    if (src.samplerIndex == -1) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler index (must be >= 0) in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.samplerIndex = src.samplerIndex;

    using AddrModeZeInfo = Types::Kernel::InlineSamplers::AddrModeT;
    using AddrModeDescriptor = NEO::KernelDescriptor::InlineSampler::AddrMode;
    constexpr LookupArray<AddrModeZeInfo, AddrModeDescriptor, 5> addrModes({{{AddrModeZeInfo::None, AddrModeDescriptor::None},
                                                                             {AddrModeZeInfo::Repeat, AddrModeDescriptor::Repeat},
                                                                             {AddrModeZeInfo::ClampEdge, AddrModeDescriptor::ClampEdge},
                                                                             {AddrModeZeInfo::ClampBorder, AddrModeDescriptor::ClampBorder},
                                                                             {AddrModeZeInfo::Mirror, AddrModeDescriptor::Mirror}}});
    auto addrMode = addrModes.find(src.addrMode);
    if (addrMode.has_value() == false) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler addressing mode in context of : " + dst.kernelMetadata.kernelName + "\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.addrMode = *addrMode;

    using FilterModeZeInfo = Types::Kernel::InlineSamplers::FilterModeT;
    using FilterModeDescriptor = NEO::KernelDescriptor::InlineSampler::FilterMode;
    constexpr LookupArray<FilterModeZeInfo, FilterModeDescriptor, 2> filterModes({{{FilterModeZeInfo::Nearest, FilterModeDescriptor::Nearest},
                                                                                   {FilterModeZeInfo::Linear, FilterModeDescriptor::Linear}}});
    auto filterMode = filterModes.find(src.filterMode);
    if (filterMode.has_value() == false) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler filterMode mode in context of : " + dst.kernelMetadata.kernelName + "\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.filterMode = *filterMode;

    inlineSampler.isNormalized = src.normalized;

    dst.payloadMappings.samplerTable.numSamplers = std::max<uint8_t>(dst.payloadMappings.samplerTable.numSamplers, static_cast<uint8_t>(inlineSampler.samplerIndex + 1));
    dst.inlineSamplers.push_back(inlineSampler);

    return DecodeError::Success;
}

DecodeError decodeZeInfoKernelPerThreadMemoryBuffers(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.perThreadMemoryBuffersNd.empty()) {
        KernelPerThreadMemoryBuffers perThreadMemoryBuffers{};
        auto perThreadMemoryBuffersErr = readZeInfoPerThreadMemoryBuffers(parser, *kernelSections.perThreadMemoryBuffersNd[0], perThreadMemoryBuffers,
                                                                          dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != perThreadMemoryBuffersErr) {
            return perThreadMemoryBuffersErr;
        }
        for (const auto &memBuff : perThreadMemoryBuffers) {
            auto decodeErr = populateKernelPerThreadMemoryBuffer(dst, memBuff, minScratchSpaceSize, outErrReason, outWarning);
            if (DecodeError::Success != decodeErr) {
                return decodeErr;
            }
        }
    }
    return DecodeError::Success;
}

DecodeError readZeInfoPerThreadMemoryBuffers(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelPerThreadMemoryBuffers &outPerThreadMemoryBuffers, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validBuffer = true;
    for (const auto &perThreadMemoryBufferNd : parser.createChildrenRange(node)) {
        outPerThreadMemoryBuffers.resize(outPerThreadMemoryBuffers.size() + 1);
        auto &perThreadMemoryBufferMetadata = *outPerThreadMemoryBuffers.rbegin();
        for (const auto &perThreadMemoryBufferMemberNd : parser.createChildrenRange(perThreadMemoryBufferNd)) {
            auto key = parser.readKey(perThreadMemoryBufferMemberNd);
            if (Tags::Kernel::PerThreadMemoryBuffer::allocationType == key) {
                validBuffer &= readZeInfoEnumChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.allocationType, context, outErrReason);
            } else if (Tags::Kernel::PerThreadMemoryBuffer::memoryUsage == key) {
                validBuffer &= readZeInfoEnumChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.memoryUsage, context, outErrReason);
            } else if (Tags::Kernel::PerThreadMemoryBuffer::size == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.size, context, outErrReason);
            } else if (Tags::Kernel::PerThreadMemoryBuffer::isSimtThread == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.isSimtThread, context, outErrReason);
            } else if (Tags::Kernel::PerThreadMemoryBuffer::slot == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.slot, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for per-thread memory buffer in context of " + context.str() + "\n");
            }
        }
    }
    return validBuffer ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError populateKernelPerThreadMemoryBuffer(KernelDescriptor &dst, const KernelPerThreadMemoryBufferBaseT &src, const uint32_t minScratchSpaceSize, std::string &outErrReason, std::string &outWarning) {
    using namespace Types::Kernel::PerThreadMemoryBuffer;
    using namespace Tags::Kernel::PerThreadMemoryBuffer::AllocationType;
    using namespace Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;
    if (src.size <= 0) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer allocation size (size must be greater than 0) in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    }

    auto size = src.size;
    if (src.isSimtThread) {
        size *= dst.kernelAttributes.simdSize;
    }
    switch (src.allocationType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer allocation type in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    case AllocationTypeGlobal:
        if (MemoryUsagePrivateSpace != src.memoryUsage) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer memory usage type for " + global.str() + " allocation type in context of : " + dst.kernelMetadata.kernelName + ". Expected : " + privateSpace.str() + ".\n");
            return DecodeError::InvalidBinary;
        }

        dst.kernelAttributes.perHwThreadPrivateMemorySize = size;
        break;
    case AllocationTypeScratch:
        if (src.slot > 1) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid scratch buffer slot " + std::to_string(src.slot) + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0 or 1.\n");
            return DecodeError::InvalidBinary;
        }
        if (0 != dst.kernelAttributes.perThreadScratchSize[src.slot]) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid duplicated scratch buffer entry " + std::to_string(src.slot) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
            return DecodeError::InvalidBinary;
        }
        uint32_t scratchSpaceSize = std::max(static_cast<uint32_t>(src.size), minScratchSpaceSize);
        scratchSpaceSize = Math::isPow2(scratchSpaceSize) ? scratchSpaceSize : Math::nextPowerOfTwo(scratchSpaceSize);
        dst.kernelAttributes.perThreadScratchSize[src.slot] = scratchSpaceSize;
        break;
    }
    return DecodeError::Success;
}

DecodeError decodeZeInfoKernelExperimentalProperties(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.experimentalPropertiesNd.empty()) {
        KernelExperimentalPropertiesBaseT experimentalProperties{};
        auto experimentalPropertiesErr = readZeInfoExperimentalProperties(parser, *kernelSections.experimentalPropertiesNd[0], experimentalProperties,
                                                                          dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != experimentalPropertiesErr) {
            return experimentalPropertiesErr;
        }
        populateKernelExperimentalProperties(dst, experimentalProperties);
    }

    return DecodeError::Success;
}

DecodeError readZeInfoExperimentalProperties(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelExperimentalPropertiesBaseT &outExperimentalProperties, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validExperimentalProperty = true;
    for (const auto &experimentalPropertyNd : parser.createChildrenRange(node)) {
        for (const auto &experimentalPropertyMemberNd : parser.createChildrenRange(experimentalPropertyNd)) {
            auto key = parser.readKey(experimentalPropertyMemberNd);
            if (Tags::Kernel::ExperimentalProperties::hasNonKernelArgLoad == key) {
                validExperimentalProperty &= readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                    outExperimentalProperties.hasNonKernelArgLoad, context, outErrReason);
            } else if (Tags::Kernel::ExperimentalProperties::hasNonKernelArgStore == key) {
                validExperimentalProperty &= readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                    outExperimentalProperties.hasNonKernelArgStore, context, outErrReason);
            } else if (Tags::Kernel::ExperimentalProperties::hasNonKernelArgAtomic == key) {
                validExperimentalProperty &= readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                    outExperimentalProperties.hasNonKernelArgAtomic, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
                validExperimentalProperty = false;
            }
        }
    }

    return validExperimentalProperty ? DecodeError::Success : DecodeError::InvalidBinary;
}

void populateKernelExperimentalProperties(KernelDescriptor &dst, const KernelExperimentalPropertiesBaseT &experimentalProperties) {
    dst.kernelAttributes.hasNonKernelArgLoad = experimentalProperties.hasNonKernelArgLoad;
    dst.kernelAttributes.hasNonKernelArgStore = experimentalProperties.hasNonKernelArgStore;
    dst.kernelAttributes.hasNonKernelArgAtomic = experimentalProperties.hasNonKernelArgAtomic;
}

DecodeError decodeZeInfoKernelBindingTableEntries(KernelDescriptor &dst, Yaml::YamlParser &parser, const ZeInfoKernelSections &kernelSections, std::string &outErrReason, std::string &outWarning) {
    if (false == kernelSections.bindingTableIndicesNd.empty()) {
        KernelBindingTableEntries bindingTableIndices;
        auto error = readZeInfoBindingTableIndices(parser, *kernelSections.bindingTableIndicesNd[0], bindingTableIndices,
                                                   dst.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != error) {
            return error;
        }

        error = populateKernelBindingTableIndicies(dst, bindingTableIndices, outErrReason);
        if (DecodeError::Success != error) {
            return error;
        }
    }
    return DecodeError::Success;
}

DecodeError readZeInfoBindingTableIndices(const Yaml::YamlParser &parser, const Yaml::Node &node, KernelBindingTableEntries &outBindingTableIndices, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validBindingTableEntries = true;
    for (const auto &bindingTableIndexNd : parser.createChildrenRange(node)) {
        outBindingTableIndices.resize(outBindingTableIndices.size() + 1);
        auto &bindingTableIndexMetadata = *outBindingTableIndices.rbegin();
        for (const auto &bindingTableIndexMemberNd : parser.createChildrenRange(bindingTableIndexNd)) {
            auto key = parser.readKey(bindingTableIndexMemberNd);
            if (Tags::Kernel::BindingTableIndex::argIndex == key) {
                validBindingTableEntries &= readZeInfoValueChecked(parser, bindingTableIndexMemberNd, bindingTableIndexMetadata.argIndex, context, outErrReason);
            } else if (Tags::Kernel::BindingTableIndex::btiValue == key) {
                validBindingTableEntries &= readZeInfoValueChecked(parser, bindingTableIndexMemberNd, bindingTableIndexMetadata.btiValue, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"" + key.str() + "\" for binding table index in context of " + context.str() + "\n");
            }
        }
    }

    return validBindingTableEntries ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError populateKernelBindingTableIndicies(KernelDescriptor &dst, const KernelBindingTableEntries &btEntries, std::string &outErrReason) {
    for (auto &btEntry : btEntries) {
        auto &explicitArg = dst.payloadMappings.explicitArgs[btEntry.argIndex];
        switch (explicitArg.type) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Invalid binding table entry for non-pointer and non-image argument idx : " + std::to_string(btEntry.argIndex) + ".\n");
            return DecodeError::InvalidBinary;
        case ArgDescriptor::ArgTImage: {
            setSSHOffsetBasedOnBti(explicitArg.as<ArgDescImage>().bindful, btEntry.btiValue, dst.payloadMappings.bindingTable.numEntries);
            break;
        }
        case ArgDescriptor::ArgTPointer: {
            setSSHOffsetBasedOnBti(explicitArg.as<ArgDescPointer>().bindful, btEntry.btiValue, dst.payloadMappings.bindingTable.numEntries);
            break;
        }
        }
    }
    return DecodeError::Success;
}

void generateSSHWithBindingTable(KernelDescriptor &dst) {
    static constexpr auto surfaceStateSize = 64U;
    static constexpr auto btiSize = sizeof(int);

    auto &bindingTable = dst.payloadMappings.bindingTable;
    bindingTable.tableOffset = bindingTable.numEntries * surfaceStateSize;
    size_t sshSize = bindingTable.tableOffset + bindingTable.numEntries * btiSize;
    dst.generatedSsh.resize(alignUp(sshSize, surfaceStateSize), 0U);

    auto bindingTableIt = reinterpret_cast<int *>(dst.generatedSsh.data() + bindingTable.tableOffset);
    for (int i = 0; i < bindingTable.numEntries; ++i) {
        *bindingTableIt = i * surfaceStateSize;
        ++bindingTableIt;
    }
}

void generateDSH(KernelDescriptor &dst) {
    constexpr auto samplerStateSize = 16U;
    constexpr auto borderColorStateSize = 64U;

    dst.kernelAttributes.flags.usesSamplers = true;
    auto &samplerTable = dst.payloadMappings.samplerTable;
    samplerTable.borderColor = 0U;
    samplerTable.tableOffset = borderColorStateSize;

    size_t dshSize = borderColorStateSize + samplerTable.numSamplers * samplerStateSize;
    dst.generatedDsh.resize(alignUp(dshSize, borderColorStateSize), 0U);
}

} // namespace NEO::Zebin::ZeInfo
