#ifndef BF_COMMON_H
#define BF_COMMON_H

#include <unordered_map>
#include <stack>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

using u64 = unsigned long long;

static constexpr u64 DEFAULT_TAPE_SIZE = 30000;

// ============================================================
//  bracket 处理策略:
//    Pre  — 构造时 O(n) 全量扫描，运行时 O(1)
//    Lazy — 运行时惰性缓存，仅执行到的括号对产生开销
// ============================================================
template<bool Lazy>
class basic_interpreter {
public:
    basic_interpreter(const std::string_view& bffile, u64 tape_size = DEFAULT_TAPE_SIZE)
        : ptr(0), table(tape_size, 0), file(bffile), current(0),
          length(bffile.size())
    {
        if constexpr (!Lazy) {
            // Pre: 全量扫描构建跳转表
            std::stack<u64> stk;
            for (u64 i = 0; i < length; ++i) {
                if (file[i] == '[')
                    stk.push(i);
                if (file[i] == ']') {
                    if (stk.empty()) {
                        std::cerr << "Syntax: Unmatched ']' at index[" << i << "]." << std::endl;
                        std::terminate();
                    }
                    u64 start = stk.top();
                    stk.pop();
                    jump[start] = i;
                    jump[i] = start;
                }
            }
            if (!stk.empty()) {
                std::cerr << "Syntax: Unmatched '[' at ";
                while (!stk.empty()) {
                    std::cerr << "index[" << stk.top() << "]";
                    stk.pop();
                    if (!stk.empty()) std::cerr << ", ";
                }
                std::cerr << "." << std::endl;
                std::terminate();
            }
        }
    }

    void interpret() {
        while (current < length) {
            proceed();
        }
        if constexpr (Lazy) {
            if (!loop_stack.empty()) {
                std::cerr << "Syntax: Unmatched '[' at program end (never closed)." << std::endl;
                std::terminate();
            }
        }
    }

private:
    void proceed() {
        switch (file[current]) {
            case '>': right();  break;
            case '<': left();   break;
            case '+': add();    break;
            case '-': minus();  break;
            case ',': in();     break;
            case '.': out();    break;
            case '[': loop();   break;
            case ']': pop();    break;
        }
        ++current;
    }

    void right() { if (++ptr >= table.size()) ptr = 0; }
    void left()  { if (ptr == 0) ptr = table.size() - 1; else --ptr; }
    void add()   { ++table[ptr]; }
    void minus() { --table[ptr]; }
    void in()    { table[ptr] = getchar(); }
    void out()   { putchar(table[ptr]); }

    // ============================================================
    //  loop / pop — 根据 Lazy 策略分发
    // ============================================================

    void loop() {
        if constexpr (Lazy) {
            // --- Lazy: 运行时惰性跳转表 ---
            if (!table[ptr]) {
                // 跳过循环体
                if (auto it = jump.find(current); it != jump.end()) {
                    current = it->second;
                } else {
                    // 首次: 前向扫描找匹配 ]
                    u64 pos = current;
                    int depth = 1;
                    while (depth) {
                        ++pos;
                        if (pos >= length) {
                            std::cerr << "Syntax: Unmatched '[' at index["
                                      << current << "]." << std::endl;
                            std::terminate();
                        }
                        if (file[pos] == '[') ++depth;
                        if (file[pos] == ']') --depth;
                    }
                    jump[current] = pos;
                    jump[pos]     = current;
                    current = pos;
                }
            } else {
                // 进入循环体
                loop_stack.push(current);
            }
        } else {
            // --- Pre: 跳转表已在构造函数中建好 ---
            if (!table[ptr]) {
                current = jump[current];
            }
        }
    }

    void pop() {
        if constexpr (Lazy) {
            // --- Lazy ---
            if (table[ptr]) {
                // 跳回循环开始
                if (auto it = jump.find(current); it != jump.end()) {
                    current = it->second;
                } else {
                    if (loop_stack.empty()) {
                        std::cerr << "Syntax: Unmatched ']' at index["
                                  << current << "]." << std::endl;
                        std::terminate();
                    }
                    u64 open = loop_stack.top();  // 窥视，不弹出
                    jump[open]    = current;
                    jump[current] = open;
                    current = open;
                }
            } else {
                // 退出循环: 弹出匹配的 [
                if (!loop_stack.empty()) {
                    u64 open = jump.count(current) ? jump[current] : loop_stack.top();
                    if (loop_stack.top() == open) {
                        loop_stack.pop();
                    }
                } else if (!jump.count(current)) {
                    std::cerr << "Syntax: Unmatched ']' at index["
                              << current << "]." << std::endl;
                    std::terminate();
                }
            }
        } else {
            // --- Pre ---
            if (table[ptr]) {
                current = jump[current];
            }
        }
    }

    // ============================================================
    //  数据成员
    // ============================================================
    u64 ptr {0};
    std::vector<unsigned char> table;
    const std::string_view& file;
    u64 current {0};
    u64 length;
    std::unordered_map<u64, u64> jump;

    // Lazy 专用: 运行时记录当前活跃的 [ 入口 (Pre 模式不使用)
    std::stack<u64> loop_stack;
};

// 对外暴露两个具体类型
using interpreter_pre  = basic_interpreter<false>;
using interpreter_lazy = basic_interpreter<true>;

#endif
