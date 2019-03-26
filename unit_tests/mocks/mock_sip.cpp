/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_sip.h"

#include "runtime/helpers/file_io.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_inc_base.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_compilers.h"

#include "cif/macros/enable.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <fstream>
#include <map>

namespace NEO {
std::vector<char> MockSipKernel::dummyBinaryForSip;
std::vector<char> MockSipKernel::getDummyGenBinary() {
    if (dummyBinaryForSip.size() == 0) {
        dummyBinaryForSip = getBinary();
    }
    return dummyBinaryForSip;
}
std::vector<char> MockSipKernel::getBinary() {
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".gen");

    void *binary = nullptr;
    auto binarySize = loadDataFromFile(testFile.c_str(), binary);

    UNRECOVERABLE_IF(binary == nullptr);

    std::vector<char> ret{reinterpret_cast<char *>(binary), reinterpret_cast<char *>(binary) + binarySize};

    deleteDataReadFromFile(binary);
    return ret;
}
void MockSipKernel::initDummyBinary() {
    dummyBinaryForSip = getBinary();
}
void MockSipKernel::shutDown() {
    MockSipKernel::dummyBinaryForSip.clear();
}
} // namespace NEO