#ifndef SRUNTIME_INSTRUCTION_H
#define SRUNTIME_INSTRUCTION_H
#include <any>
#include <string>
#include <utility>


enum OpCode : int;

class Instruction {

public:
    OpCode opCode;
    std::string name;
    std::any operand;
    std::any extra;

    Instruction(OpCode opCode, std::string name, std::any* operand) {
        this->opCode = opCode;
        this->name = std::move(name);
        this->operand = operand;
    }
};


#endif