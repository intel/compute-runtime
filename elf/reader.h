/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "types.h"

namespace CLElfLib {
using ElfSectionHeaderStorage = std::vector<SElf64SectionHeader>;
/******************************************************************************\

 Class:         CElfReader

 Description:   Class to provide simpler interaction with the ELF standard
                binary object.  SElf64Header defines the ELF header type and
                SElf64SectionHeader defines the section header type.

\******************************************************************************/
class CElfReader {
  public:
    CElfReader(ElfBinaryStorage &elfBinary);
    ElfSectionHeaderStorage &getSectionHeaders() {
        return sectionHeaders;
    }

    const SElf64Header *getElfHeader();
    char *getSectionData(Elf64_Off dataOffset);

  protected:
    void validateElfBinary(ElfBinaryStorage &elfBinary);

    ElfSectionHeaderStorage sectionHeaders;
    char *beginBinary;
    const SElf64Header *elf64Header;
};
} // namespace CLElfLib
