#define _CRT_SECURE_NO_WARNINGS
#include "khmer_segmenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// ============================================================================
// Internal Data Structures (Hash Map & String Utils)
// ============================================================================

typedef struct Entry {
    char* key;
    float value;
    struct Entry* next;
} Entry;

typedef struct {
    Entry** buckets;
    size_t size;
    size_t count;
} HashMap;

static size_t hash_str(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *(unsigned char*)str++))
        hash = ((hash << 5) + hash) + c; 
    return hash;
}

static size_t hash_str_len(const char* str, size_t len) {
    size_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)str[i];
    }
    return hash;
}

static HashMap* hashmap_create(size_t size) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    map->size = size;
    map->count = 0;
    map->buckets = (Entry**)calloc(size, sizeof(Entry*));
    return map;
}

static void hashmap_put(HashMap* map, const char* key, float value) {
    size_t index = hash_str(key) % map->size;
    Entry* entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // New entry
    Entry* new_entry = (Entry*)malloc(sizeof(Entry));
    new_entry->key = _strdup(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->count++;
}

static int hashmap_get(HashMap* map, const char* key, float* out_value) {
    size_t index = hash_str(key) % map->size;
    Entry* entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            *out_value = entry->value;
            return 1; // Found
        }
        entry = entry->next;
    }
    return 0; // Not found
}

// Optimized lookup avoiding memcpy
static int hashmap_get_len(HashMap* map, const char* key_ptr, size_t len, float* out_value) {
    size_t index = hash_str_len(key_ptr, len) % map->size;
    Entry* entry = map->buckets[index];
    while (entry) {
        if (strncmp(entry->key, key_ptr, len) == 0 && entry->key[len] == 0) {
            *out_value = entry->value;
            return 1; 
        }
        entry = entry->next;
    }
    return 0;
}

static void hashmap_free(HashMap* map) {
    for (size_t i = 0; i < map->size; i++) {
        Entry* entry = map->buckets[i];
        while (entry) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}

// ============================================================================
// UTF-8 Helper Utils
// ============================================================================

static int utf8_decode(const char* str, int* out_codepoint) {
    unsigned char c = (unsigned char)str[0];
    if (c < 0x80) {
        *out_codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *out_codepoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *out_codepoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        *out_codepoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *out_codepoint = 0;
    return 1; 
}

// ============================================================================
// Segmenter Logic
// ============================================================================

#include "khmer_normalization.h"
#include "khmer_rule_engine.h"

struct KhmerSegmenter {
    HashMap* word_costs;
    size_t max_word_length;
    float default_cost;
    float unknown_cost;
    RuleEngine* rule_engine; 
};

// --- Helper Functions ---

static int is_khmer_char(int cp) {
    return (cp >= 0x1780 && cp <= 0x17FF) || (cp >= 0x19E0 && cp <= 0x19FF);
}

static int is_digit_cp(int cp) {
    // 0-9
    if (cp >= 0x30 && cp <= 0x39) return 1;
    // Khmer Digits
    if (cp >= 0x17E0 && cp <= 0x17E9) return 1;
    return 0;
}

static int is_separator_cp(int cp) {
    // Khmer Punctuation
    if (cp >= 0x17D4 && cp <= 0x17DA) return 1;
    // Basic ASCII Punctuation & Space
    if (cp < 0x80 && (ispunct(cp) || isspace(cp))) return 1;
    return 0;
}

static int is_valid_single_base_char(int cp) {
    // Consonants: 0x1780 - 0x17A2
    if (cp >= 0x1780 && cp <= 0x17A2) return 1;
    // Independent Vowels: 0x17A3 - 0x17B3
    if (cp >= 0x17A3 && cp <= 0x17B3) return 1;
    return 0;
}

static int get_khmer_cluster_length(const char* text, size_t n, size_t start_idx, size_t* out_bytes) {
    size_t i = start_idx;
    if (i >= n) return 0;

    int cp;
    int len = utf8_decode(text + i, &cp);
    
    // Must start with Base or Indep Vowel
    if (!(cp >= 0x1780 && cp <= 0x17B3)) {
        *out_bytes = len;
        // If it's a coeng/vowel at start, invalid but consume 1 char
        return 1; 
    }
    
    i += len;
    
    // Look ahead for Subscripts and Vowels
    while (i < n) {
        int next_cp;
        int next_len = utf8_decode(text + i, &next_cp);
        
        // Coeng (0x17D2) handling
        if (next_cp == 0x17D2) {
            // Check next next
            if (i + next_len < n) {
                int sub_cp;
                int sub_len = utf8_decode(text + i + next_len, &sub_cp);
                if (sub_cp >= 0x1780 && sub_cp <= 0x17A2) {
                    i += next_len + sub_len;
                    continue;
                }
            }
            break; // Trailing coeng or invalid
        }
        
        // Vowels/Signs
        // 0x17B6 - 0x17D1, 0x17D3, 0x17DD
        if ((next_cp >= 0x17B6 && next_cp <= 0x17D1) || next_cp == 0x17D3 || next_cp == 0x17DD) {
             i += next_len;
             continue;
        }
        
        break;
    }
    
    *out_bytes = i - start_idx;
    return 1;
}

static size_t get_number_length(const char* text, size_t n, size_t start_idx) {
    size_t i = start_idx;
    int cp;
    int len = utf8_decode(text + i, &cp);
    
    // Must start with digit
    if (!is_digit_cp(cp)) return 0;
    
    i += len;
    while (i < n) {
        int next_cp;
        int next_len = utf8_decode(text + i, &next_cp);
        
        if (is_digit_cp(next_cp)) {
            i += next_len;
            continue;
        }
        
        // Separators: , . Space
        // Must be followed by digit
        if (next_cp == ',' || next_cp == '.' || next_cp == ' ') {
            if (i + next_len < n) {
                int f_cp;
                int f_len = utf8_decode(text + i + next_len, &f_cp);
                if (is_digit_cp(f_cp)) {
                    i += next_len + f_len; // Consume SEP + DIGIT
                    continue;
                }
            }
        }
        
        break;
    }
    return i - start_idx;
}

// --- Init ---

KhmerSegmenter* khmer_segmenter_init(const char* dictionary_path, const char* frequency_path) {
    (void)frequency_path; // Unused for now
    KhmerSegmenter* seg = (KhmerSegmenter*)malloc(sizeof(KhmerSegmenter));
    // Optimization: Increased hashmap size for reduced collisions (80k words -> 131k slots)
    seg->word_costs = hashmap_create(131072);
    seg->max_word_length = 0;
    seg->default_cost = 10.0f;
    seg->unknown_cost = 20.0f;
    
    seg->rule_engine = rule_engine_init(NULL);

    FILE* f_dict = fopen(dictionary_path, "r");
    if (f_dict) {
        printf("Loading dictionary from %s...\n", dictionary_path);
        char line[256]; 
        int count = 0;
        while (fgets(line, sizeof(line), f_dict)) {
            char* p = line;
            while (*p) { if (*p == '\r' || *p == '\n') *p = 0; else p++; }
            if (strlen(line) == 0) continue;
            hashmap_put(seg->word_costs, line, seg->default_cost);
            if (strlen(line) > seg->max_word_length) seg->max_word_length = strlen(line);
            count++;
            if (count % 10000 == 0) printf("\rLoaded %d words...", count);
        }
        printf("\nLoaded total %d words.\n", count);
        fclose(f_dict);
    } else {
        fprintf(stderr, "Error loading dictionary: %s\n", dictionary_path);
    }

    return seg;
}

// --- Segmentation (Viterbi) ---

// Struct for DP state
typedef struct {
    float cost;
    int prev_idx;
} State;

char* khmer_segmenter_segment(KhmerSegmenter* seg, const char* raw_text, const char* separator) {
    if (!raw_text || !*raw_text) return _strdup("");
    if (!separator) separator = "\xE2\x80\x8B"; 

    // 0. Normalize
    char* text = khmer_normalize(raw_text);

    size_t n = strlen(text);
    State* dp = (State*)malloc((n + 1) * sizeof(State));
    
    for (size_t i = 0; i <= n; i++) {
        dp[i].cost = 1e9f; 
        dp[i].prev_idx = -1;
    }
    dp[0].cost = 0;

    for (size_t i = 0; i < n; i++) {
        if (dp[i].cost >= 1e9f) continue;
        
        int cp;
        int char_len = utf8_decode(text + i, &cp);

        // 1. Number / Currency Grouping
        int is_dig = is_digit_cp(cp);
        // Check for currency start ($50)
        int is_curr_start = 0;
        if ((cp == '$' || cp == 0x17DB || cp == 0x20AC || cp == 0xA3 || cp == 0xA5) && i + char_len < n) {
             int next_c;
             int next_l = utf8_decode(text + i + char_len, &next_c);
             if (is_digit_cp(next_c)) is_curr_start = 1;
        }

        if (is_dig || is_curr_start) {
             size_t num_len = get_number_length(text, n, i);
             size_t next_idx = i + num_len;
             float step_cost = 1.0f;
             if (next_idx <= n && dp[i].cost + step_cost < dp[next_idx].cost) {
                 dp[next_idx].cost = dp[i].cost + step_cost;
                 dp[next_idx].prev_idx = (int)i;
             }
        }
        // 2. Separators
        else if (is_separator_cp(cp)) {
             size_t next_idx = i + char_len;
             float step_cost = 0.1f; // Cheap
             if (dp[i].cost + step_cost < dp[next_idx].cost) {
                 dp[next_idx].cost = dp[i].cost + step_cost;
                 dp[next_idx].prev_idx = (int)i;
             }
        }

        // 3. Dictionary Match
        size_t end_limit = i + seg->max_word_length;
        if (end_limit > n) end_limit = n;

        // Optimized lookup inner loop
        for (size_t j = i + 1; j <= end_limit; j++) {
            size_t len = j - i;
            float cost;
            // Use get_len to avoid malloc/memcpy
            if (hashmap_get_len(seg->word_costs, text + i, len, &cost)) {
                float new_cost = dp[i].cost + cost;
                if (new_cost < dp[j].cost) {
                    dp[j].cost = new_cost;
                    dp[j].prev_idx = (int)i;
                }
            }
        }
        
        // 4. Unknown
        size_t cluster_bytes = 0;
        int is_khmer = is_khmer_char(cp);
        if (is_khmer) get_khmer_cluster_length(text, n, i, &cluster_bytes);
        else cluster_bytes = char_len;

        size_t next_idx = i + cluster_bytes;
        float unk_cost = seg->unknown_cost;
        if (cluster_bytes == char_len && is_khmer) { 
             if (!is_valid_single_base_char(cp)) unk_cost += 10.0f; 
        }
        
        if (next_idx <= n) {
            float new_cost = dp[i].cost + unk_cost;
            // Only update if better than existing (dict/number might have set this)
            if (new_cost < dp[next_idx].cost) {
                dp[next_idx].cost = new_cost;
                dp[next_idx].prev_idx = (int)i;
            }
        }
    }

    // Backtrack & Build Segments List
    int curr = (int)n;
    if (dp[curr].prev_idx == -1) {
        free(dp);
        // Fallback: return copy of normalized text
        return text; // Ownership transfer? No, text was separate.
        // Wait, text was allocated by khmer_normalize.
        // If we return text, we need to make sure outside frees it.
        // khmer_normalize returns malloc'd string.
    }

    int* breaks = (int*)malloc((n + 1) * sizeof(int));
    int count = 0;
    while (curr > 0) {
        breaks[count++] = curr;
        curr = dp[curr].prev_idx;
    }
    breaks[count++] = 0; 
    
    // Build SegmentList
    SegmentList* seg_list = segment_list_create(count);
    for (int k = count - 1; k > 0; k--) {
        int start = breaks[k];
        int end = breaks[k-1];
        int len = end - start;
        char* temp_seg = (char*)malloc(len + 1);
        memcpy(temp_seg, text + start, len);
        temp_seg[len] = 0;
        segment_list_add(seg_list, temp_seg);
        free(temp_seg);
    }
    
    free(breaks);
    free(dp);
    free(text); // Free normalized text
    
    // Apply Rules
    if (seg->rule_engine) {
        rule_engine_apply(seg->rule_engine, seg_list);
    }

    // Concatenate Result
    size_t sep_len = strlen(separator);
    size_t total_len = 0;
    for (size_t i = 0; i < seg_list->count; i++) {
        total_len += strlen(seg_list->items[i]);
        if (i < seg_list->count - 1) total_len += sep_len;
    }
    
    char* final_res = (char*)malloc(total_len + 1);
    char* ptr = final_res;
    for (size_t i = 0; i < seg_list->count; i++) {
        size_t slen = strlen(seg_list->items[i]);
        memcpy(ptr, seg_list->items[i], slen);
        ptr += slen;
        if (i < seg_list->count - 1) {
            memcpy(ptr, separator, sep_len);
            ptr += sep_len;
        }
    }
    *ptr = 0;
    
    segment_list_free(seg_list);
    return final_res;
}

void khmer_segmenter_free(KhmerSegmenter* seg) {
    if (seg) {
        hashmap_free(seg->word_costs);
        if(seg->rule_engine) rule_engine_free(seg->rule_engine);
        free(seg);
    }
}
