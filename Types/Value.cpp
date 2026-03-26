#include "Value.h"

#include <variant>

Value Value::FromInt(int val) {
    Value v;
    v.type = Int;
    v.intVal = val;
    return v;
}

Value Value::FromFloat(float val) {
    Value v;
    v.type = Float;
    v.floatVal = val;
    return v;
}

Value Value::FromBool(bool val) {
    Value v;
    v.type = Bool;
    v.boolVal = val;
    return v;
}

Value Value::FromString(std::string val) {
    Value v;
    v.type = String;
    v.stringVal = val;
    return v;
}

Value Value::FromArray(std::vector<Value> val) {
    Value v;
    v.type = Array;
    v.arrayVal = val;
    return v;
}

Value Value::FromStruct(std::map<std::string, Value> val) {
    Value v;
    v.type = Struct;
    v.structVal = val;
    return v;
}

std::string Value::toString() {
    switch (type) {
        case Int:    return std::to_string(intVal);
        case Float: {
            std::string s = std::to_string(floatVal);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s.pop_back();
            return s;
        }
        case Bool:   return boolVal ? "true" : "false";
        case String: return stringVal;
        case Array: {
            std::string res = "[";
            for (size_t i = 0; i < arrayVal.size(); ++i) {
                res += arrayVal[i].toString() + (i < arrayVal.size() - 1 ? ", " : "");
            }
            return res + "]";
        }
        case Struct: {
            std::string res = "struct { ";
            for (auto it = structVal.begin(); it != structVal.end(); ++it) {
                res += it->first + ": " + it->second.toString();
                if (std::next(it) != structVal.end()) res += ", ";
            }
            return res + " }";
        }
        case Object:
            return "object " + className + " { ... }";
        default: return "null";
    }
}
