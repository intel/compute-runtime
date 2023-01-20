/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/zebin_manipulator.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

#include "opencl/test/unit_test/offline_compiler/decoder/mock/mock_iga_wrapper.h"

template <NEO::Elf::ELF_IDENTIFIER_CLASS numBits>
struct MockZebinEncoder : public NEO::ZebinManipulator::ZebinEncoder<numBits> {
    using Base = NEO::ZebinManipulator::ZebinEncoder<numBits>;
    using ErrorCode = NEO::ZebinManipulator::ErrorCode;
    using SectionInfo = NEO::ZebinManipulator::SectionInfo;
    using ElfEncoderT = NEO::Elf::ElfEncoder<numBits>;
    using Base::appendKernel;
    using Base::appendRel;
    using Base::appendRela;
    using Base::appendSymtab;
    using Base::iga;
    using Base::parseSymbols;
    using Base::printHelp;

    MockZebinEncoder(OclocArgHelper *argHelper) : Base::ZebinEncoder(argHelper) {
        auto iga = std::make_unique<MockIgaWrapper>();
        mockIga = iga.get();
        this->iga = std::move(iga);
        retValGetIntelGTNotes.resize(1);
        retValGetIntelGTNotes[0].type = NEO::Elf::IntelGTSectionType::ProductFamily;
        retValGetIntelGTNotes[0].data = ArrayRef<const uint8_t>::fromAny(&productFamily, 1);
    }

    ErrorCode loadSectionsInfo(std::vector<SectionInfo> &sectionInfos) override {
        if (callBaseLoadSectionsInfo) {
            return Base::loadSectionsInfo(sectionInfos);
        }
        return retValLoadSectionsInfo;
    }

    ErrorCode checkIfAllFilesExist(const std::vector<SectionInfo> &sectionInfos) override {
        if (callBaseCheckIfAllFilesExist) {
            return Base::checkIfAllFilesExist(sectionInfos);
        }
        return retValCheckIfAllFilesExist;
    }

    std::vector<char> getIntelGTNotesSection(const std::vector<SectionInfo> &sectionInfos) override {
        if (callBaseGetIntelGTNotesSection) {
            return Base::getIntelGTNotesSection(sectionInfos);
        }
        return {};
    }

    std::vector<NEO::Elf::IntelGTNote> getIntelGTNotes(const std::vector<char> &intelGtNotesSection) override {
        if (callBaseGetIntelGTNotes) {
            return Base::getIntelGTNotes(intelGtNotesSection);
        }
        return retValGetIntelGTNotes;
    }

    ErrorCode appendSections(ElfEncoderT &encoder, const std::vector<SectionInfo> &sectionInfos) override {
        return retValAppendSections;
    }

    std::string parseKernelAssembly(ArrayRef<const char> kernelAssembly) override {
        parseKernelAssemblyCalled = true;
        if (callBaseParseKernelAssembly) {
            return Base::parseKernelAssembly(kernelAssembly);
        }
        return {};
    }

    MockIgaWrapper *mockIga;
    std::vector<NEO::Elf::IntelGTNote> retValGetIntelGTNotes;
    ErrorCode retValLoadSectionsInfo = NEO::OclocErrorCode::SUCCESS;
    ErrorCode retValCheckIfAllFilesExist = NEO::OclocErrorCode::SUCCESS;
    ErrorCode retValAppendSections = NEO::OclocErrorCode::SUCCESS;
    bool callBaseGetIntelGTNotesSection = false;
    bool callBaseGetIntelGTNotes = false;
    bool callBaseLoadSectionsInfo = false;
    bool callBaseParseKernelAssembly = false;
    bool callBaseCheckIfAllFilesExist = false;
    bool parseKernelAssemblyCalled = false;
    PRODUCT_FAMILY productFamily = PRODUCT_FAMILY::IGFX_DG2;
};