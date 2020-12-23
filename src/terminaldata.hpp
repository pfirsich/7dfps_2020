#pragma once

#include <deque>
#include <string>

// For client
struct TerminalData {
    std::deque<std::string> history;
    std::string input;
    std::string output;
    float scroll = 0.0f;
    float lastMaxScroll = 0.0f;
    bool inputEnabled = false;
};
