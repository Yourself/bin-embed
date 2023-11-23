#pragma once

#include <ostream>
#include <string>
#include <variant>
#include <vector>

struct Error {
    std::string msg;
};

template <class T>
using ErrorOr = std::variant<Error, T>;

template <class T>
constexpr bool is_error(const ErrorOr<T>& val) {
    return std::holds_alternative<Error>(val);
}

struct GeneratorArgs {
    std::string nspace;
    std::string root;
    std::string output;
    std::vector<std::string> sources;
    std::size_t chunk = 8 << 10;
    bool usePragma = false;
};

void printUsage(std::ostream & os, std::string_view exeName);
ErrorOr<GeneratorArgs> parse(int argc, const char** argv);