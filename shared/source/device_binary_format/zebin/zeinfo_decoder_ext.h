/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"

namespace NEO::Zebin::ZeInfo {

void populateKernelExecutionEnvironmentExt(KernelDescriptor &dst, const KernelExecutionEnvBaseT &execEnv, const Types::Version &srcZeInfoVersion);

DecodeError populateKernelPayloadArgumentExt(NEO::KernelDescriptor &dst, const KernelPayloadArgBaseT &src, std::string &outErrReason);

bool readZeInfoArgTypeNameCheckedExt(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &payloadArgumentMemberNd, KernelPayloadArgBaseT &outKernelPayArg);

} // namespace NEO::Zebin::ZeInfo
