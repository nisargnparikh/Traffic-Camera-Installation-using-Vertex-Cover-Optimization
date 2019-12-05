#include <minisat/core/SolverTypes.h>
#include <minisat/core/Solver.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include <pthread.h>
#include <time.h>
#include <regex>
#include <vector>
#include <list>

#include "cnf-sat-vc.hpp"
#include "approx_vc.hpp"

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

std::string NAME = "0";
bool OUT_TO_FILE = false;
bool LOG_EN = false;

struct job {
  int vertices;
  std::vector< std::pair<int,int> > edges;
};

pthread_mutex_t job_queue1_mutex = PTHREAD_MUTEX_INITIALIZER;  //cnf_sat and IO_handler
pthread_mutex_t job_queue2_mutex = PTHREAD_MUTEX_INITIALIZER;  //approxVC1 and IO_handler
pthread_mutex_t job_queue3_mutex = PTHREAD_MUTEX_INITIALIZER;  //approxVC2 IO_handler
pthread_mutex_t result_queue1_mutex = PTHREAD_MUTEX_INITIALIZER;  //cnf_sat and IO_handler
pthread_mutex_t result_queue2_mutex = PTHREAD_MUTEX_INITIALIZER;  //approxVC1 and IO_handler
pthread_mutex_t result_queue3_mutex = PTHREAD_MUTEX_INITIALIZER;  //approxVC2 IO_handler

std::list<job*> job_queue1;  //cnf_sat_vc
std::list<job*> job_queue2;  //approxVC1
std::list<job*> job_queue3;  //approxVC2

std::list<std::string> result_queue1; //cnf_sat_vc
std::list<std::string> result_queue2; //approxVC1
std::list<std::string> result_queue3; //approxVC2

std::vector< std::pair<int,int> > parse(std::string s) {
    std::pair<int, int> edge;
    std::vector< std::pair<int,int> > result;
    
    // using regex
    try {
        std::regex re("-?[0-9]+"); //consecutive numbers matching
        std::sregex_iterator next(s.begin(), s.end(), re);
        std::sregex_iterator end;
        while (next != end) {
            std::smatch match1;
            std::smatch match2;
            
            match1 = *next;
            next++;
           
            if (next != end) {
                match2 = *next;
                edge.first = std::stoi(match1.str());
                edge.second = std::stoi(match2.str());
                result.push_back(edge);
                next++;
            }
        } 
    } 
    catch (std::regex_error& e) {
        result.clear();
    }

    return result;
}


std::string cnf_sat_vc(int vertices, std::vector< std::pair<int,int> > edges) {
    std::string result;
    VertexCover v_cover = VertexCover(vertices, edges);
    result = v_cover.linsearch_VC();

    return result;
}


void* IO_handler(void* args) {
    char cmd;
    int vertices = 0;
    std::string edges_input;
    std::vector< std::pair<int,int> > parsed_edges;

    while(std::cin >> cmd){    
        
        switch(cmd) {
            
            case 'V': case 'v':
                std::cin >> vertices;
                  
                if (vertices < 0) {
                    std::cerr << "Error: Incorrect value for vertices entered" << std::endl;
                    vertices = 0;
                }
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                break;
            
            case 'E': case 'e': 
            {
                std::cin >> edges_input;
                
                parsed_edges = parse(edges_input);
                
                struct job* incoming_job;

                // add to queue 1
                incoming_job = new job;
                incoming_job->vertices = vertices;
                incoming_job->edges = parsed_edges;
                pthread_mutex_lock (&job_queue1_mutex);
                job_queue1.push_back(incoming_job);
                pthread_mutex_unlock (&job_queue1_mutex);

                // add to queue 2
                incoming_job = new job;
                incoming_job->vertices = vertices;
                incoming_job->edges = parsed_edges;
                pthread_mutex_lock (&job_queue2_mutex);
                job_queue2.push_back(incoming_job);
                pthread_mutex_unlock (&job_queue2_mutex);

                // add to queue 3
                incoming_job = new job;
                incoming_job->vertices = vertices;
                incoming_job->edges = parsed_edges;
                pthread_mutex_lock (&job_queue3_mutex);
                job_queue3.push_back(incoming_job);
                pthread_mutex_unlock (&job_queue3_mutex);

                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                break;
            }

            default:
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cerr << "Error: command cannot be recognized" << std::endl;
        }
    }
    return NULL;
}


void* output_handler(void* args) {
    
    bool exit_flag = false;

    while(true) {  
        pthread_mutex_lock (&result_queue1_mutex);
        pthread_mutex_lock (&result_queue2_mutex);
        pthread_mutex_lock (&result_queue3_mutex);

        while(!result_queue1.empty() && !result_queue2.empty() && !result_queue3.empty()) {
            std::string result;
            
            result = result_queue1.front();
            result_queue1.pop_front();
            std::cout << result << std::endl;

            result = result_queue2.front();
            result_queue2.pop_front();
            std::cout << result << std::endl;

            result = result_queue3.front();
            result_queue3.pop_front();
            std::cout << result << std::endl;
        }
        if ( (*((bool*)args+1) && result_queue1.empty() && result_queue2.empty() && result_queue3.empty())
             || *((bool*)args+2) ) {
            exit_flag = true;
        }

        pthread_mutex_unlock (&result_queue1_mutex);
        pthread_mutex_unlock (&result_queue2_mutex);
        pthread_mutex_unlock (&result_queue3_mutex);

        if (exit_flag) {break;}
    }

    return NULL;
}


void* calc_cnf_sat_vc(void* args) {
    
    clockid_t clock_id;
    int job_number = 1;
    bool first = true;
    bool exit_flag = false;
    std::ofstream outfile;
    std::string file_name = NAME + "_CNF-SAT-VC.csv";
    if (OUT_TO_FILE) { outfile.open(file_name, std::ios_base::app); }

    while (true) {
        struct job* retrieved_job = NULL;
        std::string result;

        // use mutex and get job from queue
        pthread_mutex_lock (&job_queue1_mutex);
        if (!job_queue1.empty()) {
            retrieved_job = job_queue1.front(); 
            job_queue1.pop_front();

            // time starts
            pthread_getcpuclockid(pthread_self(), &clock_id);
            struct timespec ts_start;
            clock_gettime(clock_id, &ts_start);

            // compute result
            result = "CNF-SAT-VC: " + cnf_sat_vc(retrieved_job->vertices, retrieved_job->edges);
            
            //time ends
            struct timespec ts_end;
            clock_gettime(clock_id, &ts_end);
            long double elapsed_time_us = ((long double)ts_end.tv_sec*1000000 + (long double)ts_end.tv_nsec/1000.0) - ((long double)ts_start.tv_sec*1000000 + (long double)ts_start.tv_nsec/1000.0);
            
            // output result to terminal if LOG_EN option
            if (LOG_EN) {
                std::clog << "CNF-SAT-VC,Job_num," << job_number << "," 
                      << "time_us," << elapsed_time_us << std::endl;
            }
            
            // writing to file
            if (OUT_TO_FILE) {
                if (first) {
                    outfile << elapsed_time_us;
                    first = false;
                }
                else {
                    outfile << "," << elapsed_time_us;
                }
            }

            //write result to result queue, use mutex for thread safety
            pthread_mutex_lock (&result_queue1_mutex);
            result_queue1.push_back(result);
            pthread_mutex_unlock (&result_queue1_mutex);
            
            job_number++;
            
        }
        // exit loop when IO_handler see EOF (flag is set in int main after thread joins)
        if(*(bool*)args && job_queue1.empty()) {
            exit_flag = true;
            outfile << std::endl;
        }
        pthread_mutex_unlock (&job_queue1_mutex);

        //memory cleanup
        delete retrieved_job;
        if (exit_flag) {break;}
    }

    return NULL;
}


void* calc_aprox_vc_1(void* args) {
        
    clockid_t clock_id;
    int job_number = 1;
    bool first = true;
    bool exit_flag = false;
    std::ofstream outfile;
    std::string file_name = NAME + "_APPROX-VC-1.csv";
    if (OUT_TO_FILE) { outfile.open(file_name, std::ios_base::app); }
    
    while (true) {
        struct job* retrieved_job = NULL;
        std::string result;

        //use mutex, get job from queue
        pthread_mutex_lock (&job_queue2_mutex);
        if (!job_queue2.empty()) {
            retrieved_job = job_queue2.front(); 
            job_queue2.pop_front();

            // time starts
            pthread_getcpuclockid(pthread_self(), &clock_id);
            struct timespec ts_start;
            clock_gettime(clock_id, &ts_start);

            // compute result
            result = "APPROX-VC-1: " + approxVC1(retrieved_job->vertices, retrieved_job->edges);

            //time ends
            struct timespec ts_end;
            clock_gettime(clock_id, &ts_end);
            long double elapsed_time_us = ((long double)ts_end.tv_sec*1000000 + (long double)ts_end.tv_nsec/1000.0) - ((long double)ts_start.tv_sec*1000000 + (long double)ts_start.tv_nsec/1000.0);

            // output result to terminal if LOG_EN option
            if (LOG_EN) {
                std::clog << "APPROX-VC-1,Job_num," << job_number << "," 
                          << "time_us," << elapsed_time_us << std::endl;
            }
            
            // writing to file
            if (OUT_TO_FILE) {
                if (first) {
                    outfile << elapsed_time_us;
                    first = false;
                }
                else {
                    outfile << "," << elapsed_time_us;
                }
            }

            //write result to result queue, use mutex for thread safety
            pthread_mutex_lock (&result_queue2_mutex);
            result_queue2.push_back(result);
            pthread_mutex_unlock (&result_queue2_mutex);  

            job_number++;
        }
        // exit loop when IO_handler see EOF (flag is set in int main after thread joins)
        if(*(bool*)args && job_queue2.empty()) {
            exit_flag = true;
            outfile << std::endl;
        }
        pthread_mutex_unlock (&job_queue2_mutex);

        // memory cleanup
        delete retrieved_job;
        if (exit_flag) {break;}

    }

    return NULL;
}


void* calc_approxVC2(void* args) {
        
    clockid_t clock_id;
    int job_number = 1;
    bool first = true;
    bool exit_flag = false;
    std::ofstream outfile;
    std::string file_name = NAME + "_APPROX-VC-2.csv";
    if (OUT_TO_FILE) { outfile.open(file_name, std::ios_base::app); }
    
    while (true) {
        struct job* retrieved_job = NULL;
        std::string result;

        // use mutex and get job from queue
        pthread_mutex_lock (&job_queue3_mutex);
        if (!job_queue3.empty()) {
            retrieved_job = job_queue3.front(); 
            job_queue3.pop_front();

            // time starts
            pthread_getcpuclockid(pthread_self(), &clock_id);
            struct timespec ts_start;
            clock_gettime(clock_id, &ts_start);

            // compute result
            result = "APPROX-VC-2: " + approxVC2(retrieved_job->vertices, retrieved_job->edges);

            // time ends
            struct timespec ts_end;
            clock_gettime(clock_id, &ts_end);
            long double elapsed_time_us = ((long double)ts_end.tv_sec*1000000 + (long double)ts_end.tv_nsec/1000.0) - ((long double)ts_start.tv_sec*1000000 + (long double)ts_start.tv_nsec/1000.0);
            
            // output result to terminal if LOG_EN option
            if (LOG_EN) {
                std::clog << "APPROX-VC-2,Job_num," << job_number << "," 
                          << "time_us," << elapsed_time_us << std::endl;
            }

            // write to file if option selected
            if (OUT_TO_FILE) {
                if (first) {
                    outfile << elapsed_time_us;
                    first = false;
                }
                else {
                    outfile << "," << elapsed_time_us;
                }
            }

            //write result to result queue, use mutex for thread safety
            pthread_mutex_lock (&result_queue3_mutex);
            result_queue3.push_back(result);
            pthread_mutex_unlock (&result_queue3_mutex);
            

            job_number++;
        }
        // exit loop when IO_handler see EOF (flag is set in int main after thread joins)
        if(*(bool*)args && job_queue3.empty()) {
            exit_flag = true;
            outfile << std::endl;
        }

        pthread_mutex_unlock (&job_queue3_mutex);

        //memory  cleanup
        delete retrieved_job;
        if (exit_flag) {break;}
    }
    
    return NULL;
}


int main(int argc, char **argv) {
    pthread_t IO_thread;
    pthread_t out_thread;
    pthread_t cnf_sat_thread;
    pthread_t approx_vc1_thread;
    pthread_t approx_vc2_thread;
    bool finished_flags[3] = {false};
    
    opterr = 0;
    std::string tmp;
    bool ignore_sat = false;
    int c;

    //options are '-o' [out to file], '-n value' [name to prepend filename], '-i' [ignore cnfsat]
    while ((c = getopt (argc, argv, "on:il?")) != -1)
        switch (c)
        {
        case 'o':
            OUT_TO_FILE = true;
            break;
        case 'n':
            NAME = optarg;
            break;
        case 'i':
            ignore_sat = true;
            finished_flags[2] = true;
            break;    
        case 'l':
            LOG_EN = true;
            break;     
        case '?':
            std::cerr << "Error: unknown option: " << optopt << std::endl;
            return 1;
        default:
            return 0;
        }
    

    pthread_create (&IO_thread, NULL, &IO_handler, (void *) finished_flags);
    pthread_create (&out_thread, NULL, &output_handler, (void *) finished_flags);
    if (!ignore_sat) {
        pthread_create (&cnf_sat_thread, NULL, &calc_cnf_sat_vc, (void *) finished_flags); }
    pthread_create (&approx_vc1_thread, NULL, &calc_aprox_vc_1, (void *) finished_flags);
    pthread_create (&approx_vc2_thread, NULL, &calc_approxVC2, (void *) finished_flags);

    pthread_join (IO_thread, NULL);
    finished_flags[0] = true; // signal all threads to finish work and return, flag indicates that EOF was seen at input

    if (!ignore_sat) {
        pthread_join (cnf_sat_thread, NULL); }
    pthread_join (approx_vc1_thread, NULL);
    pthread_join (approx_vc2_thread, NULL);
    finished_flags[1] = true; //signal output writer to finish print results and return, flag indicates all jobs processed
    pthread_join (out_thread, NULL);
    return 0;
}
