/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"

#include <sstream>
namespace NEO::Zebin::ZeInfo {
void readZeInfoValueCheckedExtra(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &execEnvMetadataNd, KernelExecutionEnvBaseT &kernelExecEnv, ConstStringRef context,
                                 ConstStringRef key, std::string &outErrReason, std::string &outWarning, bool &validExecEnv, DecodeError &error) {

    std::ostringstream entry;
    entry << "\"" << key.str() << "\" in context of " << context.str();
    encounterUnknownZeInfoAttribute(entry.str(), outErrReason, outWarning, error);
}

bool readZeInfoArgTypeNameCheckedExt(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &payloadArgumentMemberNd, KernelPayloadArgBaseT &outKernelPayArg) {
    return false;
}

void populateKernelExecutionEnvironmentExt(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv, const Types::Version &srcZeInfoVersion) {
}

DecodeError populateKernelPayloadArgumentExt(NEO::KernelDescriptor &dst, const KernelPayloadArgBaseT &src, std::string &outErrReason) {
    return DecodeError::unhandledBinary;
}

} // namespace NEO::Zebin::ZeInfo

template <>
void cloneExt(ExtUniquePtrT<NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentExtT> &dst, const NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentExtT &src) {
}

template <>
void destroyExt(NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentExtT *dst) {
}

template <>
void cloneExt(ExtUniquePtrT<NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvExt> &dst, const NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvExt &src) {
}

template <>
void destroyExt(NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvExt *dst) {
}

template <>
void allocateExt(ExtUniquePtrT<NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvExt> &dst) {
}
