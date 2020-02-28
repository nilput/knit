#ifndef KNIT_MEM_STATS_H
#define KNIT_MEM_STATS_H
#include <stdio.h>

struct knit_mem_stats {
    size_t allocations;
    size_t frees;
    size_t reallocations;
    size_t total_now;
};


static void knit_mem_stats_alloc(struct knit_mem_stats *mm, size_t size) {
    mm->allocations++;
    mm->total_now += size;
}
static void knit_mem_stats_free(struct knit_mem_stats *mm, size_t size) {
    mm->frees++;
    mm->total_now -= size;
}
static void knit_mem_stats_realloc(struct knit_mem_stats *mm, size_t prev_size, size_t new_size) {
    if (!prev_size && new_size) {
        knit_mem_stats_alloc(mm, new_size);
    }
    else if (prev_size && !new_size) {
        knit_mem_stats_free(mm, prev_size);
    }
    else {
        mm->total_now -= prev_size;
        mm->total_now += new_size;
        mm->reallocations++;
    }
}
static char *humanbytes(char *buff, int buffsz, size_t bytes) {
    char *m[] = {"B", "KB", "MB", "GB", NULL};
    int i = 0;
    size_t n = bytes;
    while (m[i+1] && n > 1024) {
        i++;
        n /= 1024;
    }
    if (snprintf(buff, buffsz, "%llu %s", (unsigned long long) n, m[i]) >= buffsz)
        buff[0] = 0;
    return buff;
}
static void knit_mem_stats_init(struct knit_mem_stats *mm) {
    memset(mm, 0, sizeof *mm);
}
static void knit_mem_stats_dump(struct knit_mem_stats *mm) {
    char tmpbuff[64];
    humanbytes(tmpbuff, sizeof tmpbuff, mm->total_now);
    fprintf(stderr, "[Memory usage report]\n"
                    "Allocations:         %llu\n"
                    "Frees:               %llu\n"
                    "Reallocations:       %llu\n"
                    "Total in use:        %s\n", 
                     (unsigned long long)mm->allocations,
                     (unsigned long long)mm->frees,
                     (unsigned long long)mm->reallocations,
                     tmpbuff);
}


#ifdef KNIT_MEM_STATS
    #define KMEMSTAT_ALLOC(knit, sz)                 knit_mem_stats_alloc(&knit->mstats, sz)
    #define KMEMSTAT_FREE(knit, sz)                  knit_mem_stats_free(&knit->mstats, sz)
    #define KMEMSTAT_REALLOC(knit, prev_sz, new_sz)  knit_mem_stats_realloc(&knit->mstats, prev_sz, new_sz)
#else
    #define KMEMSTAT_ALLOC(knit, sz) 
    #define KMEMSTAT_FREE(knit, sz)  
    #define KMEMSTAT_REALLOC(knit, prev_sz, new_sz)  
#endif

#endif
