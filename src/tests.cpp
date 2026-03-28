#include "virtual_machine.hpp"
#include "binary_reader.hpp"
#include <sstream>
#include <cassert>
#include <iostream>

static Instruction mkinstr(OpCode op,
                             Value operand = Value::null_val(),
                             const std::string& name = "",
                             Value extra = Value::null_val()) {
    Instruction i;
    i.opcode  = op;
    i.operand = operand;
    i.extra   = extra;
    if (!name.empty()) i.name = std::make_shared<std::string>(name);
    return i;
}

static Instruction mknamed(OpCode op, const std::string& name) {
    return mkinstr(op, Value::null_val(), name);
}

struct TestVM {
    std::istringstream input;
    std::ostringstream output;
    VirtualMachine vm;
    TestVM(const std::string& inp = "")
        : input(inp)
        , vm(&input, &output) {}

    std::string run(std::vector<Instruction> instrs) {
        // Убираем auto-pause
        instrs.push_back(mkinstr(OpCode::Input));
        vm.execute(instrs);
        return output.str();
    }
};

void test_push_print() {
    TestVM t("\n");
    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromString("hello")),
        mknamed(OpCode::Print, ""),
    };
    prog.push_back(mkinstr(OpCode::Input));
    t.vm.execute(prog);
    assert(t.output.str() == "hello\n");
    std::cout << "[PASS] test_push_print\n";
}

void test_add_int() {
    TestVM t("\n");
    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromInt(3)),
        mkinstr(OpCode::Push, Value::fromInt(4)),
        mkinstr(OpCode::Add),
        mknamed(OpCode::Print, ""),
        mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "7\n");
    std::cout << "[PASS] test_add_int\n";
}

void test_string_concat() {
    TestVM t("\n");
    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromString("foo")),
        mkinstr(OpCode::Push, Value::fromString("bar")),
        mkinstr(OpCode::Add),
        mknamed(OpCode::Print, ""),
        mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "foobar\n");
    std::cout << "[PASS] test_string_concat\n";
}

void test_declare_load_store() {
    TestVM t("\n");
    // var x = 10; x = x + 5; print(x)
    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromInt(10)),
        mknamed(OpCode::Declare, "x"),
        mknamed(OpCode::Load, "x"),
        mkinstr(OpCode::Push, Value::fromInt(5)),
        mkinstr(OpCode::Add),
        mknamed(OpCode::Store, "x"),
        mknamed(OpCode::Load, "x"),
        mknamed(OpCode::Print, ""),
        mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "15\n");
    std::cout << "[PASS] test_declare_load_store\n";
}

void test_if_branch() {
    TestVM t("\n");
    // if (true) { print("yes") } else { print("no") }
    // JumpIfFalse -> 5 (else body)
    // Push "yes" / Print / Jump -> 7
    // Push "no"  / Print
    std::vector<Instruction> prog = {
        /*0*/ mkinstr(OpCode::Push, Value::fromBool(true)),
        /*1*/ mkinstr(OpCode::JumpIfFalse, Value::fromInt(5)),
        /*2*/ mkinstr(OpCode::Push, Value::fromString("yes")),
        /*3*/ mknamed(OpCode::Print, ""),
        /*4*/ mkinstr(OpCode::Jump, Value::fromInt(7)),
        /*5*/ mkinstr(OpCode::Push, Value::fromString("no")),
        /*6*/ mknamed(OpCode::Print, ""),
        /*7*/ mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "yes\n");
    std::cout << "[PASS] test_if_branch\n";
}

void test_function_call() {
    TestVM t("\n");
    // func double(x) { return x * 2 }
    // print(double(7))
    // Layout:
    //  0: Function "double" jumpOver=5 params=[x]
    //  1: Load x
    //  2: Push 2
    //  3: Mul
    //  4: Return
    //  5: Push 7
    //  6: Call "double" argCount=1
    //  7: Print
    //  8: Input
    auto funcInstr = mkinstr(OpCode::Function, Value::fromInt(5), "double",
                              Value::fromArray(std::make_shared<std::vector<Value>>(
                                  std::initializer_list<Value>{ Value::fromString("x") })));
    std::vector<Instruction> prog = {
        /*0*/ funcInstr,
        /*1*/ mknamed(OpCode::Load, "x"),
        /*2*/ mkinstr(OpCode::Push, Value::fromInt(2)),
        /*3*/ mkinstr(OpCode::Mul),
        /*4*/ mkinstr(OpCode::Return),
        /*5*/ mkinstr(OpCode::Push, Value::fromInt(7)),
        /*6*/ mkinstr(OpCode::Call, Value::fromInt(1), "double"),
        /*7*/ mknamed(OpCode::Print, ""),
        /*8*/ mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "14\n");
    std::cout << "[PASS] test_function_call\n";
}

void test_array() {
    TestVM t("\n");
    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromInt(1)),
        mkinstr(OpCode::Push, Value::fromInt(2)),
        mkinstr(OpCode::Push, Value::fromInt(3)),
        mkinstr(OpCode::CreateArray, Value::fromInt(3)),
        mknamed(OpCode::Declare, "arr"),
        mknamed(OpCode::Load, "arr"),
        mkinstr(OpCode::Push, Value::fromInt(1)),
        mkinstr(OpCode::ArrayLoad),
        mknamed(OpCode::Print, ""),
        // arr[1] = 99
        mknamed(OpCode::Load, "arr"),
        mkinstr(OpCode::Push, Value::fromInt(1)),
        mkinstr(OpCode::Push, Value::fromInt(99)),
        mkinstr(OpCode::ArrayStore),
        mknamed(OpCode::Load, "arr"),
        mkinstr(OpCode::Push, Value::fromInt(1)),
        mkinstr(OpCode::ArrayLoad),
        mknamed(OpCode::Print, ""),
        mkinstr(OpCode::Input),
    };
    t.vm.execute(prog);
    assert(t.output.str() == "2\n99\n");
    std::cout << "[PASS] test_array\n";
}

void test_external() {
    std::istringstream inp("\n");
    std::ostringstream out;
    std::unordered_map<std::string, ExternalFunc> ext;
    ext["mymod.greet"] = [](std::vector<Value> args) -> Value {
        return Value::fromString("Hi, " + args[0].toString() + "!");
    };
    VirtualMachine vm(&inp, &out, std::move(ext));

    std::vector<Instruction> prog = {
        mkinstr(OpCode::Push, Value::fromString("N")),
        mkinstr(OpCode::CallExternal, Value::fromInt(1), "mymod.greet"),
        mknamed(OpCode::Print, ""),
        mkinstr(OpCode::Input),
    };
    vm.execute(prog);
    assert(out.str() == "Hi, N!\n");
    std::cout << "[PASS] test_external\n";
}

int main() {
    test_push_print();
    test_add_int();
    test_string_concat();
    test_declare_load_store();
    test_if_branch();
    test_function_call();
    test_array();
    test_external();
    std::cout << "\nAll tests passed.\n";
    return 0;
}
