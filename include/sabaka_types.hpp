#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <cstdint>

enum class SabakaType : uint8_t {
    Int    = 0,
    Float  = 1,
    Bool   = 2,
    String = 3,
    Array  = 4,
    Struct = 5,
    Object = 6,
    Null   = 7
};

enum class OpCode : int32_t {
    Push         = 0,
    Add          = 1,
    Sub          = 2,
    Mul          = 3,
    Div          = 4,
    Print        = 5,
    Store        = 6,
    Jump         = 7,
    JumpIfFalse  = 8,
    Load         = 9,
    Equal        = 10,
    NotEqual     = 11,
    Greater      = 12,
    Less         = 13,
    GreaterEqual = 14,
    LessEqual    = 15,
    Negate       = 16,
    Declare      = 17,
    EnterScope   = 18,
    ExitScope    = 19,
    Not          = 20,
    Call         = 21,
    Return       = 22,
    FunctionStart= 23,
    Function     = 24,
    JumpIfTrue   = 25,
    CreateArray  = 26,
    ArrayLoad    = 27,
    ArrayStore   = 28,
    ArrayLength  = 29,
    CreateStruct = 30,
    LoadField    = 31,
    StoreField   = 32,
    CreateObject = 33,
    CallMethod   = 34,
    PushThis     = 35,
    Dup          = 36,
    Pop          = 37,
    Inherit      = 38,
    Input        = 39,
    CallExternal = 40,
    Sleep        = 41,
    Percent      = 42,
    ReadFile     = 43,
    WriteFile    = 44,
    AppendFile   = 45,
    FileExists   = 46,
    DeleteFiler   = 47,
    ReadLines    = 48,
    Time         = 49,
    TimeMs       = 50,
    HttpGet      = 51,
    HttpPost     = 52,
    HttpPostJson = 53,
    Ord          = 54,
    Chr          = 55
};

struct Value;

using ValueArray  = std::shared_ptr<std::vector<Value>>;
using ValueFields = std::shared_ptr<std::unordered_map<std::string, Value>>;

struct Value {
    SabakaType type { SabakaType::Null };

    int32_t  i { 0 };
    double   f { 0.0 };
    bool     b { false };

    std::shared_ptr<std::string> str;
    ValueArray                   array;
    ValueFields                  fields;
    std::shared_ptr<std::string> className;

    static Value fromInt(int32_t v) noexcept {
        Value val; val.type = SabakaType::Int; val.i = v; return val;
    }
    static Value fromFloat(double v) noexcept {
        Value val; val.type = SabakaType::Float; val.f = v; return val;
    }
    static Value fromBool(bool v) noexcept {
        Value val; val.type = SabakaType::Bool; val.b = v; return val;
    }
    static Value fromString(std::string s) {
        Value val; val.type = SabakaType::String;
        val.str = std::make_shared<std::string>(std::move(s));
        return val;
    }
    static Value fromArray(ValueArray arr) {
        Value val; val.type = SabakaType::Array; val.array = std::move(arr); return val;
    }
    static Value fromStruct(ValueFields f) {
        Value val; val.type = SabakaType::Struct; val.fields = std::move(f); return val;
    }
    static Value fromObject(ValueFields f, std::string cls) {
        Value val; val.type = SabakaType::Object;
        val.fields = std::move(f);
        val.className = std::make_shared<std::string>(std::move(cls));
        return val;
    }
    static Value null_val() noexcept { return Value{}; }

    int32_t     asInt()    const { return i; }
    double      asFloat()  const { return f; }
    bool        asBool()   const { return b; }
    const std::string& asString() const { return *str; }

    bool isNumber() const noexcept {
        return type == SabakaType::Int || type == SabakaType::Float;
    }
    double toDouble() const {
        if (type == SabakaType::Int)   return static_cast<double>(i);
        if (type == SabakaType::Float) return f;
        throw std::runtime_error("Value is not a number");
    }

    std::string toString() const;
};

struct Instruction {
    OpCode   opcode  { OpCode::Push };
    Value    operand;
    Value    extra;
    std::shared_ptr<std::string> name;

    int32_t operandInt() const noexcept { return operand.i; }
    const std::string& nameStr() const {
        if (!name) throw std::runtime_error("Instruction name is null");
        return *name;
    }
    bool hasName() const noexcept { return name && !name->empty(); }
};
