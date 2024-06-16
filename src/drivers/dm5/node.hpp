#pragma once

#ifndef NODE_HPP
#define NODE_HPP

extern "C" {
#include <lexbor/css/css.h>
#include <lexbor/html/html.h>
#include <lexbor/selectors/selectors.h>
}

#include <iostream>
#include <re2/re2.h>

using namespace std;

class Node {
public:
  Node(const string &rawHtml) {
    isDocument = true;

    // init document
    document = lxb_html_document_create();
    if (document == NULL)
      throw "Fail to create document";

    unsigned char *html =
        reinterpret_cast<unsigned char *>(const_cast<char *>(rawHtml.c_str()));

    // parse html
    status = lxb_html_document_parse(document, html, rawHtml.size());
    if (status != LXB_STATUS_OK)
      throw "Fail to parse html";

    body = lxb_dom_interface_node(lxb_html_document_body_element(document));
    if (body == NULL)
      throw "Fail to create node";

    // init css parser
    parser = lxb_css_parser_create();
    status = lxb_css_parser_init(parser, NULL);
    if (status != LXB_STATUS_OK)
      throw "Fail to initialize CSS parser";

    // init selectors
    selectors = lxb_selectors_create();
    status = lxb_selectors_init(selectors);
    if (status != LXB_STATUS_OK)
      throw "Fail to initialize selectors";
  }

  Node(lxb_dom_node_t *node) {
    isDocument = false;
    body = node;

    // init css parser
    parser = lxb_css_parser_create();
    status = lxb_css_parser_init(parser, NULL);
    if (status != LXB_STATUS_OK)
      throw "Fail to initialize CSS parser";

    // init selectors
    selectors = lxb_selectors_create();
    status = lxb_selectors_init(selectors);
    if (status != LXB_STATUS_OK)
      throw "Fail to initialize selectors";
  }

  ~Node() {
    // Destroy Selectors object.
    (void)lxb_selectors_destroy(selectors, true);

    // Destroy resources for CSS Parser.
    (void)lxb_css_parser_destroy(parser, true);

    // Destroy HTML Document.
    if (isDocument)
      (void)lxb_html_document_destroy(document);
  }

  Node *find(string selector) {
    vector<Node *> nodes = findAll(selector);

    if (nodes.size() < 1)
      throw "Element not found";

    if (nodes.size() > 1)
      for (size_t i = 1; i < nodes.size(); i++)
        delete nodes.at(i);

    return nodes.at(0);
  }

  Node *tryFind(string selector) {
    try {
      return this->find(selector);
    } catch (...) {
      return nullptr;
    }
  }

  vector<Node *> findAll(string selector) {
    unsigned char *slctrs =
        reinterpret_cast<unsigned char *>(const_cast<char *>(selector.c_str()));

    list = lxb_css_selectors_parse(parser, slctrs, selector.size());
    if (parser->status != LXB_STATUS_OK)
      throw "Fail to parse css";

    vector<Node *> nodes;
    status = lxb_selectors_find(
        selectors, body, list,
        [](lxb_dom_node_t *node, lxb_css_selector_specificity_t spec,
           void *ctx) -> lxb_status_t {
          vector<Node *> *nodes = reinterpret_cast<vector<Node *> *>(ctx);
          nodes->push_back(new Node(node));

          return LXB_STATUS_OK;
        },
        reinterpret_cast<void *>(&nodes));

    if (status != LXB_STATUS_OK)
      throw "Fail to find nodes";

    // Destroy all Selector List memory.
    if (list->memory != NULL) {
      lxb_css_selector_list_destroy_memory(list);
      list = nullptr;
    }

    return nodes;
  }

  string rawContent() {
    string contentString;

    (void)lxb_html_serialize_deep_cb(
        body,
        [](const lxb_char_t *data, size_t len, void *ctx) -> lxb_status_t {
          string *content = reinterpret_cast<string *>(ctx);
          content->append(reinterpret_cast<const char *>(data), len);
          return LXB_STATUS_OK;
        },
        &contentString);

    return contentString;
  }

  string text() {
    string result = this->rawContent();
    RE2::GlobalReplace(&result, RE2("<[^>]*>"), "");
    return result;
  }

  string getAttribute(string attribute) {
    string tagString;

    (void)lxb_html_serialize_cb(
        body,
        [](const lxb_char_t *data, size_t len, void *ctx) -> lxb_status_t {
          string *content = reinterpret_cast<string *>(ctx);
          content->append(reinterpret_cast<const char *>(data), len);
          return LXB_STATUS_OK;
        },
        &tagString);

    string result;
    re2::StringPiece input(tagString);
    if (RE2::PartialMatch(
            input, RE2(attribute + "\\s*=\\s*['\"]([^'\"]+)['\"]"), &result))
      return result;

    return "";
  }

  string content() {
    string result = this->rawContent();
    RE2::GlobalReplace(&result, RE2("<[^>]*>.*</[^>]*>"), "");
    return result;
  }

private:
  lxb_status_t status;
  lxb_dom_node_t *body;
  lxb_html_document_t *document;
  lxb_css_parser_t *parser;
  lxb_selectors_t *selectors;
  lxb_css_selector_list_t *list;
  bool isDocument;
};

#endif
