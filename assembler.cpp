/**
 * Assembler for the VM
 * 
 * This program takes an assembly file and converts it into a binary file
 * that can be used by the VM.
 * 
 * Programs are stored at the start of ROM.
 */

// TODO: When removing the empty FF's, we need to subtract the amount of FF's from the label jump addresses

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <map>

// LD(register) (register)
static const std::map<char, uint8_t> ld_registers = {
    {'A', 0x02},
    {'B', 0x05},
    {'C', 0x08}
};

// LD(register) (value)
static const std::map<char, uint8_t> ld_values = {
    {'A', 0x01},
    {'B', 0x04},
    {'C', 0x07}
};

// ST(register) (address)
static const std::map<char, uint8_t> st_addresses = {
    {'A', 0x03},
    {'B', 0x06},
    {'C', 0x09}
};

static const std::map<char, uint8_t> register_codes = {
    {'A', 0x00},
    {'B', 0x01},
    {'C', 0x02}
};

static const std::map<std::string, uint8_t> bitwise_operations = {
    {"AND", 0x0F},
    {"OR", 0x10},
    {"XOR", 0x11},
    {"NOT", 0x12}
};

static const std::map<std::string, uint8_t> jump_cases = {
    {"Z", 0x14},
    {"NZ", 0x15},
    {"C", 0x16},
    {"NC", 0x17}
};

static const std::vector<std::string> registers = {
    "A",
    "B",
    "C"
};

std::vector<uint8_t> program;

class Instruction {
    public:
        uint8_t opcode;
        uint8_t operand1 = EMPTY_OPERAND;
        uint8_t operand2 = EMPTY_OPERAND; // Max 2 operands

        enum {
            EMPTY_OPERAND = -1
        };
};

// Holds information about a parsed line. Primarily needed to locate labels
class ParsedLine {
    public:
        ParsedLine() : address(0), size(0), is_label(false) {}

        Instruction instruction;
        std::string label;
        bool is_label;
        uint16_t address; // Address of the last operand of the instruction
        uint16_t size; // Size of the instruction (e.g. 2 for LDA 1, 3 for ADD A, 1)
};

bool is_address(std::string& token) {
    return token.front() == '$' && token.size() > 1;
}

bool is_register(std::string& token) {
    // Make sure the token is uppercase
    std::transform(token.begin(), token.end(), token.begin(), ::toupper);

    if (token.find(',') != std::string::npos) {
        token.erase(token.find(','));
    }

    return std::find(registers.begin(), registers.end(), token) != registers.end();
}

bool is_hex(std::string& token) {
    return token.front() == '0' && token.size() > 1 && token[1] == 'X';
}

bool is_jump_case(std::string& token) {
    
    if (token.find(',') != std::string::npos) {
        token.erase(token.find(','));
    }

    return jump_cases.find(token) != jump_cases.end();
}

uint8_t get_value(std::string& token) {
    uint8_t value;

    if (is_hex(token)) {
        std::istringstream hex_iss(token);
        hex_iss >> std::hex >> value;
    } else {
        value = std::stoi(token);
    }

    return value;
}

void clean_line(std::string* line) {
    // Remove comments
    std::string::size_type pos = line->find(';');
    if (pos != std::string::npos) {
        line->erase(pos);
    }
}

void check_waiting_labels(std::map<std::string, uint16_t>* labels, std::map<std::string, uint16_t>* waiting_labels, std::string& label, uint16_t& current_address) {
    if (waiting_labels->find(label) != waiting_labels->end()) {
        uint16_t address = waiting_labels->at(label);

        // We'll need to jump back in the program and update the address of the label
        program[address - 1] = static_cast<uint8_t>(current_address >> 8);
        program[address] = static_cast<uint8_t>(current_address & 0xFF);
    }
}

uint16_t get_label_address(uint16_t& address, std::string& label, std::map<std::string, uint16_t>* labels, std::map<std::string, uint16_t>* waiting_labels) {
    if (labels->find(label) != labels->end()) {
        return labels->at(label);
    }

    // Label wasnt found, so now we have to find it
    // Address here is the address where we will write the address of the label
    // That is wordy, but it is the address of the instruction that is calling the label
    // For instance, if we have:
    // LDA 0
    // JMP Z, loop ; LOOKING FOR THE ADDRESS OF LOOP
    // loop: ; Here it is found, the address of LDA 1 will then be placed in JMP Z, loop so that it jumps to loop
    // LDA 1
    /////////////////////
    waiting_labels->insert({label, address});

    return -1;
}

ParsedLine parse_line(std::string& line, std::istringstream& iss, uint16_t* address,
    std::map<std::string, uint16_t>* labels, std::map<std::string, uint16_t>* waiting_labels) {

    Instruction instruction;
    ParsedLine parsed_line;
    uint16_t start_address = *address;

    // Get the first token
    std::string token;
    std::string prev_token;

    iss >> token;

    prev_token = token;

    // If the token starts with "LD", we'll deal with loading registers from some value or another register
    if (token.substr(0, 2) == "LD") {
        char load_to = token.back();
        
        iss >> token;
        *address += 1;

        // If next token is a register, then we know the instruction is loading a register from another register
        if (is_register(token)) {
            std::transform(token.begin(), token.end(), token.begin(), ::toupper);
            char load_from = token.back();

            instruction.opcode = ld_registers.at(load_to);
            instruction.operand1 = register_codes.at(load_from);
            *address += 1;

            goto finish;
        }

        // Only the accumulator can load from an address
        if (is_address(token) && load_to == 'A') {
            instruction.opcode = 0x0A;
            
            uint16_t load_address = std::stoul(token.substr(1), nullptr, 16);

            instruction.operand1 = static_cast<uint8_t>(load_address >> 8); // High byte
            instruction.operand2 = static_cast<uint8_t>(load_address & 0xFF); // Low byte

            *address += 1;

            goto finish;
        }

        // Handle loading either a hex or decimal value
        uint8_t load_value = get_value(token);

        instruction.opcode = ld_values.at(load_to);
        instruction.operand1 = load_value;

        *address += 1;

        goto finish;
    }

    // Handle ST(register)
    if (token.substr(0, 2) == "ST") {
        char store_from = token.back();

        iss >> token;
        *address += 1;


        instruction.opcode = st_addresses.at(store_from);
        uint16_t store_address = std::stoul(token.substr(1), nullptr, 16);

        instruction.operand1 = static_cast<uint8_t>(store_address >> 8); // High byte
        instruction.operand2 = static_cast<uint8_t>(store_address & 0xFF); // Low byte

        *address += 1;

        goto finish;
    }

    // Handle ADD (register), (register) or SUB (register), (register)
    if (token.substr(0, 3) == "ADD" || token.substr(0, 3) == "SUB") {
        std::string operation = token.substr(0, 3);

        iss >> token;
        *address += 1;

        if (is_register(token)) {
            uint8_t target_register = register_codes.at(token.back());

            iss >> token;
            *address += 1;

            // Check if we are adding or subtracting from another register
            if (is_register(token)) {
                uint8_t source_register = register_codes.at(token.back());

                instruction.opcode = operation == "ADD" ? 0x0B : 0x0C;
                instruction.operand1 = target_register;
                instruction.operand2 = source_register;
            } else {
                // We are adding or subtracting a value
                uint8_t value = get_value(token);

                instruction.opcode = operation == "ADD" ? 0x0D : 0x0E;
                instruction.operand1 = target_register;
                instruction.operand2 = value;
        
                goto finish;
            }
        }
    }

    // Handle bitwise operations
    if (bitwise_operations.find(token) != bitwise_operations.end()) {
        std::string operation = token.substr(0, 3);

        iss >> token;
        *address += 1;

        // First register
        if (is_register(token)) {
            uint8_t target_register = register_codes.at(token.back());

            instruction.opcode = bitwise_operations.at(operation);
            instruction.operand1 = target_register;

            // If the operation is NOT, we are done
            if (operation == "NOT") {
                goto finish;
            }

            iss >> token;
            *address += 1;

            // Second register
            if (is_register(token)) {
                uint8_t source_register = register_codes.at(token.back());
                instruction.operand2 = source_register;

                goto finish;
            }
        }
    }

    // Handle compare
    if (token == "CP") {
        iss >> token;
        // 2 bytes temporarily. The second operand isnt here, so it is initially seen as 0x15 (value) **0xFF**.
        // This gets automatically removed by the assembler once parsing is complete
        *address += 2;

        instruction.opcode = 0x18;
        instruction.operand1 = get_value(token);
        goto finish;
    }

    /*
    The plan for labels:
    - When jumping, we will calculate the relative offset of the label from the current address.
    - We will store the address of the label in a map, with the label as the key.
    - We might encounter a label is called, but has not yet been defined.
        - If this is the case, we will remember the label for later. Then, when we encounter the label, we will add it to the map
        - Afterwards, we will calculate the relative offset, which is likely to be positive (since the label has a higher address than the call)

    CHANGE OF PLANS:
    - If the relative offset is going to be 16-bits, why not just use the address of the label?
    */

    // Handle labels
    if (token.back() == ':') {
        token.pop_back();
        parsed_line.label = token;
        parsed_line.is_label = true;

        //std::cout << "Label: " << token << " at address: " << *address << std::endl;

        // If the label for some reason is at address 0, we dont want to increment the address
        // (Since a label is not an instruction, so the real instruction under the label will be at address 0)
        *address += (*address == 0 ? 0 : 1);

        // Store the label in the map. Address is the address of NOT THE LABEL, but the address of the instruction under the label.
        // Thats the address we are going to jump to.
        labels->insert({token, *address});

        check_waiting_labels(labels, waiting_labels, token, *address);

        goto finish;
    }

    // Handle jumps
    if (token.substr(0, 3) == "JMP") {

        iss >> token;
        *address += 1;

        if (is_jump_case(token)) {
            uint8_t jump_case = jump_cases.at(token);

            iss >> token;
            *address += 1;

            //uint16_t jump_address = labels->at(token);
            uint16_t jump_address = get_label_address(*address, token, labels, waiting_labels);

            instruction.opcode = jump_case;
            instruction.operand1 = static_cast<uint8_t>(jump_address >> 8); // High byte
            instruction.operand2 = static_cast<uint8_t>(jump_address & 0xFF); // Low byte
        } else {
            // If we're jumping directly to label
            //uint16_t jump_address = labels->at(token);
            uint16_t jump_address = get_label_address(*address, token, labels, waiting_labels);

            instruction.opcode = 0x13;
            instruction.operand1 = static_cast<uint8_t>(jump_address >> 8);
            instruction.operand2 = static_cast<uint8_t>(jump_address & 0xFF);
            *address += 1;
        }

        goto finish;
    }

    if (token == "NOP") {
        instruction.opcode = 0x00;
        goto finish;
    }


finish: // forgive me

    // If operand2 is 0xFF, we need to remove it from the program and subtract 1 for all addresses that are higher than the current address
    // TODO: REALLY IMPORTANT!!! CHECK IF THIS REALLY WORKS AND DOESNT MESS UP THE JUMP ADDRESSES
    /*if (instruction.operand2 == 0xFF) {
        // locate the FF and remove it
        for (uint16_t i = 0; i < program.size(); i++) {
            if (program[i] == 0xFF) {
                program.erase(program.begin() + i);
                break;
            }
        }

        *address -= 1;
    }*/

    if (parsed_line.is_label) {
        // We dont actually wont to save the label as an instruction, so we'll return early
        return parsed_line;
    }

    parsed_line.address = *address;
    parsed_line.size = *address - start_address;
    parsed_line.instruction = instruction;

    // TODO: Check if we need this
    *address += 1; // Increment address for the next instruction

    return parsed_line;
}

int main(int argc, char* argv[]) {
    std::string input_file = argv[1];
    std::string output_file = argv[2];

    if (input_file.empty() || output_file.empty()) {
        std::cerr << "Error: No input file provided" << std::endl;
        return 1;
    }

    std::ifstream file(input_file);

    if (!file) {
        std::cerr << "Error: Could not open file" << std::endl;
        return 1;
    }

    std::string line;
    uint16_t address = 0; // To keep track of the address of the current instruction

    // TODO: In the VM, relative offsets are 8-bits. Check if it should be 16-bits?
    std::map<std::string, uint16_t> labels;
    std::map<std::string, uint16_t> waiting_labels;

    while (std::getline(file, line)) {

        clean_line(&line);
        
        std::istringstream iss(line);

        if (line.empty()) {
            continue;
        }

        ParsedLine parsed_line = parse_line(line, iss, &address, &labels, &waiting_labels);

        Instruction instruction = parsed_line.instruction;

        if (parsed_line.is_label) {
            continue;
        }

        program.push_back(instruction.opcode);
        // Push operands if there are any
        if (instruction.operand1 != Instruction::EMPTY_OPERAND) {
            program.push_back(instruction.operand1);
        }
        if (instruction.operand2 != Instruction::EMPTY_OPERAND) {
            program.push_back(instruction.operand2);
        }
    }

    // Print program (Temporary)
    for (uint8_t byte : program) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
        //std::cout << byte << " ";
    }
    std::cout << std::endl;

    // Print labels
    //for (const auto& label : labels) {
    //    std::cout << label.first << ": " << label.second << std::endl;
    //}

    std::cout << "Program size: " << address << " bytes" << std::endl;

    // Write the program to the output file
    std::ofstream outfile(output_file, std::ios::out | std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(program.data()), program.size());
    outfile.close();

    return 0;
}