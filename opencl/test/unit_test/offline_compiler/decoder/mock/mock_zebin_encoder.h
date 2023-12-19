/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/zebin_manipulator.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

#include "opencl/test/unit_test/offline_compiler/decoder/mock/mock_iga_wrapper.h"

template <NEO::Elf::ElfIdentifierClass numBits>
struct MockZebinEncoder : public NEO::Zebin::Manipulator::ZebinEncoder<numBits> {
    using Base = NEO::Zebin::Manipulator::ZebinEncoder<numBits>;
    using ErrorCode = NEO::Zebin::Manipulator::ErrorCode;
    using SectionInfo = NEO::Zebin::Manipulator::SectionInfo;
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
        retValGetIntelGTNotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::productFamily;
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

    std::vector<NEO::Zebin::Elf::IntelGTNote> getIntelGTNotes(const std::vector<char> &intelGtNotesSection) override {
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
    std::vector<NEO::Zebin::Elf::IntelGTNote> retValGetIntelGTNotes;
    ErrorCode retValLoadSectionsInfo = OCLOC_SUCCESS;
    ErrorCode retValCheckIfAllFilesExist = OCLOC_SUCCESS;
    ErrorCode retValAppendSections = OCLOC_SUCCESS;
    bool callBaseGetIntelGTNotesSection = false;
    bool callBaseGetIntelGTNotes = false;
    bool callBaseLoadSectionsInfo = false;
    bool callBaseParseKernelAssembly = false;
    bool callBaseCheckIfAllFilesExist = false;
    bool parseKernelAssemblyCalled = false;
    PRODUCT_FAMILY productFamily = PRODUCT_FAMILY::IGFX_DG2;
};