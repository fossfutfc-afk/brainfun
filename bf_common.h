#ifndef BF_COMMON_H
#define BF_COMMON_H

#include <unordered_map>
#include <stack>
#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <iostream>

using u64 = unsigned long long;
static constexpr u64 DEFAULT_TAPE_SIZE = 30000;
static constexpr u64  IO_BUF_SIZE     = 4096;

// 枚举 0-7 替代 char dispatch, 保证跳转表而非二分决策树
// OP_NONE=0 保证未初始化的查表项默认为 NOP (static 零初始化)
enum bf_op : unsigned char {
    OP_NONE = 0, OP_RIGHT, OP_LEFT, OP_ADD, OP_MINUS,
    OP_IN, OP_OUT, OP_LOOP, OP_POP
};

// opt 模式识别: [-] / [->+<] / [-<+>] 等
enum OptPattern : int {
    PAT_CLEAR      = 0,  // [-]
    PAT_MOVE_RIGHT = 1,  // [->+<]
    PAT_MOVE_LEFT  = 2,  // [-<+>]
};

// char → 0-7 枚举映射, 编译器生成位测试/跳转表
inline bf_op char_to_op(unsigned char c) {
    switch (c) {
        case '>': return OP_RIGHT;
        case '<': return OP_LEFT;
        case '+': return OP_ADD;
        case '-': return OP_MINUS;
        case ',': return OP_IN;
        case '.': return OP_OUT;
        case '[': return OP_LOOP;
        case ']': return OP_POP;
        default:  return OP_NONE;
    }
}

// ============================================================
//  basic_interpreter<Lazy>
//    Lazy=false → Pre  构造时全量扫描跳转表
//    Lazy=true  → Lazy 运行时惰性缓存
// ============================================================
template<bool Lazy>
class basic_interpreter {
public:
    basic_interpreter(const std::string_view& bf, u64 tape = DEFAULT_TAPE_SIZE,
                      bool warn = false, bool opt = false);
    ~basic_interpreter();
    void interpret();

private:
    // --- 初始化 ---
    void build_jump_table();
    void run_warnings();
    void run_optimizations();

    // --- 执行 ---
    void proceed();

    // --- 8 个原语 ---
    void right();
    void left();
    void add();
    void minus();
    void in();
    void out();
    void loop();
    void pop();

    // --- 辅助 ---
    void flush_out();
    u64 scan_forward(u64 start);

    // --- 数据 ---
    u64 ptr {0};
    std::vector<unsigned char> table;
    const std::string_view& file;
    u64 current {0};
    u64 length;
    bool opt_on;

    std::unordered_map<u64, u64> jump;              // Pre+Lazy: 括号跳转表
    std::stack<u64> loop_stack;                    // Lazy: 运行时 [ 追踪栈
    std::unordered_map<u64, OptPattern> patterns;    // opt: [ 位置 → 模式
    std::unordered_map<u64, int> run_length;        // opt: 连续 +/-/</> 压缩计数

    // --- 缓冲 I/O ---
    unsigned char out_buf[IO_BUF_SIZE];
    u64 out_pos {0};
};

// ============================================================
//  对外类型
// ============================================================
using interpreter_pre  = basic_interpreter<false>;
using interpreter_lazy = basic_interpreter<true>;

// ============================================================
//  实现
// ============================================================

// --- 构造 / 析构 ---

template<bool Lazy>
basic_interpreter<Lazy>::basic_interpreter(
    const std::string_view& bf, u64 tape, bool warn, bool opt)
    : ptr(0), table(tape, 0), file(bf), current(0),
      length(bf.size()), opt_on(opt)
{
    if constexpr (!Lazy) build_jump_table();   // Pre: 构造时 O(n) 全扫建跳表
    if (warn) run_warnings();                  // 通用: 注释操作符泄漏检测
    if (opt)  run_optimizations();             // opt: [-] + 游程压缩预扫
}

template<bool Lazy>
basic_interpreter<Lazy>::~basic_interpreter() {
    flush_out();
}

template<bool Lazy>
void basic_interpreter<Lazy>::flush_out() {
    if (out_pos) {
        fwrite(out_buf, 1, out_pos, stdout);
        out_pos = 0;
    }
}

// --- 初始化 ---

template<bool Lazy>
void basic_interpreter<Lazy>::build_jump_table() {
    std::stack<u64> stk;
    for (u64 i = 0; i < length; ++i) {
        if (file[i] == '[')
            stk.push(i);
        if (file[i] == ']') {
            if (stk.empty()) {
                std::cerr << "Syntax: Unmatched ']' at index[" << i << "]." << std::endl;
                std::terminate();
            }
            u64 start = stk.top(); stk.pop();
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

template<bool Lazy>
void basic_interpreter<Lazy>::run_warnings() {
    for (u64 i = 0; i < length; ++i) {
        char c = file[i];
        if (c == '>' || c == '<' || c == '+' || c == '-' ||
            c == '.' || c == ',' || c == '[' || c == ']') {
            char left  = (i > 0)        ? file[i-1] : ' ';
            char right = (i < length-1) ? file[i+1] : ' ';
            bool left_op  = (left  == '>' || left  == '<' || left  == '+' || left  == '-' ||
                             left  == '.' || left  == ',' || left  == '[' || left  == ']' ||
                             left  == ' ' || left  == '\n' || left == '\t');
            bool right_op = (right == '>' || right == '<' || right == '+' || right == '-' ||
                             right == '.' || right == ',' || right == '[' || right == ']' ||
                             right == ' ' || right == '\n' || right == '\t');
            if (!left_op || !right_op) {
                std::cerr << "Warn: operator '" << c << "' at index " << i
                          << " near non-operator text, possible comment leak."
                          << std::endl;
            }
        }
    }
}

template<bool Lazy>
void basic_interpreter<Lazy>::run_optimizations() {
    for (u64 i = 0; i < length; ) {
        char c = file[i];

        if (c == '[' && i + 2 < length) {
            OptPattern pat;
            u64 end = 0;

            // [-] → set cell=0
            if (file[i+1] == '-' && file[i+2] == ']') {
                pat = PAT_CLEAR; end = i + 2;
            }
            // [->+<] → move right
            else if (i + 5 < length &&
                     file[i+1] == '-' && file[i+2] == '>' &&
                     file[i+3] == '+' && file[i+4] == '<' && file[i+5] == ']') {
                pat = PAT_MOVE_RIGHT; end = i + 5;
            }
            // [-<+>] → move left
            else if (i + 5 < length &&
                     file[i+1] == '-' && file[i+2] == '<' &&
                     file[i+3] == '+' && file[i+4] == '>' && file[i+5] == ']') {
                pat = PAT_MOVE_LEFT; end = i + 5;
            }
            else { ++i; continue; }

            patterns[i] = pat;
            if constexpr (Lazy) {
                jump[i] = end;
                jump[end] = i;
            }
            i = end + 1;
            continue;
        }

        // 连续相同操作符压缩: > < + -
        if (c == '>' || c == '<' || c == '+' || c == '-') {
            u64 j = i + 1;
            while (j < length && file[j] == c) ++j;
            int count = static_cast<int>(j - i);
            if (count > 1) {
                run_length[i] = count;
            }
            i = j;
            continue;
        }

        ++i;
    }
}

// --- 顶层 ---

template<bool Lazy>
void basic_interpreter<Lazy>::interpret() {
    while (current < length) proceed();
    flush_out();
    if constexpr (Lazy) {                      // Lazy: 结束时检查未闭合 [
        if (!loop_stack.empty()) {
            std::cerr << "Syntax: Unmatched '[' at program end (never closed)." << std::endl;
            std::terminate();
        }
    }
}

// --- 指令分发 ---

template<bool Lazy>
void basic_interpreter<Lazy>::proceed() {
    unsigned char c = static_cast<unsigned char>(file[current]);
    bf_op op = char_to_op(c);

    // opt: 游程压缩 — 连续 n 个 +/-/</> 一次批量执行
    if (opt_on && op <= OP_MINUS) {             // + - > < 四个可压缩操作符
        auto it = run_length.find(current);
        if (it != run_length.end()) {
            int n = it->second;
            switch (op) {
                case OP_ADD:   table[ptr] += static_cast<unsigned char>(n); break;
                case OP_MINUS: table[ptr] -= static_cast<unsigned char>(n); break;
                case OP_RIGHT: ptr = (ptr + static_cast<u64>(n)) % table.size(); break;
                case OP_LEFT:  ptr = (ptr + table.size() - static_cast<u64>(n) % table.size()) % table.size(); break;
                default: break;
            }
            current += n;
            return;
        }
    }

    switch (op) {
        case OP_RIGHT: right(); break;
        case OP_LEFT:  left();  break;
        case OP_ADD:   add();   break;
        case OP_MINUS: minus(); break;
        case OP_IN:    in();    break;
        case OP_OUT:   out();   break;
        case OP_LOOP:  loop();  break;
        case OP_POP:   pop();   break;
        default: break;
    }
    ++current;
}

// --- 8 原语 ---

template<bool Lazy>
void basic_interpreter<Lazy>::right() {
    if (++ptr >= table.size()) ptr = 0;
}

template<bool Lazy>
void basic_interpreter<Lazy>::left() {
    if (ptr == 0) ptr = table.size() - 1; else --ptr;
}

template<bool Lazy>
void basic_interpreter<Lazy>::add() { ++table[ptr]; }

template<bool Lazy>
void basic_interpreter<Lazy>::minus() { --table[ptr]; }

template<bool Lazy>
void basic_interpreter<Lazy>::in() {
    flush_out();
    int ch = getchar();
    table[ptr] = (ch == EOF) ? 0 : static_cast<unsigned char>(ch);
}

template<bool Lazy>
void basic_interpreter<Lazy>::out() {
    out_buf[out_pos++] = table[ptr];
    if (out_pos >= IO_BUF_SIZE) flush_out();
}

// --- 括号处理 ---

template<bool Lazy>
u64 basic_interpreter<Lazy>::scan_forward(u64 start) {
    u64 pos = start;
    int depth = 1;
    while (depth) {
        ++pos;
        if (pos >= length) {
            std::cerr << "Syntax: Unmatched '[' at index[" << start << "]." << std::endl;
            std::terminate();
        }
        if (file[pos] == '[') ++depth;
        if (file[pos] == ']') --depth;
    }
    return pos;
}

template<bool Lazy>
void basic_interpreter<Lazy>::loop() {
    // opt: 模式命中 → O(1) 批量操作并跳到 ] 之后
    if (opt_on) {
        auto pit = patterns.find(current);
        if (pit != patterns.end()) {
            u64 p1 = (ptr + 1) % table.size();
            u64 p2 = (ptr > 0) ? ptr - 1 : table.size() - 1;
            switch (pit->second) {
                case PAT_CLEAR:
                    table[ptr] = 0; break;
                case PAT_MOVE_RIGHT:
                    table[p1] += table[ptr]; table[ptr] = 0; break;
                case PAT_MOVE_LEFT:
                    table[p2] += table[ptr]; table[ptr] = 0; break;
            }
            current = jump[current];
            return;
        }
    }

    if constexpr (Lazy) {
        // Lazy: 缓存命中 O(1) / 未命中前扫 + 入栈
        if (!table[ptr]) {
            if (auto it = jump.find(current); it != jump.end()) {
                current = it->second;
            } else {
                u64 pos = scan_forward(current);
                jump[current] = pos;
                jump[pos]     = current;
                current = pos;
            }
        } else {
            loop_stack.push(current);
        }
    } else {
        // Pre: 跳转表已就绪, O(1) 直跳
        if (!table[ptr]) current = jump[current];
    }
}

template<bool Lazy>
void basic_interpreter<Lazy>::pop() {
    if constexpr (Lazy) {
        // Lazy: 窥视栈顶/缓存跳回, cell==0 时弹出
        if (table[ptr]) {
            if (auto it = jump.find(current); it != jump.end()) {
                current = it->second;
            } else {
                if (loop_stack.empty()) {
                    std::cerr << "Syntax: Unmatched ']' at index[" << current << "]." << std::endl;
                    std::terminate();
                }
                u64 open = loop_stack.top();
                jump[open]    = current;
                jump[current] = open;
                current = open;
            }
        } else {
            if (!loop_stack.empty()) {
                u64 open = jump.count(current) ? jump[current] : loop_stack.top();
                if (loop_stack.top() == open) loop_stack.pop();
            } else if (!jump.count(current)) {
                std::cerr << "Syntax: Unmatched ']' at index[" << current << "]." << std::endl;
                std::terminate();
            }
        }
    } else {
        // Pre: 跳转表已就绪, O(1) 直跳
        if (table[ptr]) current = jump[current];
    }
}

#endif
