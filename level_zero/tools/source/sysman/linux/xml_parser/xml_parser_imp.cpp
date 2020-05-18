/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/linux/xml_parser/xml_parser_imp.h"

namespace L0 {

std::vector<XmlNode *> XmlNodeImp::xPath(std::string path) {
    std::vector<XmlNode *> nodes;
    xmlXPathContextPtr cntx = pXml2Api->xmlXPathNewContext(doc);
    if (nullptr != cntx) {
        xmlXPathObjectPtr xpathObj = pXml2Api->xmlXPathNodeEval(node, path, cntx);
        if (nullptr != xpathObj) {
            xmlNodeSetPtr nodeset = xpathObj->nodesetval;
            for (int i = 0; i < xmlXPathNodeSetGetLength(nodeset); i++) {
                xmlNodePtr node = xmlXPathNodeSetItem(nodeset, i);
                if (nullptr == node) {
                    continue;
                }
                XmlNodeImp *pXmlNodeImp = new XmlNodeImp(pXml2Api, doc, node);
                UNRECOVERABLE_IF(nullptr == pXmlNodeImp);
                nodes.push_back(pXmlNodeImp);
            }
            pXml2Api->xmlXPathFreeObject(xpathObj);
        }
        pXml2Api->xmlXPathFreeContext(cntx);
    }
    return nodes;
}

std::string XmlNodeImp::getName() {
    return std::string(reinterpret_cast<const char *>(node->name));
}

std::string XmlNodeImp::getPath() {
    return pXml2Api->xmlGetNodePath(node);
}

std::string XmlNodeImp::getText() {
    return pXml2Api->xmlXPathCastNodeToString(node);
}

std::string XmlNodeImp::getAttribute(std::string attribute) {
    return pXml2Api->xmlGetProp(node, attribute);
}

std::vector<XmlNode *> XmlDocImp::xPath(std::string path) {
    std::vector<XmlNode *> nodes;
    xmlXPathContextPtr cntx = pXml2Api->xmlXPathNewContext(doc);
    if (nullptr != cntx) {
        xmlXPathObjectPtr xpathObj = pXml2Api->xmlXPathEval(path, cntx);
        if (nullptr != xpathObj) {
            xmlNodeSetPtr nodeset = xpathObj->nodesetval;
            for (int i = 0; i < xmlXPathNodeSetGetLength(nodeset); i++) {
                xmlNodePtr node = xmlXPathNodeSetItem(nodeset, i);
                if (nullptr == node) {
                    continue;
                }
                XmlNodeImp *pXmlNodeImp = new XmlNodeImp(pXml2Api, doc, node);
                UNRECOVERABLE_IF(nullptr == pXmlNodeImp);
                nodes.push_back(pXmlNodeImp);
            }
            pXml2Api->xmlXPathFreeObject(xpathObj);
        }
        pXml2Api->xmlXPathFreeContext(cntx);
    }
    return nodes;
}

XmlDoc *XmlDocImp::parseFile(Xml2Api *pXml2Api, std::string filename) {
    // Don't allow the parser to print errors or warnings. Don't allow network file access.
    xmlDocPtr doc = pXml2Api->xmlReadFile(filename, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (nullptr != doc) {
        return new XmlDocImp(pXml2Api, doc);
    }
    return nullptr;
}

XmlDoc *XmlDocImp::parseBuffer(Xml2Api *pXml2Api, std::string buffer) {
    // Don't allow the parser to print errors or warnings. Don't allow network file access.
    xmlDocPtr doc = pXml2Api->xmlReadMemory(buffer, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (nullptr != doc) {
        return new XmlDocImp(pXml2Api, doc);
    }
    return nullptr;
}

XmlDocImp::~XmlDocImp() {
    pXml2Api->xmlFreeDoc(doc);
}

XmlDoc *XmlParserImp::parseFile(std::string filename) {
    return XmlDocImp::parseFile(pXml2Api, filename);
}

XmlDoc *XmlParserImp::parseBuffer(std::string buffer) {
    return XmlDocImp::parseBuffer(pXml2Api, buffer);
}

XmlParserImp::XmlParserImp() {
    pXml2Api = Xml2Api::create();
    if (nullptr != pXml2Api) {
        pXml2Api->xmlInitParser();
    }
}

XmlParserImp::~XmlParserImp() {
    if (nullptr != pXml2Api) {
        pXml2Api->xmlCleanupParser();
        delete pXml2Api;
        pXml2Api = nullptr;
    }
}

XmlParser *XmlParser::create() {
    XmlParserImp *pXmlParserImp = new XmlParserImp;
    UNRECOVERABLE_IF(nullptr == pXmlParserImp);
    if (!pXmlParserImp->isAvailable()) {
        delete pXmlParserImp;
        pXmlParserImp = nullptr;
    }
    return pXmlParserImp;
}

} // namespace L0
