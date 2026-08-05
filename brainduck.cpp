#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include "bf_common.h"

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
static bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

class brainduck {
public:
    brainduck() {
        // === 操作类: 不依赖输入，原地操作 ===
        macros["clear"]    = "[-]";                              // 清零当前单元
        macros["copy"]     = "[->+>+<<]>>[-<<+>>]<<";            // 复制当前单元到右边 (不破坏源)
        macros["move"]     = ">[-]<[->+<]";                      // 移动: 覆盖右边, 源清零
        macros["add_to"]   = "[->+<]";                           // 加到右边, 源清零

        // === 输出类: 无输入，直接输出 ===
        macros["hello"]     = "++++++++++[>+++++++>++++++++++>+++>+<<<<-]"
                              ">++.>+.+++++++..+++.>++.<<+++++++++++++++."
                              ">.+++.------.--------.>+.>.";

        // === I/O 类: 完整的交互程序 ===
        macros["to_upper"] = ",----------[----------------------.,----------]";
        macros["add"]      = ",>++++++[<-------->-],,[<+>-],<.>.";
        macros["mul"]      = ",>,,>++++++++[<------<------>>-]"
                             "<<[>[>+>+<<-]>>[<<+>>-]<<<-]"
                             ">>>++++++[<++++++++>-],<.>.";
        macros["div"]      = ",>,>++++++[-<--------<-------->>]"
                             "<["
                             "[->+>+<<]"
                             "[-<<-"
                             "[>]>>>[<[>>>-<<<[-]]>>]<<]"
                             "<[-<<+>>]"
                             "<<]"
                             "[-]>>>>[-<<<<<+>>>>>]"
                             "<<<++++++[-<++++++++>]<.";
    }

    // 去掉 // 到行尾的注释，替换为等量空格以保持非命令字符语义
    static std::string strip_comments(const std::string_view& source) {
        std::string result;
        result.reserve(source.size());
        for (size_t i = 0; i < source.size(); ++i) {
            if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
                while (i < source.size() && source[i] != '\n') {
                    result += ' ';  // 用空格填充，不影响BF的非命令字符行为
                    ++i;
                }
                if (i < source.size()) result += '\n';
            } else {
                result += source[i];
            }
        }
        return result;
    }

    std::string expand(const std::string_view& source) {
        std::string cleaned = strip_comments(source);
        std::string result;
        result.reserve(cleaned.size() * 2);

        for (size_t i = 0; i < cleaned.size(); ++i) {
            if (cleaned[i] == '@') {
                // 读取宏名称: @后直到非字母数字的字符
                size_t start = i + 1;
                size_t end = start;
                while (end < cleaned.size() &&
                       (std::isalnum(static_cast<unsigned char>(cleaned[end])) || cleaned[end] == '_')) {
                    ++end;
                }
                std::string name(cleaned.substr(start, end - start));
                auto it = macros.find(name);
                if (it != macros.end()) {
                    result += it->second;
                } else {
                    std::cerr << "Brainduck: unknown macro '@" << name << "'" << std::endl;
                    std::terminate();
                }
                i = end - 1;  // -1 因为循环末尾会 ++i
            } else {
                result += cleaned[i];
            }
        }
        return result;
    }

private:
    std::unordered_map<std::string, std::string> macros;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: brainduck [-mode=pre|lazy] [-size=N] [-e] <file.bd>" << std::endl
                  << "  -mode=pre|lazy  bracket strategy (default: lazy)" << std::endl
                  << "  -e              expand macros only, output BF to stdout" << std::endl
                  << "  -size=N         tape size (default " << DEFAULT_TAPE_SIZE << ")" << std::endl;
        std::terminate();
    }

    std::string filename;
    bool expand_only = false;
    std::string mode = "lazy";
    u64 tape_size = DEFAULT_TAPE_SIZE;
    bool warn = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-e") {
            expand_only = true;
        } else if (starts_with(arg, "-mode=")) {
            mode = arg.substr(6);
            if (mode != "pre" && mode != "lazy") {
                std::cerr << "Brainduck: mode must be 'pre' or 'lazy'" << std::endl;
                std::terminate();
            }
        } else if (starts_with(arg, "-size=")) {
            tape_size = std::stoull(arg.substr(6));
            if (tape_size == 0) {
                std::cerr << "Brainduck: tape size must be > 0" << std::endl;
                std::terminate();
            }
        } else if (arg == "-warn") {
            warn = true;
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "Brainduck: no input file specified" << std::endl;
        std::terminate();
    }

    if (!ends_with(filename, ".bd")) {
        std::cerr << "Brainduck: file extension must be .bd" << std::endl;
        std::terminate();
    }

    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Brainduck: cannot open file \"" << filename << "\"" << std::endl;
        std::terminate();
    }

    std::string source((std::istreambuf_iterator<char>(infile)),
                        std::istreambuf_iterator<char>());

    brainduck bd;
    std::string bf_code = bd.expand(source);

    if (expand_only) {
        std::cout << bf_code;
    } else if (mode == "pre") {
        interpreter_pre vm(bf_code, tape_size, warn);
        vm.interpret();
    } else {
        interpreter_lazy vm(bf_code, tape_size, warn);
        vm.interpret();
    }
}
