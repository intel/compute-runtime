/*
 * Copyright (C) 2018-2020 Intel Corporation
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

struct MockEncoder : public BinaryEncoder {
    MockEncoder() : MockEncoder("", ""){};
    MockEncoder(const std::string &dump, const std::string &elf)
        : BinaryEncoder(dump, elf) {
        this->iga.reset(new MockIgaWrapper);
        setMessagePrinter(MessagePrinter{true});
        this->argHelper.reset(new MockOclocArgHelper(filesMap));
    };

    std::map<std::string, std::string> filesMap;

    bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength) override {
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
    using BinaryEncoder::addPadding;
    using BinaryEncoder::calculatePatchListSizes;
    using BinaryEncoder::copyBinaryToBinary;
    using BinaryEncoder::createElf;
    using BinaryEncoder::elfName;
    using BinaryEncoder::encode;
    using BinaryEncoder::iga;
    using BinaryEncoder::pathToDump;
    using BinaryEncoder::processBinary;
    using BinaryEncoder::processKernel;
    using BinaryEncoder::write;
    using BinaryEncoder::writeDeviceBinary;

    MockIgaWrapper *getMockIga() const {
        return static_cast<MockIgaWrapper *>(iga.get());
    }
};
