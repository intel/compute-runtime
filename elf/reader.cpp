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

#include "reader.h"
#include <string.h>

namespace CLElfLib {
CElfReader::CElfReader(ElfBinaryStorage &elfBinary) {
    validateElfBinary(elfBinary);
}

void CElfReader::validateElfBinary(ElfBinaryStorage &elfBinary) {
    const char *nameTable = nullptr;
    const char *end = nullptr;
    size_t ourSize = 0u;
    size_t entrySize = 0u;
    size_t indexedSectionHeaderOffset = 0u;
    beginBinary = elfBinary.data();

    if (elfBinary.size() >= sizeof(SElf64Header)) {
        // calculate a pointer to the end
        end = beginBinary + elfBinary.size();
        elf64Header = reinterpret_cast<const SElf64Header *>(elfBinary.data());

        if (!((elf64Header->Identity[ELFConstants::idIdxMagic0] == ELFConstants::elfMag0) &&
              (elf64Header->Identity[ELFConstants::idIdxMagic1] == ELFConstants::elfMag1) &&
              (elf64Header->Identity[ELFConstants::idIdxMagic2] == ELFConstants::elfMag2) &&
              (elf64Header->Identity[ELFConstants::idIdxMagic3] == ELFConstants::elfMag3) &&
              (elf64Header->Identity[ELFConstants::idIdxClass] == static_cast<uint32_t>(E_EH_CLASS::EH_CLASS_64)))) {
            throw ElfException();
        }
        ourSize += elf64Header->ElfHeaderSize;
    } else {
        throw ElfException();
    }

    entrySize = elf64Header->SectionHeaderEntrySize;

    if (elf64Header->SectionNameTableIndex < elf64Header->NumSectionHeaderEntries) {
        indexedSectionHeaderOffset = static_cast<size_t>(elf64Header->SectionHeadersOffset) + (elf64Header->SectionNameTableIndex * entrySize);

        if ((beginBinary + indexedSectionHeaderOffset) <= end) {
            nameTable = beginBinary + indexedSectionHeaderOffset;
        }
    }

    for (uint16_t i = 0u; i < elf64Header->NumSectionHeaderEntries; ++i) {
        indexedSectionHeaderOffset = static_cast<size_t>(elf64Header->SectionHeadersOffset) + (i * entrySize);

        // check section header offset
        if ((beginBinary + indexedSectionHeaderOffset) > end) {
            throw ElfException();
        }

        const SElf64SectionHeader *sectionHeader = nullptr;

        sectionHeader = reinterpret_cast<const SElf64SectionHeader *>(beginBinary + indexedSectionHeaderOffset);

        // check section data
        if ((beginBinary + sectionHeader->DataOffset + sectionHeader->DataSize) > end) {
            throw ElfException();
        }

        // check section name index
        if ((nameTable + sectionHeader->Name) > end) {
            throw ElfException();
        }

        SElf64SectionHeader stringSectionHeader = {0};
        stringSectionHeader.Type = sectionHeader->Type;
        stringSectionHeader.Flags = sectionHeader->Flags;
        stringSectionHeader.DataOffset = sectionHeader->DataOffset;
        stringSectionHeader.DataSize = sectionHeader->DataSize;
        stringSectionHeader.Name = sectionHeader->Name;
        sectionHeaders.push_back(std::move(stringSectionHeader));

        // tally up the sizes
        ourSize += static_cast<size_t>(sectionHeader->DataSize);
        ourSize += static_cast<size_t>(entrySize);
    }

    if (ourSize != elfBinary.size()) {
        throw ElfException();
    }
}

char *CElfReader::getSectionData(Elf64_Off dataOffset) {
    return beginBinary + dataOffset;
}

const SElf64Header *CElfReader::getElfHeader() {
    return elf64Header;
}

} // namespace CLElfLib
