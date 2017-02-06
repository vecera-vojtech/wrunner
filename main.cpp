/*
 * Wrathion runner for BOINC client for dictionary-based version
 *
 * Copyright (C) 2016 Radek Hranicky
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <dirent.h> 
#include <unistd.h>
#include <fstream>
#include <string>

#ifdef __WIN32
#include <windows.h>
#else
#include <unistd.h>    /* for fork */
#include <sys/types.h> /* for pid_t */
#include <sys/wait.h>  /* for wait */
#include <sys/stat.h>
#include <zip.h>
#endif

#include "boinc_api.h"
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#include "resolver.h"
#include "parser.h"
#include "main.h"
#include "control.h"
#include "socket.h"
#include "standalone.h"

using namespace std;
using std::string;
using boost::asio::ip::tcp;

bool boinc = false;

string find_exec_file(string path) {
    //cerr << "main.cpp " << __LINE__ << " find_exec_file(begin) -" << endl;
    string fitcrackerPath = "fitcrack0";
    string file;
    DIR *dir = opendir(path.c_str()); 
    if(dir)
    {
        struct dirent *ent; 
        while((ent = readdir(dir)) != NULL) 
        {
            file = ent->d_name;
            if(file != "fitcrack" && file.find("fitcrack") != std::string::npos) {
                if(boost::lexical_cast<int>(file.substr(8, file.length() - 8)) - \
                    boost::lexical_cast<int>(fitcrackerPath.substr(8, fitcrackerPath.length() - 8)) > 0) {
                    fitcrackerPath = file;
                }
            }
        } 
    }
    //cerr << "main.cpp " << __LINE__ << " find_exec_file() -" << fitcrackerPath << endl;
    return fitcrackerPath;
}

void print_secondary_process_params(char ** args) {
    int i = 0;
    cerr << "./";
    while(args[i] != NULL) {
        cerr << args[i] << " ";
        i++;
    }
    cerr << endl;
}

static void safe_create_dir(const char *dir)
{
    if (mkdir(dir, 0755) < 0) {
        if (errno != EEXIST) {
            perror(dir);
            exit(1);
        }
    }
}

int zip_extract(char *archive)
{
    struct zip *za;
    struct zip_file *zf;
    struct zip_stat sb;
    char buf[100];
    int err;
    int i, len;
    int fd;
    long long sum;
    
    if ((za = zip_open(archive, 0, &err)) == NULL) {
        return 1;
    }
    
    char * pch;
    char directory[256];
    int index;
        
    for (i = 0; i < zip_get_num_entries(za, 0); i++) {
        if (zip_stat_index(za, i, 0, &sb) == 0) {
            
            pch = strchr((char *)sb.name, '/');
            while(pch != NULL)
            {
                index = pch - sb.name + 1;
                memcpy( directory, sb.name, index);
                directory[index] = '\0';
                safe_create_dir(directory);
                pch=strchr(pch + 1, '/');
            }
            
            len = strlen(sb.name);
            if (sb.name[len - 1] == '/') {
                safe_create_dir(sb.name);
            } else {
                zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    return 1;
                }
                
                fd = open(sb.name, O_RDWR | O_TRUNC | O_CREAT, 0644);
                if (fd < 0) {
                    return 1;
                }
                
                sum = 0;
                while (sum != sb.size) {
                    len = zip_fread(zf, buf, 100);
                    if (len < 0) {
                        return 1;
                    }
                    write(fd, buf, len);
                    sum += len;
                }
                close(fd);
                zip_fclose(zf);
            }
        }
    }   
 
    if (zip_close(za) == -1) {
        return 1;
    }
 
    return 0;
}

inline bool file_exists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

int main(int argc, char **argv) {
    int retval;
    char buf[256];
    MFILE out;
    std::string password = "";
	string xmlFile;
    
    params_init();
    standalone(argc, argv, &xmlFile);
    if(task_params.mode == 'u') {
		boinc = true;
	}
    
	if(boinc) {
		retval = boinc_init();
        //cerr << "main.cpp " << __LINE__ << " boinc_init()" << endl;
        zip_extract("kernels6.zip");
	}
    if (retval) {
        fprintf(stderr, "%s boinc_init returned %d\n",
            boinc_msg_prefix(buf, sizeof(buf)), retval
        );
        exit(retval);
    }
    
    //cerr << "main.cpp " << __LINE__ << endl;
    if(boinc) {
		//cerr << "main.cpp " << __LINE__ << endl;
		getTaskParams(resolveInputFile("in"));
		//cerr << "main.cpp " << __LINE__ << endl;
		xmlFile = resolveInputFile("data");
	}
    
    cerr << "Runner is running with parameters:" << endl;
    cerr << "mode - " << task_params.mode << endl;
    cerr << "from - " << task_params.from << endl;
    cerr << "to - " << task_params.to << endl;
    cerr << "password - " << task_params.password << endl;
    cerr << "simulation - " << task_params.simulation << endl;
    cerr << "charset - " << task_params.charset << endl;
    cerr << "length - " << task_params.length << endl << endl;
    
    if(task_params.mode == 'u') {
        printf("./wrunner\n");
        printf("1. (benchmark) -m b -c lower_lattin.txt -x test.xml\n");
        printf("2. (normal) -m n -c lower_lattin.txt -f 1000 -t 2000 -x test.xml -s 1\n");
        printf("3. (verify) -m v -c lower_lattin.txt -p heslo-base64 -x test.xml\n");
        return 0;
    }
    
    //cerr << "main.cpp " << __LINE__ << endl;
    Server s(gio_service);
    //cerr << "main.cpp " << __LINE__ << endl;
    cerr << "Port " << s.listeningPort << endl;
    
    /* ****** Run Wrathion ****** */
    string fitcrackerPath = find_exec_file(".");
    chmod(fitcrackerPath.c_str(), S_IRWXU); //S_IRGRP|S_IXGRP|S_IROTH
    
    
#ifdef __WIN32
    cerr << "Windows sucks" << endl;
    /*
    std::string wrathion_win = "fitcracker.exe -m BOINC";
    wrathion_win += " -i ";
    wrathion_win += xmlFile;
    wrathion_win += " --mport ";
    wrathion_win += s.listeningPort.c_str();
    wrathion_win += " -v ";
    if(task_params.mode == 'n') {
        wrathion_win += " --index ";
        wrathion_win += task_params.from;
        wrathion_win += ":";
        wrathion_win += task_params.to;
    }
    else if(task_params.mode == 'v') {
        wrathion_win += " -g SINGLEPASS ";
        wrathion_win += " --pw64 ";
        wrathion_win += task_params.password;
    }

    SECURITY_ATTRIBUTES sa; 
    printf("\n->Start of parent execution.\n");
    // Set the bInheritHandle flag so pipe handles are inherited. 
    sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sa.bInheritHandle = TRUE; 
    sa.lpSecurityDescriptor = NULL; 
    // Create a pipe for the child process's STDERR. 
    if ( ! CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &sa, 0) ) {
        exit(1); 
    }
    // Ensure the read handle to the pipe for STDERR is not inherited.
    if ( ! SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0) ){
        exit(1);
    }
    // Create a pipe for the child process's STDOUT. 
    if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0) ) {
        exit(1);
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited
    if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) ){
        exit(1); 
    }
    // Create the child process. 
    PROCESS_INFORMATION piProcInfo = CreateChildProcess(wrathion_win);

    // Read from pipe that is the standard output for child process. 
    printf( "\n->Contents of child process STDOUT:\n\n", argv[1]);
    password = ReadFromPipe(piProcInfo); 

    printf("\n->End of parent execution.\n");
    */
#else
    int pipefd[2];
    pipe(pipefd);
    pid=fork();
    if (pid==0) { // child (Wrathion) process
		
        std::string openclConfig1 = "";
        std::string openclConfig2 = "";
        if(file_exists("../../opencl.config")) {
            openclConfig1 = "--opencl-set";
            std::ifstream ifs("../../opencl.config");
            openclConfig2.assign( (std::istreambuf_iterator<char>(ifs) ), (std::istreambuf_iterator<char>() ) );
            openclConfig2.erase(std::find_if(openclConfig2.rbegin(), openclConfig2.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), openclConfig2.end());
            fprintf(stderr, ">>>>>>>>>>>>>>%s\n", openclConfig2.c_str());
            openclConfig2 = "0:" + openclConfig2;
        }
                     
        std::stringstream port;
        port << s.listeningPort;
        
        //cerr << "main.cpp " << __LINE__ << endl;
        char * arg_bin = const_cast<char*>(fitcrackerPath.c_str());
        char * arg_xmlFile = const_cast<char*>(xmlFile.c_str());
        
        std::stringstream ss1, ss2;
        ss1 << task_params.from;
        ss2 << task_params.to;
        std::string fromTo = ss1.str() + ":" + ss2.str();
        char * arg_fromTo = const_cast<char*>(fromTo.c_str());
        char * arg_password = const_cast<char*>((task_params.password).c_str());
        char * arg_charset = const_cast<char*>((task_params.charset).c_str());
        char * arg_length = const_cast<char*>((task_params.length).c_str());
        char * arg_listeningPort = const_cast<char*>((port.str()).c_str());
        char * arg_openclConfig1 = const_cast<char*>(openclConfig1.c_str());
        char * arg_openclConfig2 = const_cast<char*>(openclConfig2.c_str());
        //cerr << "main.cpp " << __LINE__ << endl;
        
        //"-v", 
        static char *argsN[] = {arg_bin, "-m", "BOINC", "-i", arg_xmlFile, "--mport", arg_listeningPort, "-c", arg_charset, "--index", arg_fromTo, "-l", arg_length, "-o", arg_openclConfig1, arg_openclConfig2, NULL};
        static char *argsV[] = {arg_bin, "-m", "BOINC", "-i", arg_xmlFile, "--mport", arg_listeningPort, "-c", arg_charset, "-g", "SINGLEPASS", "--pw64", arg_password, "-o", arg_openclConfig1, arg_openclConfig2, NULL};
        static char *argsB[] = {arg_bin, "-m", "BOINC", "-i", arg_xmlFile, "--mport", arg_listeningPort, "-c", arg_charset, "-b", "-l", arg_length, "-o", arg_openclConfig1, arg_openclConfig2, NULL};
        
        close(pipefd[0]);    // close reading end in the child
        dup2(pipefd[1], 1);  // send stdout to the pipe
        dup2(pipefd[1], 2);  // send stderr to the pipe
        close(pipefd[1]);    // this descriptor is no longer needed
        
        
        //cerr << "main.cpp " << __LINE__ << endl;
        int result = 0;
        if(task_params.mode == 'n') {
            print_secondary_process_params(argsN);
            execv(fitcrackerPath.c_str(), argsN);
        }
        else if(task_params.mode == 'v') {
            print_secondary_process_params(argsV);
            execv(fitcrackerPath.c_str(), argsV);
        }
        else if(task_params.mode == 'b') {
            print_secondary_process_params(argsB);
            result = execv(fitcrackerPath.c_str(), argsB);
        }
        if( result != 0 )
         {
            char buffer[ 256 ];
            char * errorMessage = strerror_r( errno, buffer, 256 ); // get string message from errno

            //cerr << __LINE__ << errorMessage << endl;
         }
 
        //cerr << "main.cpp " << __LINE__ << endl;
        
        exit(127); // only if execv fails
    }
    else { // pid!=0; parent process
        
        std::string program_output = "";
        char out_buffer;
        close(pipefd[1]); 
        
        //cerr << "PID: " <<  pid << endl;
        //cerr << "main.cpp:" << __LINE__ << endl;
        s.start_accept();
        
        //int test = gio_service.run();
        //cerr << "main.cpp:" << __LINE__ << endl;
        //fprintf(stderr, "main.cpp:%d\n", __LINE__);
        
        //cerr << "main.cpp:" << __LINE__ << endl;
        
        while (read(pipefd[0], &out_buffer, 1) != 0)
        {
            program_output += out_buffer;
        }
        //cerr << "main.cpp:" << __LINE__ << endl;
        cerr << program_output << endl;
    }
    
#endif
    
    
    create_output_file(boinc);
    //cerr << "main.cpp:" << __LINE__ << endl;
    
    if(boinc) {
        boinc_fraction_done(1);
        //cerr << "main.cpp:" << __LINE__ << endl;
        boinc_finish(0);
    }
    //cerr << "main.cpp:" << __LINE__ << endl;
}


#ifdef __WIN32
// Create a child process that uses the previously created pipes
//  for STDERR and STDOUT.
PROCESS_INFORMATION CreateChildProcess(std::string proc_name){
    // Set the text I want to run
    //char szCmdline[]="test --log_level=all --report_level=detailed";
    PROCESS_INFORMATION piProcInfo; 
    STARTUPINFO siStartInfo;
    bool bSuccess = FALSE; 

    // Set up members of the PROCESS_INFORMATION structure. 
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDERR and STDOUT handles for redirection.
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdError = g_hChildStd_ERR_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    LPSTR cmdline = const_cast<char*>(proc_name.c_str());

    // Create the child process. 
    bSuccess = CreateProcess(NULL, 
        //szCmdline,     // command line 
        cmdline,
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION
    CloseHandle(g_hChildStd_ERR_Wr);
    CloseHandle(g_hChildStd_OUT_Wr);
    // If an error occurs, exit the application. 
    if ( ! bSuccess ) {
        exit(1);
    }
    return piProcInfo;
}

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data. 
std::string ReadFromPipe(PROCESS_INFORMATION piProcInfo) {
    DWORD dwRead; 
    CHAR chBuf[BUFSIZE];
    bool bSuccess = FALSE;
    std::string out = "", err = "";
    for (;;) { 
        bSuccess=ReadFile( g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if( ! bSuccess || dwRead == 0 ) break; 

        std::string s(chBuf, dwRead);
        out += s;
    } 
    dwRead = 0;
    for (;;) { 
        bSuccess=ReadFile( g_hChildStd_ERR_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if( ! bSuccess || dwRead == 0 ) break; 

        std::string s(chBuf, dwRead);
        err += s;

    } 
    std::cerr << "stdout:" << out << std::endl;
    std::cerr << "stderr:" << err << std::endl;
    return out;
}
#endif
