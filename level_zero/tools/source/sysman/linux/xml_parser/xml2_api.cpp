/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/xml_parser/xml2_api.h"

namespace L0 {

const std::string Xml2Api::xml2LibraryFile = "libxml2.so.2";
const std::string Xml2Api::xmlInitParserRoutine = "xmlInitParser";
const std::string Xml2Api::xmlCleanupParserRoutine = "xmlCleanupParser";
const std::string Xml2Api::xmlReadFileRoutine = "xmlReadFile";
const std::string Xml2Api::xmlReadMemoryRoutine = "xmlReadMemory";
const std::string Xml2Api::xmlFreeDocRoutine = "xmlFreeDoc";
const std::string Xml2Api::xmlXPathNewContextRoutine = "xmlXPathNewContext";
const std::string Xml2Api::xmlXPathFreeContextRoutine = "xmlXPathFreeContext";
const std::string Xml2Api::xmlXPathEvalRoutine = "xmlXPathEval";
const std::string Xml2Api::xmlXPathNodeEvalRoutine = "xmlXPathNodeEval";
const std::string Xml2Api::xmlXPathCastNodeToStringRoutine = "xmlXPathCastNodeToString";
const std::string Xml2Api::xmlXPathFreeObjectRoutine = "xmlXPathFreeObject";
const std::string Xml2Api::xmlGetNodePathRoutine = "xmlGetNodePath";
const std::string Xml2Api::xmlGetPropRoutine = "xmlGetProp";
const std::string Xml2Api::xmlMemGetRoutine = "xmlMemGet";

template <class T>
bool Xml2Api::getProcAddr(const std::string name, T &proc) {
    void *addr;
    addr = libraryHandle->getProcAddress(name);
    proc = reinterpret_cast<T>(addr);
    return nullptr != proc;
}

bool Xml2Api::loadEntryPoints() {
    bool ok = true;
    ok = ok && getProcAddr(xmlInitParserRoutine, xmlInitParserEntry);
    ok = ok && getProcAddr(xmlCleanupParserRoutine, xmlCleanupParserEntry);
    ok = ok && getProcAddr(xmlReadFileRoutine, xmlReadFileEntry);
    ok = ok && getProcAddr(xmlReadMemoryRoutine, xmlReadMemoryEntry);
    ok = ok && getProcAddr(xmlFreeDocRoutine, xmlFreeDocEntry);
    ok = ok && getProcAddr(xmlXPathNewContextRoutine, xmlXPathNewContextEntry);
    ok = ok && getProcAddr(xmlXPathFreeContextRoutine, xmlXPathFreeContextEntry);
    ok = ok && getProcAddr(xmlXPathEvalRoutine, xmlXPathEvalEntry);
    ok = ok && getProcAddr(xmlXPathNodeEvalRoutine, xmlXPathNodeEvalEntry);
    ok = ok && getProcAddr(xmlXPathCastNodeToStringRoutine, xmlXPathCastNodeToStringEntry);
    ok = ok && getProcAddr(xmlXPathFreeObjectRoutine, xmlXPathFreeObjectEntry);
    ok = ok && getProcAddr(xmlGetNodePathRoutine, xmlGetNodePathEntry);
    ok = ok && getProcAddr(xmlGetPropRoutine, xmlGetPropEntry);

    // Can't just get the symbol for xmlFree. Use xmlMemGet to find the xmlFree routine the library is using
    pXmlMemGet xmlMemGetEntry;
    ok = ok && getProcAddr(xmlMemGetRoutine, xmlMemGetEntry);
    ok = ok && !(*xmlMemGetEntry)(&xmlFreeEntry, nullptr, nullptr, nullptr);
    return ok;
}

void Xml2Api::xmlInitParser() {
    UNRECOVERABLE_IF(nullptr == xmlInitParserEntry);
    (*xmlInitParserEntry)();
    return;
}

void Xml2Api::xmlCleanupParser() {
    UNRECOVERABLE_IF(nullptr == xmlCleanupParserEntry);
    (*xmlCleanupParserEntry)();
    return;
}

xmlDocPtr Xml2Api::xmlReadFile(const std::string filename, int options) {
    UNRECOVERABLE_IF(nullptr == xmlReadFileEntry);
    return (*xmlReadFileEntry)(filename.c_str(), NULL, options);
}

xmlDocPtr Xml2Api::xmlReadMemory(const std::string buffer, int options) {
    UNRECOVERABLE_IF(nullptr == xmlReadMemoryEntry);
    const char *cbuf = buffer.c_str();
    int size = static_cast<int>(buffer.length());
    return (*xmlReadMemoryEntry)(cbuf, size, NULL, NULL, options);
}

void Xml2Api::xmlFreeDoc(xmlDocPtr doc) {
    UNRECOVERABLE_IF(nullptr == xmlFreeDocEntry);
    (*xmlFreeDocEntry)(doc);
    return;
}

xmlXPathContextPtr Xml2Api::xmlXPathNewContext(xmlDocPtr doc) {
    UNRECOVERABLE_IF(nullptr == xmlXPathNewContextEntry);
    return (*xmlXPathNewContextEntry)(doc);
}

void Xml2Api::xmlXPathFreeContext(xmlXPathContextPtr cntx) {
    UNRECOVERABLE_IF(nullptr == xmlXPathFreeContextEntry);
    (*xmlXPathFreeContextEntry)(cntx);
    return;
}

xmlXPathObjectPtr Xml2Api::xmlXPathEval(const std::string expr, xmlXPathContextPtr cntx) {
    UNRECOVERABLE_IF(nullptr == xmlXPathEvalEntry);
    return (*xmlXPathEvalEntry)(reinterpret_cast<const xmlChar *>(expr.c_str()), cntx);
}

xmlXPathObjectPtr Xml2Api::xmlXPathNodeEval(xmlNodePtr node, const std::string expr, xmlXPathContextPtr cntx) {
    UNRECOVERABLE_IF(nullptr == xmlXPathNodeEvalEntry);
    return (*xmlXPathNodeEvalEntry)(node, reinterpret_cast<const xmlChar *>(expr.c_str()), cntx);
}

std::string Xml2Api::xmlXPathCastNodeToString(xmlNodePtr node) {
    UNRECOVERABLE_IF(nullptr == xmlXPathCastNodeToStringEntry);
    xmlChar *pXmlChar = (*xmlXPathCastNodeToStringEntry)(node);
    if (nullptr == pXmlChar) {
        return std::string();
    } else {
        std::string str(reinterpret_cast<char *>(pXmlChar));
        this->xmlFree(pXmlChar);
        return str;
    }
}

void Xml2Api::xmlXPathFreeObject(xmlXPathObjectPtr obj) {
    UNRECOVERABLE_IF(nullptr == xmlXPathFreeObjectEntry);
    (*xmlXPathFreeObjectEntry)(obj);
    return;
}

std::string Xml2Api::xmlGetNodePath(const xmlNodePtr node) {
    UNRECOVERABLE_IF(nullptr == xmlGetNodePathEntry);
    xmlChar *pXmlChar = (*xmlGetNodePathEntry)(node);
    if (nullptr == pXmlChar) {
        return std::string();
    } else {
        std::string str(reinterpret_cast<const char *>(pXmlChar));
        this->xmlFree(pXmlChar);
        return str;
    }
}

std::string Xml2Api::xmlGetProp(const xmlNodePtr node, std::string prop) {
    UNRECOVERABLE_IF(nullptr == xmlGetPropEntry);
    xmlChar *pXmlChar = (*xmlGetPropEntry)(node, reinterpret_cast<const xmlChar *>(prop.c_str()));
    if (nullptr == pXmlChar) {
        return std::string();
    } else {
        std::string str(reinterpret_cast<char *>(pXmlChar));
        this->xmlFree(pXmlChar);
        return str;
    }
}

void Xml2Api::xmlFree(void *obj) {
    UNRECOVERABLE_IF(nullptr == xmlFreeEntry);
    (*xmlFreeEntry)(obj);
    return;
}

Xml2Api *Xml2Api::create() {
    Xml2Api *pXml2Api = new Xml2Api;
    UNRECOVERABLE_IF(nullptr == pXml2Api);

    pXml2Api->libraryHandle = NEO::OsLibrary::load(xml2LibraryFile);
    if (pXml2Api->libraryHandle == nullptr || !pXml2Api->loadEntryPoints()) {
        delete pXml2Api;
        return nullptr;
    }
    return pXml2Api;
}

Xml2Api::~Xml2Api() {
    if (nullptr != libraryHandle) {
        delete libraryHandle;
        libraryHandle = nullptr;
    }
}
} // namespace L0
