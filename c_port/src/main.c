#define _CRT_SECURE_NO_WARNINGS
#include "khmer_segmenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <psapi.h>

// --- Helpers ---

double get_time_sec() {
    LARGE_INTEGER frequency, start;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    return (double)start.QuadPart / frequency.QuadPart;
}

double get_memory_mb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
    }
    return 0.0;
}

// --- Batch Processing ---

void batch_process_file(KhmerSegmenter* seg, const char* filepath) {
    if (!filepath) return;
    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("Error: Could not open file %s\n", filepath);
        return;
    }

    char line[4096];
    printf("Processing %s...\n", filepath);
    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        char* p = line;
        while(*p) { if(*p=='\r' || *p=='\n') *p=0; else p++; }
        if (!*line) continue;
        
        char* res = khmer_segmenter_segment(seg, line, " | ");
        printf("Original:  %s\n", line);
        printf("Segmented: %s\n", res);
        printf("----------------------------------------\n");
        free(res); // Important
    }
    fclose(f);
}

// --- Benchmark ---

typedef struct {
    KhmerSegmenter* seg;
    const char* text;
    int iterations;
} ThreadData;

DWORD WINAPI benchmark_worker(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    for (int i = 0; i < data->iterations; i++) {
        char* res = khmer_segmenter_segment(data->seg, data->text, NULL);
        free(res);
    }
    return 0;
}

void run_benchmark(KhmerSegmenter* seg, int threads_count) {
    const char* text = "ក្រុមហ៊ុនទទួលបានប្រាក់ចំណូល ១ ០០០ ០០០ ដុល្លារក្នុងឆ្នាំនេះ ខណៈដែលតម្លៃភាគហ៊ុនកើនឡើង ៥% ស្មើនឹង 50.00$។"
                    "លោក ទេព សុវិចិត្រ នាយកប្រតិបត្តិដែលបញ្ចប់ការសិក្សាពីសាកលវិទ្យាល័យភូមិន្ទភ្នំពេញ (ស.ភ.ភ.ព.) "
                    "បានថ្លែងថា ភាពជោគជ័យផ្នែកហិរញ្ញវត្ថុនាឆ្នាំនេះ គឺជាសក្ខីភាពនៃកិច្ចខិតខំប្រឹងប្រែងរបស់ក្រុមការងារទាំងមូល "
                    "និងការជឿទុកចិត្តពីសំណាក់វិនិយោគិន។";
    int iterations_seq = 1000;
    int iterations_conc = 5000;
    
    printf("\n--- Benchmark Suite ---\n");
    printf("Text Length: %zu chars\n", strlen(text));
    printf("Initial Memory: %.2f MB\n", get_memory_mb());

    // 1. Warmup / Output Check
    char* check = khmer_segmenter_segment(seg, text, " | ");
    printf("\n[Output Check]\n%s\n", check);
    free(check);

    // 2. Sequential
    printf("\n[Sequential] Running %d iterations...\n", iterations_seq);
    double start = get_time_sec();
    double start_mem = get_memory_mb();
    
    for (int i=0; i<iterations_seq; i++) {
        char* res = khmer_segmenter_segment(seg, text, NULL);
        free(res);
    }
    
    double end = get_time_sec();
    double end_mem = get_memory_mb();
    double dur = end - start;
    printf("Time: %.3fs\n", dur);
    printf("Avg: %.3f ms/call\n", (dur / iterations_seq) * 1000.0);
    printf("Mem Delta: %.2f MB\n", end_mem - start_mem);

    // 3. Concurrent
    if (threads_count < 1) threads_count = 1;
    printf("\n[Concurrent] Running %d iterations with %d threads...\n", iterations_conc, threads_count);
    
    start = get_time_sec();
    start_mem = get_memory_mb();
    
    HANDLE* handles = (HANDLE*)malloc(sizeof(HANDLE) * threads_count);
    ThreadData* tdata = (ThreadData*)malloc(sizeof(ThreadData) * threads_count);
    
    int per_thread = iterations_conc / threads_count;
    
    for (int i=0; i<threads_count; i++) {
        tdata[i].seg = seg;
        tdata[i].text = text;
        tdata[i].iterations = per_thread;
        handles[i] = CreateThread(NULL, 0, benchmark_worker, &tdata[i], 0, NULL);
    }
    
    WaitForMultipleObjects(threads_count, handles, TRUE, INFINITE);
    
    for(int i=0; i<threads_count; i++) CloseHandle(handles[i]);
    free(handles);
    free(tdata);
    
    end = get_time_sec();
    end_mem = get_memory_mb();
    dur = end - start;
    
    printf("Time: %.3fs\n", dur);
    printf("Throughput: %.2f calls/sec\n", (double)iterations_conc / dur);
    printf("Mem Delta: %.2f MB\n", end_mem - start_mem);
}

// --- Main ---

int main() {
    // Windows Console Setup
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif
    setbuf(stdout, NULL);

    int argc;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw) return 1;

    // Parse Args
    char** argv = (char**)malloc(argc * sizeof(char*));
    for (int i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, NULL, 0, NULL, NULL);
        argv[i] = (char*)malloc(len);
        WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, argv[i], len, NULL, NULL);
    }
    
    // Config
    int mode_benchmark = 0;
    char* input_file = NULL;
    char* input_text = NULL;
    int threads = 4;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            mode_benchmark = 1;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            input_file = argv[++i];
        } else if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) {
            threads = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            input_text = argv[i]; // Treat positional as text
        }
    }

    printf("Initializing segmenter...\n");
    // Look for dictionary in probable locations
    const char* dict_path = "../data/khmer_dictionary_words.txt";
    const char* freq_path = "../data/khmer_word_frequencies.json"; // Not fully used yet but consistent API
    
    // Check if file exists, else try local
    FILE* check = fopen(dict_path, "r");
    if (!check) {
        dict_path = "khmer_dictionary_words.txt"; // try current dir
    } else {
        fclose(check);
    }

    KhmerSegmenter* seg = khmer_segmenter_init(dict_path, freq_path);
    if (!seg) {
        printf("Failed to init segmenter.\n");
        return 1;
    }
    printf("Initialization complete.\n");

    if (mode_benchmark) {
        run_benchmark(seg, threads);
    } else if (input_file) {
        batch_process_file(seg, input_file);
    } else if (input_text) {
        char* res = khmer_segmenter_segment(seg, input_text, " | ");
        printf("Input: %s\n", input_text);
        printf("Output: %s\n", res);
        free(res);
    } else {
        printf("Usage: khmer_segmenter.exe [files/text...]\n");
        printf("  --benchmark       Run benchmark suite\n");
        printf("  --threads <N>     Set threads for benchmark\n");
        printf("  --file <path>     Process lines from file\n");
        printf("  <text>            Process raw text\n");
    }

    khmer_segmenter_free(seg);
    LocalFree(argvw); // Free Windows Arg list
    // Free our argv conversions
    for(int i=0; i<argc; i++) free(argv[i]);
    free(argv);
    
    return 0;
}
