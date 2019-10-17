/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/elf/types.h"
#include "core/elf/writer.h"

namespace NEO {

void MockElfBinary(CLElfLib::ElfBinaryStorage &elfBinary) {
    CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);
    elfWriter.addSection(CLElfLib::SSectionNode(CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_OPTIONS, CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "BuildOptions", "\0", 1u));

    elfBinary.resize(elfWriter.getTotalBinarySize());

    elfWriter.resolveBinary(elfBinary);
}
} // namespace NEO
