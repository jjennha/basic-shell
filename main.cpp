#include <iostream>
#include <vector>
#include <zconf.h>
#include <sstream>
#include <cstring>
#include <wait.h>
#include <jmorecfg.h>
#include <fstream>
#include <fcntl.h>

using namespace std;
int running = 1;

/**
 * Process class - used solely to handle background commands
 */
class Process {

    public:
        /*id*/
        pid_t pid;
        /*command*/
        string command;
};


/**
 * delete whitespace on input ends
 *
 * @param input
 * @return
 */
string trim_ends(string input){
    int start = input.find_first_not_of(' ');
    int end = input.find_last_not_of(' ');
    int range = end - start + 1;
    input = input.substr(start, range);
    return input;
}

/**
 * translates input command to a real linux command
 *
 * @param s
 * @return command
 * */
string make_readable(string s){
    string result;
    string buf;
    stringstream ss(s);

    vector<std::string> tokens;
    while (ss >> buf)
        if(strcmp(buf.c_str(),"help")==0){
            result+= "man more ";
        }else if(strcmp(buf.c_str(),"environ")==0){
            result+= "printenv ";
        }else if(strcmp(buf.c_str(),"clr")==0){
            result+= "clear ";
        }else {
            result+= buf+" ";
        }
    return trim_ends(result);
}
/**
 * Read in a file
 *
 * @param f
 * @param arg_commands
 */
void read_file(char* f, vector<string>* arg_commands){
    string line;
    ifstream file;
    file.open(f);
    while (getline(file, line))
    {
        arg_commands->push_back(line);
    }
}

/**
 * get the current directory
 * @return
 */
char* get_cur_dir(){
    return getcwd((char *) malloc(256), 256);
}

/**
 * changes directory based on argument in input
 *
 * @param input
 * */
void change_dir(vector<string> input){
    if(input.size() < 2){
        cerr << "error: no directory specified. Current directory: " << get_cur_dir() << "\n";
        return;
    }else if(input.size() > 2){
        cerr << "error: too many arguments\n";
        return;
    }

    string dir = input[1];
    int ret = chdir(dir.c_str());

    if(ret < 0){
        cerr << "error: directory does not exist\n";
    }
}

/**
 * parse the input in a way that is readable to execvp command
 *
 * @param input
 * @param del
 * @return parsed inputs
 * */
vector<string> parse_input(string input, char del) {
    vector<string> parsed_inputs;
    if(input.size() <= 0) {return parsed_inputs;}

    input = trim_ends(input);
    string cmd = make_readable(input);

    istringstream ss(cmd);
    string token;
    while(getline(ss, token, del)) {
        parsed_inputs.push_back(token);
    }
    return parsed_inputs;
}

/**
 * executes input command
 * @param input
 * @param file_in
 * @param file_out
 * @param append
 */
void execute(vector<string> input,string file_in,string file_out,boolean append){
    int fd1, fd0;
    if(file_in.size() > 0){
        if((fd1 = open(file_in.c_str(), O_RDONLY)) < 0){
            cout << "error: unable to open file\n";
            exit(0);
        }
        dup2(fd1, 0);
        close(fd1);
    }

    if(file_out.size() > 0){
        if(append){
            fd0=open(file_out.c_str(), O_RDWR | O_CREAT | O_APPEND,  S_IRUSR | S_IWUSR); // some of these flags may not be necessary.  I had to look up the flags and an example came up with these.
        }else{
            fd0 = open(file_out.c_str(), O_RDWR | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR);
        }
        if(fd0 < 0){
            cout << "error: unable to open file\n";
            exit(0);
        }
        dup2(fd0, 1);
        close(fd0);
    }
    vector<char *> input_formatted;
    int size = input.size();
    //string vector to char* vector conversion
    for (int i = 0; i < size; i++) {
        input_formatted.push_back((char *) (input[i].c_str()));
    }

    input_formatted.push_back(NULL);
    execvp(input[0].c_str(), &input_formatted[0]);
}
/**
 * receives input command and creates child process to execute command in
 * @param b_task
 * @param processes
 * @param command
 * @param input
 * @param file_in
 * @param file_out
 * @param append
 */
void execute_cmd(boolean b_task, vector<Process>* processes,string command, vector<string> input,string& file_in,string& file_out,boolean append) {
    Process* p = new Process();
    pid_t child_pid = fork();
    if(child_pid<0){
        cerr<< "error: Process creation failed";
        return;
    }else if(child_pid==0){

        execute(input,file_in,file_out,append);
        cerr << "unknown command\n";
        exit(0);
    }
    if(b_task){
        p->pid = child_pid;
        p->command = command;
        processes->push_back(*p);
    }else{
        waitpid(child_pid,NULL,0);
    }
}

/**
 * Executes command in piping sequence
 * @param input
 * @param in
 * @param out
 * */
void execute_piped_cmd(vector<string> input, int in, int out){
    pid_t child_pid = fork();

    if(child_pid==0){
        if(in!=0){
            dup2(in,0);
            close(in);
        }
        if(out!=0){
            dup2(out,1);
            close(out);
        }
        execute(input, "","", false);
        cerr << "unknown command\n";
        exit(0);
    }
}

/**
 * Takes each command in the sequence of commands and processes them individually
 * @param command
 * @param input
 * @param b_task
 * @param processes
 */
void execute_pipe_cmd(string command,vector<string> input, boolean b_task, vector<Process>* processes){
    Process* p = new Process();
    pid_t child_pid = fork();

    if(child_pid == 0){
        int p[2];
        int in = 0;
        int c = 0;
        for(string inp : input ){
            pipe(p);
            execute_piped_cmd(parse_input(inp, ' '), in, p[1]);
            close(p[1]);
            in = p[0];
            ++c;
        }

        if (in != 0) {
            dup2(in, 0);
        }
        execute(parse_input(input[c-1], ' '),  "", "", false);
    }
    if(b_task){
        p->pid = child_pid;
        p->command = command;
        processes->push_back(*p);
    }else{
        waitpid(child_pid, NULL, 0);
    }

}

/**
 * execute commands. if multiple, handle recursively to handle nested commands
 * @param command
 * @param processes
 */
void rec_cmd_exec(string command, vector<Process>* processes){
    vector<string> input_parsed = parse_input(command, ';');

    if(input_parsed.size()>1){ // if multiple, execute separately and recursively
        for(string inp: input_parsed){
            rec_cmd_exec(inp, processes);
        }
        return;
    }
    boolean b_task = (command.back()=='&');
    if(b_task){
        command = command.substr(0, command.find_last_not_of('&'));
        command = trim_ends(command);
    }

    input_parsed = parse_input(command, '|');

    if(input_parsed.size()>1){
        execute_pipe_cmd(command,input_parsed,b_task,processes);
    }else if(input_parsed.size()>0){
        input_parsed = parse_input(command, ' ');
        int pos_inp = command.find("<");
        boolean append = false;
        string file_in,file_out;

        //finding in and out files if any specified
        for (int i = 0; i < input_parsed.size(); i++) {
            if ((input_parsed[i].compare(">") == 0)) {
                file_out = input_parsed[i + 1];
                input_parsed.erase(input_parsed.begin() + i);
                input_parsed.erase(input_parsed.begin() + i);
                i -= 1;
            }
            else if(input_parsed[i].compare(">>") == 0){
                append = true;
                file_out = input_parsed[i + 1];
                input_parsed.erase(input_parsed.begin() + i);
                input_parsed.erase(input_parsed.begin() + i);
                i -= 1;
            }
        }
        if(pos_inp >= 0){
            cout << pos_inp << "\n";
            file_in = trim_ends(parse_input(command, '<')[1]);
            input_parsed.erase(input_parsed.begin() + pos_inp + 1);
            input_parsed.erase(input_parsed.begin() + pos_inp);
        }


        if(input_parsed.size()>0) {
            if (strcmp(input_parsed[0].c_str(), "quit") == 0) {
                if(processes->size() == 0){
                    cout << "exiting...\n";
                    running = 0;
                }


            }else if(strcmp(input_parsed[0].c_str(), "pause") == 0) {
                while(1) {
                    cout << "[enter] to resume";
                    if(cin.get() == '\n') {
                        break;
                    }
                }
            }else if (strcmp(input_parsed[0].c_str(), "cd") == 0) {
                change_dir(input_parsed);
            } else {
                execute_cmd(b_task,processes,command,input_parsed,file_in,file_out,append);
            }
        }
    }
    return;
}

/**
 * print status of background processes.  I tried to make a similar output as the original shell although I'm not sure if we needed it
 *
 * @param processes
 */
void print_background(vector<Process>* processes){
    for(Process p: *processes){
        if(waitpid(p.pid, NULL, WNOHANG) == 0){
            cout << "[" << p.pid << "] is running\n";
        }else{
            cout << "[" << p.pid << "] finished" << "\t\t" << p.command <<"\n";
        }
    }
    int i = 0;
    for(Process p: *processes){
        if(waitpid(p.pid, NULL, WNOHANG) != 0){
            processes->erase(processes->begin()+i);
        }
        i++;
    }
    return;
}

int main(int argc, char** argv) {
    // set environment variables as per PDF's request
    char* path = get_cur_dir();
    string path_str(path);

    path_str += "/myshell";
    setenv("shell",path_str.c_str(),1);
    string parent = "parent";
    putenv(&parent[0]);
    setenv("parent",path_str.c_str(),1);

    boolean read_from_file = false;
    vector<string> arg_commands;
    vector<Process> processes;

    // if batch file specified, use it
    if(argv[1]){
        read_from_file = true;
        read_file(argv[1],&arg_commands);
    }

    while(running || processes.size() > 0){
        path = get_cur_dir();
        if(!read_from_file){
            cout << path << "$ ";
        }
        string command; //get command from file if one specified, or from user input
        if(read_from_file){
            if(arg_commands.empty()){ // if no more commands, exit
                command = "quit";
            }else {
                command = arg_commands.at(0);
                arg_commands.erase(arg_commands.begin());
            }
        }else{
            getline(cin, command);
        }

        rec_cmd_exec(command, &processes);
        print_background(&processes);
    }
}
