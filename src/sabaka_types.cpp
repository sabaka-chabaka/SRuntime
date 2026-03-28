#include "sabaka_types.hpp"
#include <sstream>
#include <iomanip>

std::string Value::toString() const {
    switch (type) {
        case SabakaType::Int:
            return std::to_string(i);
        case SabakaType::Float: {
            std::ostringstream oss;
            oss << std::setprecision(15) << f;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
                s += ".0";
            return s;
        }
        case SabakaType::Bool:
            return b ? "true" : "false";
        case SabakaType::String:
            return str ? *str : "";
        case SabakaType::Array: {
            if (!array) return "[]";
            std::ostringstream oss;
            oss << "[";
            for (size_t k = 0; k < array->size(); ++k) {
                if (k) oss << ", ";
                oss << (*array)[k].toString();
            }
            oss << "]";
            return oss.str();
        }
        case SabakaType::Struct: {
            if (!fields) return "struct {}";
            std::ostringstream oss;
            oss << "struct { ";
            bool first = true;
            for (auto& [k, v] : *fields) {
                if (!first) oss << ", ";
                oss << k << ": " << v.toString();
                first = false;
            }
            oss << " }";
            return oss.str();
        }
        case SabakaType::Object: {
            std::string cn = className ? *className : "object";
            return "<" + cn + ">";
        }
        default:
            return "null";
    }
}
