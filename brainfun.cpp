#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "bf_common.h"

static const char* VERSION = "1.0.0";

// C++11 兼容的 starts_with / ends_with
static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
static bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

class brainfun_app {
public:
    brainfun_app(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "-v" || arg == "--version") { show_version = true; return; }
            if (arg == "-h" || arg == "--help")    { show_help = true;    return; }
            if (starts_with(arg, "-mode="))        { mode = arg.substr(6); }
            else if (starts_with(arg, "-size="))   { tape_size = std::stoull(arg.substr(6)); }
            else if (arg == "-warn")               { warn = true; }
            else if (arg == "-o")                  { opt = true; }
            else                                   { filename = arg; }
        }
    }

    int run() {
        if (show_version) { std::cout << "brainfun " << VERSION << std::endl; return 0; }
        if (show_help)    { print_help(); return 0; }
        if (filename.empty()) { print_help(); return 1; }
        if (!validate())  { return 1; }

        std::ifstream in(filename);
        std::string source((std::istreambuf_iterator<char>(in)), {});
        if (mode == "pre")  { interpreter_pre  vm(source, tape_size, warn, opt); vm.interpret(); }
        else                { interpreter_lazy vm(source, tape_size, warn, opt); vm.interpret(); }
        return 0;
    }

private:
    bool validate() {
        if (mode != "pre" && mode != "lazy") {
            std::cerr << "Error: mode must be 'pre' or 'lazy'" << std::endl; return false;
        }
        if (tape_size == 0) {
            std::cerr << "Error: tape size must be > 0" << std::endl; return false;
        }
        if (!ends_with(filename, ".bf") && !ends_with(filename, ".brainfuck")) {
            std::cerr << "Error: file extension must be .bf or .brainfuck" << std::endl; return false;
        }
        std::ifstream test(filename);
        if (!test) {
            std::cerr << "Error: cannot open file \"" << filename << "\"" << std::endl; return false;
        }
        return true;
    }

    static void print_help() {
        std::cout << "brainfun " << VERSION << " -- a Brainfuck interpreter\n\n"
                  << "Usage: brainfun [options] <file.bf|file.brainfuck>\n\n"
                  << "Options:\n"
                  << "  -v, --version  show version\n"
                  << "  -h, --help     show this help\n"
                  << "  -mode=pre      preprocess bracket table (O(n) build, O(1) runtime)\n"
                  << "  -mode=lazy     lazy bracket cache (default, O(1) hot path)\n"
                  << "  -size=N        tape size (default " << DEFAULT_TAPE_SIZE << ")\n"
                  << "  -warn          warn if operators appear near non-operator text\n"
                  << "  -o             enable optimizations ([-] to zero, etc.)\n";
    }

    std::string filename;
    std::string mode = "lazy";
    u64 tape_size = DEFAULT_TAPE_SIZE;
    bool warn = false;
    bool opt = false;
    bool show_version = false;
    bool show_help = false;
};

int main(int argc, char* argv[]) {
    return brainfun_app(argc, argv).run();
}
