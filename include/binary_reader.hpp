#pragma once
#include "sabaka_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  BinaryReader — десериализует .sabakac
//
//  Формат (точная копия C# BinaryWriterWorker.Pack):
//
//    int32   — кол-во инструкций
//    for each instruction:
//      int32   — OpCode
//      sabaka_string — Name  (int32 byteLen, -1=null, затем UTF-8 байты)
//      object  — Operand
//      object  — Extra
//
//  object type-marker (byte):
//    0 = null
//    1 = int32
//    2 = double
//    3 = bool
//    4 = string  (C# BinaryWriter.Write(string) — 7-bit LEB128 len + UTF-8)
//    5 = enum    (LEB-string enumStr + LEB-string typeName)  → Value::fromString(enumStr)
//    6 = list    (int32 count, then count objects)           → Value::fromArray(...)
//    7 = struct  (LEB-string typeName, int32 fieldCount, fields...) → last field value
// ─────────────────────────────────────────────────────────────────────────────
class BinaryReader {
public:
    explicit BinaryReader(const std::string& path);

    std::vector<Instruction> read();

private:
    std::vector<uint8_t> _buf;
    size_t               _pos { 0 };

    // ── примитивы ──────────────────────────────────────────────────────────
    int32_t readI32();
    double  readF64();
    bool    readBool();

    // Наш WriteString: int32 byteLen (-1=null) + raw UTF-8
    std::string readSabakaStr();

    // C# BinaryWriter.Write(string): 7-bit LEB128 len + UTF-8
    std::string readLebStr();

    // Рекурсивный ReadObject
    Value readObject();
};
