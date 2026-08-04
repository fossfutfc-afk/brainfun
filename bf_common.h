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

class interpreter {
public:
    interpreter(const std::string_view& bffile, u64 tape_size = DEFAULT_TAPE_SIZE)
        : ptr(0), table(tape_size, 0), file(bffile), current(0),
          length(bffile.size()), jump({}) {
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
                if (!stk.empty())
                    std::cerr << ", ";
            }
            std::cerr << "." << std::endl;
            std::terminate();
        }
    }

    void interpret() {
        while (current < length) {
            proceed();
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

    void right()  { ++ptr; }
    void left()   { --ptr; }
    void add()    { ++table[ptr]; }
    void minus()  { --table[ptr]; }
    void in()     { table[ptr] = getchar(); }
    void out()    { putchar(table[ptr]); }

    void loop() {
        if (!jump.count(current)) {
            std::cerr << "Unknown: It's a message that shouldn't be seen. The file might be broken." << std::endl;
            std::terminate();
        }
        if (!table[ptr]) {
            current = jump[current];
        }
    }

    void pop() {
        if (table[ptr]) {
            current = jump[current];
        }
    }

    u64 ptr {0};
    std::vector<unsigned char> table;
    const std::string_view& file;
    u64 current {0};
    u64 length;
    std::unordered_map<u64, u64> jump;
};

#endif
