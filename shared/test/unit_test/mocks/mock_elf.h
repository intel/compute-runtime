/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

template <NEO::Elf::ELF_IDENTIFIER_CLASS NumBits = NEO::Elf::EI_CLASS_64>
struct MockElf : public NEO::Elf::Elf<NumBits> {
    using BaseClass = NEO::Elf::Elf<NumBits>;

    using BaseClass::relocations;
    using BaseClass::symbolTable;
};

template <NEO::Elf::ELF_IDENTIFIER_CLASS NumBits = NEO::Elf::EI_CLASS_64>
struct MockElfEncoder : public NEO::Elf::ElfEncoder<NumBits> {
    using NEO::Elf::ElfEncoder<NumBits>::sectionHeaders;

    uint32_t getLastSectionHeaderIndex() {
        return uint32_t(sectionHeaders.size()) - 1;
    }

    NEO::Elf::ElfSectionHeader<NumBits> *getSectionHeader(uint32_t idx) {
        return sectionHeaders.data() + idx;
    }
};
