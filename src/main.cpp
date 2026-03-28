#include "binary_reader.hpp"
#include "virtual_machine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: sre <file.sabakac>\n";
        return 1;
    }

    try {
        BinaryReader reader(argv[1]);
        auto instructions = reader.read();

        VirtualMachine vm;
        vm.execute(instructions);

    } catch (const std::exception& ex) {
        std::cerr << "[SRE Error] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
