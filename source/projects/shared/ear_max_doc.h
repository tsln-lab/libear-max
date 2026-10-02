/// @file
/// @brief   Reference page (maxref.xml) generator for the libear-max externals.
/// @license Use of this source code is governed by the MIT License found in the License.md file.
///
/// Min generates docs/<name>.maxref.xml from the description strings when an
/// external is first loaded in Max, but its pages list the attributes only
/// under "Attributes", do not escape '<' and '>' and cut digests at any
/// period (also "e.g."). This generator writes the pages ourselves (the unit
/// tests run it, see ear_max_test.h) with these differences:
///
/// - every settable attribute is also listed under "Messages", because in Max
///   an attribute is set by sending its name as a message ('azimuth 30'),
///   with an argument list showing the type;
/// - messages and attributes are listed alphabetically, so the output is
///   stable across platforms and can be committed to the repository;
/// - text is XML-escaped; Min's markup for attribute, message and object
///   links (@name, #name, [name]) is still supported;
/// - the default value and the range of an attribute are appended to its
///   description.
///
/// Min has no API for an attribute's default, so the generator reads the
/// values of a freshly created instance.
///
/// The classes set MIN_FLAGS{ documentation_flags::do_not_generate } so that
/// Max does not overwrite the shipped pages with Min's own version.

#pragma once

#include "c74_min.h"
#include "c74_min_doc.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace earmax::doc {

using namespace c74::min;

/// Escape the characters that are special in XML element text.
inline std::string text_escape(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

/// Escape the characters that are special in an XML attribute value.
inline std::string xml_escape(const std::string& text)
{
    std::string out;
    for (const char c : text_escape(text)) {
        switch (c) {
            case '\'': out += "&apos;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

/// A Min symbol as a string. Copy-initialisation picks the symbol's
/// std::string conversion; std::string(sym) is ambiguous for MSVC because
/// the symbol also converts to const char*.
inline std::string symbol_string(const symbol& s)
{
    return s;
}

/// Atoms as text: numbers in their shortest form, symbols as they are.
inline std::string atoms_string(const atoms& values)
{
    std::string out;
    for (const auto& a : values) {
        if (!out.empty()) {
            out += ' ';
        }
        if (a.a_type == c74::max::A_FLOAT) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(a));
            out += buffer;
        }
        else if (a.a_type == c74::max::A_LONG) {
            out += std::to_string(static_cast<long>(a));
        }
        else {
            out += std::string(a);
        }
    }
    return out;
}

/// The range of an attribute as a sentence, from Min's range string
/// ('"0" "1" ' for a numeric range, the quoted names for an enum).
inline std::string range_sentence(const std::string& range_string)
{
    strings values;
    for (auto& token : str::split(range_string, ' ')) {
        token = str::trim(token);
        if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.size() - 2);
        }
        if (!token.empty()) {
            values.push_back(token);
        }
    }
    if (values.empty()) {
        return "";
    }
    const auto numeric = [](const std::string& v) {
        char* end = nullptr;
        std::strtod(v.c_str(), &end);
        return end && *end == '\0';
    };
    if (values.size() == 2 && numeric(values[0]) && numeric(values[1])) {
        return " Range: " + values[0] + " to " + values[1] + ".";
    }
    std::string out = " Possible values: ";
    for (size_t i = 0; i < values.size(); ++i) {
        out += (i ? ", " : "") + values[i];
    }
    return out + ".";
}

/// Escape a description and convert Min's markup tokens: '@name' becomes an
/// attribute link, '#name' a message link and '[name]' an object link.
inline std::string format(const std::string& input)
{
    strings tokens = str::split(input, ' ');
    for (auto& token : tokens) {
        if (token.size() > 1 && token[0] == '@') {
            token = "<at>" + text_escape(token.substr(1)) + "</at>";
        }
        else if (token.size() > 1 && token[0] == '#') {
            token = "<m>" + text_escape(token.substr(1)) + "</m>";
        }
        else if (token.size() > 2 && token.front() == '[' && token.back() == ']') {
            token = "<o>" + text_escape(token.substr(1, token.size() - 2)) + "</o>";
        }
        else {
            token = text_escape(token);
        }
    }
    return str::trim(str::join(tokens)); // Min's join leaves a trailing space
}

/// The first sentence of a description, without the final period. A period
/// ends a sentence only when it is followed by a space and a capital letter
/// or ends the text, so "e.g. 0+5+0" does not cut the digest short.
inline std::string digest(const std::string& description)
{
    for (size_t i = 0; i < description.size(); ++i) {
        if (description[i] != '.') {
            continue;
        }
        if (i + 1 == description.size()) {
            return description.substr(0, i);
        }
        if (description[i + 1] == ' ') {
            size_t next = i + 1;
            while (next < description.size() && description[next] == ' ') {
                ++next;
            }
            if (next == description.size() || (description[next] >= 'A' && description[next] <= 'Z')) {
                return description.substr(0, i);
            }
        }
    }
    return description;
}

/// The argument type for a reference page, from the datatype of a Min attribute.
inline std::string argument_type(const std::string& datatype)
{
    if (datatype == "float64" || datatype == "float32") {
        return "float";
    }
    if (datatype == "long" || datatype == "int") {
        return "int";
    }
    if (datatype == "symbol") {
        return "symbol";
    }
    return "list";
}

template <class min_class_type>
std::string class_description()
{
    std::string description;
    if constexpr (has_class_description<min_class_type>::value) {
        description = min_class_type::class_description;
    }
    else {
        description = "Unknown";
    }
    return description;
}

/// Write the reference page of a Min class.
/// @param instance   An instance of the class (its attributes, messages and arguments are listed).
/// @param path       Where to write the page, usually docs/<max_class_name>.maxref.xml.
/// @param max_name   The name of the external as seen in Max.
template <class min_class_type>
void generate(const min_class_type& instance, const std::string& path, const std::string& max_name)
{
    std::ofstream out{ path };
    if (!out) {
        throw std::runtime_error("cannot write the reference page " + path);
    }

    strings tags;
    get_tags<min_class_type>(tags);
    for (auto& tag : tags) {
        tag = str::trim(tag);
    }

    std::string author;
    doc_get_author<min_class_type>(author);
    strings related;
    doc_get_related<min_class_type>(related);

    const std::string description = class_description<min_class_type>();

    out << "<?xml version='1.0' encoding='utf-8' standalone='yes'?>\n";
    out << "<!-- Generated by the libear-max unit tests from the descriptions in the source (see source/projects/shared/ear_max_doc.h). Do not edit. -->\n";
    out << "<c74object name='" << xml_escape(max_name) << "' category='";
    for (size_t i = 0; i < tags.size(); ++i) {
        out << (i ? ", " : "") << xml_escape(tags[i]);
    }
    out << "'>\n\n";
    out << "\t<digest>" << format(digest(description)) << "</digest>\n";
    out << "\t<description>" << format(description) << "</description>\n\n";

    // metadata
    out << "\t<!--METADATA-->\n\n\t<metadatalist>\n";
    out << "\t\t<metadata name='author'>" << xml_escape(author) << "</metadata>\n";
    for (const auto& tag : tags) {
        out << "\t\t<metadata name='tag'>" << xml_escape(tag) << "</metadata>\n";
    }
    out << "\t</metadatalist>\n\n";

    // arguments
    out << "\t<!--ARGUMENTS-->\n\n\t<objarglist>\n";
    for (const auto* arg : instance.arguments()) {
        const std::string text = arg->description_string();
        out << "\t\t<objarg name='" << xml_escape(arg->name()) << "' optional='" << (arg->required() ? "0" : "1") << "'";
        if (!arg->type().empty()) {
            out << " type='" << xml_escape(arg->type()) << "'";
        }
        out << ">\n";
        out << "\t\t\t<digest>" << format(digest(text)) << "</digest>\n";
        out << "\t\t\t<description>" << format(text) << "</description>\n";
        out << "\t\t</objarg>\n";
    }
    out << "\t</objarglist>\n\n";

    // settable attributes, sorted by name
    std::map<std::string, const attribute_base*> settable;
    std::map<std::string, const attribute_base*> listed;
    for (const auto& [name, attr] : instance.attributes()) {
        if (attr->visible() == visibility::hide) {
            continue;
        }
        listed[name] = attr;
        if (attr->writable()) {
            settable[name] = attr;
        }
    }

    // messages: the object's own messages plus one per settable attribute
    struct method {
        std::string digest;
        std::string description;
        std::string arg_name;
        std::string arg_type;
    };
    std::map<std::string, method> methods;
    for (const auto& [name, message] : instance.messages()) {
        if (message->type() == c74::max::A_CANT) {
            continue;
        }
        const std::string text = message->description_string();
        methods[name] = { digest(text), text, "", "" };
    }
    for (const auto& [name, attr] : settable) {
        if (methods.count(name)) {
            continue; // the class defines a message with the same name
        }
        const std::string text = attr->description_string();
        methods[name] = { "Set the " + name + " attribute",
                          "Set the @" + name + " attribute. " + text,
                          name, argument_type(symbol_string(attr->datatype())) };
    }

    out << "\t<!--MESSAGES-->\n\n\t<methodlist>\n";
    for (const auto& [name, m] : methods) {
        out << "\t\t<method name='" << xml_escape(name) << "'>\n";
        if (!m.arg_name.empty()) {
            out << "\t\t\t<arglist>\n";
            out << "\t\t\t\t<arg name='" << xml_escape(m.arg_name) << "' optional='0' type='" << m.arg_type << "' />\n";
            out << "\t\t\t</arglist>\n";
        }
        out << "\t\t\t<digest>" << format(m.digest) << "</digest>\n";
        out << "\t\t\t<description>" << format(m.description) << "</description>\n";
        out << "\t\t</method>\n";
    }
    out << "\t</methodlist>\n\n";

    // attributes
    out << "\t<!--ATTRIBUTES-->\n\n\t<attributelist>\n";
    for (const auto& [name, attr] : listed) {
        // The instance was just created, so its current values are the defaults
        // (Min's default_string() is empty: it never stores the default).
        std::string text = attr->description_string() + range_sentence(attr->range_string());
        const std::string fallback = atoms_string(attr->get_atoms());
        if (!fallback.empty()) {
            text += " Default: " + fallback + ".";
        }
        out << "\t\t<attribute name='" << xml_escape(name) << "' get='1' set='" << (attr->writable() ? "1" : "0")
            << "' type='" << xml_escape(symbol_string(attr->datatype())) << "' size='1'>\n";
        out << "\t\t\t<digest>" << format(digest(attr->description_string())) << "</digest>\n";
        out << "\t\t\t<description>" << format(text) << "</description>\n";
        out << "\t\t</attribute>\n";
    }
    out << "\t</attributelist>\n\n";

    // related
    out << "\t<!--RELATED-->\n\n\t<seealsolist>\n";
    for (const auto& name : related) {
        out << "\t\t<seealso name='" << xml_escape(str::trim(name)) << "' />\n";
    }
    out << "\t</seealsolist>\n\n";

    out << "</c74object>\n";
}

} // namespace earmax::doc
