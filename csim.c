/** Author: Alex Voytovich
 * User ID: avoytovi
 * School ID: 804 819 692
 *
 * To run, -lm must be used to link the math library for the
 * "double pow(double a, double b)" function otherwise the
 * the compiler see it as an unrecognized reference
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include "cachelab.h"

typedef unsigned long long mem_address;

// Structure for storing cache parameters,
// as well as trackers and counters.
typedef struct {
    int s; //Number of set index bits (log2(S))
    int b; //Number of block offset bits (log2(B))
    int E; //Number of lines per set
    long long S; //Number of sets (2^s)
    long long B; //Block size (bytes) (2^b)

    int hits;
    int misses;
    int evicts;
    bool verbosity;
}cache_param;

// Structure that holds address, tag, validity, and time stamp.
typedef struct {
    int last_used; //Time stamp if the line
    bool valid;  //Whether line is used or not
    mem_address tag; //Stored address tag
    char *block; //Line block
}cache_line;

// Structure for holding array of lines.
typedef struct {
    cache_line *lines;
}cache_set;

// Structure for holding array of sets
// *2-D array of lines
typedef struct {
    cache_set *sets;
}cache;

void run_cache(cache *cache_sim, cache_param *param, mem_address address);
int find_evict(cache_set set, cache_param *param, int *used_line);
void build_cache(long long set_count, int lines_count, cache *cache_sim);
void clean(cache *cache_sim, long long set_size);
void print_help(char *argv[]);

/** main function
 * Starts by taking run input if the format of [-hv] -s <num> -E <num> -b <num> -t <file>.
 * h: help
 * v: vebrose
 * s: number of index bits
 * E: number of lines
 * b: number of block offset bits
 * t: input file
 * After reading and storing the values of the input, it then builds the cache, and reads each
 * each line from the input file, and checks for the command. L will do nothing for the puprose
 * of the simulation, S and I run the cache, and M runs the cache twice, first to pull check the
 * cache, and store if it's a miss, second, to modify the memory. It then cleans the cache, and
 * terminates.
 * If the run input is incorrect, or the h flag is detected, the program displays the usage, and
 * terminates.
 *
 * terminates.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char** argv) {
    cache_param *track = malloc(sizeof(cache_param));

    //Take run parameters during run time
    char *trace = NULL;
    char in;
    while ((in = (char) getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (in) {
            case 's':
                track->s = atoi(optarg);
                break;
            case 'E':
                track->E = atoi(optarg);
                break;
            case 'b':
                track->b = atoi(optarg);
                break;
            case 't':
                trace = optarg;
                break;
            case 'v':
                track->verbosity = true;
                break;
            case 'h':
                print_help(argv);
                free(track);
                exit(0);
            default:
                print_help(argv);
                free(track);
                exit(1);
        }
    }

    //Check if parameters taken are valid
    if (track->s == 0 || track->b == 0 || track->E == 0 || trace == NULL) {
        printf("%s: Missing Required Command Line Argument\n", argv[0]);
        print_help(argv);
        free(track);
        exit(1);
    }

    //Initialize the parameters, and build the cache simulator
    track->S = (long long int)pow(2, track->s);
    track->B = (long long int)pow(2, track->b);
    track->hits = 0;
    track->misses = 0;
    track->evicts = 0;
    cache *cache_sim = malloc(sizeof(cache));
    build_cache(track->S, track->E, cache_sim);

    //Read the file, and for each line, run the cache simulator accordingly
    mem_address address;
    char cmd;
    int size;
    FILE *trace_file = fopen(trace, "r");
    if (trace_file != NULL) {
        while (fscanf(trace_file, " %c %llx,%d", &cmd, &address, &size) == 3) {
            switch (cmd) {
                case 'I'://Ignore I command for simulator purposes
                    break;
                case 'L'://Run simulator to load
                    if (track->verbosity) printf("%c %llu %d ", cmd, address, size);
                    run_cache(cache_sim, track, address);
                    if (track->verbosity) printf("\n");
                    break;
                case 'S'://Run simulator to store
                    if (track->verbosity) printf("%c %llu %d ", cmd, address, size);
                    run_cache(cache_sim, track, address);
                    if (track->verbosity) printf("\n");
                    break;
                case 'M'://Run simulator, first in case address is already store, second to modify
                    if (track->verbosity) printf("%c %llu %d ", cmd, address, size);
                    run_cache(cache_sim, track, address);
                    run_cache(cache_sim, track, address);
                    if (track->verbosity) printf("\n");
                    break;
            }
        }
    }
    fclose(trace_file);

    printSummary(track->hits, track->misses, track->evicts);
    //Free's all allocated structures
    free(track);
    clean(cache_sim, track->S);
    return 0;
}

/** run_cache
 * Assuming the cache is full, the function first finds the set witch it will utilize. I then
 * runs through the lines of the set, checks of there is a hit, and increments the hit counter,
 * and prints hit if verbosity is true. If there is a miss, it will then check if the line is
 * (aka filled), and if the cache is full. If the line is invalid, it will set cache_full to false.
 * If the cache is not full, it will count a miss, and print miss of verbosity is true. It will then
 * proceed to try and store the address. If the cache is full, it will find an eviction, and increment
 * evicts count, and print if verbosity is full while storing the address. If it is not full, it will
 * simply store the address.
 *
 * @param cache_sim
 * @param param
 * @param address
 */
void run_cache(cache *cache_sim, cache_param *param, mem_address address){
    bool cache_full = true;//Assume cache is full
    int prev_hit = param->hits;//Record previous hits for checking miss

    //Hashing function for getting set of lines to work with
    int tag = 64 - param->s - param->b; //t = m - s - b
    mem_address in_tag = address >> (param->s + param->b);
    mem_address temp = address << tag;
    mem_address set_index = temp >> (tag + param->b);

    //Checks if address is already stores, and checks if cache is not full
    cache_set set = cache_sim->sets[set_index];
    for (int i = 0; i < param->E; ++i){
        if (set.lines[i].valid && set.lines[i].tag == in_tag){
            set.lines[i].last_used++;
            param->hits++;
            if(param->verbosity)
                printf("hit ");
        }else if (!set.lines[i].valid && cache_full)
            cache_full = false;
    }
    //If recorded hits is the same as current hits, it's a miss
    if (prev_hit == param->hits){
        param->misses++;
        if (param->verbosity)
            printf("miss ");
    }else return;

    //Finds line to evict, and latest and first used integers for time tracking
    int *used_line = (int*)malloc(sizeof(int) * 2);
    int least_used = find_evict(set, param, used_line);

    if (cache_full){//If cache is full, evict line, and overwrite
        param->evicts++;
        if(param->verbosity)
            printf("eviction ");
        set.lines[least_used].tag = in_tag;
        set.lines[least_used].last_used = used_line[1] + 1;
    }else{//If cache is not full, find next available line, and set the last used as latest++
        for(int i = 0; i < param->E; ++i){
            if(!set.lines[i].valid){
                set.lines[i].tag = in_tag;
                set.lines[i].valid = true;
                set.lines[i].last_used = used_line[1] + 1;
                break;
            }
        }
    }
    free(used_line);
}

/** find_evict
 * Finds a line to evict when the line is full.
 *
 * @param set
 * @param param
 * @param used_line
 * @return
 */
int find_evict(cache_set set, cache_param *param, int *used_line){
    //used_line[0] is the earliest used, used_line[1] is the latest used
    used_line[0] = set.lines[0].last_used;
    used_line[1] = set.lines[0].last_used;
    int least_used = 0;

    //Find the latest and earliest used integers for eviction and latest use record
    for(int i = 1; i < param->E; ++i){
        if (used_line[0] > set.lines[i].last_used){
            least_used = i;
            used_line[0] = set.lines[i].last_used;

        }if (used_line[1] < set.lines[i].last_used)
            used_line[1] = set.lines[i].last_used;
    }
    return least_used;
}

/** build_cache
 * Cache building function that builds the simulated cache. Runs 2 for loops within each
 * other with the given perameters to build the proper 2-D cache grid.
 * Sets variables for each line to zero and false.
 *
 * @param set_count
 * @param block_size
 * @param lines_count
 * @param cache_sim
 */
void build_cache(long long set_count, int lines_count, cache *cache_sim){
    cache_sim->sets = (cache_set*) malloc(sizeof(cache_set) * set_count);
    cache_set set;
    cache_line line;

    for (int i = 0; i < set_count; ++i){
        set.lines = (cache_line*)malloc(sizeof(cache_line) * lines_count);

        for (int x = 0; x < lines_count; ++x){
            line.last_used = 0;
            line.tag = 0;
            line.valid = false;
            set.lines[x] = line;
        }
        cache_sim->sets[i] = set;
    }
}

/** clean
 * Clean function that frees the allocated cache structer.
 *
 * @param cache_sim
 * @param set_size
 */
void clean(cache *cache_sim, long long set_size) {
    cache_set set;
    for (int i = 0; i < set_size; ++i){
        set = cache_sim->sets[i];
        free(set.lines);
    }
    free(cache_sim->sets);
    free(cache_sim);
}

/** print_help
 * Usage function that prints instructions on the usage of the cache simulator.
 * Receieves argv to print run file name.
 *
 * @param argv
 */
void print_help(char *argv[]){
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("Examples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}


