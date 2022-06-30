/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/source/helpers/hash.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "mock_iga_wrapper.h"

#include <map>
#include <string>

class MockEncoder : public BinaryEncoder {
  public:
    using BinaryEncoder::addPadding;
    using BinaryEncoder::calculatePatchListSizes;
    using BinaryEncoder::copyBinaryToBinary;
    using BinaryEncoder::createElf;
    using BinaryEncoder::encode;
    using BinaryEncoder::processBinary;
    using BinaryEncoder::processKernel;
    using BinaryEncoder::write;
    using BinaryEncoder::writeDeviceBinary;

    using BinaryEncoder::argHelper;
    using BinaryEncoder::elfName;
    using BinaryEncoder::iga;
    using BinaryEncoder::ignoreIsaPadding;
    using BinaryEncoder::pathToDump;

    MockEncoder(bool suppressMessages = true) : MockEncoder("", "") {
        argHelper->getPrinterRef() = MessagePrinter(suppressMessages);
    }

    MockEncoder(const std::string &dump, const std::string &elf)
        : BinaryEncoder(dump, elf) {
        this->iga.reset(new MockIgaWrapper);

        mockArgHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        argHelper = mockArgHelper.get();
    }

    bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength) override {
        if (callBaseCopyBinaryToBinary) {
            return BinaryEncoder::copyBinaryToBinary(srcFileName, outBinary, binaryLength);
        }

        auto it = filesMap.find(srcFileName);
        if (it == filesMap.end()) {
            return false;
        }

        outBinary.write(it->second.c_str(), it->second.size());
        if (binaryLength != nullptr) {
            *binaryLength = static_cast<uint32_t>(it->second.size());
        }
        return true;
    }

    MockIgaWrapper *getMockIga() const {
        return static_cast<MockIgaWrapper *>(iga.get());
    }

    std::map<std::string, std::string> filesMap{};
    std::unique_ptr<MockOclocArgHelper> mockArgHelper{};
    bool callBaseCopyBinaryToBinary{false};
};
