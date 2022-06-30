/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/test_files.h"

#include "opencl/test/unit_test/built_ins/built_ins_file_names.h"

namespace NEO {

extern std::vector<std::string> builtInFileNames;
extern std::vector<std::string> imageBuiltInFileNames;

std::vector<std::string> getBuiltInFileNames(bool imagesSupport) {
    auto vec = builtInFileNames;
    if (imagesSupport) {
        vec.insert(vec.end(), imageBuiltInFileNames.begin(), imageBuiltInFileNames.end());
    }
    return vec;
}

std::string getBuiltInHashFileName(uint64_t hash, bool imagesSupport) {
    std::string hashName = sharedFiles + "/" + std::to_string(hash);
    if (imagesSupport) {
        hashName.append("_images");
    }
    hashName.append(".cl");
    return hashName;
}
} // namespace NEO
