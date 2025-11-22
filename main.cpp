#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
using namespace std;

struct Config {
    int eff_addr_stations;
    int fp_add_stations;
    int fp_mul_stations;
    int int_stations;
    int reorder_buffer_size;
    
    int fp_add_latency;
    int fp_sub_latency;
    int fp_mul_latency;
    int fp_div_latency;
};

struct Instruction {
    string og_line;
    string type;
    string opcode;
    string dest_reg;
    string src_reg1;
    string src_reg2;
    int memory_address;

    int issue_cycle;
    int execute_start_cycle;
    int execute_complete_cycle;
    int write_back_cycle;
    int commit_cycle;

};

string get_instruction_type(const string& opcode) {
    if (opcode == "lw" || opcode == "flw") return "LOAD";
    if (opcode == "sw" || opcode == "fsw") return "STORE";
    if (opcode == "fadd.s") return "FP_ADD";
    if (opcode == "fsub.s") return "FP_SUB";
    if (opcode == "fmul.s") return "FP_MUL";
    if (opcode == "fdiv.s") return "FP_DIV";
    if (opcode == "add") return "INT_ADD";
    if (opcode == "sub") return "INT_SUB";
    if (opcode == "beq" || opcode == "bne") return "BRANCH";
    return "UNKNOWN";
}

vector<Instruction> parse_instructions(){
    string line;
    vector<Instruction> instructions;
    while(getline(cin, line)){
        Instruction inst; 
        inst.og_line = line;

        istringstream iss(line);
        iss >> inst.opcode;

        inst.type = get_instruction_type(inst.opcode);

        string rest_of_line;
        getline(iss, rest_of_line);
        if(rest_of_line.find('(') != string::npos){
            size_t c = rest_of_line.find(',');
            size_t open_paren = rest_of_line.find('(');
            size_t close_paren = rest_of_line.find(')');
            size_t colon = rest_of_line.find(':');
            inst.memory_address = stoi(rest_of_line.substr(colon +1));

            if(inst.type == "LOAD"){
                inst.dest_reg = rest_of_line.substr(0, c);
                inst.src_reg1 = rest_of_line.substr(open_paren + 1, close_paren - open_paren -1);
                inst.src_reg2 = "";
            }
            else if(inst.type == "STORE"){
                inst.dest_reg = "";
                inst.src_reg1 = rest_of_line.substr(0, c);
                inst.src_reg2 = rest_of_line.substr(open_paren + 1, close_paren - open_paren -1);
            }
        }
        else {
            size_t c1 = rest_of_line.find(',');
            size_t c2 = rest_of_line.find(',', c1 +1);
            inst.dest_reg = rest_of_line.substr(0, c1);
            inst.src_reg1 = rest_of_line.substr(c1 +1, c2 - c1 - 1);
            inst.src_reg2 = rest_of_line.substr(c2 +1);
        }

        instructions.push_back(inst);
    }
    return instructions;
}
int parse_config(string filename, Config &config){
    ifstream file(filename);
    if(!file.is_open()){
        cerr << "Error opening config file: " << filename << endl;
        return -1;
    }
    string line; 
    string section;

    while(getline(file, line)){
        size_t start = line.find_first_not_of(" \t\r\n");
        if(start == string::npos ) continue; 
        
        if(line == "buffers"){
            section = "buffers";
            continue;
        }
        else if(line == "latencies"){
            section = "latencies"; 
            continue;
        }

        size_t colon = line.find(':');
        string key = line.substr(0, colon);
        string value = line.substr(colon + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        int int_value = stoi(value);
        if(section == "buffers"){
            if(key == "eff addr") config.eff_addr_stations = int_value;  // Note: "eff addr" not "eff_addr_stations"
            else if(key == "fp adds") config.fp_add_stations = int_value;
            else if(key == "fp muls") config.fp_mul_stations = int_value;
            else if(key == "ints") config.int_stations = int_value;
            else if(key == "reorder") config.reorder_buffer_size = int_value;
        }
        else if(section == "latencies"){
            if(key == "fp_add") config.fp_add_latency = int_value;
            else if(key == "fp_sub") config.fp_sub_latency = int_value;
            else if(key == "fp_mul") config.fp_mul_latency = int_value;
            else if(key == "fp_div") config.fp_div_latency = int_value;
        }
    }
    file.close();
    return 0;
}

class Simulator {
    
    vector<Instruction> instructions;
    int completed_instructions = 0;
        
    int fp_add_latency;
    int fp_sub_latency;
    int fp_mul_latency;
    int fp_div_latency;

    Simulator(const Config &config, const vector<Instruction> &instrs) {
        eff_addr_stations.resize(config.eff_addr_stations);
        fp_add_stations.resize(config.fp_add_stations);
        fp_mul_stations.resize(config.fp_mul_stations);
        int_stations.resize(config.int_stations);
        reorder_buffer.resize(config.reorder_buffer_size);

        instructions = instrs;

        fp_add_latency = config.fp_add_latency;
        fp_sub_latency = config.fp_sub_latency;
        fp_mul_latency = config.fp_mul_latency;
        fp_div_latency = config.fp_div_latency;
        cycle = 0;

        rb_delays = 0;
        rs_delays = 0;
        dmc_delays = 0;

        next_instr_issue = 0;
        rob_start = 0;
        rob_end = 0;
    }

    private:
    int cycle;
    int rb_delays;
    int rs_delays;
    int dmc_delays;
    map<string, int> reorder_status; // register -> ROB entry producing it (-1 if ready)

    int next_instr_issue;
    int rob_start;
    int rob_end;

    struct reservation_station_slot{
        bool busy;
        int instruction_id; //index in instructions vector
        
        //dependency tracking 
        //-1 if ready
        int operand1;
        int operand2;

        int dest_rob_entry;
        int remaining_cycles;
        bool executing;
    };
    struct reorder_buffer_entry{
        bool busy;
        int instruction_id;      //index in instructions vector
        string destination_register;
        bool ready;             

    };
    vector<reservation_station_slot> eff_addr_stations;
    vector<reservation_station_slot> fp_add_stations;
    vector<reservation_station_slot> fp_mul_stations;
    vector<reservation_station_slot> int_stations;
    vector<reorder_buffer_entry> reorder_buffer;

    vector<reservation_station_slot> *get_reservation_station(string type){
        if (type == "LOAD" || type == "STORE") return &eff_addr_stations;
        if (type == "FP_ADD" || type == "FP_SUB") return &fp_add_stations;
        if (type == "FP_MUL" || type == "FP_DIV") return &fp_mul_stations;
        if (type == "INT_ADD" || type == "INT_SUB" || type == "BRANCH") return &int_stations;
        return nullptr;
    }

    void issue(){
        if(next_instr_issue >= instructions.size()) return; 

        Instruction &inst = instructions[next_instr_issue];

        // make sure ROB is not full
        int buff_size = (rob_end - rob_start + reorder_buffer.size()) % reorder_buffer.size();
        if(buff_size >= reorder_buffer.size() - 1){
            rb_delays++;
            return;
        }

        // check reservation stations based on instruction type
        vector<reservation_station_slot> *rs = get_reservation_station(inst.type);
        if(rs == nullptr){
            cerr << "Unknown instruction type: " << inst.type << endl;
            return;
        }

        // find free slot in reservation station 
        int free_rs_index = -1;
        for(int i = 0; i < (*rs).size(); i++){
            if(!(*rs)[i].busy){
                free_rs_index = i;
                break;
            }
        }
        if(free_rs_index == -1){
            rs_delays++;
            return;
        }

        //set the ROB entry 
        reorder_buffer_entry &rob_entry = reorder_buffer[rob_end];
        rob_entry.busy = true;
        rob_entry.instruction_id = next_instr_issue;
        rob_entry.destination_register = inst.dest_reg;
        rob_entry.ready = false;

        reservation_station_slot &rs_slot = (*rs)[free_rs_index];
        rs_slot.busy = true;
        rs_slot.instruction_id = next_instr_issue;
        rs_slot.dest_rob_entry = rob_end;
        rs_slot.executing = false;

        if(!inst.src_reg1.empty()){
            if(reorder_status.count(inst.src_reg1) > 0 && reorder_status[inst.src_reg1] != -1){
                rs_slot.operand1 = reorder_status[inst.src_reg1];
            }
            else {
                rs_slot.operand1 = -1; 
            }

        }
        else rs_slot.operand1 = -1;

        if(!inst.src_reg2.empty()){
            if(reorder_status.count(inst.src_reg2) > 0 && reorder_status[inst.src_reg2] != -1){
                rs_slot.operand2 = reorder_status[inst.src_reg2];
            }
            else {
                rs_slot.operand2 = -1; 
            }
        }
        else rs_slot.operand2 = -1;

        if(!inst.dest_reg.empty()){
            reorder_status[inst.dest_reg] = rob_end;
        }
        rob_end = (rob_end + 1) % reorder_buffer.size();
        next_instr_issue++;

    }

    void exec_helper(vector<reservation_station_slot> &rs_pool){
        for(auto &rs : rs_pool){
            if(!rs.busy) continue;
            
        }
    }
    void execute(){



    }
    void mem_read(){

    }
    void write_back(){

    }
    void commit(){
        
    }

    void run(){

        while(completed_instructions < instructions.size()){
            
        }

    }

};




int main(){
    Config config;
    if(parse_config("config.txt", config) != 0) {
        cerr << "Failed to parse config file." << endl;
        return 1;
    }
    vector<Instruction> instructions = parse_instructions();

    return 0;
}