#include "args.h"

#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <regex>

namespace {
using ParseFn = std::optional<Error> (*)(const char*, GeneratorArgs&);

std::optional<Error> parseRoot(const char* arg, GeneratorArgs& args) {
    if (!arg) {
        return Error{.msg = "Expected argument to root option."};
    }
    args.root = arg;
    return std::nullopt;
}

std::optional<Error> parseOutput(const char* arg, GeneratorArgs& args) {
    if (!arg) {
        return Error{.msg = "Expected argument to output option."};
    }
    args.output = arg;
    return std::nullopt;
}

std::optional<Error> parseNSpace(const char* arg, GeneratorArgs& args) {
    if (!arg) {
        return Error{.msg = "Expected argument to namespace option."};
    }
    args.nspace = arg;
    return std::nullopt;
}

const std::regex numberRegex(R"(\s*(0x)?\d+\s*)",
                             std::regex_constants::ECMAScript | std::regex_constants::nosubs |
                                 std::regex_constants::optimize);

std::optional<Error> parseChunkSize(const char* arg, GeneratorArgs& args) {
    if (!arg) {
        return Error{.msg = "Expected argument to chunk size option."};
    }
    if (!std::regex_match(arg, numberRegex)) {
        return Error{.msg = std::format("Invalid chunk size '{}'", arg)};
    }
    args.chunk = static_cast<size_t>(std::atoll(arg));
    return std::nullopt;
}

const std::regex trueRegex(R"(\s*(true|yes|1)\s*)",
                           std::regex_constants::ECMAScript | std::regex_constants::nosubs |
                               std::regex_constants::optimize | std::regex_constants::icase);
const std::regex falseRegex(R"(\s*(false|no|0)\s*)",
                            std::regex_constants::ECMAScript | std::regex_constants::nosubs |
                                std::regex_constants::optimize | std::regex_constants::icase);

std::optional<Error> parseUsePragma(const char* arg, GeneratorArgs& args) {
    if (arg) {
        if (std::regex_match(arg, trueRegex)) {
            args.usePragma = true;
        } else if (std::regex_match(arg, falseRegex)) {
            args.usePragma = false;
        }
    } else {
        args.usePragma = true;
    }
    return std::nullopt;
}

std::optional<Error> parsePositional(const char* arg, GeneratorArgs& args) {
    args.sources.emplace_back(arg);
    return std::nullopt;
}

struct StateTransition {
    ParseFn parse;
};

const std::map<std::string, ParseFn> optParsers{{"-r", parseRoot},
                                                {"--root", parseRoot},
                                                {"-o", parseOutput},
                                                {"--output", parseOutput},
                                                {"-n", parseNSpace},
                                                {"--namespace", parseNSpace},
                                                {"--pragma-once", parseUsePragma}};

std::string_view trimExeName(std::string_view exeName) {
    auto sepOffset = exeName.find_last_of("\\/");
    auto extOffset = exeName.find_last_of('.', sepOffset);
    auto b = sepOffset == std::string::npos ? exeName.begin() : std::next(exeName.begin(), sepOffset + 1);
    auto e = extOffset == std::string::npos ? exeName.end() : std::next(exeName.begin(), extOffset);
    return {b, e};
}
}  // namespace

void printUsage(std::ostream& os, std::string_view exeName) {
    os << "Usage: " << trimExeName(exeName) << " [--help]"
       << " [--pragma-once]"
       << " [--namespace <NAMESPACE>]"
       << " --root <ROOT>"
       << " --output <OUTPUT>"
       << " <SOURCES>"
       << "\n\n";

    os << "Generate C++ header that embeds the contents of files\n\n";

    os << "Optional arguments:\n"
       << "  -h, --help       shows help message and exits\n"
       << "  --pragma-once    use #pragma once instead of traditional header guard\n"
       << "  -n, --namespace  use the specified namespace for the generated API\n"
       << "\n";

    os << "Required arguments:\n"
       << "  -r, --root       the root path shared by all the resource files\n"
       << "  -o, --output     the output filename\n"
       << "\n";

    os << "Positional arguments:\n"
       << "  SOURCES          relative paths from root to each resource file to embed\n"
       << "\n";
}

ErrorOr<GeneratorArgs> parse(int argc, const char** argv) {
    GeneratorArgs args;

    using namespace std::string_literals;

    if (std::any_of(argv, argv + argc, [](const char* arg) { return arg == "-h"s || arg == "-help"s; })) {
        printUsage(std::cout, argv[0]);
        return Error{};
    }

    ParseFn next = nullptr;
    for (int i = 1; i <= argc; ++i) {
        if (next) {
            if (auto result = next(argv[i], args)) {
                return std::move(*result);
            }
            next = nullptr;
        } else if (argv[i][0] == '-') {
            if (next) {
                next(nullptr, args);
            }
            auto it = optParsers.find(argv[i]);
            if (it == optParsers.end()) {
                return Error{.msg = std::format("Invalid argument: {}", argv[i])};
            }
            next = it->second;
        } else {
            if (auto result = parsePositional(argv[i], args)) {
                return std::move(*result);
            }
        }
    }

    if (next) {
        next(nullptr, args);
    }

    return std::move(args);
}
