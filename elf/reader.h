/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
