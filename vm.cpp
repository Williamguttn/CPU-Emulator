/*
We will create our own VM, with our own 8-bit CPU architecture. The point is not to fully create a CPU, but to instead create the language for it.


Rules:
- All math will be done in binary
- All values will be in hexadecimal
- All addresses will be in hexadecimal
- This is a VM. We will utilize C++ features as little as possible. Only to move values from A to B, or to communicate with the console.
- This means that division for example, will NOT be done in C++ (Like A / B).
- Uses little endian

Registers (8-bit)
- A (Accumulator) (0x00)
- B (0x01)
- C (0x02)
- F (Flags) (cannot be changed by the programmer)

Special-Purpose Registers (16-bit)
- PC (Program Counter)
- SP (Stack Pointer)

Memory
- RAM (Random Access Memory), 4KB
- ROM (Read-Only Memory), 4KB



INSTRUCTIONS
- NOP (0x00)
// Load & Store
- LDA, value (0x01)
- LDA, register (0x02)
- LDA, address (0x0A)
- STA, address (0x03)
- LDB, value (0x04)
- LDB, register (0x05)
- STB, address (0x06)
- LDC, value (0x07)
- LDC, register (0x08)
- STC, address (0x09)
// Arithmetic
- ADD register, register (0x0B)
- SUB register, register (0x0C)
- ADD register, value (0x0D)
- SUB register, value (0x0E)
// Bitwise
- AND register, register (0x0F)
- OR register, register (0x10)
- XOR register, register (0x11)
- NOT register (0x12)
// Jumps
- JMP directly to address (0x13)
- JMP if flags match ZERO (0x14)
- JMP if flags match NOT ZERO (0x15)
- JMP if flags match CARRY (0x16)
- JMP if flags match NOT CARRY (0x17)
// Compare
- CP value (0x18) (Compare value with accumulator)


// More modern changes
- 3 registers is not sufficient. From now on, we will have: A, B, C, D, E, (F), G, H
- 16-bit registers will be available. You can use BC, DE, GH
*/

#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include <sstream>
#include <string>

bool isBitSet(int n, int k) {
    return (n & (1 << k)) != 0;
}

class VM {
private:
    std::vector<uint8_t> ram;
    std::vector<uint8_t> rom;

    // Registers
    // A, B, C, F
    // Opcodes for registers: 0x00, 0x01, 0x02, 0x03
    std::array<uint8_t, 4> registers;
    enum Register {
        A_REG = 0x00,
        B_REG = 0x01,
        C_REG = 0x02,
        F_REG = 0x03
    };
    
    std::array<uint16_t, 3> registers16;
    enum Register16 {
        BC_REG = 0x00,
        DE_REG = 0x01,
        GH_REG = 0x02
    };

    // Flags
    enum Flag {
        ZeroMask = 0b01,
        CarryMask = 0b10
    };

    enum JumpCases {
        Z = 0x00, // In program code, jp z, 0x01 will for instance be 0x16, 0x00, 0x01
        NZ = 0x01,
        C = 0x02,
        NC = 0x03
    };

    // Special-Purpose Registers
    uint16_t PC;
    uint16_t SP;

public:
    bool debug = false;

    VM() : registers{}, PC(0), SP(0) {
        ram.resize(4096);
        rom.resize(4096);
    }

    void load_program(const std::vector<uint8_t>& program) {
        std::copy(program.begin(), program.end(), rom.begin());
        PC = 0;
    }

    void run() {
        int i = 0;
        
        while (PC < rom.size()) {
            uint8_t instruction = rom[PC++];
            int result = execute(instruction);

            if (debug && instruction != 0xFF) {
                i++;
                std::cout << "Instruction " << i << ": " << std::hex << static_cast<int>(instruction) << " - PC: " << std::hex << PC << " - F: " << std::hex << static_cast<int>(registers[F_REG]) << std::endl;
                std::cout << "Accumulator for debug: " << std::hex << static_cast<int>(registers[A_REG]) << std::endl;
            }

            // Exit if the program is finished
            if (result == -1) {
                break;
            }
        }
    }

    int execute(uint8_t opcode) {
        uint8_t value;
        uint8_t reg;
        uint8_t reg2;
        uint16_t location;
        uint8_t flags;
        
        switch (opcode) {
            case 0x01:
                // Load value into register A

                // LDA, value
                value = rom[PC++];
                registers[A_REG] = value;
                break;
            case 0x02:
                // Load value from register into register A

                // LDA, register
                reg = rom[PC++];
                registers[A_REG] = registers[reg];
                break;
            case 0x03:
                // Store value from register A into memory

                // STA, address
                location = (rom[PC++] << 8) | rom[PC++];
                ram[location] = registers[A_REG];
                break;
            case 0x04:
                // Load value into register B

                // LDB, value
                value = rom[PC++];
                registers[1] = value;
                break;
            case 0x05:
                // Load value from register into register B

                // LDB, register
                reg = rom[PC++];
                registers[1] = registers[reg];
                break;
            case 0x06:
                // Store value from register B into memory

                // STB, address
                location = (rom[PC++] << 8) | rom[PC++];
                ram[location] = registers[1];
                break;
            case 0x07:
                // Load value into register C

                // LDC, value
                value = rom[PC++];
                registers[2] = value;
                break;
            case 0x08:
                // Load value from register into register C

                // LDC, register
                reg = rom[PC++];
                registers[2] = registers[reg];
                break;
            case 0x09:
                // Store value from register C into memory

                // STC, address
                location = (rom[PC++] << 8) | rom[PC++];
                ram[location] = registers[2];
                break;
            case 0x0A:
                // Load value from memory into register A
                // Address is 16-bits, so there will be 2 bytes for the address

                // LDA, address
                location = (rom[PC++] << 8) | rom[PC++];
                registers[A_REG] = ram[location];

                break;
            case 0x0B:
                // Add reg to reg

                // ADD register, register
                reg = rom[PC++];
                reg2 = rom[PC++];
                registers[reg] |= registers[reg2];
                break;
            case 0x0C:
                // Subtract reg from reg

                // SUB register, register
                reg = rom[PC++];
                reg2 = rom[PC++];
                registers[reg] &= ~registers[reg2];
                break;
            case 0x0D:
                // Add value to register

                // ADD register, value
                reg = rom[PC++];
                value = rom[PC++];
                registers[reg] += value;
                break;
            case 0x0E:
                // Subtract value from register

                // SUB register, value
                reg = rom[PC++];
                value = rom[PC++];
                registers[reg] &= ~value;
                break;
            case 0x0F:
                // AND register, register
                reg = rom[PC++];
                reg2 = rom[PC++];
                registers[reg] &= registers[reg2];
                break;
            case 0x10:
                // OR register, register
                reg = rom[PC++];
                reg2 = rom[PC++];
                registers[reg] |= registers[reg2];
                break;
            case 0x11:
                // XOR register, register
                reg = rom[PC++];
                reg2 = rom[PC++];
                registers[reg] ^= registers[reg2];
                break;
            case 0x12:
                // NOT register
                reg = rom[PC++];
                registers[reg] = ~registers[reg];
                break;
            case 0x13:
                // JMP directly to address
                value = (rom[PC++] << 8) | rom[PC++];
                PC = value;
                
                break;

            case 0x14:
                // JMP if flags match ZERO
                value = (rom[PC++] << 8) | rom[PC++];

                if ((registers[F_REG] & Flag::ZeroMask) != 0) {
                    PC = value;
                }

                break;
            case 0x15:
                // JMP if flags match NOT ZERO
                value = (rom[PC++] << 8) | rom[PC++];

                if ((registers[F_REG] & Flag::ZeroMask) == 0) {
                    PC = value - 1; // TODO: -1 here only works if the address we're jumping to is less than the current PC. Fix later
                }

                break;
            case 0x16:
                // JMP if flags match CARRY
                value = (rom[PC++] << 8) | rom[PC++];

                if (isBitSet(registers[F_REG], 1)) {
                    PC = value;
                }

                break;
            case 0x17:
                // JMP if flags match NOT CARRY
                value = (rom[PC++] << 8) | rom[PC++];

                if (!isBitSet(registers[F_REG], 1)) {
                    PC = value;
                }

                break;
            case 0x18:
                // Compare value with accumulator
                // CP value
                value = rom[PC++];
                reg = registers[A_REG];

                if (reg == value) {
                    registers[F_REG] = Flag::ZeroMask;
                }

                if (reg != value) {
                    registers[F_REG] &= ~Flag::ZeroMask;
                }

                if (reg > value) {
                    registers[F_REG] = 0; // Carry and zero flags reset
                }

                if (reg < value) {
                    registers[F_REG] = Flag::CarryMask;
                }

                break;
            /*case 0x17:
                // Jump if flags match

                flags = rom[PC++];
                value = (rom[PC++] << 8) | rom[PC++];
                    // Start of Selection

                // ZERO Flag
                if (flags == JumpCases::Z && (registers[F_REG] & Flag::ZeroMask) != 0) {
                    PC = value;
                }

                // NOT-ZERO Flag
                if (flags == JumpCases::NZ && (registers[F_REG] & Flag::ZeroMask) == 0) {
                    PC = value;
                    // Start of Selection
                    //std::cout << "Jumping to: " << std::hex << static_cast<int>(rom[PC]) << std::endl;

                }

                // CARRY Flag
                if (flags == JumpCases::C && isBitSet(registers[F_REG], 1)) {
                    PC = value;
                }

                // NOT-CARRY Flag
                if (flags == JumpCases::NC && !isBitSet(registers[F_REG], 1)) {
                    PC = value;
                }

                break;*/

            case 0x00:
                // NOP
                return -1;
            case 0xFF:
                // Just skip, this is a small assembler bug that will be fixed later
                break;
            default:
                std::cout << "Unknown opcode: 0x" << std::hex << static_cast<int>(opcode) << std::endl;
                return -1;

            return 1;
        }
    }

    void memory_dump() {
        // Dump entire memory to a file
        std::ofstream file("memory.dump", std::ios::out | std::ios::binary);
        file.write(reinterpret_cast<const char*>(ram.data()), ram.size());
        file.close();
    }

    void memory_print() {
        // Only print memory locations that are not empty. Have address and value.
        for (int i = 0; i < ram.size(); i++) {
            if (ram[i] != 0) {
                std::cout << "RAM[0x" << std::hex << i << "] = 0x" << std::hex << static_cast<int>(ram[i]) << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {

    // In the beggining, there will be no assembly language.
    // Use machine code directly for now.

    /*std::vector<uint8_t> program = {
        0x01, 0x01, // LDA 1
        0x0D, 0x00, 0x01, // ADD A, 1
        0x15, 0x0A, // CP 10
        0x17, 0x01, 0x05, // JMP NEGATIVE 5 IF FLAG IS NOT ZERO
        0x03, 0x00, // STA $0000
        0x00 // NOP
    };*/

    /*std::vector<uint8_t> program = {
        // Debug program
        0x04, 0x01, // LDA 0
        0x06, 0x00, // STA $0000
        0x00 // NOP
    };*/

    //std::vector<uint8_t> program;

    /*std::ifstream file ("program.bin");

    if (!file) {
        std::cout << "Error: Could not open file" << std::endl;
        return 1;
    }

    // Read program.bin into program vector, split by spaces/newlines and add to program vector
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        
        std::string token;

        while (iss >> token) {
            int value = std::stoi(token, nullptr, 16);
            program.push_back(static_cast<uint8_t>(value));
        }
        

        // This will be used when we have a assembler
        /*uint8_t byte;
        iss << std::hex << byte;
        program.push_back(byte);*
    }*/

    VM vm;

   // Lazy approach, just check if --debug is in the arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--debug") {
            vm.debug = true;
        }
    }

    std::string input_file = argv[1];

    std::ifstream infile(input_file, std::ios::binary);

    infile.seekg(0, std::ios::end);
    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<uint8_t> program(size);

    if (!infile.read(reinterpret_cast<char*>(program.data()), size)) {
        std::cout << "Error: Could not read file" << std::endl;
        return 1;
    }

    infile.close();

    if (vm.debug) {
        // Print the program
        for (uint8_t byte : program) {
            std::cout << std::hex << static_cast<int>(byte) << " ";
        }
        std::cout << std::endl;
    }

    vm.load_program(program);
    vm.run();

    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--dump") {
            vm.memory_dump();
        }

        if (std::string(argv[i]) == "--print") {
            vm.memory_print();
        }
    }

    return 0;
}