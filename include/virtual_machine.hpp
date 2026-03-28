#pragma once
#include "sabaka_types.hpp"
#include <functional>
#include <stack>
#include <unordered_map>
#include <vector>
#include <string>
#include <iosfwd>

struct FunctionInfo {
    int32_t              address { 0 };
    std::vector<std::string> params;
};

using ExternalFunc = std::function<Value(std::vector<Value>)>;

class VirtualMachine {
public:
    explicit VirtualMachine(
        std::istream*  input    = nullptr,
        std::ostream*  output   = nullptr,
        std::unordered_map<std::string, ExternalFunc> externals = {}
    );

    void execute(std::vector<Instruction>& instructions);

    void callFunction(const std::string& name, std::vector<Value> args);

private:
    std::vector<Value>  _stack;

    std::vector<std::unordered_map<std::string, Value>> _scopes;

    std::vector<int32_t> _callStack;
    std::vector<int32_t> _scopeDepthStack;
    std::vector<int32_t> _stackDepthStack;
    std::vector<Value>   _thisStack;
    std::vector<bool>    _methodCallStack;

    std::unordered_map<std::string, FunctionInfo> _functions;
    std::unordered_map<std::string, std::string>  _inheritance;

    std::istream* _input;
    std::ostream* _output;

    std::unordered_map<std::string, ExternalFunc> _externals;

    std::vector<Instruction>* _instructions { nullptr };

    void runLoop(std::vector<Instruction>& instructions, int32_t& ip);
    void executeFrom(int32_t startIp);

    void  enterScope();
    void  exitScope();
    Value resolve(const std::string& name);
    void  assign(const std::string& name, Value val);

    void binaryNumeric(double(*op)(double, double));
    void compareNumeric(bool(*op)(double, double));

    inline Value pop() {
        if (_stack.empty()) throw std::runtime_error("Stack underflow");
        Value v = std::move(_stack.back()); _stack.pop_back(); return v;
    }
    inline void push(Value v) { _stack.push_back(std::move(v)); }
    inline Value& top() {
        if (_stack.empty()) throw std::runtime_error("Stack underflow");
        return _stack.back();
    }

    const FunctionInfo& resolveMethod(const std::string& cls, const std::string& method);

    int32_t             unwrapInt(const Value& v) const noexcept;
    std::string         unwrapString(const Value& v) const;
    std::vector<std::string> unwrapStringList(const Value& v) const;

    bool isStringAtTop() const noexcept;

    void prescan(std::vector<Instruction>& instructions);
};
