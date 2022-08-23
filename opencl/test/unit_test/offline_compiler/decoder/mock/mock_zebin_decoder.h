/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/zebin_manipulator.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"

template <NEO::Elf::ELF_IDENTIFIER_CLASS numBits>
struct MockZebinDecoder : public NEO::ZebinManipulator::ZebinDecoder<numBits> {
    using Base = NEO::ZebinManipulator::ZebinDecoder<numBits>;
    using ErrorCode = NEO::ZebinManipulator::ErrorCode;
    using ElfT = NEO::Elf::Elf<numBits>;
    using Base::arguments;
    using Base::decodeZebin;
    using Base::dumpKernelData;
    using Base::dumpSectionInfo;
    using Base::iga;
    using Base::printHelp;

    MockZebinDecoder(OclocArgHelper *argHelper) : Base::ZebinDecoder(argHelper) {
        auto iga = std::make_unique<MockIgaWrapper>();
        mockIga = iga.get();
        this->iga = std::move(iga);
    };

    ErrorCode decodeZebin(ArrayRef<const uint8_t> zebin, ElfT &outElf) override {
        if (callBaseDecodeZebin) {
            return Base::decodeZebin(zebin, outElf);
        }
        return returnValueDecodeZebin;
    }

    std::vector<NEO::Elf::IntelGTNote> getIntelGTNotes(ElfT &elf) override {
        getIntelGTNotesCalled = true;
        if (callBaseGetIntelGTNotes) {
            return Base::getIntelGTNotes(elf);
        }
        return returnValueGetIntelGTNotes;
    }

    std::vector<NEO::ZebinManipulator::SectionInfo> dumpElfSections(ElfT &elf) override {
        return returnValueDumpElfSections;
    }

    void dumpSectionInfo(const std::vector<NEO::ZebinManipulator::SectionInfo> &sectionInfos) override {
        return;
    }

    bool callBaseGetIntelGTNotes = false;
    bool callBaseDecodeZebin = false;
    ErrorCode returnValueDecodeZebin = NEO::OclocErrorCode::SUCCESS;
    std::vector<NEO::ZebinManipulator::SectionInfo> returnValueDumpElfSections;
    std::vector<NEO::Elf::IntelGTNote> returnValueGetIntelGTNotes;
    bool getIntelGTNotesCalled = false;
    MockIgaWrapper *mockIga;
};