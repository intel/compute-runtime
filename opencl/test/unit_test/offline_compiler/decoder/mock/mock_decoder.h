/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/binary_decoder.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "mock_iga_wrapper.h"

#include <memory>
#include <optional>
#include <vector>

class MockDecoder : public BinaryDecoder {
  public:
    using BinaryDecoder::decode;
    using BinaryDecoder::dumpField;
    using BinaryDecoder::getSize;
    using BinaryDecoder::loadPatchList;
    using BinaryDecoder::processBinary;
    using BinaryDecoder::processKernel;
    using BinaryDecoder::readPatchTokens;
    using BinaryDecoder::readStructFields;

    using BinaryDecoder::argHelper;
    using BinaryDecoder::binaryFile;
    using BinaryDecoder::iga;
    using BinaryDecoder::ignoreIsaPadding;
    using BinaryDecoder::kernelHeader;
    using BinaryDecoder::patchTokens;
    using BinaryDecoder::pathToDump;
    using BinaryDecoder::pathToPatch;
    using BinaryDecoder::programHeader;

    MockDecoder(bool suppressMessages = true) : MockDecoder("", "", "") {
        argHelper->getPrinterRef().setSuppressMessages(suppressMessages);
    }

    MockDecoder(const std::string &file, const std::string &patch, const std::string &dump)
        : BinaryDecoder(file, patch, dump) {
        this->iga.reset(new MockIgaWrapper);

        mockArgHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        argHelper = mockArgHelper.get();
    }

    MockIgaWrapper *getMockIga() const {
        return static_cast<MockIgaWrapper *>(iga.get());
    }

    std::pair<const void *, size_t> getDevBinary() override {
        if (callBaseGetDevBinary) {
            return BinaryDecoder::getDevBinary();
        }
        return {devBinaryToReturn, binarySize};
    }

    void parseTokens() override {
        if (callBaseParseTokens) {
            return BinaryDecoder::parseTokens();
        }
    }

    std::map<std::string, std::string> filesMap{};
    std::unique_ptr<MockOclocArgHelper> mockArgHelper{};

    size_t binarySize = 0u;
    bool callBaseParseTokens{true};
    bool callBaseGetDevBinary{true};
    const void *devBinaryToReturn{nullptr};
};
