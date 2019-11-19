#ifdef __linux__

/* ****************************
*  Definitions
******************************/
#include <memory>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <libgen.h>
#include <random>
#include <string>
#include <vector>

#include "libflagstats.h"
#include "linux-perf-events.h"
#include "aligned_alloc.h"

#ifdef ALIGN
#include "memalloc.h"
#   define memory_allocate(size) aligned_alloc(64, (size))
#else
#   define memory_allocate(size) malloc(size)
#endif

// FLAGSTATS_func methods[] = {FLAGSTAT_sse4, FLAGSTAT_avx2, FLAGSTAT_avx512};

void print16(uint32_t *flags) {
    for (int k = 0; k < 16; k++)
        printf(" %8u ", flags[k]);
    printf("\n");
}

std::vector<unsigned long long>
compute_mins(std::vector< std::vector<unsigned long long> > allresults) {
    if (allresults.size() == 0)
        return std::vector<unsigned long long>();
    
    std::vector<unsigned long long> answer = allresults[0];
    
    for (size_t k = 1; k < allresults.size(); k++) {
        assert(allresults[k].size() == answer.size());
        for (size_t z = 0; z < answer.size(); z++) {
            if (allresults[k][z] < answer[z])
                answer[z] = allresults[k][z];
        }
    }
    return answer;
}

std::vector<double>
compute_averages(std::vector< std::vector<unsigned long long> > allresults) {
    if (allresults.size() == 0)
        return std::vector<double>();
    
    std::vector<double> answer(allresults[0].size());
    
    for (size_t k = 0; k < allresults.size(); k++) {
        assert(allresults[k].size() == answer.size());
        for (size_t z = 0; z < answer.size(); z++) {
            answer[z] += allresults[k][z];
        }
    }

    for (size_t z = 0; z < answer.size(); z++) {
        answer[z] /= allresults.size();
    }
    return answer;
}

/**
 * @brief 
 * 
 * @param n          Number of integers.
 * @param iterations Number of iterations.
 * @param fn         Target function pointer.
 * @param verbose    Flag enabling verbose output.
 * @return           Returns true if the results are correct. Returns false if the results
 *                   are either incorrect or the target function is not supported.
 */
bool benchmark(uint32_t n, uint32_t iterations, FLAGSTATS_func fn, bool verbose, bool test) {
    std::vector<int> evts;
    uint16_t* vdata = (uint16_t*)memory_allocate(n * sizeof(uint16_t));
    std::unique_ptr<uint16_t, decltype(&free)> dataholder(vdata, free);
    if(verbose) {
      printf("alignment: %d\n", get_alignment(vdata));
    }
    evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
    evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
    evts.push_back(PERF_COUNT_HW_BRANCH_MISSES);
    evts.push_back(PERF_COUNT_HW_CACHE_REFERENCES);
    evts.push_back(PERF_COUNT_HW_CACHE_MISSES);
    evts.push_back(PERF_COUNT_HW_REF_CPU_CYCLES);
    LinuxEvents<PERF_TYPE_HARDWARE> unified(evts);
    std::vector<unsigned long long> results; // tmp buffer
    std::vector< std::vector<unsigned long long> > allresults;
    results.resize(evts.size());
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);

    bool isok = true;
    for (uint32_t i = 0; i < iterations; i++) {
        for (size_t k = 0; k < n; k++) {
            vdata[k] = dis(gen); // random init.
        }
        uint32_t correctflags[16] = {0};
        FLAGSTAT_scalar(vdata, n, correctflags); // this is our gold standard
        uint32_t flags[16] = {0};
        
        unified.start();
        fn(vdata, n, flags);
        unified.end(results);

        uint64_t tot_obs = 0;
        for (size_t k = 0; k < 16; ++k) tot_obs += flags[k];
        if (tot_obs == 0) { // when a method is not supported it returns all zero
            return false;
        }

        for (size_t k = 0; k < 16; k++) {
            if (correctflags[k] != flags[k]) {
                if (test) {
                    printf("bug:\n");
                    printf("expected : ");
                    print16(correctflags);
                    printf("got      : ");
                    print16(flags);
                    return false;
                } else {
                    isok = false;
                }
            }
        }
        allresults.push_back(results);
    }

    std::vector<unsigned long long> mins = compute_mins(allresults);
    std::vector<double> avg = compute_averages(allresults);
    
    if (verbose) {
        printf("instructions per cycle %4.2f, cycles per 16-bit word:  %4.3f, "
               "instructions per 16-bit word %4.3f \n",
                double(mins[1]) / mins[0], double(mins[0]) / n, double(mins[1]) / n);
        // first we display mins
        printf("min: %8llu cycles, %8llu instructions, \t%8llu branch mis., %8llu "
               "cache ref., %8llu cache mis.\n",
                mins[0], mins[1], mins[2], mins[3], mins[4]);
        printf("avg: %8.1f cycles, %8.1f instructions, \t%8.1f branch mis., %8.1f "
               "cache ref., %8.1f cache mis.\n",
                avg[0], avg[1], avg[2], avg[3], avg[4]);
    } else {
        printf("cycles per 16-bit word:  %4.3f; ref cycles per 16-bit word: %4.3f \n", double(mins[0]) / n, double(mins[5]) / n);
    }

    return isok;
}

/**
 * @brief 
 * 
 * @param n          Number of integers.
 * @parem m          Number of arrays.
 * @param iterations Number of iterations.
 * @param fn         Target function pointer.
 * @param verbose    Flag enabling verbose output.
 * @return           Returns true if the results are correct. Returns false if the results
 *                   are either incorrect or the target function is not supported.
 */
bool benchmarkMany(uint32_t n, uint32_t m, uint32_t iterations, FLAGSTATS_func fn, bool verbose, bool test) {
    std::vector<int> evts;
#ifdef ALIGN
    std::vector<std::vector<uint16_t,AlignedSTLAllocator<uint16_t,64>>> vdata(m, std::vector<uint16_t,AlignedSTLAllocator<uint16_t,64>>(n));
#else
    std::vector<std::vector<uint16_t>> vdata(m, std::vector<uint16_t>(n));
#endif
#ifdef ALIGN
    for(auto & x : vdata) {
      assert(get_alignment(x.data()) == 64);
    }
#endif
    if(verbose) {
      printf("alignments: ");
      for(auto & x : vdata) {
        printf("%d ", get_alignment(x.data()));
      }
      printf("\n");
    }    
    evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
    evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
    evts.push_back(PERF_COUNT_HW_BRANCH_MISSES);
    evts.push_back(PERF_COUNT_HW_CACHE_REFERENCES);
    evts.push_back(PERF_COUNT_HW_CACHE_MISSES);
    evts.push_back(PERF_COUNT_HW_REF_CPU_CYCLES);
    LinuxEvents<PERF_TYPE_HARDWARE> unified(evts);
    std::vector<unsigned long long> results; // tmp buffer
    std::vector< std::vector<unsigned long long> > allresults;
    results.resize(evts.size());
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);

    bool isok = true;
    for (uint32_t i = 0; i < iterations; i++) {
        for (size_t k = 0; k < vdata.size(); k++) {
            for(size_t k2 = 0; k2 < vdata[k].size() ; k2++) { 
               vdata[k][k2] = dis(gen); // random init.
            }
        }

        std::vector<std::vector<uint32_t>> flags(m,std::vector<uint32_t>(16*2));
        
        unified.start();
        for (size_t k = 0; k < m ; k++) {
          fn(vdata[k].data(), vdata[k].size(), flags[k].data());
        }
        unified.end(results);
        allresults.push_back(results);
    }

    std::vector<unsigned long long> mins = compute_mins(allresults);
    std::vector<double> avg = compute_averages(allresults);
    
    if (verbose) {
        printf("instructions per cycle %4.2f, cycles per 16-bit word:  %4.3f, "
               "instructions per 16-bit word %4.3f \n",
                double(mins[1]) / mins[0], double(mins[0]) / (n*m), double(mins[1]) / (n*m));
        // first we display mins
        printf("min: %8llu cycles, %8llu instructions, \t%8llu branch mis., %8llu "
               "cache ref., %8llu cache mis.\n",
                mins[0], mins[1], mins[2], mins[3], mins[4]);
        printf("avg: %8.1f cycles, %8.1f instructions, \t%8.1f branch mis., %8.1f "
               "cache ref., %8.1f cache mis.\n",
                avg[0], avg[1], avg[2], avg[3], avg[4]);
    } else {
        printf("cycles per 16-bit word:  %4.3f; ref cycles per 16-bit word: %4.3f \n", double(mins[0]) / (n*m), double(mins[5]) / (n*m));
    }

    return isok;
}

void measureoverhead(uint32_t n, uint32_t iterations, bool verbose) {
    std::vector<int> evts;
    evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
    evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
    evts.push_back(PERF_COUNT_HW_BRANCH_MISSES);
    evts.push_back(PERF_COUNT_HW_CACHE_REFERENCES);
    evts.push_back(PERF_COUNT_HW_CACHE_MISSES);
    evts.push_back(PERF_COUNT_HW_REF_CPU_CYCLES);
    LinuxEvents<PERF_TYPE_HARDWARE> unified(evts);
    std::vector<unsigned long long> results; // tmp buffer
    std::vector< std::vector<unsigned long long> > allresults;
    results.resize(evts.size());
    
    for (uint32_t i = 0; i < iterations; i++) {
        unified.start();
        unified.end(results);
        allresults.push_back(results);
    }

    std::vector<unsigned long long> mins = compute_mins(allresults);
    std::vector<double> avg = compute_averages(allresults);
    printf("%-40s\t","nothing");    
    if (verbose) {
        printf("instructions per cycle %4.2f, cycles per 16-bit word:  %4.3f, "
               "instructions per 16-bit word %4.3f \n",
                double(mins[1]) / mins[0], double(mins[0]) / n, double(mins[1]) / n);
        // first we display mins
        printf("min: %8llu cycles, %8llu instructions, \t%8llu branch mis., %8llu "
               "cache ref., %8llu cache mis.\n",
                mins[0], mins[1], mins[2], mins[3], mins[4]);
        printf("avg: %8.1f cycles, %8.1f instructions, \t%8.1f branch mis., %8.1f "
               "cache ref., %8.1f cache mis.\n",
                avg[0], avg[1], avg[2], avg[3], avg[4]);
    } else {
        printf("cycles per 16-bit word:  %4.3f; ref cycles per 16-bit word: %4.3f \n", double(mins[0]) / n, double(mins[5]) / n);
    }
}

static void print_usage(char *command) {
    printf(" Try %s -n 100000 -i 15 -v \n", command);
    printf("-n is the number of 16-bit words \n");
    printf("-i is the number of tests or iterations \n");
    printf("-v makes things verbose\n");
}

int main(int argc, char **argv) {
    size_t n = 100000;
    size_t m = 1;
    size_t iterations = 0; 
    bool verbose = false;
    int c;

    while ((c = getopt(argc, argv, "vhm:n:i:")) != -1) {
        switch (c) {
        case 'n':
            n = atoll(optarg);
            break;
        case 'm':
            m = atoll(optarg);
            break;
        case 'v':
            verbose = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'i':
            iterations = atoi(optarg);
            break;
        default:
            abort();
        }
    }

    if(n > UINT32_MAX) {
       printf("setting n to %u \n", UINT32_MAX);
       n = UINT32_MAX;
    }

    if(iterations > UINT32_MAX) {
       printf("setting iterations to %u \n", UINT32_MAX);
       iterations = UINT32_MAX;
    }

    if(iterations == 0) {
      if(m*n < 1000000) iterations = 100;
      else iterations = 10;
    }
    printf("n = %zu m = %zu \n", n, m);
    printf("iterations = %zu \n", iterations);
    if(n == 0) {
       printf("n cannot be zero.\n");
       return EXIT_FAILURE;
    }

    size_t array_in_bytes = sizeof(uint16_t) * n * m;
    if(array_in_bytes < 1024) {
      printf("array size: %zu B\n", array_in_bytes);
    } else if (array_in_bytes < 1024 * 1024) {
      printf("array size: %.3f kB\n", array_in_bytes / 1024.);
    } else {
      printf("array size: %.3f MB\n", array_in_bytes / (1024 * 1024.));
    }

    measureoverhead(n*m, iterations, verbose);

#if defined(STORM_HAVE_CPUID)
    #if defined(__cplusplus)
    /* C++11 thread-safe singleton */
    static const int cpuid = STORM_get_cpuid();
    #else
    static int cpuid_ = -1;
    int cpuid = cpuid_;
    if (cpuid == -1) {
        cpuid = STORM_get_cpuid();

    #if defined(_MSC_VER)
        _InterlockedCompareExchange(&cpuid_, cpuid, -1);
    #else
        __sync_val_compare_and_swap(&cpuid_, -1, cpuid);
    #endif
    }
    #endif
#endif

    #if defined(STORM_HAVE_SSE42)
        if ((cpuid & STORM_CPUID_runtime_bit_SSE42)) {
            printf("libflagstats-sse4.2\t");
            fflush(NULL);
            bool isok = benchmarkMany(n, m, iterations, FLAGSTAT_sse4, verbose, true);
            if (isok == false) {
                printf("Problem detected with %u.\n", 0);
            }
            if (verbose)
                printf("\n");
        }
    // }
    #endif

    #if defined(STORM_HAVE_AVX2)
        if ((cpuid & STORM_CPUID_runtime_bit_AVX2)) {
            printf("libflagstats-avx2\t");
            fflush(NULL);
            bool isok = benchmarkMany(n, m, iterations, FLAGSTAT_avx2, verbose, true);
            if (isok == false) {
                printf("Problem detected with %u.\n", 1);
            }
            if (verbose)
                printf("\n");
        }
    // }
    #endif

    #if defined(STORM_HAVE_AVX512)
        if ((cpuid & STORM_CPUID_runtime_bit_AVX512BW)) {
            printf("libflagstats-avx512bw\t");
            fflush(NULL);
            bool isok = benchmarkMany(n, m, iterations, FLAGSTAT_avx512, verbose, true);
            if (isok == false) {
                printf("Problem detected with %u.\n", 2);
            }
            if (verbose)
                printf("\n");
        // }
        }
    #endif

    if (!verbose)
        printf("Try -v to get more details.\n");

    return EXIT_SUCCESS;
}
#else //  __linux__

#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("This is a linux-specific benchmark\n");
    return EXIT_SUCCESS;
}

#endif