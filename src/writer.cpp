#include "writer.h"

#include <execution>
#include <filesystem>
#include <fstream>

namespace {

namespace fs = std::filesystem;
const char IncludeGuardName[] = "GENERATED_BIN_EMBED_RESOURCES_H_";

struct FileReader {
  public:
    using iterator = std::istreambuf_iterator<char>;

    template <class... Args>
    FileReader(Args&&... args) : fd_(std::forward<Args>(args)...) {}

    iterator begin() { return {fd_.rdbuf()}; }
    iterator end() { return {}; }

  private:
    std::ifstream fd_;
};

void writePreamble(std::ostream& os, const GeneratorArgs& args) {
    if (args.usePragma) {
        os << "#pragma once\n";
    } else {
        os << "#ifndef " << IncludeGuardName << '\n' << "#define " << IncludeGuardName << '\n';
    }
    os << '\n'
       << "#include <cstdint>\n"
       << "#include <map>\n"
       << (args.headerOnly ? "#include <string>\n" : "") << "#include <string_view>\n"
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
            os << "\\x" << std::hex << std::noshowbase << std::setfill('0') << std::setw(2) << static_cast<int>(c)
               << "\" \"";
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

std::ostream& writeIdentifier(std::ostream& os, std::string_view str) {
    for (const auto c : str) {
        os << (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    }
    return os;
}

std::ostream& writeGetFunction(std::ostream& os, const std::string& path) {
    os << "std::string_view get_";
    writeIdentifier(os, path);
    os << "()";
    return os;
}

void writeFileData(std::ostream& os, const std::string& root, const std::string& path, std::size_t chunkSize) {
    fs::path source(root);
    source /= path;
    auto size = fs::file_size(source);
    FileReader reader(source, std::ios::binary);
    bool useSimple = chunkSize == 0 || size <= chunkSize;
    if (useSimple) {
        os << "const char ";
        writeIdentifier(os, path);
        os << "[] = \"";
        for (char c : reader) {
            writeStringSafeChar(os, c);
        }
        os << "\";\n";
    }

    os << "inline ";
    writeGetFunction(os, path) << '{';

    if (useSimple) {
        os << " return ";
        writeIdentifier(os, path) << ';';
    } else {
        os << "\n"
           << "  static auto ret = []() {\n"
           << "    std::string s;\n"
           << "    s.reserve(" << std::dec << size << ");\n"
           << "    s.append(\"";
        std::size_t count = 0;
        for (char c : reader) {
            writeStringSafeChar(os, c);
            if (++count == chunkSize) {
                os << "\", " << std::dec << chunkSize << ");\n"
                   << "    s.append(\"";
                count = 0;
            }
        }
        os << "\", " << std::dec << count << ");\n"
           << "    return s;\n"
           << "  }();\n"
           << "  return ret;\n";
    }

    os << "}\n";
}

void writeDataSection(std::ostream& os, const GeneratorArgs& args) {
    os << "namespace resources_detail {\n";
    if (args.headerOnly) {
        for (const auto& file : args.sources) {
            writeFileData(os, args.root, file, args.chunk);
        }
    } else {
        for (const auto& file : args.sources) {
            writeGetFunction(os, file) << ";\n";
        }
    }
    os << "}\n";
}

void writeManager(std::ostream& os, const GeneratorArgs& args) {
    os << "inline std::string_view find_resource(std::string_view path) {\n"
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

void writeImpls(std::string_view header, const fs::path& projectRoot, const GeneratorArgs& args) {
    std::for_each(std::execution::par_unseq, args.sources.begin(), args.sources.end(), [&](const std::string& file) {
        fs::path outPath = projectRoot / (file + ".cpp");
        fs::create_directories(outPath);
        std::ofstream out(outPath);

        out << "#include \"" << header << "\"\n\n"
            << "#include <string>\n\n"
            << "namespace ";
        if (!args.nspace.empty()) {
            out << args.nspace << "::";
        }
        out << "resources_detail {\n";
        writeFileData(out, args.root, file, args.chunk);
        out << "}\n";
    });
}

}  // namespace

void writeHeader(std::ostream& os, const GeneratorArgs& args) {
    writePreamble(os, args);
    writeDataSection(os, args);
    writeManager(os, args);
    writePostamble(os, args);

    if (!args.headerOnly) {
        fs::path headerPath(args.output);
        std::string header = headerPath.filename().string();
        fs::path projectRoot = headerPath.parent_path();

        writeImpls(header, projectRoot, args);
    }
}