#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <climits>
#include <vector>
#include <cstring>
using namespace std;

#define LINESIZE 80
#define MAXLINES 500


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
    int mem_read_cycle;
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

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
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
                inst.dest_reg = trim(rest_of_line.substr(0, c));
                inst.src_reg1 = trim(rest_of_line.substr(open_paren + 1, close_paren - open_paren -1));
                inst.src_reg2 = "";
            }
            else if(inst.type == "STORE"){
                inst.dest_reg = "";
                inst.src_reg1 = trim(rest_of_line.substr(0, c));
                inst.src_reg2 = trim(rest_of_line.substr(open_paren + 1, close_paren - open_paren -1));
            }
        }
        else {
            size_t c1 = rest_of_line.find(',');
            size_t c2 = rest_of_line.find(',', c1 +1);
            if(inst.type == "BRANCH"){
                inst.dest_reg = "";
                inst.src_reg1 = trim(rest_of_line.substr(0, c1));
                inst.src_reg2 = trim(rest_of_line.substr(c1 + 1, c2 - c1 - 1));
            }
            else{
                inst.dest_reg = trim(rest_of_line.substr(0, c1));
                inst.src_reg1 = trim(rest_of_line.substr(c1 +1, c2 - c1 - 1));
                inst.src_reg2 = trim(rest_of_line.substr(c2 +1));
            }
        }

        inst.issue_cycle = -1;
        inst.execute_start_cycle = -1;
        inst.execute_complete_cycle = -1;
        inst.write_back_cycle = -1;
        inst.commit_cycle = -1;
        inst.mem_read_cycle = -1;
        instructions.push_back(inst);
    }
    return instructions;
}

int parse_config(string filename, Config &config){
    ifstream file(filename);
    if(!file.is_open()){
        cerr << "can't open config: " << filename << endl;
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
            if(key == "eff addr") config.eff_addr_stations = int_value; 
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
    public:
    vector<Instruction> instructions;
    int completed_instructions = 0;
        
    int fp_add_latency;
    int fp_sub_latency;
    int fp_mul_latency;
    int fp_div_latency;

    char out[MAXLINES][LINESIZE];
    int lines;
    bool first_output;

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
        true_dep_delays = 0;

        next_instr_issue = 0;
        rob_start = 0;
        rob_end = 0;
        mem_used = false;
        first_output = true;
        committed_this_cycle = false;
    }

    void print_output(){
        printf("Configuration\n");
        printf("-------------\n");
        printf("buffers:\n");
        printf("   eff addr: %d\n", (int)eff_addr_stations.size());
        printf("    fp adds: %d\n", (int)fp_add_stations.size());
        printf("    fp muls: %d\n", (int)fp_mul_stations.size());
        printf("       ints: %d\n", (int)int_stations.size());
        printf("    reorder: %d\n", (int)reorder_buffer.size());
        printf("\n");
        printf("latencies:\n");
        printf("   fp add: %d\n", fp_add_latency);
        printf("   fp sub: %d\n", fp_sub_latency);
        printf("   fp mul: %d\n", fp_mul_latency);
        printf("   fp div: %d\n", fp_div_latency);
        printf("\n\n");
        for(int i = 0; i < lines; i++){
            printf("%s", out[i]);
        }
        printf("\n\n");
        printf("Delays\n");
        printf("------\n");
        printf("reorder buffer delays: %d\n", rb_delays);
        printf("reservation station delays: %d\n", rs_delays);
        printf("data memory conflict delays: %d\n", dmc_delays);
        printf("true dependence delays: %d\n", true_dep_delays);
    }

    void run(){

        while(completed_instructions < instructions.size()){
            cycle++;
            mem_used = false;
            committed_this_cycle = false;

            issue();
            execute();
            mem_read();
            write_back();
            commit();

        }

        print_output();

    }


    private:
    int cycle;
    // once per cycle
    bool committed_this_cycle;
    bool mem_used;
    int rb_delays;
    int rs_delays;
    int dmc_delays;
    int true_dep_delays;
    map<string, int> reorder_status; // register -> ROB entry producing it (-1 if ready)
    // key = when it was issued
    // valid pair<int,int> = <ROB entry, RS type>
    map<int, pair<int,int>> write_back_candidates;

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
        int store_data_dependency;         

    };
    vector<reservation_station_slot> eff_addr_stations;
    vector<reservation_station_slot> fp_add_stations;
    vector<reservation_station_slot> fp_mul_stations;
    vector<reservation_station_slot> int_stations;
    vector<reorder_buffer_entry> reorder_buffer;


    int get_latency(string type) {
        if (type == "FP_ADD") return fp_add_latency;
        if (type == "FP_SUB") return fp_sub_latency;
        if (type == "FP_MUL") return fp_mul_latency;
        if (type == "FP_DIV") return fp_div_latency;
        if (type == "LOAD" || type == "STORE") return 1;  
        if (type == "INT_ADD" || type == "INT_SUB") return 1;
        if (type == "BRANCH") return 1;
        return 1;
    }

    vector<reservation_station_slot> *get_reservation_station(string type){
        if (type == "LOAD" || type == "STORE") return &eff_addr_stations;
        if (type == "FP_ADD" || type == "FP_SUB") return &fp_add_stations;
        if (type == "FP_MUL" || type == "FP_DIV") return &fp_mul_stations;
        if (type == "INT_ADD" || type == "INT_SUB" || type == "BRANCH") return &int_stations;
        return nullptr;
    }
    // on loads need to check for RAW since we don't actually access mem (hard codede addr)
    bool check_mem_dependency(int load_id){
        Instruction &load_inst = instructions[load_id];
        for(int i = 0; i < load_id; i++){
            Instruction &prev_inst = instructions[i];
            if(prev_inst.type == "STORE" && prev_inst.memory_address == load_inst.memory_address){
                if(prev_inst.execute_complete_cycle == -1){
                    return true;
                }
                if(prev_inst.commit_cycle == -1){
                    return true;
                }
            }
        }
        return false;
    }
    void issue(){

        if(next_instr_issue >= instructions.size()) return; 

        Instruction &inst = instructions[next_instr_issue];


        if(reorder_buffer[rob_end].busy && rob_start == rob_end){
            //commit ROB and issue in same cycle!!
            commit();
            if(reorder_buffer[rob_end].busy && rob_start == rob_end){
                rb_delays++;  
                return;

            }
        }


        //Make sure ROB is not full
        if(reorder_buffer[rob_end].busy && rob_start == rob_end){
            rb_delays++;
            return;
        }
      
        vector<reservation_station_slot> *rs = get_reservation_station(inst.type);
        if(rs == nullptr){
            cerr << "Unknown instruction type should not be null!! " << inst.type << endl;
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
        rob_entry.store_data_dependency = -1;

        // set reservation station slot
        reservation_station_slot &rs_slot = (*rs)[free_rs_index];
        rs_slot.busy = true;
        rs_slot.instruction_id = next_instr_issue;
        rs_slot.dest_rob_entry = rob_end;
        rs_slot.executing = false;
        rs_slot.remaining_cycles = get_latency(inst.type);


        // set status of source operands

        if(!inst.src_reg1.empty()){
            if(reorder_status.count(inst.src_reg1) > 0 && reorder_status[inst.src_reg1] != -1){
                int res_ind = reorder_status[inst.src_reg1];
                // The dependency may already be cleared if it went through WB and not commited yet (reoder_status only track commit status)
                if(!reorder_buffer[res_ind].busy || reorder_buffer[res_ind].ready){
                    rs_slot.operand1 = -1;
                    if(inst.type == "STORE") rob_entry.store_data_dependency = -1;
                }
                else{
                    rs_slot.operand1 = res_ind;
                    if(inst.type == "STORE") rob_entry.store_data_dependency = res_ind;
                }
            }
            else {
                rs_slot.operand1 = -1; 
                if (inst.type == "STORE") rob_entry.store_data_dependency = -1;
            }

        }
        else rs_slot.operand1 = -1;

        if(!inst.src_reg2.empty()){
            if(reorder_status.count(inst.src_reg2) > 0 && reorder_status[inst.src_reg2] != -1){
                int res_ind = reorder_status[inst.src_reg2];
                if(!reorder_buffer[res_ind].busy || reorder_buffer[res_ind].ready){
                    rs_slot.operand2 = -1;
                }
                else{
                    rs_slot.operand2 = res_ind;
                }
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
        inst.issue_cycle = cycle;

    }

    void exec_helper(vector<reservation_station_slot> &rs_pool){

        // continue executing items already executing given RS
        for(auto &rs : rs_pool){
            if(!rs.busy) continue;
            Instruction &inst = instructions[rs.instruction_id];
            if(rs.executing){
                rs.remaining_cycles--;
                if(rs.remaining_cycles == 0){
                    inst.execute_complete_cycle = cycle;
                    rs.executing = false;
                    if(inst.type == "STORE" || inst.type == "BRANCH"){
                        reorder_buffer[rs.dest_rob_entry].ready = true;
                    }
                    if(inst.type != "LOAD")rs.busy = false;
                }
                continue;
            }
            if(inst.issue_cycle == cycle || inst.execute_complete_cycle != -1) continue;
        
            
            // True dependnency check
            if(inst.type == "STORE"){
                if(rs.operand2 != -1){
                    true_dep_delays++;
                    continue;
                }
            }
            // Another true dependency check
            else if(rs.operand1 != -1 || rs.operand2 != -1){
                true_dep_delays++;
                continue;
            }
            

            inst.execute_start_cycle = cycle;
            int latency = get_latency(inst.type);
            // do work for newly starting execution
            if(latency == 1){
                // Complete immediately
                inst.execute_complete_cycle = cycle;
                rs.executing = false;  // Not executing (done!)
                rs.remaining_cycles = 0;
                if(inst.type == "STORE" || inst.type == "BRANCH"){
                    reorder_buffer[rs.dest_rob_entry].ready = true;
                }
                if(inst.type != "LOAD") rs.busy = false; 

            } else {
                // Multi-cycle operation
                rs.executing = true;
                rs.remaining_cycles = latency - 1;
            }
         
        }
    }
    void execute(){
        exec_helper(eff_addr_stations);
        exec_helper(fp_add_stations);
        exec_helper(fp_mul_stations);
        exec_helper(int_stations);
    }

    void mem_read(){
        //If a store is ready to commit this cycle, block all loads
        // Store only blocks if it is able to commit this cycle!!!!!
        bool blocking_store = false;
        if ((rob_start != rob_end || reorder_buffer[rob_start].busy) && !committed_this_cycle && !mem_used){
            reorder_buffer_entry &head = reorder_buffer[rob_start];
            if(head.busy && head.ready){
                Instruction &head_inst = instructions[head.instruction_id];
                if(head_inst.type == "STORE" &&
                   head.store_data_dependency == -1 &&
                   head_inst.execute_complete_cycle != cycle &&
                   head_inst.mem_read_cycle != cycle &&
                   head_inst.write_back_cycle != cycle){
                    blocking_store = true;
                }
            }
        }

        // Go through ROB and find loads that can read (1 read per cycle so lots of continues)
        for(int i = 0; i < reorder_buffer.size(); i++){
            int rob_index = (rob_start + i) % reorder_buffer.size();
            if(rob_index == rob_end && !reorder_buffer[rob_index].busy) break;
            
            if(!reorder_buffer[rob_index].busy) continue;

            Instruction &inst = instructions[reorder_buffer[rob_index].instruction_id];

            if(inst.type != "LOAD" || inst.execute_complete_cycle == -1 || inst.mem_read_cycle != -1 || inst.execute_complete_cycle == cycle) continue;

            if(blocking_store){
                dmc_delays++;
                continue;
            }
            if(mem_used){
                dmc_delays++;
                return;
            }

            if(check_mem_dependency(reorder_buffer[rob_index].instruction_id)){
                true_dep_delays++;
                continue; 
            }

            

            inst.mem_read_cycle = cycle;
            mem_used = true;


            //Free loads reservation station since for some reason load is the only RS that doesn't get freed in execute
            for(auto &rs : eff_addr_stations){
                if(rs.busy && rs.instruction_id == reorder_buffer[rob_index].instruction_id){
                    rs.busy = false;
                }
            }
            return;
        }


    }
    
    void write_back(){  
     
        int earliest_cycle = INT_MAX;
        int earliest_ind = -1;

        //iterate through the reorder buffer because the resrvation stations can be cleared after execution
        for(int i = 0; i < reorder_buffer.size(); i++){
            int rob_index = (rob_start + i) % reorder_buffer.size();
            if(rob_index == rob_end && !reorder_buffer[rob_index].busy) break;
            
            if(!reorder_buffer[rob_index].busy) continue;

            Instruction &inst = instructions[reorder_buffer[rob_index].instruction_id];

            if(inst.type == "STORE" || inst.type == "BRANCH" || inst.write_back_cycle != -1) continue;
            
            bool can_wb = false;
            if(inst.type == "LOAD"){
                can_wb = (inst.mem_read_cycle != -1 && inst.mem_read_cycle != cycle);
            }
            else {
                can_wb = (inst.execute_complete_cycle != -1 && inst.execute_complete_cycle != cycle);
            }

            if(can_wb && inst.issue_cycle < earliest_cycle){
                earliest_cycle = inst.issue_cycle;
                earliest_ind = rob_index;
            }

        }
        //The earliest instruciton takes priority
        if(earliest_ind == -1) return;
        Instruction &earliest = instructions[reorder_buffer[earliest_ind].instruction_id];
        earliest.write_back_cycle = cycle;
        reorder_buffer[earliest_ind].ready = true;

        // update dependencies (same as commit)
        auto update_deps = [&](vector<reservation_station_slot>& rs_pool){
            for(auto &slot : rs_pool){
                if(slot.busy){
                    if(slot.operand1 == earliest_ind) slot.operand1 = -1;
                    if(slot.operand2 == earliest_ind) slot.operand2 = -1; 
                }
            }
        };

        for(auto &entry : reorder_buffer){
            if(entry.busy && entry.store_data_dependency == earliest_ind){
                entry.store_data_dependency = -1;
            }
        }

        update_deps(eff_addr_stations);
        update_deps(fp_add_stations);
        update_deps(fp_mul_stations);
        update_deps(int_stations);
    }


    void commit(){
        if(committed_this_cycle) return;
        if(rob_start == rob_end && !reorder_buffer[rob_start].busy){
            return;
        } 

        //must be ready to commit in the ROB
        reorder_buffer_entry &rob_entry = reorder_buffer[rob_start];
        if(!rob_entry.busy || !rob_entry.ready) return;

        Instruction &inst = instructions[rob_entry.instruction_id];
        if(inst.mem_read_cycle == cycle || inst.write_back_cycle == cycle) return;

        if(inst.type == "STORE"){
            if(rob_entry.store_data_dependency != -1){
                int dep = rob_entry.store_data_dependency;
                // if dep is read in ROB than we can clear it 
                if (!reorder_buffer[dep].busy || reorder_buffer[dep].ready){
                    rob_entry.store_data_dependency = -1;
                }
                else{
                    true_dep_delays++;
                    return;
                }
            }

            if(mem_used){
                dmc_delays++;
                return;
            }
            mem_used = true;
        }


        if(inst.execute_complete_cycle == cycle || inst.mem_read_cycle == cycle || inst.write_back_cycle == cycle) return;  

        inst.commit_cycle = cycle;

        // update corresponding deps in reservation stations 

        auto update_deps = [&](vector<reservation_station_slot>& rs_pool){
            for(auto &slot : rs_pool){
                if(slot.busy){
                    if(slot.operand1 == rob_start) slot.operand1 = -1;
                    if(slot.operand2 == rob_start) slot.operand2 = -1;
                }
            }
        };

        update_deps(eff_addr_stations);
        update_deps(fp_add_stations);
        update_deps(fp_mul_stations);
        update_deps(int_stations);

        // indipendently update the store dep flag
        for(auto &entry : reorder_buffer){
            if(entry.busy && entry.store_data_dependency == rob_start){
                entry.store_data_dependency = -1;
            }
        }




        if(first_output){
            lines = 0;
            sprintf(out[lines++],
                    "                    Pipeline Simulation\n");
            sprintf(out[lines++],
                    "-----------------------------------------------------------\n");
            sprintf(out[lines++],
                    "                                      Memory Writes\n");
            sprintf(out[lines++],
                    "     Instruction      Issues Executes  Read  Result Commits\n");
            sprintf(out[lines++],
                    "--------------------- ------ -------- ------ ------ -------\n");
            first_output = false;
        }
        char line_buf[LINESIZE];
        sprintf(out[lines], "%-21s %6d %3d -%3d ",
                inst.og_line.c_str(),
                inst.issue_cycle,
                inst.execute_start_cycle,
                inst.execute_complete_cycle);
        
        if(inst.mem_read_cycle == -1){
            sprintf(line_buf, "       ");
        } else {
            sprintf(line_buf, "%6d ", inst.mem_read_cycle);
        }
        strcat(out[lines], line_buf);
        
        if(inst.type == "STORE" || inst.type == "BRANCH"){
            sprintf(line_buf, "       ");
        } else {
            sprintf(line_buf, "%6d ", inst.write_back_cycle);
        }
        strcat(out[lines], line_buf);
        
        sprintf(line_buf, "%7d\n", cycle);
        strcat(out[lines++], line_buf);


        if(!inst.dest_reg.empty()){
            if(reorder_status.count(inst.dest_reg) > 0 && reorder_status[inst.dest_reg] == rob_start){
                reorder_status[inst.dest_reg] = -1;
            }
        }

        rob_start = (rob_start + 1) % reorder_buffer.size();
        completed_instructions++;
        rob_entry.busy = false;
        committed_this_cycle = true;
    }

};




int main(){
    Config config;
    if(parse_config("config.txt", config) != 0) {
        cerr << "could not parse config file" << endl;
        return 1;
    }
    vector<Instruction> instructions = parse_instructions();
    Simulator simulator(config, instructions);
    simulator.run();

    return 0;
}