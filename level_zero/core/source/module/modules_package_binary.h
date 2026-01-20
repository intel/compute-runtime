/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cinttypes>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace L0 {

struct ModulesPackageManifest {
    struct ModulesPacageUnitManifest {
        std::string fileName;
    };

    struct ModulesPackageVersion {
        uint16_t major = 0;
        uint16_t minor = 0;
    };

    static constexpr std::string_view packageManifestFileName = "modules.pkg.yml";
    static constexpr ModulesPackageVersion compiledVersion = {1, 0};

    ModulesPackageVersion version = compiledVersion;

    std::vector<ModulesPacageUnitManifest> units;

    static std::optional<ModulesPackageManifest> load(std::string_view str, std::string &outErrReason, std::string &outWarning);

    std::string dump() const;
};

struct ModulesPackageBinary {
    ModulesPackageManifest manifest;

    std::vector<uint8_t> encode();

    static bool isModulesPackageBinary(std::span<const uint8_t> binary);

    static std::optional<ModulesPackageBinary> decode(std::span<const uint8_t> binary, std::string &outErrReason, std::string &outWarning);

    struct Unit {
        std::span<const uint8_t> data;
    };

    std::vector<Unit> units;
};

} // namespace L0
