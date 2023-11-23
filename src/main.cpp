#include "args.h"
#include "writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, const char** argv) {
    std::cout << std::endl;
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
    writeHeader(output, args);
    return 0;
}