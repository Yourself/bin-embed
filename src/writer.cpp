#include "writer.h"

#include <filesystem>
#include <fstream>

namespace {

namespace fs = std::filesystem;
const char IncludeGuardName[] = "GENERATED_BIN_EMBED_RESOURCES_H_";

void writePreamble(std::ostream& os, const GeneratorArgs& args) {
    if (args.usePragma) {
        os << "#pragma once\n";
    } else {
        os << "#ifndef " << IncludeGuardName << '\n' << "#define " << IncludeGuardName << '\n';
    }
    os << '\n'
       << "#include <cstdint>\n"
       << "#include <map>\n"
       << "#include <string>\n"
       << "#include <string_view>\n"
       << '\n';
}

void writePostamble(std::ostream& os, const GeneratorArgs& args) {
    if (!args.usePragma) {
        os << "#endif // " << IncludeGuardName << '\n';
    }
}

void writeStringSafeChar(std::ostream& os, unsigned char c) {
    switch (c) {
    case '\\':
        os << "\\\\";
        break;
    case '"':
        os << "\\\"";
        break;
    case '\n':
        os << "\\n";
        break;
    case '\f':
        os << "\\f";
        break;
    case '\r':
        os << "\\r";
        break;
    case '\t':
        os << "\\t";
        break;
    case '\b':
        os << "\\b";
        break;
    default:
        if (std::isprint(c)) {
            os << c;
        } else {
            os << "\\x" << std::hex << std::noshowbase << std::setfill('0') << std::setw(2)
               << static_cast<int>(c) << "\" \"";
        }
    }
}

void writeStringLiteral(std::ostream& os, std::string_view str) {
    os << "\"";
    for (char c : str) {
        writeStringSafeChar(os, c);
    }
    os << "\"";
}

void writeIdentifier(std::ostream& os, std::string_view str) {
    for (const auto c : str) {
        os << (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    }
}

void writeFileData(std::ostream& os, const std::string& root, const std::string& path, std::size_t chunkSize) {
    fs::path source(root);
    source /= path;
    auto size = fs::file_size(source);
    std::ifstream fd(source.c_str(), std::ios::binary);
    bool useSimple = chunkSize == 0 || size <= chunkSize;
    if (useSimple) {
        os << "const char ";
        writeIdentifier(os, path);
        os << "[] = \"";
        while (true) {
            char c;
            fd.get(c);
            if (fd.eof()) {
                break;
            }
            writeStringSafeChar(os, c);
        }
        os << "\";\n";
    }

    os << "std::string_view get_";
    writeIdentifier(os, path);
    os << "() {";

    if (useSimple) {
        os << " return ";
        writeIdentifier(os, path);
        os << ";";
    } else {
        os << "\n"
           << "  static auto ret = []() {\n"
           << "    std::string s;\n"
           << "    s.reserve(" << std::dec << size << ");\n"
           << "    s.append(\"";
        std::size_t count = 0;
        while (true) {
            char c;
            fd.get(c);
            if (fd.eof()) {
                break;
            }
            writeStringSafeChar(os, c);
            if (++count == chunkSize) {
                os << "\");\n"
                   << "    ret.append(\"";
                count = 0;
            }
        }
        os << "\");\n"
           << "    return s;\n"
           << "  }();\n"
           << "  return ret;\n";
    }

    os << "}\n";
}

void writeDataSection(std::ostream& os, const GeneratorArgs& args) {
    os << "namespace resources_detail {\n";
    for (const auto& file : args.sources) {
        writeFileData(os, args.root, file, args.chunk);
    }
    os << "}\n";
}

void writeManager(std::ostream& os, const GeneratorArgs& args) {
    os << "std::string_view find_resource(std::string_view path) {\n"
       << "  using Fn = std::string_view (*)();\n"
       << "  static auto pathTable = []() {\n"
       << "    return std::map<std::string, Fn, std::less<>>{\n";
    bool first = true;
    for (const auto& file : args.sources) {
        if (!first) {
            os << ",\n";
        }
        first = false;
        os << "      {";
        writeStringLiteral(os, file);
        os << ", &resources_detail::get_";
        writeIdentifier(os, file);
        os << "}";
    }
    os << "\n"
       << "    };\n"
       << "  }();\n"
       << "  auto it = pathTable.find(path);\n"
       << "  return it != pathTable.end() ? it->second() : std::string_view{};\n"
       << "}\n";
}

}  // namespace

void writeHeader(std::ostream& os, const GeneratorArgs& args) {
    writePreamble(os, args);
    writeDataSection(os, args);
    writeManager(os, args);
    writePostamble(os, args);
}