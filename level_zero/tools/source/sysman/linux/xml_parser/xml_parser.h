/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

namespace L0 {

class XmlNode {
  public:
    virtual ~XmlNode() = default;

    virtual std::vector<XmlNode *> xPath(std::string path) = 0;
    virtual std::string getName() = 0;
    virtual std::string getPath() = 0;
    virtual std::string getText() = 0;
    virtual std::string getAttribute(std::string attribute) = 0;
};

class XmlDoc {
  public:
    virtual ~XmlDoc() = default;

    virtual std::vector<XmlNode *> xPath(std::string path) = 0;
};

class XmlParser {
  public:
    virtual ~XmlParser() = default;

    virtual XmlDoc *parseFile(std::string filename) = 0;
    virtual XmlDoc *parseBuffer(std::string buffer) = 0;

    static XmlParser *create();
};

} // namespace L0
