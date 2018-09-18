/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "elf/types.h"
#include "elf/writer.h"

namespace OCLRT {

void MockElfBinary(CLElfLib::ElfBinaryStorage &elfBinary) {
    CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);
    elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "BuildOptions", "\0", 1u));

    elfBinary.resize(elfWriter.getTotalBinarySize());

    elfWriter.resolveBinary(elfBinary);
}
} // namespace OCLRT
