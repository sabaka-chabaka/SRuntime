#ifndef SRUNTIME_VALUE_H
#define SRUNTIME_VALUE_H
#include <map>
#include <string>
#include <vector>

enum SabakaType : int
{
    Int,
    Float,
    Bool,
    String,
    Array,
    Struct,
    Object
};

class Value {
public:
    SabakaType type;
    int intVal;
    double floatVal;
    bool boolVal;
    std::string stringVal;
    std::vector<Value> arrayVal;
    std::map<std::string, Value> structVal;
    std::map<std::string, Value> objectFields;
    std::string className;

    static Value FromInt(int val);
    static Value FromFloat(float val);
    static Value FromBool(bool val);
    static Value FromString(std::string val);
    static Value FromArray(std::vector<Value> val);
    static Value FromStruct(std::map<std::string, Value> val);

    std::string toString();
};


#endif