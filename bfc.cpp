#include <string>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string src;
    if (argc > 1) {
        std::ifstream in(argv[1]);
        if (!in) {
            std::cerr << "bfc: cannot open '" << argv[1] << "'" << std::endl;
            return 1;
        }
        src.assign((std::istreambuf_iterator<char>(in)), {});
    } else {
        src.assign((std::istreambuf_iterator<char>(std::cin)), {});
    }
    for (char c : src)
        if (c == '>' || c == '<' || c == '+' || c == '-' ||
            c == '.' || c == ',' || c == '[' || c == ']')
            std::cout << c;
}
