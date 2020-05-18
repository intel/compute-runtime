/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

namespace L0 {

typedef void (*pXmlInitParser)();
typedef void (*pXmlCleanupParser)();
typedef xmlDocPtr (*pXmlReadFile)(const char *, const char *, int);
typedef xmlDocPtr (*pXmlReadMemory)(const char *, int, const char *, const char *, int);
typedef void (*pXmlFreeDoc)(xmlDocPtr);
typedef xmlXPathContextPtr (*pXmlXPathNewContext)(xmlDocPtr);
typedef void (*pXmlXPathFreeContext)(xmlXPathContextPtr);
typedef xmlXPathObjectPtr (*pXmlXPathEval)(const xmlChar *, xmlXPathContextPtr);
typedef xmlXPathObjectPtr (*pXmlXPathNodeEval)(xmlNodePtr, const xmlChar *, xmlXPathContextPtr);
typedef xmlChar *(*pXmlXPathCastNodeToString)(xmlNodePtr);
typedef void (*pXmlXPathFreeObject)(xmlXPathObjectPtr);
typedef xmlChar *(*pXmlGetNodePath)(const xmlNodePtr);
typedef xmlChar *(*pXmlGetProp)(const xmlNodePtr, const xmlChar *);
typedef int (*pXmlMemGet)(xmlFreeFunc *, xmlMallocFunc *, xmlReallocFunc *, xmlStrdupFunc *);

class Xml2Api : public NEO::NonCopyableOrMovableClass {
  public:
    void xmlInitParser();
    void xmlCleanupParser();
    xmlDocPtr xmlReadFile(const std::string filename, int options);
    xmlDocPtr xmlReadMemory(const std::string buffer, int options);
    void xmlFreeDoc(xmlDocPtr doc);
    xmlXPathContextPtr xmlXPathNewContext(xmlDocPtr doc);
    void xmlXPathFreeContext(xmlXPathContextPtr);
    xmlXPathObjectPtr xmlXPathEval(const std::string expr, xmlXPathContextPtr cntx);
    xmlXPathObjectPtr xmlXPathNodeEval(xmlNodePtr node, const std::string expr, xmlXPathContextPtr cntx);
    std::string xmlXPathCastNodeToString(xmlNodePtr node);
    void xmlXPathFreeObject(xmlXPathObjectPtr obj);
    std::string xmlGetNodePath(const xmlNodePtr node);
    std::string xmlGetProp(const xmlNodePtr node, std::string prop);

    ~Xml2Api();
    static Xml2Api *create();

  private:
    template <class T>
    bool getProcAddr(const std::string name, T &proc);
    Xml2Api() = default;

    bool loadEntryPoints();
    void xmlFree(void *obj);

    NEO::OsLibrary *libraryHandle = nullptr;
    static const std::string xml2LibraryFile;
    static const std::string xmlInitParserRoutine;
    static const std::string xmlCleanupParserRoutine;
    static const std::string xmlReadFileRoutine;
    static const std::string xmlReadMemoryRoutine;
    static const std::string xmlFreeDocRoutine;
    static const std::string xmlXPathNewContextRoutine;
    static const std::string xmlXPathFreeContextRoutine;
    static const std::string xmlXPathEvalRoutine;
    static const std::string xmlXPathNodeEvalRoutine;
    static const std::string xmlXPathCastNodeToStringRoutine;
    static const std::string xmlXPathFreeObjectRoutine;
    static const std::string xmlGetNodePathRoutine;
    static const std::string xmlGetPropRoutine;
    static const std::string xmlMemGetRoutine;

    pXmlInitParser xmlInitParserEntry = nullptr;
    pXmlCleanupParser xmlCleanupParserEntry = nullptr;
    pXmlReadFile xmlReadFileEntry = nullptr;
    pXmlReadMemory xmlReadMemoryEntry = nullptr;
    pXmlFreeDoc xmlFreeDocEntry = nullptr;
    pXmlXPathNewContext xmlXPathNewContextEntry = nullptr;
    pXmlXPathFreeContext xmlXPathFreeContextEntry = nullptr;
    pXmlXPathEval xmlXPathEvalEntry = nullptr;
    pXmlXPathNodeEval xmlXPathNodeEvalEntry = nullptr;
    pXmlXPathCastNodeToString xmlXPathCastNodeToStringEntry = nullptr;
    pXmlXPathFreeObject xmlXPathFreeObjectEntry = nullptr;
    pXmlGetNodePath xmlGetNodePathEntry = nullptr;
    pXmlGetProp xmlGetPropEntry = nullptr;
    xmlFreeFunc xmlFreeEntry = nullptr;
};

} // namespace L0
