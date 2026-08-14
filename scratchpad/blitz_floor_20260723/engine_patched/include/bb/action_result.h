#pragma once

namespace bb {

struct ActionResult {
    bool success = true;
    bool turnover = false;

    static ActionResult ok() { return {true, false}; }
    static ActionResult fail() { return {false, false}; }
    static ActionResult turnovr() { return {false, true}; }
};

} // namespace bb
