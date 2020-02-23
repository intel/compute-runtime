/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/built_ins/built_ins_file_names.h"

namespace NEO {

extern std::vector<std::string> buintInFileNames;

std::vector<std::string> getBuiltInFileNames() {
    return buintInFileNames;
}

std::string getBuiltInHashFileName(uint64_t hash) {
    std::string hashName = "test_files/" + std::to_string(hash) + ".cl";
    return hashName;
}
} // namespace NEO
