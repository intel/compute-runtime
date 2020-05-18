/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/linux/xml_parser/xml2_api.h"
#include "level_zero/tools/source/sysman/linux/xml_parser/xml_parser.h"

namespace L0 {

class XmlNodeImp : public XmlNode, NEO::NonCopyableOrMovableClass {
  public:
    XmlNodeImp() = delete;
    XmlNodeImp(Xml2Api *pXml2Api, xmlDocPtr doc, xmlNodePtr node) : pXml2Api(pXml2Api), doc(doc), node(node) {}
    ~XmlNodeImp() override = default;

    std::vector<XmlNode *> xPath(std::string path) override;
    std::string getName() override;
    std::string getPath() override;
    std::string getText() override;
    std::string getAttribute(std::string attribute) override;

  private:
    Xml2Api *pXml2Api = nullptr;
    xmlDocPtr doc = nullptr;
    xmlNodePtr node = nullptr;
};

class XmlDocImp : public XmlDoc, NEO::NonCopyableOrMovableClass {
  public:
    XmlDocImp() = delete;
    ~XmlDocImp() override;

    std::vector<XmlNode *> xPath(std::string path) override;

    static XmlDoc *parseFile(Xml2Api *pXml2Api, std::string filename);
    static XmlDoc *parseBuffer(Xml2Api *pXml2Api, std::string buffer);

  private:
    XmlDocImp(Xml2Api *pXml2Api, xmlDocPtr doc) : pXml2Api(pXml2Api), doc(doc) {}

    Xml2Api *pXml2Api = nullptr;
    xmlDocPtr doc = nullptr;
};

class XmlParserImp : public XmlParser, NEO::NonCopyableOrMovableClass {
  public:
    XmlParserImp();
    ~XmlParserImp() override;

    XmlDoc *parseFile(std::string filename) override;
    XmlDoc *parseBuffer(std::string buffer) override;

    bool isAvailable() { return nullptr != pXml2Api; }

  private:
    Xml2Api *pXml2Api = nullptr;
};

} // namespace L0
