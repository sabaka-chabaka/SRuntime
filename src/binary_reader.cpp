#include "binary_reader.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>

BinaryReader::BinaryReader(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    auto sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    _buf.resize(sz);
    f.read(reinterpret_cast<char*>(_buf.data()), sz);
}

int32_t BinaryReader::readI32() {
    if (_pos + 4 > _buf.size())
        throw std::runtime_error("BinaryReader: EOF reading int32 at " + std::to_string(_pos));
    int32_t v; std::memcpy(&v, _buf.data() + _pos, 4); _pos += 4; return v;
}

double BinaryReader::readF64() {
    if (_pos + 8 > _buf.size())
        throw std::runtime_error("BinaryReader: EOF reading double at " + std::to_string(_pos));
    double v; std::memcpy(&v, _buf.data() + _pos, 8); _pos += 8; return v;
}

bool BinaryReader::readBool() {
    if (_pos >= _buf.size())
        throw std::runtime_error("BinaryReader: EOF reading bool at " + std::to_string(_pos));
    return _buf[_pos++] != 0;
}

std::string BinaryReader::readSabakaStr() {
    int32_t len = readI32();
    if (len == -1) return "";
    if (len < 0 || _pos + static_cast<size_t>(len) > _buf.size())
        throw std::runtime_error("BinaryReader: bad string length " + std::to_string(len));
    std::string s(reinterpret_cast<const char*>(_buf.data() + _pos), len);
    _pos += len;
    return s;
}

std::string BinaryReader::readLebStr() {
    uint32_t len = 0; int shift = 0;
    while (true) {
        if (_pos >= _buf.size()) throw std::runtime_error("BinaryReader: EOF in LEB128");
        uint8_t b = _buf[_pos++];
        len |= static_cast<uint32_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
        if (shift >= 35) throw std::runtime_error("BinaryReader: LEB128 overflow");
    }
    if (_pos + len > _buf.size()) throw std::runtime_error("BinaryReader: EOF in LEB string body");
    std::string s(reinterpret_cast<const char*>(_buf.data() + _pos), len);
    _pos += len;
    return s;
}

Value BinaryReader::readObject() {
    if (_pos >= _buf.size()) throw std::runtime_error("BinaryReader: EOF reading type marker");
    uint8_t marker = _buf[_pos++];

    switch (marker) {
    case 0: return Value::null_val();
    case 1: return Value::fromInt(readI32());
    case 2: return Value::fromFloat(readF64());
    case 3: return Value::fromBool(readBool());
    case 4: return Value::fromString(readLebStr());

    case 5: {
        std::string enumStr = readLebStr();
        readLebStr();
        return Value::fromString(enumStr);
    }

    case 6: {
        int32_t count = readI32();
        auto arr = std::make_shared<std::vector<Value>>();
        arr->reserve(count);
        for (int32_t i = 0; i < count; ++i)
            arr->push_back(readObject());
        return Value::fromArray(std::move(arr));
    }

    case 7: {
        std::string typeName = readLebStr();
        int32_t fieldCount   = readI32();

        std::vector<Value> fields;
        fields.reserve(fieldCount);
        for (int32_t i = 0; i < fieldCount; ++i)
            fields.push_back(readObject());

        if (typeName.find("Value") == std::string::npos || fieldCount < 4)
            return fields.empty() ? Value::null_val() : fields.back();

        std::string typetag;
        if (!fields.empty() && fields[0].type == SabakaType::String && fields[0].str)
            typetag = *fields[0].str;

        if (typetag == "Int")
            return Value::fromInt(fieldCount > 1 ? fields[1].i : 0);

        if (typetag == "Float")
            return Value::fromFloat(fieldCount > 2 ? fields[2].f : 0.0);

        if (typetag == "Bool")
            return Value::fromBool(fieldCount > 3 && fields[3].b);

        if (typetag == "String") {
            if (fieldCount > 4 && fields[4].type == SabakaType::String && fields[4].str)
                return Value::fromString(*fields[4].str);
            return Value::fromString("");
        }

        if (typetag == "Array") {
            if (fieldCount > 5 && fields[5].type == SabakaType::Array)
                return fields[5];
            return Value::fromArray(std::make_shared<std::vector<Value>>());
        }

        if (typetag == "Struct") {
            if (fieldCount > 6 && fields[6].type == SabakaType::Struct)
                return fields[6];
            return Value::fromStruct(std::make_shared<std::unordered_map<std::string, Value>>());
        }

        if (typetag == "Object") {
            std::string cls;
            if (fieldCount > 8 && fields[8].type == SabakaType::String && fields[8].str)
                cls = *fields[8].str;
            return Value::fromObject(
                std::make_shared<std::unordered_map<std::string, Value>>(),
                std::move(cls));
        }

        return Value::null_val();
    }

    default:
        throw std::runtime_error(
            "BinaryReader: unknown type marker " + std::to_string(marker) +
            " at pos " + std::to_string(_pos - 1));
    }
}

std::vector<Instruction> BinaryReader::read() {
    int32_t count = readI32();
    if (count < 0 || count > 10'000'000)
        throw std::runtime_error("BinaryReader: suspicious instruction count " + std::to_string(count));

    std::vector<Instruction> list;
    list.reserve(count);

    for (int32_t i = 0; i < count; ++i) {
        Instruction instr;
        instr.opcode = static_cast<OpCode>(readI32());

        std::string n = readSabakaStr();
        if (!n.empty())
            instr.name = std::make_shared<std::string>(std::move(n));

        instr.operand = readObject();
        instr.extra   = readObject();

        list.push_back(std::move(instr));
    }
    return list;
}