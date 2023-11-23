#include "args.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs = std::filesystem;
const char IncludeGuardName[] = "GENERATED_BIN_EMBED_RESOURCES_H_";
void WritePreamble(std::ostream& os, bool usePragma) {
    if (usePragma) {
        os << "#pragma once\n";
    } else {
        os << "#ifndef " << IncludeGuardName << '\n' << "#define " << IncludeGuardName << '\n';
    }
    os << '\n'
       << "#include <cstdint>\n"
       << "#include <map>\n"
       << "#include <span>\n"
       << "#include <string>\n"
       << "#include <string_view>\n"
       << '\n';
}

void WritePostamble(std::ostream& os, bool usePragma) {
    if (usePragma) {
        return;
    }
    os << "#endif // " << IncludeGuardName << '\n';
}

void WriteHexEscaped(std::ostream& os, const char* data, std::streamsize count) {
    auto digits = "0123456789abcdef";
    for (auto last = data + count; data != last; ++data) {
        os << "\\x" << digits[(*data >> 4) & 0x0f] << digits[*data & 0x0f];
    }
}

void WriteEmbed(std::ostream& os, const std::string& root, const std::string& path, std::size_t chunkSize) {
    fs::path source(root);
    source /= path;
    auto size = fs::file_size(source);
    std::ifstream fd(source.c_str(), std::ios::binary);
    std::vector<char> buf(chunkSize);

    os << "    {\n"
       << "      auto &data = _resources[\"" << path << "\"];\n"
       << "      data.clear();\n";
    if (size > chunkSize) {
        os << "      data.reserve(" << size << ");\n";
    }

    while (fd) {
        fd.read(buf.data(), buf.size());
        auto count = fd.gcount();
        if (!count) {
            break;
        }

        os << "      data.append(\"";
        WriteHexEscaped(os, buf.data(), count);
        os << "\", " << count << ");\n";
    }

    os << "    }\n";
}

void Write(std::ostream& os, const GeneratorArgs& options) {
    WritePreamble(os, options.usePragma);

    if (!options.nspace.empty()) {
        os << "namespace " << options.nspace << " {\n";
    }

    os << "class ResourceManager {\n"
       << " public:\n"
       << "  static ResourceManager &Instance() {\n"
       << "    static ResourceManager manager;\n"
       << "    return manager;\n"
       << "  }\n"
       << "\n"
       << "  static std::string_view Get(std::string_view path) {\n"
       << "    auto it = Instance()._resources.find(path);\n"
       << "    return it != Instance()._resources.end() ? it->second : std::string_view{};\n"
       << "  }\n"
       << "\n"
       << "  static std::span<const uint8_t> GetBytes(std::string_view path) {\n"
       << "    auto sv = Get(path);\n"
       << "    return {reinterpret_cast<const uint8_t*>(sv.data()), sv.size()};\n"
       << "  }\n"
       << "\n"
       << " private:\n"
       << "  ResourceManager() {\n";

    for (const auto& source : options.sources) {
        WriteEmbed(os, options.root, source, options.chunk);
    }

    os << "  }\n"
       << "\n"
       << "  std::map<std::string, std::string, std::less<>> _resources;\n"
       << "};\n";

    if (!options.nspace.empty()) {
        os << "} // namespace " << options.nspace << '\n';
    }

    WritePostamble(os, options.usePragma);
}
}  // namespace

int main(int argc, const char** argv) {
    auto result = parse(argc, argv);
    if (is_error(result)) {
        const auto& msg = std::get<Error>(result).msg;
        if (!msg.empty()) {
            std::cerr << "Error parsing arguments: " << std::get<Error>(result).msg << std::endl;
            return -1;
        }
        return 0;
    }

    auto args = std::move(std::get<GeneratorArgs>(result));
    std::ofstream output(args.output);
    Write(output, args);
    return 0;
}