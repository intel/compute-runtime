/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/spirv_capabilities_parser.h"

#include "shared/source/helpers/debug_helpers.h"

#include "cif/builtins/memory/buffer/buffer.h"
#include "cif/helpers/error.h"
#include "igc_interface.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_options_and_capabilities.h"

namespace NEO {

std::string SpirvCapabilitiesParser::getSpirvExtensionsYAMLFromDeviceCtx(NEO::IgcOclDeviceCtxTag *deviceCtx, CIF::CIFMain *igcMain) {
    constexpr uint32_t spirvQueriesMinVersion = 5;
    if ((deviceCtx == nullptr) || (igcMain == nullptr) || (deviceCtx->GetUnderlyingVersion() < spirvQueriesMinVersion)) {
        return "";
    }

    auto optionsAndCaps = deviceCtx->GetIgcOptionsAndCapabilitiesHandle();
    if (optionsAndCaps == nullptr) {
        return "";
    }

    auto yamlBuffer = CIF::Builtins::CreateConstBuffer(igcMain, nullptr, 0);
    if (yamlBuffer == nullptr) {
        return "";
    }

    optionsAndCaps->GetCompilerSupportedSPIRVExtensionsYAML(yamlBuffer.get());

    if (yamlBuffer->GetSizeRaw() == 0) {
        return "";
    }

    const char *yamlData = reinterpret_cast<const char *>(yamlBuffer->GetMemoryRaw());
    return std::string(yamlData, yamlBuffer->GetSizeRaw());
}

bool SpirvCapabilitiesParser::parseSpirvExtensionsYAML(const std::string &yamlText,
                                                       std::vector<SpirvExtensionInfo> &outExtensions,
                                                       std::string &outErrReason,
                                                       std::string &outWarning) {
    outExtensions.clear();
    outErrReason.clear();
    outWarning.clear();

    if (yamlText.empty()) {
        outErrReason = "Empty YAML input";
        return false;
    }

    Yaml::YamlParser parser;
    bool success = parser.parse(yamlText, outErrReason, outWarning);

    if (!success) {
        return false;
    }

    if (parser.empty()) {
        return true; // nothing to parse
    }

    const auto *extensionsParent = parser.findNodeWithKeyDfs("spirv_extensions");
    if (extensionsParent == nullptr) {
        extensionsParent = parser.getRoot();
    }

    auto extensionsSeq = parser.createChildrenRange(*extensionsParent);

    for (const auto &extNode : extensionsSeq) {
        SpirvExtensionInfo extInfo;

        auto extFields = parser.createChildrenRange(extNode);
        for (const auto &field : extFields) {
            auto key = parser.readKey(field);

            if (key == "name") {
                if (!parser.readValueChecked(field, extInfo.name)) {
                    outWarning += "Warning: Failed to read 'name' field\n";
                }
            } else if ((key == "spec_url") || (key == "url")) {
                if (!parser.readValueChecked(field, extInfo.url)) {
                    outWarning += "Warning: Failed to read 'url' field\n";
                }
            } else if (key == "supported_capabilities") {
                auto capabilities = parser.createChildrenRange(field);
                for (const auto &capNode : capabilities) {
                    SpirvCapabilityInfo capInfo;
                    auto capFields = parser.createChildrenRange(capNode);
                    for (const auto &capField : capFields) {
                        auto capKey = parser.readKey(capField);
                        if (capKey == "name") {
                            parser.readValueChecked(capField, capInfo.name);
                        } else if (capKey == "id") {
                            parser.readValueChecked(capField, capInfo.id);
                        }
                    }
                    if (!capInfo.name.empty() && capInfo.id != 0) {
                        extInfo.supportedCapabilities.push_back(capInfo);
                    }
                }
            }
        }

        if (!extInfo.name.empty()) {
            outExtensions.push_back(std::move(extInfo));
        }
    }

    return true;
}

} // namespace NEO
