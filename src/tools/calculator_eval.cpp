#include "tools/calculator.h"

using Kind = CalcToken::Kind;

static int precedence(Kind op) {
    if (op == Kind::Times || op == Kind::Divide) return 2;
    if (op == Kind::Plus || op == Kind::Minus) return 1;
    return 0;
}

static bool fail_calc(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

static bool apply(std::vector<double>& values, std::vector<Kind>& ops,
                  std::string* error) {
    if (ops.empty() || values.size() < 2) {
        return fail_calc(error, "missing operand or operator");
    }
    const Kind op = ops.back();
    ops.pop_back();
    const double right = values.back();
    values.pop_back();
    const double left = values.back();
    values.pop_back();

    double result = 0;
    switch (op) {
        case Kind::Plus: result = left + right; break;
        case Kind::Minus: result = left - right; break;
        case Kind::Times: result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) return fail_calc(error, "division by zero");
            result = left / right;
            break;
        default: return fail_calc(error, "invalid operator");
    }
    values.push_back(result);
    return true;
}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    if (error != nullptr) error->clear();
    if (out == nullptr) return fail_calc(error, "missing output value");
    std::vector<double> values;
    std::vector<Kind> ops;
    bool need_value = true;

    for (const auto& token : tokens) {
        if (token.kind == Kind::Number) {
            if (!need_value) return fail_calc(error, "missing operator");
            values.push_back(token.value);
            need_value = false;
        } else if (token.kind == Kind::LParen) {
            if (!need_value) return fail_calc(error, "missing operator before '('");
            ops.push_back(token.kind);
        } else if (token.kind == Kind::RParen) {
            if (need_value) return fail_calc(error, "missing value before ')'");
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply(values, ops, error)) return false;
            }
            if (ops.empty()) return fail_calc(error, "unmatched ')'");
            ops.pop_back();
        } else {
            if (precedence(token.kind) == 0) return fail_calc(error, "invalid token");
            if (need_value) return fail_calc(error, "missing value before operator");
            // Equal precedence also runs now: subtraction and division go left to right.
            while (!ops.empty() && ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply(values, ops, error)) return false;
            }
            ops.push_back(token.kind);
            need_value = true;
        }
    }
    if (need_value) return fail_calc(error, "missing value");
    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) return fail_calc(error, "unmatched '('");
        if (!apply(values, ops, error)) return false;
    }
    if (values.size() != 1) return fail_calc(error, "expected one result");
    *out = values.back();
    return true;
}
