#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "bf_common.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: brainfun [-size=N] <file.bf|file.brainfuck>" << std::endl
                  << "  -size=N  tape size (default " << DEFAULT_TAPE_SIZE << ")" << std::endl;
        std::terminate();
    }

    std::string filename;
    u64 tape_size = DEFAULT_TAPE_SIZE;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.starts_with("-size=")) {
            tape_size = std::stoull(arg.substr(6));
            if (tape_size == 0) {
                std::cerr << "Error: tape size must be > 0" << std::endl;
                std::terminate();
            }
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: no input file specified" << std::endl;
        std::terminate();
    }

    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Error: cannot open file \"" << filename << "\"" << std::endl;
        std::terminate();
    }
    if (!filename.ends_with(".bf") && !filename.ends_with(".brainfuck")) {
        std::cerr << "Error: file extension must be .bf or .brainfuck" << std::endl;
        std::terminate();
    }

    std::string source((std::istreambuf_iterator<char>(infile)),
                        std::istreambuf_iterator<char>());
    interpreter vm(source, tape_size);
    vm.interpret();
}
