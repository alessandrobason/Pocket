#include "xml.h"

#include "logging.h"
#include "std/file.h"
#include "std/stream.h"
#include "std/arena.h"

static xml_t xml__parse(arena_t *arena, str_t data, xmlflags_e flags);
static xmlelem_t *xml__parse_element(arena_t *arena, instream_t *in, xmlflags_e flags);
static bool xml__parse_attributes(arena_t *arena, instream_t *in, xmlelem_t *el, xmlflags_e flags);
static void xml__parse_body(arena_t *arena, instream_t *in, xmlelem_t *element, xmlflags_e flags);
static void xml__parse_body_html(arena_t *arena, instream_t *in, xmlelem_t *element, xmlflags_e flags);
static void xml__try_add_body_html(arena_t *arena, xmlelem_t *element, strview_t body);

xml_t xmlParse(arena_t *arena, const char *filename, xmlflags_e flags) {
    return xml__parse(arena, fileReadWholeText(arena, filename), flags);
}

xml_t xmlParseStr(arena_t *arena, str_t str, xmlflags_e flags) {
    return xml__parse(arena, flags & XML_COPY_STR ? strDup(arena, &str) : str, flags);
}

xmlelem_t *xmlGet(const xmlelem_t *self, strview_t tag) {
    if (!self) return NULL;
    xmlelem_t *el = self->children;
    while (el) {
        if (strvEqual(el->tag, tag)) {
            break;
        }
        el = el->next;
    }
    return el;
}

xmlattr_t *xmlGetAttr(const xmlelem_t *self, strview_t key) {
    if (!self) return NULL;
    xmlattr_t *at = self->attributes;
    while (at) {
        if (strvEqual(at->key, key)) {
            break;
        }
        at = at->next;
    }
    return at;
}

static xml_t xml__parse(arena_t *arena, str_t data, xmlflags_e flags) {
    if (strIsEmpty(&data)) {
        err("No data: %zu", data.len);
        goto fail;
    }

    xml_t doc = {
        .text = data
    };

    instream_t in = istrLen(data.buf, data.len);

    if (!istrExpect(&in, '<')) {
        err("missing opening tag: %.10s", in.cur);
        goto fail;
    }
    doc.root = xml__parse_element(arena, &in, flags);
    istrSkipWhitespace(&in);

    while (!istrIsFinished(&in)) {
        switch (istrPeek(&in)) {
            case '<': {
                istrSkip(&in, 1);
                // addElement
                xmlelem_t *element = xml__parse_element(arena, &in, flags);
                listAdd(doc.root->children, doc.root->child_tail, element);
                break;
            }
            default:
                info("unrecognised: %c", istrPeek(&in));
                istrSkip(&in , 1);
        }
        istrSkipWhitespace(&in);
    }

    return doc;
fail:
    err("failed to parse xml file");
    return (xml_t){0};
}

static xmlelem_t *xml__parse_element(arena_t *arena, instream_t *in, xmlflags_e flags) {
    // check if it is a special element
    switch (istrPeek(in)) {
        // first xml element
        case '?':
            istrSkip(in, 1);
            break;
        // comment
        case '!':
            if (istrExpectView(in, strv("!--"))) {
                istrSkip(in, 3);
                istrIgnoreViewAndSkip(in, strv("-->"));
                return NULL;
            }
            if (istrExpectView(in, strv("!DOCTYPE"))) {
                istrIgnoreAndSkip(in, '>');
                return NULL;
            }
            istrSkip(in, 1);
            break;
    }

    xmlelem_t *element = new(arena, xmlelem_t);
    byte test__buf[sizeof(xmlelem_t)] = {0};
    assert(memcmp(element, test__buf, sizeof(xmlelem_t)) == 0);
    element->tag = istrGetViewEither(in, strv(" />"));
    istrSkipWhitespace(in);
    // if element is finished, ie <img/>
    if (xml__parse_attributes(arena, in, element, flags)) {
        return element;
    }

    istrSkipWhitespace(in);
    
    if (flags & XML_HTML) {
        xml__parse_body_html(arena, in, element, flags);
    }
    else {
        xml__parse_body(arena, in, element, flags);
    }

    return element;
}

static bool xml__parse_attributes(arena_t *arena, instream_t *in, xmlelem_t *el, xmlflags_e flags) {
    while (!istrIsFinished(in) && !istrExpect(in, '>')) {
        // check special endings
        switch (istrPeek(in)) {
            // <?xml ?>
            case '?':
                if (!istrExpectView(in, strv("?>"))) {
                    warn("expexted '?>' (%.10s)", in->cur);
                }
                return true;
            // empty tag <tag/>
            case '/':
                if (!istrExpectView(in, strv("/>"))) {
                    warn("expexted '/>' (%.10s)", in->cur);
                }
                return true;
        }

        strview_t key = strvTrim(istrGetView(in, '='));
        istrSkip(in, 1); // skip divider
        char delim = istrPeek(in);
        istrSkip(in, 1); // skip either ' or "
        strview_t value = strvTrim(istrGetView(in, delim));
        istrSkip(in, 1); // skip either ' or "

        xmlattr_t *at = new(arena, xmlattr_t);
        at->key = key;
        at->value = value;
        listAdd(el->attributes, el->attr_tail, at);

        istrSkipWhitespace(in);
    }
    return false;
}

static void xml__parse_body(arena_t *arena, instream_t *in, xmlelem_t *element, xmlflags_e flags) {
    while (istrExpect(in, '<')) {
        if (istrPeek(in) == '/') {
            istrRewindN(in, 1);
            break;
        }
        xmlelem_t *child = xml__parse_element(arena, in, flags);
        if (child) {
            listAdd(element->children, element->child_tail, child);
        }
        istrSkipWhitespace(in);
    }
    element->body = istrGetView(in, '<');

    if (!istrExpectView(in, strv("</"))) {
        warn("expected '</' (%.10s)", in->cur);
    }
    if (!istrExpectView(in, element->tag)) {
        warn("closing tag different than opening tag: %.*s !- %.10s", fmtstrv(element->tag), in->cur);
    }
    if (!istrExpect(in, '>')) {
        warn("expected '>' (%.10s)", in->cur);
    }
}

static void xml__parse_body_html(arena_t *arena, instream_t *in, xmlelem_t *element, xmlflags_e flags) {
    bool is_finished = false;
    while (!is_finished) {
        // try to read a child
        while (istrExpect(in, '<')) {
            if (istrPeek(in) == '/') {
                istrRewindN(in, 1);
                break;
            }
            xmlelem_t *child = xml__parse_element(arena, in, flags);
            if (child) {
                listAdd(element->children, element->child_tail, child);
            }
            istrSkipWhitespace(in);
        }

        // try to read body
        strview_t body = istrGetView(in, '<');
        xml__try_add_body_html(arena, element, body);
        istrSkipWhitespace(in);

        // check if the tag is finished, otherwise go back to parsing tags
        is_finished = istrExpectView(in, strv("</"));
    }
    
    if (!istrExpectView(in, element->tag)) {
        warn("closing tag different than opening tag: %.*s !- %.10s", fmtstrv(element->tag), in->cur);
    }
    if (!istrExpect(in, '>')) {
        warn("expected '>' (%.10s)", in->cur);
    }
}

static void xml__try_add_body_html(arena_t *arena, xmlelem_t *element, strview_t body) {
    if (strvIsEmpty(body)) return;
    // add an empty child with only the body
    xmlelem_t *child = new(arena, xmlelem_t);
    child->body = body;
    listAdd(element->children, element->child_tail, child);
}