/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"

namespace NEO::Zebin::ZeInfo {
void readZeInfoValueCheckedExtra(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &execEnvMetadataNd, KernelExecutionEnvBaseT &kernelExecEnv, ConstStringRef context,
                                 ConstStringRef key, std::string &outErrReason, std::string &outWarning, bool &validExecEnv, DecodeError &error) {

    encounterUnknownZeInfoAttribute("\"" + key.str() + "\" in context of " + context.str(), outErrReason, outWarning, error);
}
} // namespace NEO::Zebin::ZeInfo
