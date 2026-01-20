/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/modules_package_binary.h"

#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"

#include <memory>
#include <ranges>
#include <set>
#include <sstream>
#include <string>

namespace L0 {

std::optional<ModulesPackageManifest> ModulesPackageManifest::load(std::string_view str, std::string &outErrReason, std::string &outWarning) {
    std::optional<ModulesPackageManifest> ret = ModulesPackageManifest{};
    NEO::Yaml::YamlParser parser;
    auto yamlOk = parser.parse(NEO::ConstStringRef(str), outErrReason, outWarning);
    if (false == yamlOk) {
        ret.reset();
        return ret;
    }
    bool dataOk = true;
    ret->version.major = 0;
    ret->version.minor = 0;
    auto versionNode = parser.findNodeWithKeyDfs("version");
    if (versionNode) {
        auto major = parser.getChild(*versionNode, "major");
        auto minor = parser.getChild(*versionNode, "minor");
        dataOk &= parser.readValueChecked(major, ret->version.major, "NEO::ModulesPackage : Failed to read major version in manifest\n", outErrReason);
        dataOk &= parser.readValueChecked(minor, ret->version.minor, "NEO::ModulesPackage : Failed to read minor version in manifest\n", outErrReason);
    } else {
        outErrReason += "NEO::ModulesPackage : Missing version information\n";
        dataOk = false;
    }

    auto unitsNode = parser.findNodeWithKeyDfs("units");
    if (unitsNode) {
        ret->units.reserve(unitsNode->numChildren);
        for (const auto &unitNode : parser.createChildrenRange(*unitsNode)) {
            ret->units.resize(ret->units.size() + 1);
            auto &currentUnit = *ret->units.rbegin();
            auto fileNameNode = parser.getChild(unitNode, "filename");
            dataOk &= parser.readValueNoQuotes(fileNameNode, currentUnit.fileName, false, "NEO::ModulesPackage : Failed to read unit's filename in manifest\n", outErrReason);
        }
    }

    if (ret->version.major > ModulesPackageManifest::compiledVersion.major) {
        outErrReason += "NEO::ModulesPackage : Unhandled package manifest major version\n";
        dataOk = false;
    }

    if (false == dataOk) {
        ret.reset();
    }

    return ret;
}

std::string ModulesPackageManifest::dump() const {
    std::stringstream yaml;
    yaml << "version : \n";
    yaml << "  major : " << this->version.major << "\n";
    yaml << "  minor : " << this->version.minor << "\n";
    if (false == units.empty()) {
        yaml << "units : "
             << "\n";
        for (const auto &unit : units) {
            yaml << "  - filename: " << unit.fileName << "\n";
        }
    }
    return yaml.str();
}

std::vector<uint8_t> ModulesPackageBinary::encode() {
    NEO::Ar::ArEncoder arEnc;
    ModulesPackageManifest manifest;
    uint32_t unitId = 0;
    manifest.units.reserve(units.size());
    for (const auto &unit : units) {
        auto unitFName = "pkg.unit." + std::to_string(unitId);
        arEnc.appendFileEntry(unitFName, unit.data);
        manifest.units.push_back({unitFName});
        ++unitId;
    }
    std::string manifestStr = manifest.dump();
    arEnc.appendFileEntry(NEO::ConstStringRef(ModulesPackageManifest::packageManifestFileName),
                          ArrayRef<const uint8_t>::fromAny(manifestStr.data(), manifestStr.size()));
    return arEnc.encode();
}

bool ModulesPackageBinary::isModulesPackageBinary(std::span<const uint8_t> binary) {
    if (false == NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::archive>(binary)) {
        return false;
    }
    std::string err;
    std::string warn;
    auto ar = NEO::Ar::decodeAr(binary, err, warn);
    return std::ranges::any_of(ar.files, [](const auto &f) { return f.fileName == NEO::ConstStringRef(ModulesPackageManifest::packageManifestFileName); });
}

std::optional<ModulesPackageBinary> ModulesPackageBinary::decode(std::span<const uint8_t> binary, std::string &outErrReason, std::string &outWarning) {
    std::optional<ModulesPackageBinary> ret = ModulesPackageBinary{};
    auto ar = NEO::Ar::decodeAr(binary, outErrReason, outWarning);
    auto manifestFileEntry = std::ranges::find_if(ar.files, [](const auto &f) { return f.fileName == NEO::ConstStringRef(ModulesPackageManifest::packageManifestFileName); });
    if (ar.files.end() == manifestFileEntry) {
        outErrReason += "NEO::ModulesPackage : Could not find manifest file " + std::string(ModulesPackageManifest::packageManifestFileName) + " in packge\n";
        ret.reset();
        return ret;
    }

    auto manifest = ModulesPackageManifest::load(std::string_view(reinterpret_cast<const char *>(manifestFileEntry->fileData.begin()), manifestFileEntry->fileData.size()),
                                                 outErrReason, outWarning);
    if (false == manifest.has_value()) {
        outErrReason += "NEO::ModulesPackage : Could not load manifest file\n";
        ret.reset();
        return ret;
    }

    ret->units.reserve(manifest->units.size());
    for (const auto &unitManifest : manifest->units) {
        ret->units.resize(ret->units.size() + 1);
        auto &currentUnit = *ret->units.rbegin();

        auto unitFileEntry = std::ranges::find_if(ar.files, [&](const auto &f) { return f.fileName == NEO::ConstStringRef(unitManifest.fileName); });
        if (ar.files.end() == unitFileEntry) {
            outErrReason += "NEO::ModulesPackage : Missing file " + unitManifest.fileName + " in provided package\n";
            ret.reset();
            return ret;
        }

        currentUnit.data = std::span<const uint8_t>(unitFileEntry->fileData.begin(), unitFileEntry->fileData.size());
    }

    return ret;
}

} // namespace L0
