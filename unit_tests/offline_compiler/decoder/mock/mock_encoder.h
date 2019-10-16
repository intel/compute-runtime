/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hash.h"
#include "offline_compiler/decoder/binary_encoder.h"

#include "mock_iga_wrapper.h"

#include <map>
#include <string>

struct MockEncoder : public BinaryEncoder {
    MockEncoder() : MockEncoder("", ""){};
    MockEncoder(const std::string &dump, const std::string &elf)
        : BinaryEncoder(dump, elf) {
        this->iga.reset(new MockIgaWrapper);
        setMessagePrinter(MessagePrinter{true});
    };

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

    bool fileExists(const std::string &path) const override {
        return filesMap.count(path) || BinaryEncoder::fileExists(path);
    }

    std::vector<char> readBinaryFile(const std::string &path) const override {
        return filesMap.count(path) ? std::vector<char>(filesMap.at(path).c_str(), filesMap.at(path).c_str() + filesMap.at(path).size())
                                    : BinaryEncoder::readBinaryFile(path);
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

    std::map<std::string, std::string> filesMap;
};
