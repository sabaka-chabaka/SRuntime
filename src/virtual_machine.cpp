#include "virtual_machine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif
#include <algorithm>
#include <cctype>
#include <stdexcept>

#ifdef SABAKA_HTTP
  #include <curl/curl.h>
  static size_t curlWriteCb(void* ptr, size_t sz, size_t n, std::string* out) {
      out->append(static_cast<char*>(ptr), sz * n); return sz * n;
  }
  static std::string httpGet(const std::string& url) {
      CURL* c = curl_easy_init(); std::string out;
      curl_easy_setopt(c, CURLOPT_URL, url.c_str());
      curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCb);
      curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
      curl_easy_perform(c); curl_easy_cleanup(c); return out;
  }
  static std::string httpPost(const std::string& url,
                              const std::string& body,
                              const std::string& ct) {
      CURL* c = curl_easy_init(); std::string out;
      curl_slist* hdrs = nullptr;
      hdrs = curl_slist_append(hdrs, ("Content-Type: " + ct).c_str());
      curl_easy_setopt(c, CURLOPT_URL, url.c_str());
      curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
      curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCb);
      curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
      curl_easy_perform(c);
      curl_slist_free_all(hdrs); curl_easy_cleanup(c); return out;
  }
#else
  static std::string httpGet(const std::string&) {
      throw std::runtime_error("HTTP support not compiled (define SABAKA_HTTP and link libcurl)");
  }
  static std::string httpPost(const std::string&, const std::string&, const std::string&) {
      throw std::runtime_error("HTTP support not compiled");
  }
#endif

VirtualMachine::VirtualMachine(
    std::istream*  input,
    std::ostream*  output,
    std::unordered_map<std::string, ExternalFunc> externals)
    : _input(input ? input : &std::cin)
    , _output(output ? output : &std::cout)
    , _externals(std::move(externals))
{
    _stack.reserve(256);
    _scopes.reserve(32);
}

void VirtualMachine::prescan(std::vector<Instruction>& instructions) {
    for (int32_t i = 0; i < static_cast<int32_t>(instructions.size()); ++i) {
        auto& instr = instructions[i];
        if (instr.opcode == OpCode::Function) {
            FunctionInfo fi;
            fi.address = i + 1;
            fi.params  = unwrapStringList(instr.extra);
            _functions[instr.nameStr()] = std::move(fi);
        } else if (instr.opcode == OpCode::Inherit) {
            _inheritance[instr.nameStr()] = unwrapString(instr.operand);
        }
    }
}

void VirtualMachine::execute(std::vector<Instruction>& instructions) {
    _instructions = &instructions;
    prescan(instructions);

    _scopes.push_back({});  // global scope

    if (instructions.empty() || instructions.back().opcode != OpCode::Input) {
        Instruction msg;
        msg.opcode  = OpCode::Push;
        msg.operand = Value::fromString("[VM] Program has been successfully executed, press enter for exit...");
        instructions.push_back(msg);
        Instruction pr; pr.opcode = OpCode::Print; instructions.push_back(pr);
        Instruction in; in.opcode = OpCode::Input; instructions.push_back(in);
    }

    int32_t ip = 0;
    runLoop(instructions, ip);
}

void VirtualMachine::callFunction(const std::string& name, std::vector<Value> args) {
    if (!_instructions)
        throw std::runtime_error("callFunction: execute() not called yet");
    auto it = _functions.find(name);
    if (it == _functions.end())
        throw std::runtime_error("callFunction: '" + name + "' not found");
    auto& func = it->second;

    _callStack.push_back(static_cast<int32_t>(_instructions->size())); // return = EOF
    _scopeDepthStack.push_back(static_cast<int32_t>(_scopes.size()));
    _stackDepthStack.push_back(static_cast<int32_t>(_stack.size()));
    _methodCallStack.push_back(false);

    enterScope();
    auto& scope = _scopes.back();
    for (size_t i = 0; i < func.params.size(); ++i)
        scope[func.params[i]] = (i < args.size()) ? args[i] : Value::fromString("");

    executeFrom(func.address);
}

void VirtualMachine::executeFrom(int32_t startIp) {
    int32_t ip = startIp;
    runLoop(*_instructions, ip);
}

void VirtualMachine::runLoop(std::vector<Instruction>& instructions, int32_t& ip) {
    const int32_t sz = static_cast<int32_t>(instructions.size());

    while (ip < sz) {
        const Instruction& instr = instructions[ip];

#define NEXT() { ++ip; continue; }
#define JUMP(target) { ip = (target); continue; }

        try {
        switch (instr.opcode) {

        case OpCode::Push:
            push(instr.operand);
            NEXT();

        case OpCode::Dup: {
            if (_stack.empty()) throw std::runtime_error("Stack empty in Dup");
            push(_stack.back());
            NEXT();
        }
        case OpCode::Pop: {
            pop(); NEXT();
        }

        case OpCode::Add: {
            if (_stack.size() < 2) throw std::runtime_error("Stack empty in Add");
            // String concat если хоть одна сторона — строка
            if (isStringAtTop()) {
                auto rhs = pop(); auto lhs = pop();
                push(Value::fromString(lhs.toString() + rhs.toString()));
            } else {
                binaryNumeric([](double a, double b){ return a + b; });
            }
            NEXT();
        }
        case OpCode::Sub:  binaryNumeric([](double a, double b){ return a - b; }); NEXT();
        case OpCode::Mul:  binaryNumeric([](double a, double b){ return a * b; }); NEXT();
        case OpCode::Div:  binaryNumeric([](double a, double b){ return a / b; }); NEXT();
        case OpCode::Percent: binaryNumeric([](double a, double b){ return static_cast<double>(static_cast<int64_t>(a) % static_cast<int64_t>(b)); }); NEXT();

        case OpCode::Negate: {
            auto v = pop();
            if (v.type == SabakaType::Int)   push(Value::fromInt(-v.i));
            else if (v.type == SabakaType::Float) push(Value::fromFloat(-v.f));
            else throw std::runtime_error("Negate requires numeric type");
            NEXT();
        }
        case OpCode::Not: {
            auto v = pop();
            if (v.type != SabakaType::Bool) throw std::runtime_error("! requires bool");
            push(Value::fromBool(!v.b));
            NEXT();
        }

        case OpCode::Equal: {
            auto b = pop(); auto a = pop();
            bool res;
            if (a.isNumber() && b.isNumber()) {
                res = (a.toDouble() == b.toDouble());
            } else if (a.type != b.type) {
                throw std::runtime_error("Type mismatch in ==");
            } else {
                switch (a.type) {
                    case SabakaType::Bool:   res = (a.b == b.b); break;
                    case SabakaType::String: res = (*a.str == *b.str); break;
                    case SabakaType::Array:  res = (a.array == b.array); break; // ref eq
                    default: throw std::runtime_error("Invalid type for ==");
                }
            }
            push(Value::fromBool(res)); NEXT();
        }
        case OpCode::NotEqual: {
            auto b = pop(); auto a = pop();
            bool res;
            if (a.isNumber() && b.isNumber()) {
                res = (a.toDouble() != b.toDouble());
            } else if (a.type != b.type) {
                throw std::runtime_error("Type mismatch in !=");
            } else {
                switch (a.type) {
                    case SabakaType::Bool:   res = (a.b != b.b); break;
                    case SabakaType::String: res = (*a.str != *b.str); break;
                    case SabakaType::Array:  res = (a.array != b.array); break;
                    default: throw std::runtime_error("Invalid type for !=");
                }
            }
            push(Value::fromBool(res)); NEXT();
        }
        case OpCode::Greater:      compareNumeric([](double a, double b){ return a > b;  }); NEXT();
        case OpCode::Less:         compareNumeric([](double a, double b){ return a < b;  }); NEXT();
        case OpCode::GreaterEqual: compareNumeric([](double a, double b){ return a >= b; }); NEXT();
        case OpCode::LessEqual:    compareNumeric([](double a, double b){ return a <= b; }); NEXT();

        case OpCode::Declare: {
            auto val = pop();
            auto& scope = _scopes.back();
            if (scope.count(instr.nameStr()))
                throw std::runtime_error("Variable already declared: " + instr.nameStr());
            scope[instr.nameStr()] = std::move(val);
            NEXT();
        }
        case OpCode::Store: {
            assign(instr.nameStr(), pop()); NEXT();
        }
        case OpCode::Load: {
            push(resolve(instr.nameStr())); NEXT();
        }

        case OpCode::EnterScope: enterScope(); NEXT();
        case OpCode::ExitScope:  exitScope();  NEXT();

        case OpCode::Jump:
            JUMP(unwrapInt(instr.operand));
        case OpCode::JumpIfFalse: {
            auto cond = pop();
            if (cond.type != SabakaType::Bool) throw std::runtime_error("Condition must be bool");
            if (!cond.b) JUMP(unwrapInt(instr.operand));
            NEXT();
        }
        case OpCode::JumpIfTrue: {
            auto cond = pop();
            if (cond.type != SabakaType::Bool) throw std::runtime_error("Condition must be bool");
            if (cond.b) JUMP(unwrapInt(instr.operand));
            NEXT();
        }

        case OpCode::Function:
            JUMP(unwrapInt(instr.operand));

        case OpCode::Call: {
            int32_t argCount = unwrapInt(instr.operand);
            std::vector<Value> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) args[i] = pop();

            auto fit = _functions.find(instr.nameStr());
            if (fit == _functions.end())
                throw std::runtime_error("Undefined function '" + instr.nameStr() + "'");
            auto& func = fit->second;

            _callStack.push_back(ip + 1);
            _scopeDepthStack.push_back(static_cast<int32_t>(_scopes.size()));
            _stackDepthStack.push_back(static_cast<int32_t>(_stack.size()));
            _methodCallStack.push_back(false);

            enterScope();
            auto& scope = _scopes.back();
            for (size_t i = 0; i < func.params.size(); ++i)
                scope[func.params[i]] = (i < static_cast<size_t>(argCount)) ? args[i] : Value::fromString("");

            JUMP(func.address);
        }

        case OpCode::CallMethod: {
            int32_t argCount = unwrapInt(instr.operand);
            std::vector<Value> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) args[i] = pop();

            auto obj = pop();
            if (obj.type != SabakaType::Object)
                throw std::runtime_error("Cannot call method on non-object");

            std::string startClass = *obj.className;
            // super call: Extra может содержать базовый класс
            if (instr.extra.type == SabakaType::String)
                startClass = instr.extra.asString();

            // Проверяем external метод
            std::string extKey = startClass;
            std::transform(extKey.begin(), extKey.end(), extKey.begin(), ::tolower);
            extKey += "." + instr.nameStr();
            {
                std::string methodLower = instr.nameStr();
                std::transform(methodLower.begin(), methodLower.end(), methodLower.begin(), ::tolower);
                std::string fullKey = extKey.substr(0, extKey.rfind('.') + 1) + methodLower;
                auto eit = _externals.find(fullKey);
                if (eit != _externals.end()) {
                    push(eit->second(args));
                    NEXT();
                }
            }

            auto& func = resolveMethod(startClass, instr.nameStr());

            _callStack.push_back(ip + 1);
            _scopeDepthStack.push_back(static_cast<int32_t>(_scopes.size()));
            _stackDepthStack.push_back(static_cast<int32_t>(_stack.size()));
            _thisStack.push_back(obj);
            _methodCallStack.push_back(true);

            enterScope();
            auto& scope = _scopes.back();
            for (size_t i = 0; i < func.params.size(); ++i)
                scope[func.params[i]] = (i < static_cast<size_t>(argCount)) ? args[i] : Value::null_val();

            JUMP(func.address);
        }

        case OpCode::CallExternal: {
            int32_t argCount = unwrapInt(instr.operand);
            std::vector<Value> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) args[i] = pop();

            auto eit = _externals.find(instr.nameStr());
            if (eit == _externals.end())
                throw std::runtime_error("External function '" + instr.nameStr() + "' not registered");
            push(eit->second(std::move(args)));
            NEXT();
        }

        case OpCode::Return: {
            int32_t targetDepth = _stackDepthStack.empty() ? 0 : _stackDepthStack.back();
            if (!_stackDepthStack.empty()) _stackDepthStack.pop_back();

            Value retVal = Value::fromInt(0);
            if (static_cast<int32_t>(_stack.size()) > targetDepth)
                retVal = pop();
            while (static_cast<int32_t>(_stack.size()) > targetDepth && !_stack.empty())
                _stack.pop_back();

            bool isMethod = !_methodCallStack.empty() && _methodCallStack.back();
            if (!_methodCallStack.empty()) _methodCallStack.pop_back();
            if (isMethod && !_thisStack.empty()) _thisStack.pop_back();

            if (!_scopeDepthStack.empty()) {
                int32_t tgt = _scopeDepthStack.back(); _scopeDepthStack.pop_back();
                while (static_cast<int32_t>(_scopes.size()) > tgt) _scopes.pop_back();
            } else {
                if (!_scopes.empty()) _scopes.pop_back();
            }

            if (_callStack.empty()) return;
            ip = _callStack.back(); _callStack.pop_back();
            push(retVal);
            continue;
        }

        case OpCode::PushThis: {
            if (_thisStack.empty()) throw std::runtime_error("No 'this' in current context");
            push(_thisStack.back());
            NEXT();
        }

        case OpCode::Print: {
            auto v = pop();
            *_output << v.toString() << "\n";
            NEXT();
        }
        case OpCode::Input: {
            std::string line;
            std::getline(*_input, line);
            push(Value::fromString(std::move(line)));
            NEXT();
        }
        case OpCode::Sleep: {
            auto v = pop();
            double secs = v.type == SabakaType::Float ? v.f : static_cast<double>(v.i);
            auto ms = static_cast<uint64_t>(secs * 1000.0);
#ifdef _WIN32
            Sleep(static_cast<DWORD>(ms));
#else
            usleep(static_cast<useconds_t>(ms * 1000));
#endif
            NEXT();
        }

        case OpCode::ReadFile: {
            auto path = pop().toString();
            std::ifstream f(path);
            if (!f.is_open()) { push(Value::fromString("")); NEXT(); }
            std::ostringstream ss; ss << f.rdbuf();
            push(Value::fromString(ss.str()));
            NEXT();
        }
        case OpCode::WriteFile: {
            auto content = pop().toString();
            auto path    = pop().toString();
            std::ofstream f(path, std::ios::trunc);
            f << content;
            NEXT();
        }
        case OpCode::AppendFile: {
            auto content = pop().toString();
            auto path    = pop().toString();
            std::ofstream f(path, std::ios::app);
            f << content;
            NEXT();
        }
        case OpCode::FileExists: {
            auto path = pop().toString();
            std::ifstream f(path);
            push(Value::fromBool(f.good()));
            NEXT();
        }
        case OpCode::DeleteFiler: {
            auto path = pop().toString();
            std::remove(path.c_str());
            NEXT();
        }
        case OpCode::ReadLines: {
            auto path = pop().toString();
            std::ifstream f(path);
            if (!f.is_open()) throw std::runtime_error("File not found: " + path);
            auto arr = std::make_shared<std::vector<Value>>();
            std::string line;
            while (std::getline(f, line)) arr->push_back(Value::fromString(line));
            push(Value::fromArray(std::move(arr)));
            NEXT();
        }

        case OpCode::Time: {
            using namespace std::chrono;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            push(Value::fromFloat(static_cast<double>(ms) / 1000.0));
            NEXT();
        }
        case OpCode::TimeMs: {
            using namespace std::chrono;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            push(Value::fromInt(static_cast<int32_t>(ms % INT32_MAX)));
            NEXT();
        }

        case OpCode::HttpGet: {
            auto url = pop().toString();
            push(Value::fromString(httpGet(url)));
            NEXT();
        }
        case OpCode::HttpPost: {
            auto body = pop().toString();
            auto url  = pop().toString();
            push(Value::fromString(httpPost(url, body, "text/plain")));
            NEXT();
        }
        case OpCode::HttpPostJson: {
            auto json = pop().toString();
            auto url  = pop().toString();
            push(Value::fromString(httpPost(url, json, "application/json")));
            NEXT();
        }

        case OpCode::Ord: {
            auto s = pop().toString();
            if (s.empty()) throw std::runtime_error("ord: empty string");
            push(Value::fromInt(static_cast<uint8_t>(s[0])));
            NEXT();
        }
        case OpCode::Chr: {
            auto code = pop().i;
            push(Value::fromString(std::string(1, static_cast<char>(code))));
            NEXT();
        }

        case OpCode::CreateArray: {
            int32_t count = unwrapInt(instr.operand);
            auto arr = std::make_shared<std::vector<Value>>(count);
            for (int i = count - 1; i >= 0; --i) (*arr)[i] = pop();
            push(Value::fromArray(std::move(arr)));
            NEXT();
        }
        case OpCode::ArrayLoad: {
            auto idx = pop(); auto arr = pop();
            if (idx.type != SabakaType::Int) throw std::runtime_error("Index must be int");
            if (arr.type == SabakaType::String) {
                auto& s = arr.asString();
                if (idx.i < 0 || idx.i >= static_cast<int32_t>(s.size()))
                    throw std::runtime_error("String index out of range");
                push(Value::fromString(std::string(1, s[idx.i])));
                NEXT();
            }
            if (arr.type != SabakaType::Array || !arr.array)
                throw std::runtime_error("Not an array");
            if (idx.i < 0 || idx.i >= static_cast<int32_t>(arr.array->size()))
                throw std::runtime_error("ArrayLoad out of bounds");
            push((*arr.array)[idx.i]);
            NEXT();
        }
        case OpCode::ArrayStore: {
            auto val  = pop();
            auto idx  = pop();
            auto arr  = pop();
            if (arr.type != SabakaType::Array || !arr.array)
                throw std::runtime_error("Not an array");
            if (idx.type != SabakaType::Int) throw std::runtime_error("Index must be int");
            auto& vec = *arr.array;
            while (static_cast<int32_t>(vec.size()) <= idx.i) vec.push_back(val);
            vec[idx.i] = std::move(val);
            NEXT();
        }
        case OpCode::ArrayLength: {
            auto v = pop();
            if (v.type == SabakaType::String)
                push(Value::fromInt(static_cast<int32_t>(v.str ? v.str->size() : 0)));
            else if (v.type == SabakaType::Array)
                push(Value::fromInt(static_cast<int32_t>(v.array ? v.array->size() : 0)));
            else throw std::runtime_error("ArrayLength: not array or string");
            NEXT();
        }

        case OpCode::CreateStruct: {
            auto fieldNames = unwrapStringList(instr.extra);
            auto fmap = std::make_shared<std::unordered_map<std::string, Value>>();
            for (auto& fn : fieldNames) (*fmap)[fn] = Value::fromInt(0);
            push(Value::fromStruct(std::move(fmap)));
            NEXT();
        }

        case OpCode::CreateObject: {
            auto fieldNames = unwrapStringList(instr.extra);
            auto fmap = std::make_shared<std::unordered_map<std::string, Value>>();
            for (auto& fn : fieldNames) (*fmap)[fn] = Value::fromInt(0);
            push(Value::fromObject(std::move(fmap), instr.hasName() ? instr.nameStr() : ""));
            NEXT();
        }
        case OpCode::LoadField: {
            auto obj = pop();
            if (obj.type != SabakaType::Object && obj.type != SabakaType::Struct)
                throw std::runtime_error("Cannot load field from non-object/struct");
            if (!obj.fields) throw std::runtime_error("Fields dictionary is null");
            auto it2 = obj.fields->find(instr.nameStr());
            push(it2 != obj.fields->end() ? it2->second : Value::fromInt(0));
            NEXT();
        }
        case OpCode::StoreField: {
            auto val = pop(); auto obj = pop();
            if (obj.type != SabakaType::Object && obj.type != SabakaType::Struct)
                throw std::runtime_error("Cannot store field in non-object/struct");
            if (!obj.fields) throw std::runtime_error("Fields dictionary is null");
            (*obj.fields)[instr.nameStr()] = std::move(val);
            NEXT();
        }

        case OpCode::Inherit:
            NEXT();

        case OpCode::FunctionStart:
            NEXT();

        default:
            throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<int>(instr.opcode)));
        }
        } catch (const std::exception& ex) {
            throw std::runtime_error(
                "[ip=" + std::to_string(ip) +
                " op=" + std::to_string(static_cast<int>(instr.opcode)) +
                (instr.hasName() ? " name=" + instr.nameStr() : "") +
                "] " + ex.what());
        }

#undef NEXT
#undef JUMP
    }
}

void VirtualMachine::enterScope() {
    _scopes.push_back({});
}
void VirtualMachine::exitScope() {
    if (_scopes.empty()) throw std::runtime_error("exitScope: no scope");
    _scopes.pop_back();
}

Value VirtualMachine::resolve(const std::string& name) {
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }

    if (!_thisStack.empty()) {
        auto& obj = _thisStack.back();
        if (obj.fields) {
            auto f = obj.fields->find(name);
            if (f != obj.fields->end()) return f->second;
        }
    }
    throw std::runtime_error("Undefined variable '" + name + "'");
}

void VirtualMachine::assign(const std::string& name, Value val) {
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) { f->second = std::move(val); return; }
    }
    if (!_thisStack.empty()) {
        auto& obj = _thisStack.back();
        if (obj.fields) {
            auto f = obj.fields->find(name);
            if (f != obj.fields->end()) { f->second = std::move(val); return; }
        }
    }
    throw std::runtime_error("Undefined variable '" + name + "'");
}

void VirtualMachine::binaryNumeric(double(*op)(double, double)) {
    auto b = pop(); auto a = pop();
    if (!a.isNumber() || !b.isNumber()) throw std::runtime_error("Operation requires numbers");
    if (a.type == SabakaType::Int && b.type == SabakaType::Int)
        push(Value::fromInt(static_cast<int32_t>(op(a.i, b.i))));
    else
        push(Value::fromFloat(op(a.toDouble(), b.toDouble())));
}

void VirtualMachine::compareNumeric(bool(*op)(double, double)) {
    auto b = pop(); auto a = pop();
    if (!a.isNumber() || !b.isNumber()) throw std::runtime_error("Comparison requires numbers");
    push(Value::fromBool(op(a.toDouble(), b.toDouble())));
}

bool VirtualMachine::isStringAtTop() const noexcept {
    if (_stack.size() < 2) return false;
    return _stack[_stack.size()-1].type == SabakaType::String ||
           _stack[_stack.size()-2].type == SabakaType::String;
}

int32_t VirtualMachine::unwrapInt(const Value& v) const noexcept {
    if (v.type == SabakaType::Int) return v.i;
    return 0;
}
std::string VirtualMachine::unwrapString(const Value& v) const {
    if (v.type == SabakaType::String && v.str) return *v.str;
    return v.toString();
}
std::vector<std::string> VirtualMachine::unwrapStringList(const Value& v) const {
    std::vector<std::string> result;
    if (v.type == SabakaType::Array && v.array) {
        for (auto& item : *v.array) {
            if (item.type == SabakaType::String && item.str) result.push_back(*item.str);
            else result.push_back(item.toString());
        }
    }
    return result;
}

const FunctionInfo& VirtualMachine::resolveMethod(const std::string& cls, const std::string& method) {
    std::string fqn = cls + "." + method;
    auto it = _functions.find(fqn);
    if (it != _functions.end()) return it->second;

    auto ih = _inheritance.find(cls);
    if (ih != _inheritance.end()) {
        std::string nextMethod = method;
        if (method == cls) nextMethod = ih->second; // super constructor
        return resolveMethod(ih->second, nextMethod);
    }
    throw std::runtime_error("Undefined method '" + method + "' in class '" + cls + "'");
}
