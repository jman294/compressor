// `levenshtein.c` - levenshtein
// MIT licensed.
// Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Returns a size_t, depicting the difference between `a` and `b`.
// See <http://en.wikipedia.org/wiki/Levenshtein_distance> for more information.
size_t
levenshtein_n(const char *a, const size_t length, const char *b, const size_t bLength) {
  size_t *cache = calloc(length, sizeof(size_t));
  size_t index = 0;
  size_t bIndex = 0;
  size_t distance;
  size_t bDistance;
  size_t result;
  char code;

  // Shortcut optimizations / degenerate cases.
  if (a == b) {
    return 0;
  }

  if (length == 0) {
    return bLength;
  }

  if (bLength == 0) {
    return length;
  }

  // initialize the vector.
  while (index < length) {
    cache[index] = index + 1;
    index++;
  }

  // Loop.
  while (bIndex < bLength) {
    code = b[bIndex];
    result = distance = bIndex++;
    index = SIZE_MAX;

    while (++index < length) {
      bDistance = code == a[index] ? distance : distance + 1;
      distance = cache[index];

      cache[index] = result = distance > result
        ? bDistance > result
          ? result + 1
          : bDistance
        : bDistance > distance
          ? distance + 1
          : bDistance;
    }
  }

  free(cache);

  return result;
}

size_t
levenshtein(const char *a, const char *b) {
  const size_t length = strlen(a);
  const size_t bLength = strlen(b);

  return levenshtein_n(a, length, b, bLength);
}
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnetdata.h"

#ifdef __APPLE__
#define INHERIT_NONE 0
#endif /* __APPLE__ */
#if defined(__FreeBSD__) || defined(__APPLE__)
#    define O_NOATIME     0
#    define MADV_DONTFORK INHERIT_NONE
#endif /* __FreeBSD__ || __APPLE__*/

struct rlimit rlimit_nofile = { .rlim_cur = 1024, .rlim_max = 1024 };
int enable_ksm = 1;

volatile sig_atomic_t netdata_exit = 0;
const char *os_type = NETDATA_OS_TYPE;
const char *program_version = VERSION;

// ----------------------------------------------------------------------------
// memory allocation functions that handle failures

// although netdata does not use memory allocations too often (netdata tries to
// maintain its memory footprint stable during runtime, i.e. all buffers are
// allocated during initialization and are adapted to current use throughout
// its lifetime), these can be used to override the default system allocation
// routines.

#ifdef NETDATA_LOG_ALLOCATIONS
static __thread struct memory_statistics {
    volatile ssize_t malloc_calls_made;
    volatile ssize_t calloc_calls_made;
    volatile ssize_t realloc_calls_made;
    volatile ssize_t strdup_calls_made;
    volatile ssize_t free_calls_made;
    volatile ssize_t memory_calls_made;
    volatile ssize_t allocated_memory;
    volatile ssize_t mmapped_memory;
} memory_statistics = { 0, 0, 0, 0, 0, 0, 0, 0 };

__thread size_t log_thread_memory_allocations = 0;

static inline void print_allocations(const char *file, const char *function, const unsigned long line, const char *type, size_t size) {
    static __thread struct memory_statistics old = { 0, 0, 0, 0, 0, 0, 0, 0 };

    fprintf(stderr, "%s iteration %zu MEMORY TRACE: %lu@%s : %s : %s : %zu\n",
            netdata_thread_tag(),
            log_thread_memory_allocations,
            line, file, function,
            type, size
    );

    fprintf(stderr, "%s iteration %zu MEMORY ALLOCATIONS: (%04lu@%-40.40s:%-40.40s): Allocated %zd KiB (%+zd B), mmapped %zd KiB (%+zd B): %s : malloc %zd (%+zd), calloc %zd (%+zd), realloc %zd (%+zd), strdup %zd (%+zd), free %zd (%+zd)\n",
            netdata_thread_tag(),
            log_thread_memory_allocations,
            line, file, function,
            (memory_statistics.allocated_memory + 512) / 1024, memory_statistics.allocated_memory - old.allocated_memory,
            (memory_statistics.mmapped_memory + 512) / 1024, memory_statistics.mmapped_memory - old.mmapped_memory,
            type,
            memory_statistics.malloc_calls_made, memory_statistics.malloc_calls_made - old.malloc_calls_made,
            memory_statistics.calloc_calls_made, memory_statistics.calloc_calls_made - old.calloc_calls_made,
            memory_statistics.realloc_calls_made, memory_statistics.realloc_calls_made - old.realloc_calls_made,
            memory_statistics.strdup_calls_made, memory_statistics.strdup_calls_made - old.strdup_calls_made,
            memory_statistics.free_calls_made, memory_statistics.free_calls_made - old.free_calls_made
    );

    memcpy(&old, &memory_statistics, sizeof(struct memory_statistics));
}

static inline void mmap_accounting(size_t size) {
    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.mmapped_memory += size;
    }
}

void *mallocz_int(const char *file, const char *function, const unsigned long line, size_t size) {
    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.malloc_calls_made++;
        memory_statistics.allocated_memory += size;
        print_allocations(file, function, line, "malloc()", size);
    }

    size_t *n = (size_t *)malloc(sizeof(size_t) + size);
    if (unlikely(!n)) fatal("mallocz() cannot allocate %zu bytes of memory.", size);
    *n = size;
    return (void *)&n[1];
}

void *callocz_int(const char *file, const char *function, const unsigned long line, size_t nmemb, size_t size) {
    size = nmemb * size;

    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.calloc_calls_made++;
        memory_statistics.allocated_memory += size;
        print_allocations(file, function, line, "calloc()", size);
    }

    size_t *n = (size_t *)calloc(1, sizeof(size_t) + size);
    if (unlikely(!n)) fatal("callocz() cannot allocate %zu bytes of memory.", size);
    *n = size;
    return (void *)&n[1];
}

void *reallocz_int(const char *file, const char *function, const unsigned long line, void *ptr, size_t size) {
    if(!ptr) return mallocz_int(file, function, line, size);

    size_t *n = (size_t *)ptr;
    n--;
    size_t old_size = *n;

    n = realloc(n, sizeof(size_t) + size);
    if (unlikely(!n)) fatal("reallocz() cannot allocate %zu bytes of memory (from %zu bytes).", size, old_size);

    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.realloc_calls_made++;
        memory_statistics.allocated_memory += (size - old_size);
        print_allocations(file, function, line, "realloc()", size - old_size);
    }

    *n = size;
    return (void *)&n[1];
}

char *strdupz_int(const char *file, const char *function, const unsigned long line, const char *s) {
    size_t size = strlen(s) + 1;

    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.strdup_calls_made++;
        memory_statistics.allocated_memory += size;
        print_allocations(file, function, line, "strdup()", size);
    }

    size_t *n = (size_t *)malloc(sizeof(size_t) + size);
    if (unlikely(!n)) fatal("strdupz() cannot allocate %zu bytes of memory.", size);

    *n = size;
    char *t = (char *)&n[1];
    strcpy(t, s);
    return t;
}

void freez_int(const char *file, const char *function, const unsigned long line, void *ptr) {
    if(unlikely(!ptr)) return;

    size_t *n = (size_t *)ptr;
    n--;
    size_t size = *n;

    if(log_thread_memory_allocations) {
        memory_statistics.memory_calls_made++;
        memory_statistics.free_calls_made++;
        memory_statistics.allocated_memory -= size;
        print_allocations(file, function, line, "free()", size);
    }

    free(n);
}
#else

char *strdupz(const char *s) {
    char *t = strdup(s);
    if (unlikely(!t)) fatal("Cannot strdup() string '%s'", s);
    return t;
}

void freez(void *ptr) {
    free(ptr);
}

void *mallocz(size_t size) {
    void *p = malloc(size);
    if (unlikely(!p)) fatal("Cannot allocate %zu bytes of memory.", size);
    return p;
}

void *callocz(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    if (unlikely(!p)) fatal("Cannot allocate %zu bytes of memory.", nmemb * size);
    return p;
}

void *reallocz(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (unlikely(!p)) fatal("Cannot re-allocate memory to %zu bytes.", size);
    return p;
}

#endif

// --------------------------------------------------------------------------------------------------------------------

void json_escape_string(char *dst, const char *src, size_t size) {
    const char *t;
    char *d = dst, *e = &dst[size - 1];

    for(t = src; *t && d < e ;t++) {
        if(unlikely(*t == '\\' || *t == '"')) {
            if(unlikely(d + 1 >= e)) break;
            *d++ = '\\';
        }
        *d++ = *t;
    }

    *d = '\0';
}

void json_fix_string(char *s) {
    unsigned char c;
    while((c = (unsigned char)*s)) {
        if(unlikely(c == '\\'))
            *s++ = '/';
        else if(unlikely(c == '"'))
            *s++ = '\'';
        else if(unlikely(isspace(c) || iscntrl(c)))
            *s++ = ' ';
        else if(unlikely(!isprint(c) || c > 127))
            *s++ = '_';
        else
            s++;
    }
}

unsigned char netdata_map_chart_names[256] = {
        [0] = '\0', //
        [1] = '_', //
        [2] = '_', //
        [3] = '_', //
        [4] = '_', //
        [5] = '_', //
        [6] = '_', //
        [7] = '_', //
        [8] = '_', //
        [9] = '_', //
        [10] = '_', //
        [11] = '_', //
        [12] = '_', //
        [13] = '_', //
        [14] = '_', //
        [15] = '_', //
        [16] = '_', //
        [17] = '_', //
        [18] = '_', //
        [19] = '_', //
        [20] = '_', //
        [21] = '_', //
        [22] = '_', //
        [23] = '_', //
        [24] = '_', //
        [25] = '_', //
        [26] = '_', //
        [27] = '_', //
        [28] = '_', //
        [29] = '_', //
        [30] = '_', //
        [31] = '_', //
        [32] = '_', //
        [33] = '_', // !
        [34] = '_', // "
        [35] = '_', // #
        [36] = '_', // $
        [37] = '_', // %
        [38] = '_', // &
        [39] = '_', // '
        [40] = '_', // (
        [41] = '_', // )
        [42] = '_', // *
        [43] = '_', // +
        [44] = '.', // ,
        [45] = '-', // -
        [46] = '.', // .
        [47] = '/', // /
        [48] = '0', // 0
        [49] = '1', // 1
        [50] = '2', // 2
        [51] = '3', // 3
        [52] = '4', // 4
        [53] = '5', // 5
        [54] = '6', // 6
        [55] = '7', // 7
        [56] = '8', // 8
        [57] = '9', // 9
        [58] = '_', // :
        [59] = '_', // ;
        [60] = '_', // <
        [61] = '_', // =
        [62] = '_', // >
        [63] = '_', // ?
        [64] = '_', // @
        [65] = 'a', // A
        [66] = 'b', // B
        [67] = 'c', // C
        [68] = 'd', // D
        [69] = 'e', // E
        [70] = 'f', // F
        [71] = 'g', // G
        [72] = 'h', // H
        [73] = 'i', // I
        [74] = 'j', // J
        [75] = 'k', // K
        [76] = 'l', // L
        [77] = 'm', // M
        [78] = 'n', // N
        [79] = 'o', // O
        [80] = 'p', // P
        [81] = 'q', // Q
        [82] = 'r', // R
        [83] = 's', // S
        [84] = 't', // T
        [85] = 'u', // U
        [86] = 'v', // V
        [87] = 'w', // W
        [88] = 'x', // X
        [89] = 'y', // Y
        [90] = 'z', // Z
        [91] = '_', // [
        [92] = '/', // backslash
        [93] = '_', // ]
        [94] = '_', // ^
        [95] = '_', // _
        [96] = '_', // `
        [97] = 'a', // a
        [98] = 'b', // b
        [99] = 'c', // c
        [100] = 'd', // d
        [101] = 'e', // e
        [102] = 'f', // f
        [103] = 'g', // g
        [104] = 'h', // h
        [105] = 'i', // i
        [106] = 'j', // j
        [107] = 'k', // k
        [108] = 'l', // l
        [109] = 'm', // m
        [110] = 'n', // n
        [111] = 'o', // o
        [112] = 'p', // p
        [113] = 'q', // q
        [114] = 'r', // r
        [115] = 's', // s
        [116] = 't', // t
        [117] = 'u', // u
        [118] = 'v', // v
        [119] = 'w', // w
        [120] = 'x', // x
        [121] = 'y', // y
        [122] = 'z', // z
        [123] = '_', // {
        [124] = '_', // |
        [125] = '_', // }
        [126] = '_', // ~
        [127] = '_', //
        [128] = '_', //
        [129] = '_', //
        [130] = '_', //
        [131] = '_', //
        [132] = '_', //
        [133] = '_', //
        [134] = '_', //
        [135] = '_', //
        [136] = '_', //
        [137] = '_', //
        [138] = '_', //
        [139] = '_', //
        [140] = '_', //
        [141] = '_', //
        [142] = '_', //
        [143] = '_', //
        [144] = '_', //
        [145] = '_', //
        [146] = '_', //
        [147] = '_', //
        [148] = '_', //
        [149] = '_', //
        [150] = '_', //
        [151] = '_', //
        [152] = '_', //
        [153] = '_', //
        [154] = '_', //
        [155] = '_', //
        [156] = '_', //
        [157] = '_', //
        [158] = '_', //
        [159] = '_', //
        [160] = '_', //
        [161] = '_', //
        [162] = '_', //
        [163] = '_', //
        [164] = '_', //
        [165] = '_', //
        [166] = '_', //
        [167] = '_', //
        [168] = '_', //
        [169] = '_', //
        [170] = '_', //
        [171] = '_', //
        [172] = '_', //
        [173] = '_', //
        [174] = '_', //
        [175] = '_', //
        [176] = '_', //
        [177] = '_', //
        [178] = '_', //
        [179] = '_', //
        [180] = '_', //
        [181] = '_', //
        [182] = '_', //
        [183] = '_', //
        [184] = '_', //
        [185] = '_', //
        [186] = '_', //
        [187] = '_', //
        [188] = '_', //
        [189] = '_', //
        [190] = '_', //
        [191] = '_', //
        [192] = '_', //
        [193] = '_', //
        [194] = '_', //
        [195] = '_', //
        [196] = '_', //
        [197] = '_', //
        [198] = '_', //
        [199] = '_', //
        [200] = '_', //
        [201] = '_', //
        [202] = '_', //
        [203] = '_', //
        [204] = '_', //
        [205] = '_', //
        [206] = '_', //
        [207] = '_', //
        [208] = '_', //
        [209] = '_', //
        [210] = '_', //
        [211] = '_', //
        [212] = '_', //
        [213] = '_', //
        [214] = '_', //
        [215] = '_', //
        [216] = '_', //
        [217] = '_', //
        [218] = '_', //
        [219] = '_', //
        [220] = '_', //
        [221] = '_', //
        [222] = '_', //
        [223] = '_', //
        [224] = '_', //
        [225] = '_', //
        [226] = '_', //
        [227] = '_', //
        [228] = '_', //
        [229] = '_', //
        [230] = '_', //
        [231] = '_', //
        [232] = '_', //
        [233] = '_', //
        [234] = '_', //
        [235] = '_', //
        [236] = '_', //
        [237] = '_', //
        [238] = '_', //
        [239] = '_', //
        [240] = '_', //
        [241] = '_', //
        [242] = '_', //
        [243] = '_', //
        [244] = '_', //
        [245] = '_', //
        [246] = '_', //
        [247] = '_', //
        [248] = '_', //
        [249] = '_', //
        [250] = '_', //
        [251] = '_', //
        [252] = '_', //
        [253] = '_', //
        [254] = '_', //
        [255] = '_'  //
};

// make sure the supplied string
// is good for a netdata chart/dimension ID/NAME
void netdata_fix_chart_name(char *s) {
    while ((*s = netdata_map_chart_names[(unsigned char) *s])) s++;
}

unsigned char netdata_map_chart_ids[256] = {
        [0] = '\0', //
        [1] = '_', //
        [2] = '_', //
        [3] = '_', //
        [4] = '_', //
        [5] = '_', //
        [6] = '_', //
        [7] = '_', //
        [8] = '_', //
        [9] = '_', //
        [10] = '_', //
        [11] = '_', //
        [12] = '_', //
        [13] = '_', //
        [14] = '_', //
        [15] = '_', //
        [16] = '_', //
        [17] = '_', //
        [18] = '_', //
        [19] = '_', //
        [20] = '_', //
        [21] = '_', //
        [22] = '_', //
        [23] = '_', //
        [24] = '_', //
        [25] = '_', //
        [26] = '_', //
        [27] = '_', //
        [28] = '_', //
        [29] = '_', //
        [30] = '_', //
        [31] = '_', //
        [32] = '_', //
        [33] = '_', // !
        [34] = '_', // "
        [35] = '_', // #
        [36] = '_', // $
        [37] = '_', // %
        [38] = '_', // &
        [39] = '_', // '
        [40] = '_', // (
        [41] = '_', // )
        [42] = '_', // *
        [43] = '_', // +
        [44] = '.', // ,
        [45] = '-', // -
        [46] = '.', // .
        [47] = '_', // /
        [48] = '0', // 0
        [49] = '1', // 1
        [50] = '2', // 2
        [51] = '3', // 3
        [52] = '4', // 4
        [53] = '5', // 5
        [54] = '6', // 6
        [55] = '7', // 7
        [56] = '8', // 8
        [57] = '9', // 9
        [58] = '_', // :
        [59] = '_', // ;
        [60] = '_', // <
        [61] = '_', // =
        [62] = '_', // >
        [63] = '_', // ?
        [64] = '_', // @
        [65] = 'a', // A
        [66] = 'b', // B
        [67] = 'c', // C
        [68] = 'd', // D
        [69] = 'e', // E
        [70] = 'f', // F
        [71] = 'g', // G
        [72] = 'h', // H
        [73] = 'i', // I
        [74] = 'j', // J
        [75] = 'k', // K
        [76] = 'l', // L
        [77] = 'm', // M
        [78] = 'n', // N
        [79] = 'o', // O
        [80] = 'p', // P
        [81] = 'q', // Q
        [82] = 'r', // R
        [83] = 's', // S
        [84] = 't', // T
        [85] = 'u', // U
        [86] = 'v', // V
        [87] = 'w', // W
        [88] = 'x', // X
        [89] = 'y', // Y
        [90] = 'z', // Z
        [91] = '_', // [
        [92] = '/', // backslash
        [93] = '_', // ]
        [94] = '_', // ^
        [95] = '_', // _
        [96] = '_', // `
        [97] = 'a', // a
        [98] = 'b', // b
        [99] = 'c', // c
        [100] = 'd', // d
        [101] = 'e', // e
        [102] = 'f', // f
        [103] = 'g', // g
        [104] = 'h', // h
        [105] = 'i', // i
        [106] = 'j', // j
        [107] = 'k', // k
        [108] = 'l', // l
        [109] = 'm', // m
        [110] = 'n', // n
        [111] = 'o', // o
        [112] = 'p', // p
        [113] = 'q', // q
        [114] = 'r', // r
        [115] = 's', // s
        [116] = 't', // t
        [117] = 'u', // u
        [118] = 'v', // v
        [119] = 'w', // w
        [120] = 'x', // x
        [121] = 'y', // y
        [122] = 'z', // z
        [123] = '_', // {
        [124] = '_', // |
        [125] = '_', // }
        [126] = '_', // ~
        [127] = '_', //
        [128] = '_', //
        [129] = '_', //
        [130] = '_', //
        [131] = '_', //
        [132] = '_', //
        [133] = '_', //
        [134] = '_', //
        [135] = '_', //
        [136] = '_', //
        [137] = '_', //
        [138] = '_', //
        [139] = '_', //
        [140] = '_', //
        [141] = '_', //
        [142] = '_', //
        [143] = '_', //
        [144] = '_', //
        [145] = '_', //
        [146] = '_', //
        [147] = '_', //
        [148] = '_', //
        [149] = '_', //
        [150] = '_', //
        [151] = '_', //
        [152] = '_', //
        [153] = '_', //
        [154] = '_', //
        [155] = '_', //
        [156] = '_', //
        [157] = '_', //
        [158] = '_', //
        [159] = '_', //
        [160] = '_', //
        [161] = '_', //
        [162] = '_', //
        [163] = '_', //
        [164] = '_', //
        [165] = '_', //
        [166] = '_', //
        [167] = '_', //
        [168] = '_', //
        [169] = '_', //
        [170] = '_', //
        [171] = '_', //
        [172] = '_', //
        [173] = '_', //
        [174] = '_', //
        [175] = '_', //
        [176] = '_', //
        [177] = '_', //
        [178] = '_', //
        [179] = '_', //
        [180] = '_', //
        [181] = '_', //
        [182] = '_', //
        [183] = '_', //
        [184] = '_', //
        [185] = '_', //
        [186] = '_', //
        [187] = '_', //
        [188] = '_', //
        [189] = '_', //
        [190] = '_', //
        [191] = '_', //
        [192] = '_', //
        [193] = '_', //
        [194] = '_', //
        [195] = '_', //
        [196] = '_', //
        [197] = '_', //
        [198] = '_', //
        [199] = '_', //
        [200] = '_', //
        [201] = '_', //
        [202] = '_', //
        [203] = '_', //
        [204] = '_', //
        [205] = '_', //
        [206] = '_', //
        [207] = '_', //
        [208] = '_', //
        [209] = '_', //
        [210] = '_', //
        [211] = '_', //
        [212] = '_', //
        [213] = '_', //
        [214] = '_', //
        [215] = '_', //
        [216] = '_', //
        [217] = '_', //
        [218] = '_', //
        [219] = '_', //
        [220] = '_', //
        [221] = '_', //
        [222] = '_', //
        [223] = '_', //
        [224] = '_', //
        [225] = '_', //
        [226] = '_', //
        [227] = '_', //
        [228] = '_', //
        [229] = '_', //
        [230] = '_', //
        [231] = '_', //
        [232] = '_', //
        [233] = '_', //
        [234] = '_', //
        [235] = '_', //
        [236] = '_', //
        [237] = '_', //
        [238] = '_', //
        [239] = '_', //
        [240] = '_', //
        [241] = '_', //
        [242] = '_', //
        [243] = '_', //
        [244] = '_', //
        [245] = '_', //
        [246] = '_', //
        [247] = '_', //
        [248] = '_', //
        [249] = '_', //
        [250] = '_', //
        [251] = '_', //
        [252] = '_', //
        [253] = '_', //
        [254] = '_', //
        [255] = '_'  //
};

// make sure the supplied string
// is good for a netdata chart/dimension ID/NAME
void netdata_fix_chart_id(char *s) {
    while ((*s = netdata_map_chart_ids[(unsigned char) *s])) s++;
}

/*
// http://stackoverflow.com/questions/7666509/hash-function-for-string
uint32_t simple_hash(const char *name)
{
    const char *s = name;
    uint32_t hash = 5381;
    int i;

    while((i = *s++)) hash = ((hash << 5) + hash) + i;

    // fprintf(stderr, "HASH: %lu %s\n", hash, name);

    return hash;
}
*/

/*
// http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
uint32_t simple_hash(const char *name) {
    unsigned char *s = (unsigned char *) name;
    uint32_t hval = 0x811c9dc5;

    // FNV-1a algorithm
    while (*s) {
        // multiply by the 32 bit FNV magic prime mod 2^32
        // NOTE: No need to optimize with left shifts.
        //       GCC will use imul instruction anyway.
        //       Tested with 'gcc -O3 -S'
        //hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        hval *= 16777619;

        // xor the bottom with the current octet
        hval ^= (uint32_t) *s++;
    }

    // fprintf(stderr, "HASH: %u = %s\n", hval, name);
    return hval;
}

uint32_t simple_uhash(const char *name) {
    unsigned char *s = (unsigned char *) name;
    uint32_t hval = 0x811c9dc5, c;

    // FNV-1a algorithm
    while ((c = *s++)) {
        if (unlikely(c >= 'A' && c <= 'Z')) c += 'a' - 'A';
        hval *= 16777619;
        hval ^= c;
    }
    return hval;
}
*/

/*
// http://eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx
// one at a time hash
uint32_t simple_hash(const char *name) {
    unsigned char *s = (unsigned char *)name;
    uint32_t h = 0;

    while(*s) {
        h += *s++;
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    // fprintf(stderr, "HASH: %u = %s\n", h, name);

    return h;
}
*/

void strreverse(char *begin, char *end) {
    while (end > begin) {
        // clearer code.
        char aux = *end;
        *end-- = *begin;
        *begin++ = aux;
    }
}

char *strsep_on_1char(char **ptr, char c) {
    if(unlikely(!ptr || !*ptr))
        return NULL;

    // remember the position we started
    char *s = *ptr;

    // skip separators in front
    while(*s == c) s++;
    char *ret = s;

    // find the next separator
    while(*s++) {
        if(unlikely(*s == c)) {
            *s++ = '\0';
            *ptr = s;
            return ret;
        }
    }

    *ptr = NULL;
    return ret;
}

char *mystrsep(char **ptr, char *s) {
    char *p = "";
    while (p && !p[0] && *ptr) p = strsep(ptr, s);
    return (p);
}

char *trim(char *s) {
    // skip leading spaces
    while (*s && isspace(*s)) s++;
    if (!*s) return NULL;

    // skip tailing spaces
    // this way is way faster. Writes only one NUL char.
    ssize_t l = strlen(s);
    if (--l >= 0) {
        char *p = s + l;
        while (p > s && isspace(*p)) p--;
        *++p = '\0';
    }

    if (!*s) return NULL;

    return s;
}

inline char *trim_all(char *buffer) {
    char *d = buffer, *s = buffer;

    // skip spaces
    while(isspace(*s)) s++;

    while(*s) {
        // copy the non-space part
        while(*s && !isspace(*s)) *d++ = *s++;

        // add a space if we have to
        if(*s && isspace(*s)) {
            *d++ = ' ';
            s++;
        }

        // skip spaces
        while(isspace(*s)) s++;
    }

    *d = '\0';

    if(d > buffer) {
        d--;
        if(isspace(*d)) *d = '\0';
    }

    if(!buffer[0]) return NULL;
    return buffer;
}

static int memory_file_open(const char *filename, size_t size) {
    // info("memory_file_open('%s', %zu", filename, size);

    int fd = open(filename, O_RDWR | O_CREAT | O_NOATIME, 0664);
    if (fd != -1) {
        if (lseek(fd, size, SEEK_SET) == (off_t) size) {
            if (write(fd, "", 1) == 1) {
                if (ftruncate(fd, size))
                    error("Cannot truncate file '%s' to size %zu. Will use the larger file.", filename, size);
            }
            else error("Cannot write to file '%s' at position %zu.", filename, size);
        }
        else error("Cannot seek file '%s' to size %zu.", filename, size);
    }
    else error("Cannot create/open file '%s'.", filename);

    return fd;
}

// mmap_shared is used for memory mode = map
static void *memory_file_mmap(const char *filename, size_t size, int flags) {
    // info("memory_file_mmap('%s', %zu", filename, size);
    static int log_madvise = 1;

    int fd = -1;
    if(filename) {
        fd = memory_file_open(filename, size);
        if(fd == -1) return MAP_FAILED;
    }

    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (mem != MAP_FAILED) {
#ifdef NETDATA_LOG_ALLOCATIONS
        mmap_accounting(size);
#endif
        int advise = MADV_SEQUENTIAL | MADV_DONTFORK;
        if (flags & MAP_SHARED) advise |= MADV_WILLNEED;

        if (madvise(mem, size, advise) != 0 && log_madvise) {
            error("Cannot advise the kernel about shared memory usage.");
            log_madvise--;
        }
    }

    if(fd != -1)
        close(fd);

    return mem;
}

#ifdef MADV_MERGEABLE
static void *memory_file_mmap_ksm(const char *filename, size_t size, int flags) {
    // info("memory_file_mmap_ksm('%s', %zu", filename, size);
    static int log_madvise_2 = 1, log_madvise_3 = 1;

    int fd = -1;
    if(filename) {
        fd = memory_file_open(filename, size);
        if(fd == -1) return MAP_FAILED;
    }

    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, flags | MAP_ANONYMOUS, -1, 0);
    if (mem != MAP_FAILED) {
#ifdef NETDATA_LOG_ALLOCATIONS
        mmap_accounting(size);
#endif
        if(fd != -1) {
            if (lseek(fd, 0, SEEK_SET) == 0) {
                if (read(fd, mem, size) != (ssize_t) size)
                    error("Cannot read from file '%s'", filename);
            }
            else error("Cannot seek to beginning of file '%s'.", filename);
        }

        // don't use MADV_SEQUENTIAL|MADV_DONTFORK, they disable MADV_MERGEABLE
        if (madvise(mem, size, MADV_SEQUENTIAL | MADV_DONTFORK) != 0 && log_madvise_2) {
            error("Cannot advise the kernel about the memory usage (MADV_SEQUENTIAL|MADV_DONTFORK) of file '%s'.", filename);
            log_madvise_2--;
        }

        if (madvise(mem, size, MADV_MERGEABLE) != 0 && log_madvise_3) {
            error("Cannot advise the kernel about the memory usage (MADV_MERGEABLE) of file '%s'.", filename);
            log_madvise_3--;
        }
    }

    if(fd != -1)
        close(fd);

    return mem;
}
#else
static void *memory_file_mmap_ksm(const char *filename, size_t size, int flags) {
    // info("memory_file_mmap_ksm FALLBACK ('%s', %zu", filename, size);

    if(filename)
        return memory_file_mmap(filename, size, flags);

    // when KSM is not available and no filename is given (memory mode = ram),
    // we just report failure
    return MAP_FAILED;
}
#endif

void *mymmap(const char *filename, size_t size, int flags, int ksm) {
    void *mem = NULL;

    if (filename && (flags & MAP_SHARED || !enable_ksm || !ksm))
        // memory mode = map | save
        // when KSM is not enabled
        // MAP_SHARED is used for memory mode = map (no KSM possible)
        mem = memory_file_mmap(filename, size, flags);

    else
        // memory mode = save | ram
        // when KSM is enabled
        // for memory mode = ram, the filename is NULL
        mem = memory_file_mmap_ksm(filename, size, flags);

    if(mem == MAP_FAILED) return NULL;

    errno = 0;
    return mem;
}

int memory_file_save(const char *filename, void *mem, size_t size) {
    char tmpfilename[FILENAME_MAX + 1];

    snprintfz(tmpfilename, FILENAME_MAX, "%s.%ld.tmp", filename, (long) getpid());

    int fd = open(tmpfilename, O_RDWR | O_CREAT | O_NOATIME, 0664);
    if (fd < 0) {
        error("Cannot create/open file '%s'.", filename);
        return -1;
    }

    if (write(fd, mem, size) != (ssize_t) size) {
        error("Cannot write to file '%s' %ld bytes.", filename, (long) size);
        close(fd);
        return -1;
    }

    close(fd);

    if (rename(tmpfilename, filename)) {
        error("Cannot rename '%s' to '%s'", tmpfilename, filename);
        return -1;
    }

    return 0;
}

int fd_is_valid(int fd) {
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

char *fgets_trim_len(char *buf, size_t buf_size, FILE *fp, size_t *len) {
    char *s = fgets(buf, (int)buf_size, fp);
    if (!s) return NULL;

    char *t = s;
    if (*t != '\0') {
        // find the string end
        while (*++t != '\0');

        // trim trailing spaces/newlines/tabs
        while (--t > s && *t == '\n')
            *t = '\0';
    }

    if (len)
        *len = t - s + 1;

    return s;
}

int vsnprintfz(char *dst, size_t n, const char *fmt, va_list args) {
    int size = vsnprintf(dst, n, fmt, args);

    if (unlikely((size_t) size > n)) {
        // truncated
        size = (int)n;
    }

    dst[size] = '\0';
    return size;
}

int snprintfz(char *dst, size_t n, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int ret = vsnprintfz(dst, n, fmt, args);
    va_end(args);

    return ret;
}

/*
// poor man cycle counting
static unsigned long tsc;
void begin_tsc(void) {
    unsigned long a, d;
    asm volatile ("cpuid\nrdtsc" : "=a" (a), "=d" (d) : "0" (0) : "ebx", "ecx");
    tsc = ((unsigned long)d << 32) | (unsigned long)a;
}
unsigned long end_tsc(void) {
    unsigned long a, d;
    asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "ecx");
    return (((unsigned long)d << 32) | (unsigned long)a) - tsc;
}
*/

int recursively_delete_dir(const char *path, const char *reason) {
    DIR *dir = opendir(path);
    if(!dir) {
        error("Cannot read %s directory to be deleted '%s'", reason?reason:"", path);
        return -1;
    }

    int ret = 0;
    struct dirent *de = NULL;
    while((de = readdir(dir))) {
        if(de->d_type == DT_DIR
           && (
                   (de->d_name[0] == '.' && de->d_name[1] == '\0')
                   || (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')
           ))
            continue;

        char fullpath[FILENAME_MAX + 1];
        snprintfz(fullpath, FILENAME_MAX, "%s/%s", path, de->d_name);

        if(de->d_type == DT_DIR) {
            int r = recursively_delete_dir(fullpath, reason);
            if(r > 0) ret += r;
            continue;
        }

        info("Deleting %s file '%s'", reason?reason:"", fullpath);
        if(unlikely(unlink(fullpath) == -1))
            error("Cannot delete %s file '%s'", reason?reason:"", fullpath);
        else
            ret++;
    }

    info("Deleting empty directory '%s'", path);
    if(unlikely(rmdir(path) == -1))
        error("Cannot delete empty directory '%s'", path);
    else
        ret++;

    closedir(dir);

    return ret;
}

static int is_virtual_filesystem(const char *path, char **reason) {

#if defined(__APPLE__) || defined(__FreeBSD__)
    (void)path;
    (void)reason;
#else
    struct statfs stat;
    // stat.f_fsid.__val[0] is a file system id
    // stat.f_fsid.__val[1] is the inode
    // so their combination uniquely identifies the file/dir

    if (statfs(path, &stat) == -1) {
        if(reason) *reason = "failed to statfs()";
        return -1;
    }

    if(stat.f_fsid.__val[0] != 0 || stat.f_fsid.__val[1] != 0) {
        errno = EINVAL;
        if(reason) *reason = "is not a virtual file system";
        return -1;
    }
#endif

    return 0;
}

int verify_netdata_host_prefix() {
    if(!netdata_configured_host_prefix)
        netdata_configured_host_prefix = "";

    if(!*netdata_configured_host_prefix)
        return 0;

    char buffer[FILENAME_MAX + 1];
    char *path = netdata_configured_host_prefix;
    char *reason = "unknown reason";
    errno = 0;

    struct stat sb;
    if (stat(path, &sb) == -1) {
        reason = "failed to stat()";
        goto failed;
    }

    if((sb.st_mode & S_IFMT) != S_IFDIR) {
        errno = EINVAL;
        reason = "is not a directory";
        goto failed;
    }

    path = buffer;
    snprintfz(path, FILENAME_MAX, "%s/proc", netdata_configured_host_prefix);
    if(is_virtual_filesystem(path, &reason) == -1)
        goto failed;

    snprintfz(path, FILENAME_MAX, "%s/sys", netdata_configured_host_prefix);
    if(is_virtual_filesystem(path, &reason) == -1)
        goto failed;

    if(netdata_configured_host_prefix && *netdata_configured_host_prefix)
        info("Using host prefix directory '%s'", netdata_configured_host_prefix);

    return 0;

failed:
    error("Ignoring host prefix '%s': path '%s' %s", netdata_configured_host_prefix, path, reason);
    netdata_configured_host_prefix = "";
    return -1;
}

char *strdupz_path_subpath(const char *path, const char *subpath) {
    if(unlikely(!path || !*path)) path = ".";
    if(unlikely(!subpath)) subpath = "";

    // skip trailing slashes in path
    size_t len = strlen(path);
    while(len > 0 && path[len - 1] == '/') len--;

    // skip leading slashes in subpath
    while(subpath[0] == '/') subpath++;

    // if the last character in path is / and (there is a subpath or path is now empty)
    // keep the trailing slash in path and remove the additional slash
    char *slash = "/";
    if(path[len] == '/' && (*subpath || len == 0)) {
        slash = "";
        len++;
    }
    else if(!*subpath) {
        // there is no subpath
        // no need for trailing slash
        slash = "";
    }

    char buffer[FILENAME_MAX + 1];
    snprintfz(buffer, FILENAME_MAX, "%.*s%s%s", (int)len, path, slash, subpath);
    return strdupz(buffer);
}

int path_is_dir(const char *path, const char *subpath) {
    char *s = strdupz_path_subpath(path, subpath);

    size_t max_links = 100;

    int is_dir = 0;
    struct stat statbuf;
    while(max_links-- && stat(s, &statbuf) == 0) {
        if((statbuf.st_mode & S_IFMT) == S_IFDIR) {
            is_dir = 1;
            break;
        }
        else if((statbuf.st_mode & S_IFMT) == S_IFLNK) {
            char buffer[FILENAME_MAX + 1];
            ssize_t l = readlink(s, buffer, FILENAME_MAX);
            if(l > 0) {
                buffer[l] = '\0';
                freez(s);
                s = strdupz(buffer);
                continue;
            }
            else {
                is_dir = 0;
                break;
            }
        }
        else {
            is_dir = 0;
            break;
        }
    }

    freez(s);
    return is_dir;
}

int path_is_file(const char *path, const char *subpath) {
    char *s = strdupz_path_subpath(path, subpath);

    size_t max_links = 100;

    int is_file = 0;
    struct stat statbuf;
    while(max_links-- && stat(s, &statbuf) == 0) {
        if((statbuf.st_mode & S_IFMT) == S_IFREG) {
            is_file = 1;
            break;
        }
        else if((statbuf.st_mode & S_IFMT) == S_IFLNK) {
            char buffer[FILENAME_MAX + 1];
            ssize_t l = readlink(s, buffer, FILENAME_MAX);
            if(l > 0) {
                buffer[l] = '\0';
                freez(s);
                s = strdupz(buffer);
                continue;
            }
            else {
                is_file = 0;
                break;
            }
        }
        else {
            is_file = 0;
            break;
        }
    }

    freez(s);
    return is_file;
}

void recursive_config_double_dir_load(const char *user_path, const char *stock_path, const char *subpath, int (*callback)(const char *filename, void *data), void *data, size_t depth) {
    if(depth > 3) {
        error("CONFIG: Max directory depth reached while reading user path '%s', stock path '%s', subpath '%s'", user_path, stock_path, subpath);
        return;
    }

    char *udir = strdupz_path_subpath(user_path, subpath);
    char *sdir = strdupz_path_subpath(stock_path, subpath);

    debug(D_HEALTH, "CONFIG traversing user-config directory '%s', stock config directory '%s'", udir, sdir);

    DIR *dir = opendir(udir);
    if (!dir) {
        error("CONFIG cannot open user-config directory '%s'.", udir);
    }
    else {
        struct dirent *de = NULL;
        while((de = readdir(dir))) {
            if(de->d_type == DT_DIR || de->d_type == DT_LNK) {
                if( !de->d_name[0] ||
                    (de->d_name[0] == '.' && de->d_name[1] == '\0') ||
                    (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')
                        ) {
                    debug(D_HEALTH, "CONFIG ignoring user-config directory '%s/%s'", udir, de->d_name);
                    continue;
                }

                if(path_is_dir(udir, de->d_name)) {
                    recursive_config_double_dir_load(udir, sdir, de->d_name, callback, data, depth + 1);
                    continue;
                }
            }

            if(de->d_type == DT_UNKNOWN || de->d_type == DT_REG || de->d_type == DT_LNK) {
                size_t len = strlen(de->d_name);
                if(path_is_file(udir, de->d_name) &&
                   len > 5 && !strcmp(&de->d_name[len - 5], ".conf")) {
                    char *filename = strdupz_path_subpath(udir, de->d_name);
                    debug(D_HEALTH, "CONFIG calling callback for user file '%s'", filename);
                    callback(filename, data);
                    freez(filename);
                    continue;
                }
            }

            debug(D_HEALTH, "CONFIG ignoring user-config file '%s/%s' of type %d", udir, de->d_name, (int)de->d_type);
        }

        closedir(dir);
    }

    debug(D_HEALTH, "CONFIG traversing stock config directory '%s', user config directory '%s'", sdir, udir);

    dir = opendir(sdir);
    if (!dir) {
        error("CONFIG cannot open stock config directory '%s'.", sdir);
    }
    else {
        struct dirent *de = NULL;
        while((de = readdir(dir))) {
            if(de->d_type == DT_DIR || de->d_type == DT_LNK) {
                if( !de->d_name[0] ||
                    (de->d_name[0] == '.' && de->d_name[1] == '\0') ||
                    (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')
                        ) {
                    debug(D_HEALTH, "CONFIG ignoring stock config directory '%s/%s'", sdir, de->d_name);
                    continue;
                }

                if(path_is_dir(sdir, de->d_name)) {
                    // we recurse in stock subdirectory, only when there is no corresponding
                    // user subdirectory - to avoid reading the files twice

                    if(!path_is_dir(udir, de->d_name))
                        recursive_config_double_dir_load(udir, sdir, de->d_name, callback, data, depth + 1);

                    continue;
                }
            }

            if(de->d_type == DT_UNKNOWN || de->d_type == DT_REG || de->d_type == DT_LNK) {
                size_t len = strlen(de->d_name);
                if(path_is_file(sdir, de->d_name) && !path_is_file(udir, de->d_name) &&
                   len > 5 && !strcmp(&de->d_name[len - 5], ".conf")) {
                    char *filename = strdupz_path_subpath(sdir, de->d_name);
                    debug(D_HEALTH, "CONFIG calling callback for stock file '%s'", filename);
                    callback(filename, data);
                    freez(filename);
                    continue;
                }

            }

            debug(D_HEALTH, "CONFIG ignoring stock-config file '%s/%s' of type %d", udir, de->d_name, (int)de->d_type);
        }

        closedir(dir);
    }

    debug(D_HEALTH, "CONFIG done traversing user-config directory '%s', stock config directory '%s'", udir, sdir);

    freez(udir);
    freez(sdir);
}
/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
 * All Rights Reserved.
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#include "miniz.h"

typedef unsigned char mz_validate_uint16[sizeof(mz_uint16) == 2 ? 1 : -1];
typedef unsigned char mz_validate_uint32[sizeof(mz_uint32) == 4 ? 1 : -1];
typedef unsigned char mz_validate_uint64[sizeof(mz_uint64) == 8 ? 1 : -1];

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------- zlib-style API's */

mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len)
{
    mz_uint32 i, s1 = (mz_uint32)(adler & 0xffff), s2 = (mz_uint32)(adler >> 16);
    size_t block_len = buf_len % 5552;
    if (!ptr)
        return MZ_ADLER32_INIT;
    while (buf_len)
    {
        for (i = 0; i + 7 < block_len; i += 8, ptr += 8)
        {
            s1 += ptr[0], s2 += s1;
            s1 += ptr[1], s2 += s1;
            s1 += ptr[2], s2 += s1;
            s1 += ptr[3], s2 += s1;
            s1 += ptr[4], s2 += s1;
            s1 += ptr[5], s2 += s1;
            s1 += ptr[6], s2 += s1;
            s1 += ptr[7], s2 += s1;
        }
        for (; i < block_len; ++i)
            s1 += *ptr++, s2 += s1;
        s1 %= 65521U, s2 %= 65521U;
        buf_len -= block_len;
        block_len = 5552;
    }
    return (s2 << 16) + s1;
}

/* Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C implementation that balances processor cache usage against speed": http://www.geocities.com/malbrain/ */
#if 0
    mz_ulong mz_crc32(mz_ulong crc, const mz_uint8 *ptr, size_t buf_len)
    {
        static const mz_uint32 s_crc32[16] = { 0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                                               0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c };
        mz_uint32 crcu32 = (mz_uint32)crc;
        if (!ptr)
            return MZ_CRC32_INIT;
        crcu32 = ~crcu32;
        while (buf_len--)
        {
            mz_uint8 b = *ptr++;
            crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
            crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)];
        }
        return ~crcu32;
    }
#else
/* Faster, but larger CPU cache footprint.
 */
mz_ulong mz_crc32(mz_ulong crc, const mz_uint8 *ptr, size_t buf_len)
{
    static const mz_uint32 s_crc_table[256] =
        {
          0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535,
          0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD,
          0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D,
          0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
          0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
          0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
          0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
          0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
          0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
          0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
          0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB,
          0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
          0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA,
          0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
          0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
          0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
          0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409,
          0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
          0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739,
          0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
          0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268,
          0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
          0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
          0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
          0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
          0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
          0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
          0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
          0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE,
          0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
          0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
          0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
          0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D,
          0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
          0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
          0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
          0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
        };

    mz_uint32 crc32 = (mz_uint32)crc ^ 0xFFFFFFFF;
    const mz_uint8 *pByte_buf = (const mz_uint8 *)ptr;

    while (buf_len >= 4)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[1]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[2]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[3]) & 0xFF];
        pByte_buf += 4;
        buf_len -= 4;
    }

    while (buf_len)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        ++pByte_buf;
        --buf_len;
    }

    return ~crc32;
}
#endif

void mz_free(void *p)
{
    MZ_FREE(p);
}

void *miniz_def_alloc_func(void *opaque, size_t items, size_t size)
{
    (void)opaque, (void)items, (void)size;
    return MZ_MALLOC(items * size);
}
void miniz_def_free_func(void *opaque, void *address)
{
    (void)opaque, (void)address;
    MZ_FREE(address);
}
void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size)
{
    (void)opaque, (void)address, (void)items, (void)size;
    return MZ_REALLOC(address, items * size);
}

const char *mz_version(void)
{
    return MZ_VERSION;
}

#ifndef MINIZ_NO_ZLIB_APIS

int mz_deflateInit(mz_streamp pStream, int level)
{
    return mz_deflateInit2(pStream, level, MZ_DEFLATED, MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
}

int mz_deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy)
{
    tdefl_compressor *pComp;
    mz_uint comp_flags = TDEFL_COMPUTE_ADLER32 | tdefl_create_comp_flags_from_zip_params(level, window_bits, strategy);

    if (!pStream)
        return MZ_STREAM_ERROR;
    if ((method != MZ_DEFLATED) || ((mem_level < 1) || (mem_level > 9)) || ((window_bits != MZ_DEFAULT_WINDOW_BITS) && (-window_bits != MZ_DEFAULT_WINDOW_BITS)))
        return MZ_PARAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = MZ_ADLER32_INIT;
    pStream->msg = NULL;
    pStream->reserved = 0;
    pStream->total_in = 0;
    pStream->total_out = 0;
    if (!pStream->zalloc)
        pStream->zalloc = miniz_def_alloc_func;
    if (!pStream->zfree)
        pStream->zfree = miniz_def_free_func;

    pComp = (tdefl_compressor *)pStream->zalloc(pStream->opaque, 1, sizeof(tdefl_compressor));
    if (!pComp)
        return MZ_MEM_ERROR;

    pStream->state = (struct mz_internal_state *)pComp;

    if (tdefl_init(pComp, NULL, NULL, comp_flags) != TDEFL_STATUS_OKAY)
    {
        mz_deflateEnd(pStream);
        return MZ_PARAM_ERROR;
    }

    return MZ_OK;
}

int mz_deflateReset(mz_streamp pStream)
{
    if ((!pStream) || (!pStream->state) || (!pStream->zalloc) || (!pStream->zfree))
        return MZ_STREAM_ERROR;
    pStream->total_in = pStream->total_out = 0;
    tdefl_init((tdefl_compressor *)pStream->state, NULL, NULL, ((tdefl_compressor *)pStream->state)->m_flags);
    return MZ_OK;
}

int mz_deflate(mz_streamp pStream, int flush)
{
    size_t in_bytes, out_bytes;
    mz_ulong orig_total_in, orig_total_out;
    int mz_status = MZ_OK;

    if ((!pStream) || (!pStream->state) || (flush < 0) || (flush > MZ_FINISH) || (!pStream->next_out))
        return MZ_STREAM_ERROR;
    if (!pStream->avail_out)
        return MZ_BUF_ERROR;

    if (flush == MZ_PARTIAL_FLUSH)
        flush = MZ_SYNC_FLUSH;

    if (((tdefl_compressor *)pStream->state)->m_prev_return_status == TDEFL_STATUS_DONE)
        return (flush == MZ_FINISH) ? MZ_STREAM_END : MZ_BUF_ERROR;

    orig_total_in = pStream->total_in;
    orig_total_out = pStream->total_out;
    for (;;)
    {
        tdefl_status defl_status;
        in_bytes = pStream->avail_in;
        out_bytes = pStream->avail_out;

        defl_status = tdefl_compress((tdefl_compressor *)pStream->state, pStream->next_in, &in_bytes, pStream->next_out, &out_bytes, (tdefl_flush)flush);
        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tdefl_get_adler32((tdefl_compressor *)pStream->state);

        pStream->next_out += (mz_uint)out_bytes;
        pStream->avail_out -= (mz_uint)out_bytes;
        pStream->total_out += (mz_uint)out_bytes;

        if (defl_status < 0)
        {
            mz_status = MZ_STREAM_ERROR;
            break;
        }
        else if (defl_status == TDEFL_STATUS_DONE)
        {
            mz_status = MZ_STREAM_END;
            break;
        }
        else if (!pStream->avail_out)
            break;
        else if ((!pStream->avail_in) && (flush != MZ_FINISH))
        {
            if ((flush) || (pStream->total_in != orig_total_in) || (pStream->total_out != orig_total_out))
                break;
            return MZ_BUF_ERROR; /* Can't make forward progress without some input.
 */
        }
    }
    return mz_status;
}

int mz_deflateEnd(mz_streamp pStream)
{
    if (!pStream)
        return MZ_STREAM_ERROR;
    if (pStream->state)
    {
        pStream->zfree(pStream->opaque, pStream->state);
        pStream->state = NULL;
    }
    return MZ_OK;
}

mz_ulong mz_deflateBound(mz_streamp pStream, mz_ulong source_len)
{
    (void)pStream;
    /* This is really over conservative. (And lame, but it's actually pretty tricky to compute a true upper bound given the way tdefl's blocking works.) */
    return MZ_MAX(128 + (source_len * 110) / 100, 128 + source_len + ((source_len / (31 * 1024)) + 1) * 5);
}

int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level)
{
    int status;
    mz_stream stream;
    memset(&stream, 0, sizeof(stream));

    /* In case mz_ulong is 64-bits (argh I hate longs). */
    if ((source_len | *pDest_len) > 0xFFFFFFFFU)
        return MZ_PARAM_ERROR;

    stream.next_in = pSource;
    stream.avail_in = (mz_uint32)source_len;
    stream.next_out = pDest;
    stream.avail_out = (mz_uint32)*pDest_len;

    status = mz_deflateInit(&stream, level);
    if (status != MZ_OK)
        return status;

    status = mz_deflate(&stream, MZ_FINISH);
    if (status != MZ_STREAM_END)
    {
        mz_deflateEnd(&stream);
        return (status == MZ_OK) ? MZ_BUF_ERROR : status;
    }

    *pDest_len = stream.total_out;
    return mz_deflateEnd(&stream);
}

int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
{
    return mz_compress2(pDest, pDest_len, pSource, source_len, MZ_DEFAULT_COMPRESSION);
}

mz_ulong mz_compressBound(mz_ulong source_len)
{
    return mz_deflateBound(NULL, source_len);
}

typedef struct
{
    tinfl_decompressor m_decomp;
    mz_uint m_dict_ofs, m_dict_avail, m_first_call, m_has_flushed;
    int m_window_bits;
    mz_uint8 m_dict[TINFL_LZ_DICT_SIZE];
    tinfl_status m_last_status;
} inflate_state;

int mz_inflateInit2(mz_streamp pStream, int window_bits)
{
    inflate_state *pDecomp;
    if (!pStream)
        return MZ_STREAM_ERROR;
    if ((window_bits != MZ_DEFAULT_WINDOW_BITS) && (-window_bits != MZ_DEFAULT_WINDOW_BITS))
        return MZ_PARAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = 0;
    pStream->msg = NULL;
    pStream->total_in = 0;
    pStream->total_out = 0;
    pStream->reserved = 0;
    if (!pStream->zalloc)
        pStream->zalloc = miniz_def_alloc_func;
    if (!pStream->zfree)
        pStream->zfree = miniz_def_free_func;

    pDecomp = (inflate_state *)pStream->zalloc(pStream->opaque, 1, sizeof(inflate_state));
    if (!pDecomp)
        return MZ_MEM_ERROR;

    pStream->state = (struct mz_internal_state *)pDecomp;

    tinfl_init(&pDecomp->m_decomp);
    pDecomp->m_dict_ofs = 0;
    pDecomp->m_dict_avail = 0;
    pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
    pDecomp->m_first_call = 1;
    pDecomp->m_has_flushed = 0;
    pDecomp->m_window_bits = window_bits;

    return MZ_OK;
}

int mz_inflateInit(mz_streamp pStream)
{
    return mz_inflateInit2(pStream, MZ_DEFAULT_WINDOW_BITS);
}

int mz_inflateReset(mz_streamp pStream)
{
    inflate_state *pDecomp;
    if (!pStream)
        return MZ_STREAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = 0;
    pStream->msg = NULL;
    pStream->total_in = 0;
    pStream->total_out = 0;
    pStream->reserved = 0;

    pDecomp = (inflate_state *)pStream->state;

    tinfl_init(&pDecomp->m_decomp);
    pDecomp->m_dict_ofs = 0;
    pDecomp->m_dict_avail = 0;
    pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
    pDecomp->m_first_call = 1;
    pDecomp->m_has_flushed = 0;
    /* pDecomp->m_window_bits = window_bits */;

    return MZ_OK;
}

int mz_inflate(mz_streamp pStream, int flush)
{
    inflate_state *pState;
    mz_uint n, first_call, decomp_flags = TINFL_FLAG_COMPUTE_ADLER32;
    size_t in_bytes, out_bytes, orig_avail_in;
    tinfl_status status;

    if ((!pStream) || (!pStream->state))
        return MZ_STREAM_ERROR;
    if (flush == MZ_PARTIAL_FLUSH)
        flush = MZ_SYNC_FLUSH;
    if ((flush) && (flush != MZ_SYNC_FLUSH) && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;

    pState = (inflate_state *)pStream->state;
    if (pState->m_window_bits > 0)
        decomp_flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
    orig_avail_in = pStream->avail_in;

    first_call = pState->m_first_call;
    pState->m_first_call = 0;
    if (pState->m_last_status < 0)
        return MZ_DATA_ERROR;

    if (pState->m_has_flushed && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;
    pState->m_has_flushed |= (flush == MZ_FINISH);

    if ((flush == MZ_FINISH) && (first_call))
    {
        /* MZ_FINISH on the first call implies that the input and output buffers are large enough to hold the entire compressed/decompressed file. */
        decomp_flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        in_bytes = pStream->avail_in;
        out_bytes = pStream->avail_out;
        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pStream->next_out, pStream->next_out, &out_bytes, decomp_flags);
        pState->m_last_status = status;
        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);
        pStream->next_out += (mz_uint)out_bytes;
        pStream->avail_out -= (mz_uint)out_bytes;
        pStream->total_out += (mz_uint)out_bytes;

        if (status < 0)
            return MZ_DATA_ERROR;
        else if (status != TINFL_STATUS_DONE)
        {
            pState->m_last_status = TINFL_STATUS_FAILED;
            return MZ_BUF_ERROR;
        }
        return MZ_STREAM_END;
    }
    /* flush != MZ_FINISH then we must assume there's more input. */
    if (flush != MZ_FINISH)
        decomp_flags |= TINFL_FLAG_HAS_MORE_INPUT;

    if (pState->m_dict_avail)
    {
        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);
        return ((pState->m_last_status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
    }

    for (;;)
    {
        in_bytes = pStream->avail_in;
        out_bytes = TINFL_LZ_DICT_SIZE - pState->m_dict_ofs;

        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pState->m_dict, pState->m_dict + pState->m_dict_ofs, &out_bytes, decomp_flags);
        pState->m_last_status = status;

        pStream->next_in += (mz_uint)in_bytes;
        pStream->avail_in -= (mz_uint)in_bytes;
        pStream->total_in += (mz_uint)in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);

        pState->m_dict_avail = (mz_uint)out_bytes;

        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);

        if (status < 0)
            return MZ_DATA_ERROR; /* Stream is corrupted (there could be some uncompressed data left in the output dictionary - oh well). */
        else if ((status == TINFL_STATUS_NEEDS_MORE_INPUT) && (!orig_avail_in))
            return MZ_BUF_ERROR; /* Signal caller that we can't make forward progress without supplying more input or by setting flush to MZ_FINISH. */
        else if (flush == MZ_FINISH)
        {
            /* The output buffer MUST be large to hold the remaining uncompressed data when flush==MZ_FINISH. */
            if (status == TINFL_STATUS_DONE)
                return pState->m_dict_avail ? MZ_BUF_ERROR : MZ_STREAM_END;
            /* status here must be TINFL_STATUS_HAS_MORE_OUTPUT, which means there's at least 1 more byte on the way. If there's no more room left in the output buffer then something is wrong. */
            else if (!pStream->avail_out)
                return MZ_BUF_ERROR;
        }
        else if ((status == TINFL_STATUS_DONE) || (!pStream->avail_in) || (!pStream->avail_out) || (pState->m_dict_avail))
            break;
    }

    return ((status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
}

int mz_inflateEnd(mz_streamp pStream)
{
    if (!pStream)
        return MZ_STREAM_ERROR;
    if (pStream->state)
    {
        pStream->zfree(pStream->opaque, pStream->state);
        pStream->state = NULL;
    }
    return MZ_OK;
}

int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
{
    mz_stream stream;
    int status;
    memset(&stream, 0, sizeof(stream));

    /* In case mz_ulong is 64-bits (argh I hate longs). */
    if ((source_len | *pDest_len) > 0xFFFFFFFFU)
        return MZ_PARAM_ERROR;

    stream.next_in = pSource;
    stream.avail_in = (mz_uint32)source_len;
    stream.next_out = pDest;
    stream.avail_out = (mz_uint32)*pDest_len;

    status = mz_inflateInit(&stream);
    if (status != MZ_OK)
        return status;

    status = mz_inflate(&stream, MZ_FINISH);
    if (status != MZ_STREAM_END)
    {
        mz_inflateEnd(&stream);
        return ((status == MZ_BUF_ERROR) && (!stream.avail_in)) ? MZ_DATA_ERROR : status;
    }
    *pDest_len = stream.total_out;

    return mz_inflateEnd(&stream);
}

const char *mz_error(int err)
{
    static struct
    {
        int m_err;
        const char *m_pDesc;
    } s_error_descs[] =
        {
          { MZ_OK, "" }, { MZ_STREAM_END, "stream end" }, { MZ_NEED_DICT, "need dictionary" }, { MZ_ERRNO, "file error" }, { MZ_STREAM_ERROR, "stream error" }, { MZ_DATA_ERROR, "data error" }, { MZ_MEM_ERROR, "out of memory" }, { MZ_BUF_ERROR, "buf error" }, { MZ_VERSION_ERROR, "version error" }, { MZ_PARAM_ERROR, "parameter error" }
        };
    mz_uint i;
    for (i = 0; i < sizeof(s_error_descs) / sizeof(s_error_descs[0]); ++i)
        if (s_error_descs[i].m_err == err)
            return s_error_descs[i].m_pDesc;
    return NULL;
}

#endif /*MINIZ_NO_ZLIB_APIS */

#ifdef __cplusplus
}
#endif

/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/
/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * arabic.c: functions for Arabic language
 *
 * Author: Nadim Shaikli & Isam Bayazidi
 */

#include "vim.h"

#if defined(FEAT_ARABIC) || defined(PROTO)

static int  A_firstc_laa(int c1, int c);
static int  A_is_harakat(int c);
static int  A_is_iso(int c);
static int  A_is_formb(int c);
static int  A_is_ok(int c);
static int  A_is_valid(int c);
static int  A_is_special(int c);


/*
 * Returns True if c is an ISO-8859-6 shaped ARABIC letter (user entered)
 */
    static int
A_is_a(int cur_c)
{
    switch (cur_c)
    {
	case a_HAMZA:
	case a_ALEF_MADDA:
	case a_ALEF_HAMZA_ABOVE:
	case a_WAW_HAMZA:
	case a_ALEF_HAMZA_BELOW:
	case a_YEH_HAMZA:
	case a_ALEF:
	case a_BEH:
	case a_TEH_MARBUTA:
	case a_TEH:
	case a_THEH:
	case a_JEEM:
	case a_HAH:
	case a_KHAH:
	case a_DAL:
	case a_THAL:
	case a_REH:
	case a_ZAIN:
	case a_SEEN:
	case a_SHEEN:
	case a_SAD:
	case a_DAD:
	case a_TAH:
	case a_ZAH:
	case a_AIN:
	case a_GHAIN:
	case a_TATWEEL:
	case a_FEH:
	case a_QAF:
	case a_KAF:
	case a_LAM:
	case a_MEEM:
	case a_NOON:
	case a_HEH:
	case a_WAW:
	case a_ALEF_MAKSURA:
	case a_YEH:
	    return TRUE;
    }

    return FALSE;
}


/*
 * Returns True if c is an Isolated Form-B ARABIC letter
 */
    static int
A_is_s(int cur_c)
{
    switch (cur_c)
    {
	case a_s_HAMZA:
	case a_s_ALEF_MADDA:
	case a_s_ALEF_HAMZA_ABOVE:
	case a_s_WAW_HAMZA:
	case a_s_ALEF_HAMZA_BELOW:
	case a_s_YEH_HAMZA:
	case a_s_ALEF:
	case a_s_BEH:
	case a_s_TEH_MARBUTA:
	case a_s_TEH:
	case a_s_THEH:
	case a_s_JEEM:
	case a_s_HAH:
	case a_s_KHAH:
	case a_s_DAL:
	case a_s_THAL:
	case a_s_REH:
	case a_s_ZAIN:
	case a_s_SEEN:
	case a_s_SHEEN:
	case a_s_SAD:
	case a_s_DAD:
	case a_s_TAH:
	case a_s_ZAH:
	case a_s_AIN:
	case a_s_GHAIN:
	case a_s_FEH:
	case a_s_QAF:
	case a_s_KAF:
	case a_s_LAM:
	case a_s_MEEM:
	case a_s_NOON:
	case a_s_HEH:
	case a_s_WAW:
	case a_s_ALEF_MAKSURA:
	case a_s_YEH:
	    return TRUE;
    }

    return FALSE;
}


/*
 * Returns True if c is a Final shape of an ARABIC letter
 */
    static int
A_is_f(int cur_c)
{
    switch (cur_c)
    {
	case a_f_ALEF_MADDA:
	case a_f_ALEF_HAMZA_ABOVE:
	case a_f_WAW_HAMZA:
	case a_f_ALEF_HAMZA_BELOW:
	case a_f_YEH_HAMZA:
	case a_f_ALEF:
	case a_f_BEH:
	case a_f_TEH_MARBUTA:
	case a_f_TEH:
	case a_f_THEH:
	case a_f_JEEM:
	case a_f_HAH:
	case a_f_KHAH:
	case a_f_DAL:
	case a_f_THAL:
	case a_f_REH:
	case a_f_ZAIN:
	case a_f_SEEN:
	case a_f_SHEEN:
	case a_f_SAD:
	case a_f_DAD:
	case a_f_TAH:
	case a_f_ZAH:
	case a_f_AIN:
	case a_f_GHAIN:
	case a_f_FEH:
	case a_f_QAF:
	case a_f_KAF:
	case a_f_LAM:
	case a_f_MEEM:
	case a_f_NOON:
	case a_f_HEH:
	case a_f_WAW:
	case a_f_ALEF_MAKSURA:
	case a_f_YEH:
	case a_f_LAM_ALEF_MADDA_ABOVE:
	case a_f_LAM_ALEF_HAMZA_ABOVE:
	case a_f_LAM_ALEF_HAMZA_BELOW:
	case a_f_LAM_ALEF:
	    return TRUE;
    }
    return FALSE;
}


/*
 * Change shape - from ISO-8859-6/Isolated to Form-B Isolated
 */
    static int
chg_c_a2s(int cur_c)
{
    switch (cur_c)
    {
	case a_HAMZA: return a_s_HAMZA;
	case a_ALEF_MADDA: return a_s_ALEF_MADDA;
	case a_ALEF_HAMZA_ABOVE: return a_s_ALEF_HAMZA_ABOVE;
	case a_WAW_HAMZA: return a_s_WAW_HAMZA;
	case a_ALEF_HAMZA_BELOW: return a_s_ALEF_HAMZA_BELOW;
	case a_YEH_HAMZA: return a_s_YEH_HAMZA;
	case a_ALEF: return a_s_ALEF;
	case a_TEH_MARBUTA: return a_s_TEH_MARBUTA;
	case a_DAL: return a_s_DAL;
	case a_THAL: return a_s_THAL;
	case a_REH: return a_s_REH;
	case a_ZAIN: return a_s_ZAIN;
	case a_TATWEEL: return cur_c;	/* exceptions */
	case a_WAW: return a_s_WAW;
	case a_ALEF_MAKSURA: return a_s_ALEF_MAKSURA;
	case a_BEH: return a_s_BEH;
	case a_TEH: return a_s_TEH;
	case a_THEH: return a_s_THEH;
	case a_JEEM: return a_s_JEEM;
	case a_HAH: return a_s_HAH;
	case a_KHAH: return a_s_KHAH;
	case a_SEEN: return a_s_SEEN;
	case a_SHEEN: return a_s_SHEEN;
	case a_SAD: return a_s_SAD;
	case a_DAD: return a_s_DAD;
	case a_TAH: return a_s_TAH;
	case a_ZAH: return a_s_ZAH;
	case a_AIN: return a_s_AIN;
	case a_GHAIN: return a_s_GHAIN;
	case a_FEH: return a_s_FEH;
	case a_QAF: return a_s_QAF;
	case a_KAF: return a_s_KAF;
	case a_LAM: return a_s_LAM;
	case a_MEEM: return a_s_MEEM;
	case a_NOON: return a_s_NOON;
	case a_HEH: return a_s_HEH;
	case a_YEH: return a_s_YEH;
    }
    return 0;
}


/*
 * Change shape - from ISO-8859-6/Isolated to Initial
 */
    static int
chg_c_a2i(int cur_c)
{
    switch (cur_c)
    {
	case a_YEH_HAMZA: return a_i_YEH_HAMZA;
	case a_HAMZA:			/* exceptions */
	    return a_s_HAMZA;
	case a_ALEF_MADDA:		/* exceptions */
	    return a_s_ALEF_MADDA;
	case a_ALEF_HAMZA_ABOVE:	/* exceptions */
	    return a_s_ALEF_HAMZA_ABOVE;
	case a_WAW_HAMZA:		/* exceptions */
	    return a_s_WAW_HAMZA;
	case a_ALEF_HAMZA_BELOW:	/* exceptions */
	    return a_s_ALEF_HAMZA_BELOW;
	case a_ALEF:			/* exceptions */
	    return a_s_ALEF;
	case a_TEH_MARBUTA:		/* exceptions */
	    return a_s_TEH_MARBUTA;
	case a_DAL:			/* exceptions */
	    return a_s_DAL;
	case a_THAL:			/* exceptions */
	    return a_s_THAL;
	case a_REH:			/* exceptions */
	    return a_s_REH;
	case a_ZAIN:			/* exceptions */
	    return a_s_ZAIN;
	case a_TATWEEL:			/* exceptions */
	    return cur_c;
	case a_WAW:			/* exceptions */
	    return a_s_WAW;
	case a_ALEF_MAKSURA:		/* exceptions */
	    return a_s_ALEF_MAKSURA;
	case a_BEH: return a_i_BEH;
	case a_TEH: return a_i_TEH;
	case a_THEH: return a_i_THEH;
	case a_JEEM: return a_i_JEEM;
	case a_HAH: return a_i_HAH;
	case a_KHAH: return a_i_KHAH;
	case a_SEEN: return a_i_SEEN;
	case a_SHEEN: return a_i_SHEEN;
	case a_SAD: return a_i_SAD;
	case a_DAD: return a_i_DAD;
	case a_TAH: return a_i_TAH;
	case a_ZAH: return a_i_ZAH;
	case a_AIN: return a_i_AIN;
	case a_GHAIN: return a_i_GHAIN;
	case a_FEH: return a_i_FEH;
	case a_QAF: return a_i_QAF;
	case a_KAF: return a_i_KAF;
	case a_LAM: return a_i_LAM;
	case a_MEEM: return a_i_MEEM;
	case a_NOON: return a_i_NOON;
	case a_HEH: return a_i_HEH;
	case a_YEH: return a_i_YEH;
    }
    return 0;
}


/*
 * Change shape - from ISO-8859-6/Isolated to Medial
 */
    static int
chg_c_a2m(int cur_c)
{
    switch (cur_c)
    {
	case a_HAMZA: return a_s_HAMZA;	/* exception */
	case a_ALEF_MADDA: return a_f_ALEF_MADDA;	/* exception */
	case a_ALEF_HAMZA_ABOVE: return a_f_ALEF_HAMZA_ABOVE;	/* exception */
	case a_WAW_HAMZA: return a_f_WAW_HAMZA;	/* exception */
	case a_ALEF_HAMZA_BELOW: return a_f_ALEF_HAMZA_BELOW;	/* exception */
	case a_YEH_HAMZA: return a_m_YEH_HAMZA;
	case a_ALEF: return a_f_ALEF;	/* exception */
	case a_BEH: return a_m_BEH;
	case a_TEH_MARBUTA: return a_f_TEH_MARBUTA;	/* exception */
	case a_TEH: return a_m_TEH;
	case a_THEH: return a_m_THEH;
	case a_JEEM: return a_m_JEEM;
	case a_HAH: return a_m_HAH;
	case a_KHAH: return a_m_KHAH;
	case a_DAL: return a_f_DAL;	/* exception */
	case a_THAL: return a_f_THAL;	/* exception */
	case a_REH: return a_f_REH;	/* exception */
	case a_ZAIN: return a_f_ZAIN;	/* exception */
	case a_SEEN: return a_m_SEEN;
	case a_SHEEN: return a_m_SHEEN;
	case a_SAD: return a_m_SAD;
	case a_DAD: return a_m_DAD;
	case a_TAH: return a_m_TAH;
	case a_ZAH: return a_m_ZAH;
	case a_AIN: return a_m_AIN;
	case a_GHAIN: return a_m_GHAIN;
	case a_TATWEEL: return cur_c;	/* exception */
	case a_FEH: return a_m_FEH;
	case a_QAF: return a_m_QAF;
	case a_KAF: return a_m_KAF;
	case a_LAM: return a_m_LAM;
	case a_MEEM: return a_m_MEEM;
	case a_NOON: return a_m_NOON;
	case a_HEH: return a_m_HEH;
	case a_WAW: return a_f_WAW;	/* exception */
	case a_ALEF_MAKSURA: return a_f_ALEF_MAKSURA;	/* exception */
	case a_YEH: return a_m_YEH;
    }
    return 0;
}


/*
 * Change shape - from ISO-8859-6/Isolated to final
 */
    static int
chg_c_a2f(int cur_c)
{
    /* NOTE: these encodings need to be accounted for
     * a_f_ALEF_MADDA;
     * a_f_ALEF_HAMZA_ABOVE;
     * a_f_ALEF_HAMZA_BELOW;
     * a_f_LAM_ALEF_MADDA_ABOVE;
     * a_f_LAM_ALEF_HAMZA_ABOVE;
     * a_f_LAM_ALEF_HAMZA_BELOW;
     */
    switch (cur_c)
    {
	case a_HAMZA: return a_s_HAMZA;	/* exception */
	case a_ALEF_MADDA: return a_f_ALEF_MADDA;
	case a_ALEF_HAMZA_ABOVE: return a_f_ALEF_HAMZA_ABOVE;
	case a_WAW_HAMZA: return a_f_WAW_HAMZA;
	case a_ALEF_HAMZA_BELOW: return a_f_ALEF_HAMZA_BELOW;
	case a_YEH_HAMZA: return a_f_YEH_HAMZA;
	case a_ALEF: return a_f_ALEF;
	case a_BEH: return a_f_BEH;
	case a_TEH_MARBUTA: return a_f_TEH_MARBUTA;
	case a_TEH: return a_f_TEH;
	case a_THEH: return a_f_THEH;
	case a_JEEM: return a_f_JEEM;
	case a_HAH: return a_f_HAH;
	case a_KHAH: return a_f_KHAH;
	case a_DAL: return a_f_DAL;
	case a_THAL: return a_f_THAL;
	case a_REH: return a_f_REH;
	case a_ZAIN: return a_f_ZAIN;
	case a_SEEN: return a_f_SEEN;
	case a_SHEEN: return a_f_SHEEN;
	case a_SAD: return a_f_SAD;
	case a_DAD: return a_f_DAD;
	case a_TAH: return a_f_TAH;
	case a_ZAH: return a_f_ZAH;
	case a_AIN: return a_f_AIN;
	case a_GHAIN: return a_f_GHAIN;
	case a_TATWEEL:	return cur_c;	/* exception */
	case a_FEH: return a_f_FEH;
	case a_QAF: return a_f_QAF;
	case a_KAF: return a_f_KAF;
	case a_LAM: return a_f_LAM;
	case a_MEEM: return a_f_MEEM;
	case a_NOON: return a_f_NOON;
	case a_HEH: return a_f_HEH;
	case a_WAW: return a_f_WAW;
	case a_ALEF_MAKSURA: return a_f_ALEF_MAKSURA;
	case a_YEH: return a_f_YEH;
    }
    return 0;
}


/*
 * Change shape - from Initial to Medial
 * This code is unreachable, because for the relevant characters ARABIC_CHAR()
 * is FALSE;
 */
#if 0
    static int
chg_c_i2m(int cur_c)
{
    switch (cur_c)
    {
	case a_i_YEH_HAMZA: return a_m_YEH_HAMZA;
	case a_i_BEH: return a_m_BEH;
	case a_i_TEH: return a_m_TEH;
	case a_i_THEH: return a_m_THEH;
	case a_i_JEEM: return a_m_JEEM;
	case a_i_HAH: return a_m_HAH;
	case a_i_KHAH: return a_m_KHAH;
	case a_i_SEEN: return a_m_SEEN;
	case a_i_SHEEN: return a_m_SHEEN;
	case a_i_SAD: return a_m_SAD;
	case a_i_DAD: return a_m_DAD;
	case a_i_TAH: return a_m_TAH;
	case a_i_ZAH: return a_m_ZAH;
	case a_i_AIN: return a_m_AIN;
	case a_i_GHAIN: return a_m_GHAIN;
	case a_i_FEH: return a_m_FEH;
	case a_i_QAF: return a_m_QAF;
	case a_i_KAF: return a_m_KAF;
	case a_i_LAM: return a_m_LAM;
	case a_i_MEEM: return a_m_MEEM;
	case a_i_NOON: return a_m_NOON;
	case a_i_HEH: return a_m_HEH;
	case a_i_YEH: return a_m_YEH;
    }
    return 0;
}
#endif


/*
 * Change shape - from Final to Medial
 */
    static int
chg_c_f2m(int cur_c)
{
    switch (cur_c)
    {
	/* NOTE: these encodings are multi-positional, no ?
	 * case a_f_ALEF_MADDA:
	 * case a_f_ALEF_HAMZA_ABOVE:
	 * case a_f_ALEF_HAMZA_BELOW:
	 */
	case a_f_YEH_HAMZA: return a_m_YEH_HAMZA;
	case a_f_WAW_HAMZA:		/* exceptions */
	case a_f_ALEF:
	case a_f_TEH_MARBUTA:
	case a_f_DAL:
	case a_f_THAL:
	case a_f_REH:
	case a_f_ZAIN:
	case a_f_WAW:
	case a_f_ALEF_MAKSURA:
		return cur_c;
	case a_f_BEH: return a_m_BEH;
	case a_f_TEH: return a_m_TEH;
	case a_f_THEH: return a_m_THEH;
	case a_f_JEEM: return a_m_JEEM;
	case a_f_HAH: return a_m_HAH;
	case a_f_KHAH: return a_m_KHAH;
	case a_f_SEEN: return a_m_SEEN;
	case a_f_SHEEN: return a_m_SHEEN;
	case a_f_SAD: return a_m_SAD;
	case a_f_DAD: return a_m_DAD;
	case a_f_TAH: return a_m_TAH;
	case a_f_ZAH: return a_m_ZAH;
	case a_f_AIN: return a_m_AIN;
	case a_f_GHAIN: return a_m_GHAIN;
	case a_f_FEH: return a_m_FEH;
	case a_f_QAF: return a_m_QAF;
	case a_f_KAF: return a_m_KAF;
	case a_f_LAM: return a_m_LAM;
	case a_f_MEEM: return a_m_MEEM;
	case a_f_NOON: return a_m_NOON;
	case a_f_HEH: return a_m_HEH;
	case a_f_YEH: return a_m_YEH;

	/* NOTE: these encodings are multi-positional, no ?
	 * case a_f_LAM_ALEF_MADDA_ABOVE:
	 * case a_f_LAM_ALEF_HAMZA_ABOVE:
	 * case a_f_LAM_ALEF_HAMZA_BELOW:
	 * case a_f_LAM_ALEF:
	 */
    }
    return 0;
}


/*
 * Change shape - from Combination (2 char) to an Isolated
 */
    static int
chg_c_laa2i(int hid_c)
{
    switch (hid_c)
    {
	case a_ALEF_MADDA: return a_s_LAM_ALEF_MADDA_ABOVE;
	case a_ALEF_HAMZA_ABOVE: return a_s_LAM_ALEF_HAMZA_ABOVE;
	case a_ALEF_HAMZA_BELOW: return a_s_LAM_ALEF_HAMZA_BELOW;
	case a_ALEF: return a_s_LAM_ALEF;
    }
    return 0;
}


/*
 * Change shape - from Combination-Isolated to Final
 */
    static int
chg_c_laa2f(int hid_c)
{
    switch (hid_c)
    {
	case a_ALEF_MADDA: return a_f_LAM_ALEF_MADDA_ABOVE;
	case a_ALEF_HAMZA_ABOVE: return a_f_LAM_ALEF_HAMZA_ABOVE;
	case a_ALEF_HAMZA_BELOW: return a_f_LAM_ALEF_HAMZA_BELOW;
	case a_ALEF: return a_f_LAM_ALEF;
    }
    return 0;
}

/*
 * Do "half-shaping" on character "c".  Return zero if no shaping.
 */
    static int
half_shape(int c)
{
    if (A_is_a(c))
	return chg_c_a2i(c);
    if (A_is_valid(c) && A_is_f(c))
	return chg_c_f2m(c);
    return 0;
}

/*
 * Do Arabic shaping on character "c".  Returns the shaped character.
 * out:    "ccp" points to the first byte of the character to be shaped.
 * in/out: "c1p" points to the first composing char for "c".
 * in:     "prev_c"  is the previous character (not shaped)
 * in:     "prev_c1" is the first composing char for the previous char
 *		     (not shaped)
 * in:     "next_c"  is the next character (not shaped).
 */
    int
arabic_shape(
    int		c,
    int		*ccp,
    int		*c1p,
    int		prev_c,
    int		prev_c1,
    int		next_c)
{
    int		curr_c;
    int		shape_c;
    int		curr_laa;
    int		prev_laa;

    /* Deal only with Arabic character, pass back all others */
    if (!A_is_ok(c))
	return c;

    /* half-shape current and previous character */
    shape_c = half_shape(prev_c);

    /* Save away current character */
    curr_c = c;

    curr_laa = A_firstc_laa(c, *c1p);
    prev_laa = A_firstc_laa(prev_c, prev_c1);

    if (curr_laa)
    {
	if (A_is_valid(prev_c) && !A_is_f(shape_c)
					 && !A_is_s(shape_c) && !prev_laa)
	    curr_c = chg_c_laa2f(curr_laa);
	else
	    curr_c = chg_c_laa2i(curr_laa);

	/* Remove the composing character */
	*c1p = 0;
    }
    else if (!A_is_valid(prev_c) && A_is_valid(next_c))
	curr_c = chg_c_a2i(c);
    else if (!shape_c || A_is_f(shape_c) || A_is_s(shape_c) || prev_laa)
	curr_c = A_is_valid(next_c) ? chg_c_a2i(c) : chg_c_a2s(c);
    else if (A_is_valid(next_c))
#if 0
	curr_c = A_is_iso(c) ? chg_c_a2m(c) : chg_c_i2m(c);
#else
	curr_c = A_is_iso(c) ? chg_c_a2m(c) : 0;
#endif
    else if (A_is_valid(prev_c))
	curr_c = chg_c_a2f(c);
    else
	curr_c = chg_c_a2s(c);

    /* Sanity check -- curr_c should, in the future, never be 0.
     * We should, in the future, insert a fatal error here. */
    if (curr_c == NUL)
	curr_c = c;

    if (curr_c != c && ccp != NULL)
    {
	char_u buf[MB_MAXBYTES + 1];

	/* Update the first byte of the character. */
	(*mb_char2bytes)(curr_c, buf);
	*ccp = buf[0];
    }

    /* Return the shaped character */
    return curr_c;
}


/*
 * A_firstc_laa returns first character of LAA combination if it exists
 */
    static int
A_firstc_laa(
    int c,	/* base character */
    int c1)	/* first composing character */
{
    if (c1 != NUL && c == a_LAM && !A_is_harakat(c1))
	return c1;
    return 0;
}


/*
 * A_is_harakat returns TRUE if 'c' is an Arabic Harakat character
 *		(harakat/tanween)
 */
    static int
A_is_harakat(int c)
{
    return (c >= a_FATHATAN && c <= a_SUKUN);
}


/*
 * A_is_iso returns TRUE if 'c' is an Arabic ISO-8859-6 character
 *		(alphabet/number/punctuation)
 */
    static int
A_is_iso(int c)
{
    return ((c >= a_HAMZA && c <= a_GHAIN)
	    || (c >= a_TATWEEL && c <= a_HAMZA_BELOW)
	    || c == a_MINI_ALEF);
}


/*
 * A_is_formb returns TRUE if 'c' is an Arabic 10646-1 FormB character
 *		(alphabet/number/punctuation)
 */
    static int
A_is_formb(int c)
{
    return ((c >= a_s_FATHATAN && c <= a_s_DAMMATAN)
	    || c == a_s_KASRATAN
	    || (c >= a_s_FATHA && c <= a_f_LAM_ALEF)
	    || c == a_BYTE_ORDER_MARK);
}


/*
 * A_is_ok returns TRUE if 'c' is an Arabic 10646 (8859-6 or Form-B)
 */
    static int
A_is_ok(int c)
{
    return (A_is_iso(c) || A_is_formb(c));
}


/*
 * A_is_valid returns TRUE if 'c' is an Arabic 10646 (8859-6 or Form-B)
 *		with some exceptions/exclusions
 */
    static int
A_is_valid(int c)
{
    return (A_is_ok(c) && !A_is_special(c));
}


/*
 * A_is_special returns TRUE if 'c' is not a special Arabic character.
 *		Specials don't adhere to most of the rules.
 */
    static int
A_is_special(int c)
{
    return (c == a_HAMZA || c == a_s_HAMZA);
}

#endif /* FEAT_ARABIC */
/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *			Visual Workshop integration by Gordon Prieur
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

#include "vim.h"

#if defined(FEAT_BEVAL) || defined(PROTO)

/*
 * Get the text and position to be evaluated for "beval".
 * If "getword" is true the returned text is not the whole line but the
 * relevant word in allocated memory.
 * Returns OK or FAIL.
 */
    int
get_beval_info(
    BalloonEval	*beval,
    int		getword,
    win_T	**winp,
    linenr_T	*lnump,
    char_u	**textp,
    int		*colp)
{
    win_T	*wp;
    int		row, col;
    char_u	*lbuf;
    linenr_T	lnum;

    *textp = NULL;
# ifdef FEAT_BEVAL_TERM
#  ifdef FEAT_GUI
    if (!gui.in_use)
#  endif
    {
	row = mouse_row;
	col = mouse_col;
    }
# endif
# ifdef FEAT_GUI
    if (gui.in_use)
    {
	row = Y_2_ROW(beval->y);
	col = X_2_COL(beval->x);
    }
#endif
    wp = mouse_find_win(&row, &col);
    if (wp != NULL && row >= 0 && row < wp->w_height && col < wp->w_width)
    {
	/* Found a window and the cursor is in the text.  Now find the line
	 * number. */
	if (!mouse_comp_pos(wp, &row, &col, &lnum))
	{
	    /* Not past end of the file. */
	    lbuf = ml_get_buf(wp->w_buffer, lnum, FALSE);
	    if (col <= win_linetabsize(wp, lbuf, (colnr_T)MAXCOL))
	    {
		/* Not past end of line. */
		if (getword)
		{
		    /* For Netbeans we get the relevant part of the line
		     * instead of the whole line. */
		    int		len;
		    pos_T	*spos = NULL, *epos = NULL;

		    if (VIsual_active)
		    {
			if (LT_POS(VIsual, curwin->w_cursor))
			{
			    spos = &VIsual;
			    epos = &curwin->w_cursor;
			}
			else
			{
			    spos = &curwin->w_cursor;
			    epos = &VIsual;
			}
		    }

		    col = vcol2col(wp, lnum, col);

		    if (VIsual_active
			    && wp->w_buffer == curwin->w_buffer
			    && (lnum == spos->lnum
				? col >= (int)spos->col
				: lnum > spos->lnum)
			    && (lnum == epos->lnum
				? col <= (int)epos->col
				: lnum < epos->lnum))
		    {
			/* Visual mode and pointing to the line with the
			 * Visual selection: return selected text, with a
			 * maximum of one line. */
			if (spos->lnum != epos->lnum || spos->col == epos->col)
			    return FAIL;

			lbuf = ml_get_buf(curwin->w_buffer, VIsual.lnum, FALSE);
			len = epos->col - spos->col;
			if (*p_sel != 'e')
			    len += MB_PTR2LEN(lbuf + epos->col);
			lbuf = vim_strnsave(lbuf + spos->col, len);
			lnum = spos->lnum;
			col = spos->col;
		    }
		    else
		    {
			/* Find the word under the cursor. */
			++emsg_off;
			len = find_ident_at_pos(wp, lnum, (colnr_T)col, &lbuf,
					FIND_IDENT + FIND_STRING + FIND_EVAL);
			--emsg_off;
			if (len == 0)
			    return FAIL;
			lbuf = vim_strnsave(lbuf, len);
		    }
		}

		*winp = wp;
		*lnump = lnum;
		*textp = lbuf;
		*colp = col;
#ifdef FEAT_VARTABS
		vim_free(beval->vts);
		beval->vts = tabstop_copy(wp->w_buffer->b_p_vts_array);
#endif
		beval->ts = wp->w_buffer->b_p_ts;
		return OK;
	    }
	}
    }

    return FAIL;
}

/*
 * Show a balloon with "mesg" or "list".
 */
    void
post_balloon(BalloonEval *beval UNUSED, char_u *mesg, list_T *list UNUSED)
{
# ifdef FEAT_BEVAL_TERM
#  ifdef FEAT_GUI
    if (!gui.in_use)
#  endif
	ui_post_balloon(mesg, list);
# endif
# ifdef FEAT_BEVAL_GUI
    if (gui.in_use)
	/* GUI can't handle a list */
	gui_mch_post_balloon(beval, mesg);
# endif
}

/*
 * Returns TRUE if the balloon eval has been enabled:
 * 'ballooneval' for the GUI and 'balloonevalterm' for the terminal.
 * Also checks if the screen isn't scrolled up.
 */
    int
can_use_beval(void)
{
    return (0
#ifdef FEAT_BEVAL_GUI
		|| (gui.in_use && p_beval)
#endif
#ifdef FEAT_BEVAL_TERM
		|| (
# ifdef FEAT_GUI
		    !gui.in_use &&
# endif
		    p_bevalterm)
#endif
	     ) && msg_scrolled == 0;
}

/*
 * Common code, invoked when the mouse is resting for a moment.
 */
    void
general_beval_cb(BalloonEval *beval, int state UNUSED)
{
#ifdef FEAT_EVAL
    win_T	*wp;
    int		col;
    int		use_sandbox;
    linenr_T	lnum;
    char_u	*text;
    static char_u  *result = NULL;
    long	winnr = 0;
    char_u	*bexpr;
    buf_T	*save_curbuf;
    size_t	len;
    win_T	*cw;
#endif
    static int	recursive = FALSE;

    /* Don't do anything when 'ballooneval' is off, messages scrolled the
     * windows up or we have no beval area. */
    if (!can_use_beval() || beval == NULL)
	return;

    /* Don't do this recursively.  Happens when the expression evaluation
     * takes a long time and invokes something that checks for CTRL-C typed. */
    if (recursive)
	return;
    recursive = TRUE;

#ifdef FEAT_EVAL
    if (get_beval_info(beval, TRUE, &wp, &lnum, &text, &col) == OK)
    {
	bexpr = (*wp->w_buffer->b_p_bexpr == NUL) ? p_bexpr
						    : wp->w_buffer->b_p_bexpr;
	if (*bexpr != NUL)
	{
	    /* Convert window pointer to number. */
	    for (cw = firstwin; cw != wp; cw = cw->w_next)
		++winnr;

	    set_vim_var_nr(VV_BEVAL_BUFNR, (long)wp->w_buffer->b_fnum);
	    set_vim_var_nr(VV_BEVAL_WINNR, winnr);
	    set_vim_var_nr(VV_BEVAL_WINID, wp->w_id);
	    set_vim_var_nr(VV_BEVAL_LNUM, (long)lnum);
	    set_vim_var_nr(VV_BEVAL_COL, (long)(col + 1));
	    set_vim_var_string(VV_BEVAL_TEXT, text, -1);
	    vim_free(text);

	    /*
	     * Temporarily change the curbuf, so that we can determine whether
	     * the buffer-local balloonexpr option was set insecurely.
	     */
	    save_curbuf = curbuf;
	    curbuf = wp->w_buffer;
	    use_sandbox = was_set_insecurely((char_u *)"balloonexpr",
				 *curbuf->b_p_bexpr == NUL ? 0 : OPT_LOCAL);
	    curbuf = save_curbuf;
	    if (use_sandbox)
		++sandbox;
	    ++textlock;

	    vim_free(result);
	    result = eval_to_string(bexpr, NULL, TRUE);

	    /* Remove one trailing newline, it is added when the result was a
	     * list and it's hardly ever useful.  If the user really wants a
	     * trailing newline he can add two and one remains. */
	    if (result != NULL)
	    {
		len = STRLEN(result);
		if (len > 0 && result[len - 1] == NL)
		    result[len - 1] = NUL;
	    }

	    if (use_sandbox)
		--sandbox;
	    --textlock;

	    set_vim_var_string(VV_BEVAL_TEXT, NULL, -1);
	    if (result != NULL && result[0] != NUL)
	    {
		post_balloon(beval, result, NULL);
		recursive = FALSE;
		return;
	    }
	}
    }
#endif
#ifdef FEAT_NETBEANS_INTG
    if (bevalServers & BEVAL_NETBEANS)
	netbeans_beval_cb(beval, state);
#endif
#ifdef FEAT_SUN_WORKSHOP
    if (bevalServers & BEVAL_WORKSHOP)
	workshop_beval_cb(beval, state);
#endif

    recursive = FALSE;
}

#endif

/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 *
 * Blowfish encryption for Vim; in Blowfish cipher feedback mode.
 * Contributed by Mohsin Ahmed, http://www.cs.albany.edu/~mosh
 * Based on http://www.schneier.com/blowfish.html by Bruce Schneier.
 *
 * There are two variants:
 * - The old one "blowfish" has a flaw which makes it much easier to crack the
 *   key.  To see this, make a text file with one line of 1000 "x" characters
 *   and write it encrypted.  Use "xxd" to inspect the bytes in the file.  You
 *   will see that a block of 8 bytes repeats 8 times.
 * - The new one "blowfish2" is better.  It uses an 8 byte CFB to avoid the
 *   repeats.
 */

#include "vim.h"

#if defined(FEAT_CRYPT) || defined(PROTO)

#define ARRAY_LENGTH(A)      (sizeof(A)/sizeof(A[0]))

#define BF_BLOCK    8
#define BF_BLOCK_MASK 7
#define BF_MAX_CFB_LEN  (8 * BF_BLOCK)

typedef union {
    UINT32_T ul[2];
    char_u   uc[8];
} block8;

#if defined(WIN3264)
  /* MS-Windows is always little endian */
#else
# ifdef HAVE_CONFIG_H
   /* in configure.ac AC_C_BIGENDIAN() defines WORDS_BIGENDIAN when needed */
# else
   error!
   Please change this code to define WORDS_BIGENDIAN for big-endian machines.
# endif
#endif

/* The state of encryption, referenced by cryptstate_T. */
typedef struct {
    UINT32_T	pax[18];	    /* P-array */
    UINT32_T	sbx[4][256];	    /* S-boxes */
    int		randbyte_offset;
    int		update_offset;
    char_u	cfb_buffer[BF_MAX_CFB_LEN]; /* up to 64 bytes used */
    int		cfb_len;	    /* size of cfb_buffer actually used */
} bf_state_T;


/* Blowfish code */
static UINT32_T pax_init[18] = {
    0x243f6a88u, 0x85a308d3u, 0x13198a2eu,
    0x03707344u, 0xa4093822u, 0x299f31d0u,
    0x082efa98u, 0xec4e6c89u, 0x452821e6u,
    0x38d01377u, 0xbe5466cfu, 0x34e90c6cu,
    0xc0ac29b7u, 0xc97c50ddu, 0x3f84d5b5u,
    0xb5470917u, 0x9216d5d9u, 0x8979fb1bu
};

static UINT32_T sbx_init[4][256] = {
   {0xd1310ba6u, 0x98dfb5acu, 0x2ffd72dbu, 0xd01adfb7u,
    0xb8e1afedu, 0x6a267e96u, 0xba7c9045u, 0xf12c7f99u,
    0x24a19947u, 0xb3916cf7u, 0x0801f2e2u, 0x858efc16u,
    0x636920d8u, 0x71574e69u, 0xa458fea3u, 0xf4933d7eu,
    0x0d95748fu, 0x728eb658u, 0x718bcd58u, 0x82154aeeu,
    0x7b54a41du, 0xc25a59b5u, 0x9c30d539u, 0x2af26013u,
    0xc5d1b023u, 0x286085f0u, 0xca417918u, 0xb8db38efu,
    0x8e79dcb0u, 0x603a180eu, 0x6c9e0e8bu, 0xb01e8a3eu,
    0xd71577c1u, 0xbd314b27u, 0x78af2fdau, 0x55605c60u,
    0xe65525f3u, 0xaa55ab94u, 0x57489862u, 0x63e81440u,
    0x55ca396au, 0x2aab10b6u, 0xb4cc5c34u, 0x1141e8ceu,
    0xa15486afu, 0x7c72e993u, 0xb3ee1411u, 0x636fbc2au,
    0x2ba9c55du, 0x741831f6u, 0xce5c3e16u, 0x9b87931eu,
    0xafd6ba33u, 0x6c24cf5cu, 0x7a325381u, 0x28958677u,
    0x3b8f4898u, 0x6b4bb9afu, 0xc4bfe81bu, 0x66282193u,
    0x61d809ccu, 0xfb21a991u, 0x487cac60u, 0x5dec8032u,
    0xef845d5du, 0xe98575b1u, 0xdc262302u, 0xeb651b88u,
    0x23893e81u, 0xd396acc5u, 0x0f6d6ff3u, 0x83f44239u,
    0x2e0b4482u, 0xa4842004u, 0x69c8f04au, 0x9e1f9b5eu,
    0x21c66842u, 0xf6e96c9au, 0x670c9c61u, 0xabd388f0u,
    0x6a51a0d2u, 0xd8542f68u, 0x960fa728u, 0xab5133a3u,
    0x6eef0b6cu, 0x137a3be4u, 0xba3bf050u, 0x7efb2a98u,
    0xa1f1651du, 0x39af0176u, 0x66ca593eu, 0x82430e88u,
    0x8cee8619u, 0x456f9fb4u, 0x7d84a5c3u, 0x3b8b5ebeu,
    0xe06f75d8u, 0x85c12073u, 0x401a449fu, 0x56c16aa6u,
    0x4ed3aa62u, 0x363f7706u, 0x1bfedf72u, 0x429b023du,
    0x37d0d724u, 0xd00a1248u, 0xdb0fead3u, 0x49f1c09bu,
    0x075372c9u, 0x80991b7bu, 0x25d479d8u, 0xf6e8def7u,
    0xe3fe501au, 0xb6794c3bu, 0x976ce0bdu, 0x04c006bau,
    0xc1a94fb6u, 0x409f60c4u, 0x5e5c9ec2u, 0x196a2463u,
    0x68fb6fafu, 0x3e6c53b5u, 0x1339b2ebu, 0x3b52ec6fu,
    0x6dfc511fu, 0x9b30952cu, 0xcc814544u, 0xaf5ebd09u,
    0xbee3d004u, 0xde334afdu, 0x660f2807u, 0x192e4bb3u,
    0xc0cba857u, 0x45c8740fu, 0xd20b5f39u, 0xb9d3fbdbu,
    0x5579c0bdu, 0x1a60320au, 0xd6a100c6u, 0x402c7279u,
    0x679f25feu, 0xfb1fa3ccu, 0x8ea5e9f8u, 0xdb3222f8u,
    0x3c7516dfu, 0xfd616b15u, 0x2f501ec8u, 0xad0552abu,
    0x323db5fau, 0xfd238760u, 0x53317b48u, 0x3e00df82u,
    0x9e5c57bbu, 0xca6f8ca0u, 0x1a87562eu, 0xdf1769dbu,
    0xd542a8f6u, 0x287effc3u, 0xac6732c6u, 0x8c4f5573u,
    0x695b27b0u, 0xbbca58c8u, 0xe1ffa35du, 0xb8f011a0u,
    0x10fa3d98u, 0xfd2183b8u, 0x4afcb56cu, 0x2dd1d35bu,
    0x9a53e479u, 0xb6f84565u, 0xd28e49bcu, 0x4bfb9790u,
    0xe1ddf2dau, 0xa4cb7e33u, 0x62fb1341u, 0xcee4c6e8u,
    0xef20cadau, 0x36774c01u, 0xd07e9efeu, 0x2bf11fb4u,
    0x95dbda4du, 0xae909198u, 0xeaad8e71u, 0x6b93d5a0u,
    0xd08ed1d0u, 0xafc725e0u, 0x8e3c5b2fu, 0x8e7594b7u,
    0x8ff6e2fbu, 0xf2122b64u, 0x8888b812u, 0x900df01cu,
    0x4fad5ea0u, 0x688fc31cu, 0xd1cff191u, 0xb3a8c1adu,
    0x2f2f2218u, 0xbe0e1777u, 0xea752dfeu, 0x8b021fa1u,
    0xe5a0cc0fu, 0xb56f74e8u, 0x18acf3d6u, 0xce89e299u,
    0xb4a84fe0u, 0xfd13e0b7u, 0x7cc43b81u, 0xd2ada8d9u,
    0x165fa266u, 0x80957705u, 0x93cc7314u, 0x211a1477u,
    0xe6ad2065u, 0x77b5fa86u, 0xc75442f5u, 0xfb9d35cfu,
    0xebcdaf0cu, 0x7b3e89a0u, 0xd6411bd3u, 0xae1e7e49u,
    0x00250e2du, 0x2071b35eu, 0x226800bbu, 0x57b8e0afu,
    0x2464369bu, 0xf009b91eu, 0x5563911du, 0x59dfa6aau,
    0x78c14389u, 0xd95a537fu, 0x207d5ba2u, 0x02e5b9c5u,
    0x83260376u, 0x6295cfa9u, 0x11c81968u, 0x4e734a41u,
    0xb3472dcau, 0x7b14a94au, 0x1b510052u, 0x9a532915u,
    0xd60f573fu, 0xbc9bc6e4u, 0x2b60a476u, 0x81e67400u,
    0x08ba6fb5u, 0x571be91fu, 0xf296ec6bu, 0x2a0dd915u,
    0xb6636521u, 0xe7b9f9b6u, 0xff34052eu, 0xc5855664u,
    0x53b02d5du, 0xa99f8fa1u, 0x08ba4799u, 0x6e85076au},
   {0x4b7a70e9u, 0xb5b32944u, 0xdb75092eu, 0xc4192623u,
    0xad6ea6b0u, 0x49a7df7du, 0x9cee60b8u, 0x8fedb266u,
    0xecaa8c71u, 0x699a17ffu, 0x5664526cu, 0xc2b19ee1u,
    0x193602a5u, 0x75094c29u, 0xa0591340u, 0xe4183a3eu,
    0x3f54989au, 0x5b429d65u, 0x6b8fe4d6u, 0x99f73fd6u,
    0xa1d29c07u, 0xefe830f5u, 0x4d2d38e6u, 0xf0255dc1u,
    0x4cdd2086u, 0x8470eb26u, 0x6382e9c6u, 0x021ecc5eu,
    0x09686b3fu, 0x3ebaefc9u, 0x3c971814u, 0x6b6a70a1u,
    0x687f3584u, 0x52a0e286u, 0xb79c5305u, 0xaa500737u,
    0x3e07841cu, 0x7fdeae5cu, 0x8e7d44ecu, 0x5716f2b8u,
    0xb03ada37u, 0xf0500c0du, 0xf01c1f04u, 0x0200b3ffu,
    0xae0cf51au, 0x3cb574b2u, 0x25837a58u, 0xdc0921bdu,
    0xd19113f9u, 0x7ca92ff6u, 0x94324773u, 0x22f54701u,
    0x3ae5e581u, 0x37c2dadcu, 0xc8b57634u, 0x9af3dda7u,
    0xa9446146u, 0x0fd0030eu, 0xecc8c73eu, 0xa4751e41u,
    0xe238cd99u, 0x3bea0e2fu, 0x3280bba1u, 0x183eb331u,
    0x4e548b38u, 0x4f6db908u, 0x6f420d03u, 0xf60a04bfu,
    0x2cb81290u, 0x24977c79u, 0x5679b072u, 0xbcaf89afu,
    0xde9a771fu, 0xd9930810u, 0xb38bae12u, 0xdccf3f2eu,
    0x5512721fu, 0x2e6b7124u, 0x501adde6u, 0x9f84cd87u,
    0x7a584718u, 0x7408da17u, 0xbc9f9abcu, 0xe94b7d8cu,
    0xec7aec3au, 0xdb851dfau, 0x63094366u, 0xc464c3d2u,
    0xef1c1847u, 0x3215d908u, 0xdd433b37u, 0x24c2ba16u,
    0x12a14d43u, 0x2a65c451u, 0x50940002u, 0x133ae4ddu,
    0x71dff89eu, 0x10314e55u, 0x81ac77d6u, 0x5f11199bu,
    0x043556f1u, 0xd7a3c76bu, 0x3c11183bu, 0x5924a509u,
    0xf28fe6edu, 0x97f1fbfau, 0x9ebabf2cu, 0x1e153c6eu,
    0x86e34570u, 0xeae96fb1u, 0x860e5e0au, 0x5a3e2ab3u,
    0x771fe71cu, 0x4e3d06fau, 0x2965dcb9u, 0x99e71d0fu,
    0x803e89d6u, 0x5266c825u, 0x2e4cc978u, 0x9c10b36au,
    0xc6150ebau, 0x94e2ea78u, 0xa5fc3c53u, 0x1e0a2df4u,
    0xf2f74ea7u, 0x361d2b3du, 0x1939260fu, 0x19c27960u,
    0x5223a708u, 0xf71312b6u, 0xebadfe6eu, 0xeac31f66u,
    0xe3bc4595u, 0xa67bc883u, 0xb17f37d1u, 0x018cff28u,
    0xc332ddefu, 0xbe6c5aa5u, 0x65582185u, 0x68ab9802u,
    0xeecea50fu, 0xdb2f953bu, 0x2aef7dadu, 0x5b6e2f84u,
    0x1521b628u, 0x29076170u, 0xecdd4775u, 0x619f1510u,
    0x13cca830u, 0xeb61bd96u, 0x0334fe1eu, 0xaa0363cfu,
    0xb5735c90u, 0x4c70a239u, 0xd59e9e0bu, 0xcbaade14u,
    0xeecc86bcu, 0x60622ca7u, 0x9cab5cabu, 0xb2f3846eu,
    0x648b1eafu, 0x19bdf0cau, 0xa02369b9u, 0x655abb50u,
    0x40685a32u, 0x3c2ab4b3u, 0x319ee9d5u, 0xc021b8f7u,
    0x9b540b19u, 0x875fa099u, 0x95f7997eu, 0x623d7da8u,
    0xf837889au, 0x97e32d77u, 0x11ed935fu, 0x16681281u,
    0x0e358829u, 0xc7e61fd6u, 0x96dedfa1u, 0x7858ba99u,
    0x57f584a5u, 0x1b227263u, 0x9b83c3ffu, 0x1ac24696u,
    0xcdb30aebu, 0x532e3054u, 0x8fd948e4u, 0x6dbc3128u,
    0x58ebf2efu, 0x34c6ffeau, 0xfe28ed61u, 0xee7c3c73u,
    0x5d4a14d9u, 0xe864b7e3u, 0x42105d14u, 0x203e13e0u,
    0x45eee2b6u, 0xa3aaabeau, 0xdb6c4f15u, 0xfacb4fd0u,
    0xc742f442u, 0xef6abbb5u, 0x654f3b1du, 0x41cd2105u,
    0xd81e799eu, 0x86854dc7u, 0xe44b476au, 0x3d816250u,
    0xcf62a1f2u, 0x5b8d2646u, 0xfc8883a0u, 0xc1c7b6a3u,
    0x7f1524c3u, 0x69cb7492u, 0x47848a0bu, 0x5692b285u,
    0x095bbf00u, 0xad19489du, 0x1462b174u, 0x23820e00u,
    0x58428d2au, 0x0c55f5eau, 0x1dadf43eu, 0x233f7061u,
    0x3372f092u, 0x8d937e41u, 0xd65fecf1u, 0x6c223bdbu,
    0x7cde3759u, 0xcbee7460u, 0x4085f2a7u, 0xce77326eu,
    0xa6078084u, 0x19f8509eu, 0xe8efd855u, 0x61d99735u,
    0xa969a7aau, 0xc50c06c2u, 0x5a04abfcu, 0x800bcadcu,
    0x9e447a2eu, 0xc3453484u, 0xfdd56705u, 0x0e1e9ec9u,
    0xdb73dbd3u, 0x105588cdu, 0x675fda79u, 0xe3674340u,
    0xc5c43465u, 0x713e38d8u, 0x3d28f89eu, 0xf16dff20u,
    0x153e21e7u, 0x8fb03d4au, 0xe6e39f2bu, 0xdb83adf7u},
   {0xe93d5a68u, 0x948140f7u, 0xf64c261cu, 0x94692934u,
    0x411520f7u, 0x7602d4f7u, 0xbcf46b2eu, 0xd4a20068u,
    0xd4082471u, 0x3320f46au, 0x43b7d4b7u, 0x500061afu,
    0x1e39f62eu, 0x97244546u, 0x14214f74u, 0xbf8b8840u,
    0x4d95fc1du, 0x96b591afu, 0x70f4ddd3u, 0x66a02f45u,
    0xbfbc09ecu, 0x03bd9785u, 0x7fac6dd0u, 0x31cb8504u,
    0x96eb27b3u, 0x55fd3941u, 0xda2547e6u, 0xabca0a9au,
    0x28507825u, 0x530429f4u, 0x0a2c86dau, 0xe9b66dfbu,
    0x68dc1462u, 0xd7486900u, 0x680ec0a4u, 0x27a18deeu,
    0x4f3ffea2u, 0xe887ad8cu, 0xb58ce006u, 0x7af4d6b6u,
    0xaace1e7cu, 0xd3375fecu, 0xce78a399u, 0x406b2a42u,
    0x20fe9e35u, 0xd9f385b9u, 0xee39d7abu, 0x3b124e8bu,
    0x1dc9faf7u, 0x4b6d1856u, 0x26a36631u, 0xeae397b2u,
    0x3a6efa74u, 0xdd5b4332u, 0x6841e7f7u, 0xca7820fbu,
    0xfb0af54eu, 0xd8feb397u, 0x454056acu, 0xba489527u,
    0x55533a3au, 0x20838d87u, 0xfe6ba9b7u, 0xd096954bu,
    0x55a867bcu, 0xa1159a58u, 0xcca92963u, 0x99e1db33u,
    0xa62a4a56u, 0x3f3125f9u, 0x5ef47e1cu, 0x9029317cu,
    0xfdf8e802u, 0x04272f70u, 0x80bb155cu, 0x05282ce3u,
    0x95c11548u, 0xe4c66d22u, 0x48c1133fu, 0xc70f86dcu,
    0x07f9c9eeu, 0x41041f0fu, 0x404779a4u, 0x5d886e17u,
    0x325f51ebu, 0xd59bc0d1u, 0xf2bcc18fu, 0x41113564u,
    0x257b7834u, 0x602a9c60u, 0xdff8e8a3u, 0x1f636c1bu,
    0x0e12b4c2u, 0x02e1329eu, 0xaf664fd1u, 0xcad18115u,
    0x6b2395e0u, 0x333e92e1u, 0x3b240b62u, 0xeebeb922u,
    0x85b2a20eu, 0xe6ba0d99u, 0xde720c8cu, 0x2da2f728u,
    0xd0127845u, 0x95b794fdu, 0x647d0862u, 0xe7ccf5f0u,
    0x5449a36fu, 0x877d48fau, 0xc39dfd27u, 0xf33e8d1eu,
    0x0a476341u, 0x992eff74u, 0x3a6f6eabu, 0xf4f8fd37u,
    0xa812dc60u, 0xa1ebddf8u, 0x991be14cu, 0xdb6e6b0du,
    0xc67b5510u, 0x6d672c37u, 0x2765d43bu, 0xdcd0e804u,
    0xf1290dc7u, 0xcc00ffa3u, 0xb5390f92u, 0x690fed0bu,
    0x667b9ffbu, 0xcedb7d9cu, 0xa091cf0bu, 0xd9155ea3u,
    0xbb132f88u, 0x515bad24u, 0x7b9479bfu, 0x763bd6ebu,
    0x37392eb3u, 0xcc115979u, 0x8026e297u, 0xf42e312du,
    0x6842ada7u, 0xc66a2b3bu, 0x12754cccu, 0x782ef11cu,
    0x6a124237u, 0xb79251e7u, 0x06a1bbe6u, 0x4bfb6350u,
    0x1a6b1018u, 0x11caedfau, 0x3d25bdd8u, 0xe2e1c3c9u,
    0x44421659u, 0x0a121386u, 0xd90cec6eu, 0xd5abea2au,
    0x64af674eu, 0xda86a85fu, 0xbebfe988u, 0x64e4c3feu,
    0x9dbc8057u, 0xf0f7c086u, 0x60787bf8u, 0x6003604du,
    0xd1fd8346u, 0xf6381fb0u, 0x7745ae04u, 0xd736fcccu,
    0x83426b33u, 0xf01eab71u, 0xb0804187u, 0x3c005e5fu,
    0x77a057beu, 0xbde8ae24u, 0x55464299u, 0xbf582e61u,
    0x4e58f48fu, 0xf2ddfda2u, 0xf474ef38u, 0x8789bdc2u,
    0x5366f9c3u, 0xc8b38e74u, 0xb475f255u, 0x46fcd9b9u,
    0x7aeb2661u, 0x8b1ddf84u, 0x846a0e79u, 0x915f95e2u,
    0x466e598eu, 0x20b45770u, 0x8cd55591u, 0xc902de4cu,
    0xb90bace1u, 0xbb8205d0u, 0x11a86248u, 0x7574a99eu,
    0xb77f19b6u, 0xe0a9dc09u, 0x662d09a1u, 0xc4324633u,
    0xe85a1f02u, 0x09f0be8cu, 0x4a99a025u, 0x1d6efe10u,
    0x1ab93d1du, 0x0ba5a4dfu, 0xa186f20fu, 0x2868f169u,
    0xdcb7da83u, 0x573906feu, 0xa1e2ce9bu, 0x4fcd7f52u,
    0x50115e01u, 0xa70683fau, 0xa002b5c4u, 0x0de6d027u,
    0x9af88c27u, 0x773f8641u, 0xc3604c06u, 0x61a806b5u,
    0xf0177a28u, 0xc0f586e0u, 0x006058aau, 0x30dc7d62u,
    0x11e69ed7u, 0x2338ea63u, 0x53c2dd94u, 0xc2c21634u,
    0xbbcbee56u, 0x90bcb6deu, 0xebfc7da1u, 0xce591d76u,
    0x6f05e409u, 0x4b7c0188u, 0x39720a3du, 0x7c927c24u,
    0x86e3725fu, 0x724d9db9u, 0x1ac15bb4u, 0xd39eb8fcu,
    0xed545578u, 0x08fca5b5u, 0xd83d7cd3u, 0x4dad0fc4u,
    0x1e50ef5eu, 0xb161e6f8u, 0xa28514d9u, 0x6c51133cu,
    0x6fd5c7e7u, 0x56e14ec4u, 0x362abfceu, 0xddc6c837u,
    0xd79a3234u, 0x92638212u, 0x670efa8eu, 0x406000e0u},
   {0x3a39ce37u, 0xd3faf5cfu, 0xabc27737u, 0x5ac52d1bu,
    0x5cb0679eu, 0x4fa33742u, 0xd3822740u, 0x99bc9bbeu,
    0xd5118e9du, 0xbf0f7315u, 0xd62d1c7eu, 0xc700c47bu,
    0xb78c1b6bu, 0x21a19045u, 0xb26eb1beu, 0x6a366eb4u,
    0x5748ab2fu, 0xbc946e79u, 0xc6a376d2u, 0x6549c2c8u,
    0x530ff8eeu, 0x468dde7du, 0xd5730a1du, 0x4cd04dc6u,
    0x2939bbdbu, 0xa9ba4650u, 0xac9526e8u, 0xbe5ee304u,
    0xa1fad5f0u, 0x6a2d519au, 0x63ef8ce2u, 0x9a86ee22u,
    0xc089c2b8u, 0x43242ef6u, 0xa51e03aau, 0x9cf2d0a4u,
    0x83c061bau, 0x9be96a4du, 0x8fe51550u, 0xba645bd6u,
    0x2826a2f9u, 0xa73a3ae1u, 0x4ba99586u, 0xef5562e9u,
    0xc72fefd3u, 0xf752f7dau, 0x3f046f69u, 0x77fa0a59u,
    0x80e4a915u, 0x87b08601u, 0x9b09e6adu, 0x3b3ee593u,
    0xe990fd5au, 0x9e34d797u, 0x2cf0b7d9u, 0x022b8b51u,
    0x96d5ac3au, 0x017da67du, 0xd1cf3ed6u, 0x7c7d2d28u,
    0x1f9f25cfu, 0xadf2b89bu, 0x5ad6b472u, 0x5a88f54cu,
    0xe029ac71u, 0xe019a5e6u, 0x47b0acfdu, 0xed93fa9bu,
    0xe8d3c48du, 0x283b57ccu, 0xf8d56629u, 0x79132e28u,
    0x785f0191u, 0xed756055u, 0xf7960e44u, 0xe3d35e8cu,
    0x15056dd4u, 0x88f46dbau, 0x03a16125u, 0x0564f0bdu,
    0xc3eb9e15u, 0x3c9057a2u, 0x97271aecu, 0xa93a072au,
    0x1b3f6d9bu, 0x1e6321f5u, 0xf59c66fbu, 0x26dcf319u,
    0x7533d928u, 0xb155fdf5u, 0x03563482u, 0x8aba3cbbu,
    0x28517711u, 0xc20ad9f8u, 0xabcc5167u, 0xccad925fu,
    0x4de81751u, 0x3830dc8eu, 0x379d5862u, 0x9320f991u,
    0xea7a90c2u, 0xfb3e7bceu, 0x5121ce64u, 0x774fbe32u,
    0xa8b6e37eu, 0xc3293d46u, 0x48de5369u, 0x6413e680u,
    0xa2ae0810u, 0xdd6db224u, 0x69852dfdu, 0x09072166u,
    0xb39a460au, 0x6445c0ddu, 0x586cdecfu, 0x1c20c8aeu,
    0x5bbef7ddu, 0x1b588d40u, 0xccd2017fu, 0x6bb4e3bbu,
    0xdda26a7eu, 0x3a59ff45u, 0x3e350a44u, 0xbcb4cdd5u,
    0x72eacea8u, 0xfa6484bbu, 0x8d6612aeu, 0xbf3c6f47u,
    0xd29be463u, 0x542f5d9eu, 0xaec2771bu, 0xf64e6370u,
    0x740e0d8du, 0xe75b1357u, 0xf8721671u, 0xaf537d5du,
    0x4040cb08u, 0x4eb4e2ccu, 0x34d2466au, 0x0115af84u,
    0xe1b00428u, 0x95983a1du, 0x06b89fb4u, 0xce6ea048u,
    0x6f3f3b82u, 0x3520ab82u, 0x011a1d4bu, 0x277227f8u,
    0x611560b1u, 0xe7933fdcu, 0xbb3a792bu, 0x344525bdu,
    0xa08839e1u, 0x51ce794bu, 0x2f32c9b7u, 0xa01fbac9u,
    0xe01cc87eu, 0xbcc7d1f6u, 0xcf0111c3u, 0xa1e8aac7u,
    0x1a908749u, 0xd44fbd9au, 0xd0dadecbu, 0xd50ada38u,
    0x0339c32au, 0xc6913667u, 0x8df9317cu, 0xe0b12b4fu,
    0xf79e59b7u, 0x43f5bb3au, 0xf2d519ffu, 0x27d9459cu,
    0xbf97222cu, 0x15e6fc2au, 0x0f91fc71u, 0x9b941525u,
    0xfae59361u, 0xceb69cebu, 0xc2a86459u, 0x12baa8d1u,
    0xb6c1075eu, 0xe3056a0cu, 0x10d25065u, 0xcb03a442u,
    0xe0ec6e0eu, 0x1698db3bu, 0x4c98a0beu, 0x3278e964u,
    0x9f1f9532u, 0xe0d392dfu, 0xd3a0342bu, 0x8971f21eu,
    0x1b0a7441u, 0x4ba3348cu, 0xc5be7120u, 0xc37632d8u,
    0xdf359f8du, 0x9b992f2eu, 0xe60b6f47u, 0x0fe3f11du,
    0xe54cda54u, 0x1edad891u, 0xce6279cfu, 0xcd3e7e6fu,
    0x1618b166u, 0xfd2c1d05u, 0x848fd2c5u, 0xf6fb2299u,
    0xf523f357u, 0xa6327623u, 0x93a83531u, 0x56cccd02u,
    0xacf08162u, 0x5a75ebb5u, 0x6e163697u, 0x88d273ccu,
    0xde966292u, 0x81b949d0u, 0x4c50901bu, 0x71c65614u,
    0xe6c6c7bdu, 0x327a140au, 0x45e1d006u, 0xc3f27b9au,
    0xc9aa53fdu, 0x62a80f00u, 0xbb25bfe2u, 0x35bdd2f6u,
    0x71126905u, 0xb2040222u, 0xb6cbcf7cu, 0xcd769c2bu,
    0x53113ec0u, 0x1640e3d3u, 0x38abbd60u, 0x2547adf0u,
    0xba38209cu, 0xf746ce76u, 0x77afa1c5u, 0x20756060u,
    0x85cbfe4eu, 0x8ae88dd8u, 0x7aaaf9b0u, 0x4cf9aa7eu,
    0x1948c25cu, 0x02fb8a8cu, 0x01c36ae4u, 0xd6ebe1f9u,
    0x90d4f869u, 0xa65cdea0u, 0x3f09252du, 0xc208e69fu,
    0xb74e6132u, 0xce77e25bu, 0x578fdfe3u, 0x3ac372e6u
 }
};

#define F1(i) \
    xl ^= bfs->pax[i]; \
    xr ^= ((bfs->sbx[0][xl >> 24] + \
    bfs->sbx[1][(xl & 0xFF0000) >> 16]) ^ \
    bfs->sbx[2][(xl & 0xFF00) >> 8]) + \
    bfs->sbx[3][xl & 0xFF];

#define F2(i) \
    xr ^= bfs->pax[i]; \
    xl ^= ((bfs->sbx[0][xr >> 24] + \
    bfs->sbx[1][(xr & 0xFF0000) >> 16]) ^ \
    bfs->sbx[2][(xr & 0xFF00) >> 8]) + \
    bfs->sbx[3][xr & 0xFF];

    static void
bf_e_block(
    bf_state_T *bfs,
    UINT32_T *p_xl,
    UINT32_T *p_xr)
{
    UINT32_T temp;
    UINT32_T xl = *p_xl;
    UINT32_T xr = *p_xr;

    F1(0) F2(1)
    F1(2) F2(3)
    F1(4) F2(5)
    F1(6) F2(7)
    F1(8) F2(9)
    F1(10) F2(11)
    F1(12) F2(13)
    F1(14) F2(15)
    xl ^= bfs->pax[16];
    xr ^= bfs->pax[17];
    temp = xl;
    xl = xr;
    xr = temp;
    *p_xl = xl;
    *p_xr = xr;
}


#ifdef WORDS_BIGENDIAN
# define htonl2(x) \
    x = ((((x) &     0xffL) << 24) | (((x) & 0xff00L)     <<  8) | \
	 (((x) & 0xff0000L) >>  8) | (((x) & 0xff000000L) >> 24))
#else
# define htonl2(x)
#endif

    static void
bf_e_cblock(
    bf_state_T *bfs,
    char_u *block)
{
    block8	bk;

    memcpy(bk.uc, block, 8);
    htonl2(bk.ul[0]);
    htonl2(bk.ul[1]);
    bf_e_block(bfs, &bk.ul[0], &bk.ul[1]);
    htonl2(bk.ul[0]);
    htonl2(bk.ul[1]);
    memcpy(block, bk.uc, 8);
}

/*
 * Initialize the crypt method using "password" as the encryption key and
 * "salt[salt_len]" as the salt.
 */
    static void
bf_key_init(
    bf_state_T	*bfs,
    char_u	*password,
    char_u	*salt,
    int		salt_len)
{
    int      i, j, keypos = 0;
    unsigned u;
    UINT32_T val, data_l, data_r;
    char_u   *key;
    int      keylen;

    /* Process the key 1001 times.
     * See http://en.wikipedia.org/wiki/Key_strengthening. */
    key = sha256_key(password, salt, salt_len);
    for (i = 0; i < 1000; i++)
	key = sha256_key(key, salt, salt_len);

    /* Convert the key from 64 hex chars to 32 binary chars. */
    keylen = (int)STRLEN(key) / 2;
    if (keylen == 0)
    {
	IEMSG(_("E831: bf_key_init() called with empty password"));
	return;
    }
    for (i = 0; i < keylen; i++)
    {
	sscanf((char *)&key[i * 2], "%2x", &u);
	key[i] = u;
    }

    /* Use "key" to initialize the P-array ("pax") and S-boxes ("sbx") of
     * Blowfish. */
    mch_memmove(bfs->sbx, sbx_init, 4 * 4 * 256);

    for (i = 0; i < 18; ++i)
    {
	val = 0;
	for (j = 0; j < 4; ++j)
	    val = (val << 8) | key[keypos++ % keylen];
	bfs->pax[i] = pax_init[i] ^ val;
    }

    data_l = data_r = 0;
    for (i = 0; i < 18; i += 2)
    {
	bf_e_block(bfs, &data_l, &data_r);
	bfs->pax[i + 0] = data_l;
	bfs->pax[i + 1] = data_r;
    }

    for (i = 0; i < 4; ++i)
    {
	for (j = 0; j < 256; j += 2)
	{
	    bf_e_block(bfs, &data_l, &data_r);
	    bfs->sbx[i][j + 0] = data_l;
	    bfs->sbx[i][j + 1] = data_r;
	}
    }
}

/*
 * Blowfish self-test for corrupted tables or instructions.
 */
    static int
bf_check_tables(
    UINT32_T pax[18],
    UINT32_T sbx[4][256],
    UINT32_T val)
{
    int i, j;
    UINT32_T c = 0;

    for (i = 0; i < 18; i++)
	c ^= pax[i];
    for (i = 0; i < 4; i++)
	for (j = 0; j < 256; j++)
	    c ^= sbx[i][j];
    return c == val;
}

typedef struct {
    char_u   password[64];
    char_u   salt[9];
    char_u   plaintxt[9];
    char_u   cryptxt[9];
    char_u   badcryptxt[9]; /* cryptxt when big/little endian is wrong */
    UINT32_T keysum;
} struct_bf_test_data;

/*
 * Assert bf(password, plaintxt) is cryptxt.
 * Assert csum(pax sbx(password)) is keysum.
 */
static struct_bf_test_data bf_test_data[] = {
  {
      "password",
      "salt",
      "plaintxt",
      "\xad\x3d\xfa\x7f\xe8\xea\x40\xf6", /* cryptxt */
      "\x72\x50\x3b\x38\x10\x60\x22\xa7", /* badcryptxt */
      0x56701b5du /* keysum */
  },
};

/*
 * Return FAIL when there is something wrong with blowfish encryption.
 */
    static int
bf_self_test(void)
{
    int    i, bn;
    int    err = 0;
    block8 bk;
    UINT32_T ui = 0xffffffffUL;
    bf_state_T state;

    vim_memset(&state, 0, sizeof(bf_state_T));
    state.cfb_len = BF_MAX_CFB_LEN;

    /* We can't simply use sizeof(UINT32_T), it would generate a compiler
     * warning. */
    if (ui != 0xffffffffUL || ui + 1 != 0) {
	err++;
	EMSG(_("E820: sizeof(uint32_t) != 4"));
    }

    if (!bf_check_tables(pax_init, sbx_init, 0x6ffa520a))
	err++;

    bn = ARRAY_LENGTH(bf_test_data);
    for (i = 0; i < bn; i++)
    {
	bf_key_init(&state, (char_u *)(bf_test_data[i].password),
		    bf_test_data[i].salt,
		    (int)STRLEN(bf_test_data[i].salt));
	if (!bf_check_tables(state.pax, state.sbx, bf_test_data[i].keysum))
	    err++;

	/* Don't modify bf_test_data[i].plaintxt, self test is idempotent. */
	memcpy(bk.uc, bf_test_data[i].plaintxt, 8);
	bf_e_cblock(&state, bk.uc);
	if (memcmp(bk.uc, bf_test_data[i].cryptxt, 8) != 0)
	{
	    if (err == 0 && memcmp(bk.uc, bf_test_data[i].badcryptxt, 8) == 0)
		EMSG(_("E817: Blowfish big/little endian use wrong"));
	    err++;
	}
    }

    return err > 0 ? FAIL : OK;
}

/*
 * CFB: Cipher Feedback Mode.
 */

/*
 * Initialize with seed "seed[seed_len]".
 */
    static void
bf_cfb_init(
    bf_state_T	*bfs,
    char_u	*seed,
    int		seed_len)
{
    int i, mi;

    bfs->randbyte_offset = bfs->update_offset = 0;
    vim_memset(bfs->cfb_buffer, 0, bfs->cfb_len);
    if (seed_len > 0)
    {
	mi = seed_len > bfs->cfb_len ? seed_len : bfs->cfb_len;
	for (i = 0; i < mi; i++)
	    bfs->cfb_buffer[i % bfs->cfb_len] ^= seed[i % seed_len];
    }
}

#define BF_CFB_UPDATE(bfs, c) { \
    bfs->cfb_buffer[bfs->update_offset] ^= (char_u)c; \
    if (++bfs->update_offset == bfs->cfb_len) \
	bfs->update_offset = 0; \
}

#define BF_RANBYTE(bfs, t) { \
    if ((bfs->randbyte_offset & BF_BLOCK_MASK) == 0) \
	bf_e_cblock(bfs, &(bfs->cfb_buffer[bfs->randbyte_offset])); \
    t = bfs->cfb_buffer[bfs->randbyte_offset]; \
    if (++bfs->randbyte_offset == bfs->cfb_len) \
	bfs->randbyte_offset = 0; \
}

/*
 * Encrypt "from[len]" into "to[len]".
 * "from" and "to" can be equal to encrypt in place.
 */
    void
crypt_blowfish_encode(
    cryptstate_T *state,
    char_u	*from,
    size_t	len,
    char_u	*to)
{
    bf_state_T *bfs = state->method_state;
    size_t	i;
    int		ztemp, t;

    for (i = 0; i < len; ++i)
    {
	ztemp = from[i];
	BF_RANBYTE(bfs, t);
	BF_CFB_UPDATE(bfs, ztemp);
	to[i] = t ^ ztemp;
    }
}

/*
 * Decrypt "from[len]" into "to[len]".
 */
    void
crypt_blowfish_decode(
    cryptstate_T *state,
    char_u	*from,
    size_t	len,
    char_u	*to)
{
    bf_state_T *bfs = state->method_state;
    size_t	i;
    int		t;

    for (i = 0; i < len; ++i)
    {
	BF_RANBYTE(bfs, t);
	to[i] = from[i] ^ t;
	BF_CFB_UPDATE(bfs, to[i]);
    }
}

    void
crypt_blowfish_init(
    cryptstate_T	*state,
    char_u*		key,
    char_u*		salt,
    int			salt_len,
    char_u*		seed,
    int			seed_len)
{
    bf_state_T	*bfs = (bf_state_T *)alloc_clear(sizeof(bf_state_T));

    state->method_state = bfs;

    /* "blowfish" uses a 64 byte buffer, causing it to repeat 8 byte groups 8
     * times.  "blowfish2" uses a 8 byte buffer to avoid repeating. */
    bfs->cfb_len = state->method_nr == CRYPT_M_BF ? BF_MAX_CFB_LEN : BF_BLOCK;

    if (blowfish_self_test() == FAIL)
	return;

    bf_key_init(bfs, key, salt, salt_len);
    bf_cfb_init(bfs, seed, seed_len);
}

/*
 * Run a test to check if the encryption works as expected.
 * Give an error and return FAIL when not.
 */
    int
blowfish_self_test(void)
{
    if (sha256_self_test() == FAIL)
    {
	EMSG(_("E818: sha256 test failed"));
	return FAIL;
    }
    if (bf_self_test() == FAIL)
    {
	EMSG(_("E819: Blowfish test failed"));
	return FAIL;
    }
    return OK;
}

#endif /* FEAT_CRYPT */
/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * buffer.c: functions for dealing with the buffer structure
 */

/*
 * The buffer list is a double linked list of all buffers.
 * Each buffer can be in one of these states:
 * never loaded: BF_NEVERLOADED is set, only the file name is valid
 *   not loaded: b_ml.ml_mfp == NULL, no memfile allocated
 *	 hidden: b_nwindows == 0, loaded but not displayed in a window
 *	 normal: loaded and displayed in a window
 *
 * Instead of storing file names all over the place, each file name is
 * stored in the buffer list. It can be referenced by a number.
 *
 * The current implementation remembers all file names ever used.
 */

#include "vim.h"

static char_u	*buflist_match(regmatch_T *rmp, buf_T *buf, int ignore_case);
static char_u	*fname_match(regmatch_T *rmp, char_u *name, int ignore_case);
static void	buflist_setfpos(buf_T *buf, win_T *win, linenr_T lnum, colnr_T col, int copy_options);
#ifdef UNIX
static buf_T	*buflist_findname_stat(char_u *ffname, stat_T *st);
static int	otherfile_buf(buf_T *buf, char_u *ffname, stat_T *stp);
static int	buf_same_ino(buf_T *buf, stat_T *stp);
#else
static int	otherfile_buf(buf_T *buf, char_u *ffname);
#endif
#ifdef FEAT_TITLE
static int	value_changed(char_u *str, char_u **last);
#endif
static int	append_arg_number(win_T *wp, char_u *buf, int buflen, int add_file);
static void	free_buffer(buf_T *);
static void	free_buffer_stuff(buf_T *buf, int free_options);
static void	clear_wininfo(buf_T *buf);

#ifdef UNIX
# define dev_T dev_t
#else
# define dev_T unsigned
#endif

#if defined(FEAT_QUICKFIX)
static char *msg_loclist = N_("[Location List]");
static char *msg_qflist = N_("[Quickfix List]");
#endif
static char *e_auabort = N_("E855: Autocommands caused command to abort");

/* Number of times free_buffer() was called. */
static int	buf_free_count = 0;

/* Read data from buffer for retrying. */
    static int
read_buffer(
    int		read_stdin,	    /* read file from stdin, otherwise fifo */
    exarg_T	*eap,		    /* for forced 'ff' and 'fenc' or NULL */
    int		flags)		    /* extra flags for readfile() */
{
    int		retval = OK;
    linenr_T	line_count;

    /*
     * Read from the buffer which the text is already filled in and append at
     * the end.  This makes it possible to retry when 'fileformat' or
     * 'fileencoding' was guessed wrong.
     */
    line_count = curbuf->b_ml.ml_line_count;
    retval = readfile(
	    read_stdin ? NULL : curbuf->b_ffname,
	    read_stdin ? NULL : curbuf->b_fname,
	    (linenr_T)line_count, (linenr_T)0, (linenr_T)MAXLNUM, eap,
	    flags | READ_BUFFER);
    if (retval == OK)
    {
	/* Delete the binary lines. */
	while (--line_count >= 0)
	    ml_delete((linenr_T)1, FALSE);
    }
    else
    {
	/* Delete the converted lines. */
	while (curbuf->b_ml.ml_line_count > line_count)
	    ml_delete(line_count, FALSE);
    }
    /* Put the cursor on the first line. */
    curwin->w_cursor.lnum = 1;
    curwin->w_cursor.col = 0;

    if (read_stdin)
    {
	/* Set or reset 'modified' before executing autocommands, so that
	 * it can be changed there. */
	if (!readonlymode && !BUFEMPTY())
	    changed();
	else if (retval == OK)
	    unchanged(curbuf, FALSE);

	if (retval == OK)
	{
#ifdef FEAT_EVAL
	    apply_autocmds_retval(EVENT_STDINREADPOST, NULL, NULL, FALSE,
							      curbuf, &retval);
#else
	    apply_autocmds(EVENT_STDINREADPOST, NULL, NULL, FALSE, curbuf);
#endif
	}
    }
    return retval;
}

/*
 * Open current buffer, that is: open the memfile and read the file into
 * memory.
 * Return FAIL for failure, OK otherwise.
 */
    int
open_buffer(
    int		read_stdin,	    /* read file from stdin */
    exarg_T	*eap,		    /* for forced 'ff' and 'fenc' or NULL */
    int		flags)		    /* extra flags for readfile() */
{
    int		retval = OK;
    bufref_T	old_curbuf;
#ifdef FEAT_SYN_HL
    long	old_tw = curbuf->b_p_tw;
#endif
    int		read_fifo = FALSE;

    /*
     * The 'readonly' flag is only set when BF_NEVERLOADED is being reset.
     * When re-entering the same buffer, it should not change, because the
     * user may have reset the flag by hand.
     */
    if (readonlymode && curbuf->b_ffname != NULL
					&& (curbuf->b_flags & BF_NEVERLOADED))
	curbuf->b_p_ro = TRUE;

    if (ml_open(curbuf) == FAIL)
    {
	/*
	 * There MUST be a memfile, otherwise we can't do anything
	 * If we can't create one for the current buffer, take another buffer
	 */
	close_buffer(NULL, curbuf, 0, FALSE);
	FOR_ALL_BUFFERS(curbuf)
	    if (curbuf->b_ml.ml_mfp != NULL)
		break;
	/*
	 * if there is no memfile at all, exit
	 * This is OK, since there are no changes to lose.
	 */
	if (curbuf == NULL)
	{
	    EMSG(_("E82: Cannot allocate any buffer, exiting..."));
	    getout(2);
	}
	EMSG(_("E83: Cannot allocate buffer, using other one..."));
	enter_buffer(curbuf);
#ifdef FEAT_SYN_HL
	if (old_tw != curbuf->b_p_tw)
	    check_colorcolumn(curwin);
#endif
	return FAIL;
    }

    /* The autocommands in readfile() may change the buffer, but only AFTER
     * reading the file. */
    set_bufref(&old_curbuf, curbuf);
    modified_was_set = FALSE;

    /* mark cursor position as being invalid */
    curwin->w_valid = 0;

    if (curbuf->b_ffname != NULL
#ifdef FEAT_NETBEANS_INTG
	    && netbeansReadFile
#endif
       )
    {
	int old_msg_silent = msg_silent;
#ifdef UNIX
	int save_bin = curbuf->b_p_bin;
	int perm;
#endif
#ifdef FEAT_NETBEANS_INTG
	int oldFire = netbeansFireChanges;

	netbeansFireChanges = 0;
#endif
#ifdef UNIX
	perm = mch_getperm(curbuf->b_ffname);
	if (perm >= 0 && (S_ISFIFO(perm)
		      || S_ISSOCK(perm)
# ifdef OPEN_CHR_FILES
		      || (S_ISCHR(perm) && is_dev_fd_file(curbuf->b_ffname))
# endif
		    ))
		read_fifo = TRUE;
	if (read_fifo)
	    curbuf->b_p_bin = TRUE;
#endif
	if (shortmess(SHM_FILEINFO))
	    msg_silent = 1;
	retval = readfile(curbuf->b_ffname, curbuf->b_fname,
		  (linenr_T)0, (linenr_T)0, (linenr_T)MAXLNUM, eap,
		  flags | READ_NEW | (read_fifo ? READ_FIFO : 0));
#ifdef UNIX
	if (read_fifo)
	{
	    curbuf->b_p_bin = save_bin;
	    if (retval == OK)
		retval = read_buffer(FALSE, eap, flags);
	}
#endif
	msg_silent = old_msg_silent;
#ifdef FEAT_NETBEANS_INTG
	netbeansFireChanges = oldFire;
#endif
	/* Help buffer is filtered. */
	if (bt_help(curbuf))
	    fix_help_buffer();
    }
    else if (read_stdin)
    {
	int	save_bin = curbuf->b_p_bin;

	/*
	 * First read the text in binary mode into the buffer.
	 * Then read from that same buffer and append at the end.  This makes
	 * it possible to retry when 'fileformat' or 'fileencoding' was
	 * guessed wrong.
	 */
	curbuf->b_p_bin = TRUE;
	retval = readfile(NULL, NULL, (linenr_T)0,
		  (linenr_T)0, (linenr_T)MAXLNUM, NULL,
		  flags | (READ_NEW + READ_STDIN));
	curbuf->b_p_bin = save_bin;
	if (retval == OK)
	    retval = read_buffer(TRUE, eap, flags);
    }

    /* if first time loading this buffer, init b_chartab[] */
    if (curbuf->b_flags & BF_NEVERLOADED)
    {
	(void)buf_init_chartab(curbuf, FALSE);
#ifdef FEAT_CINDENT
	parse_cino(curbuf);
#endif
    }

    /*
     * Set/reset the Changed flag first, autocmds may change the buffer.
     * Apply the automatic commands, before processing the modelines.
     * So the modelines have priority over autocommands.
     */
    /* When reading stdin, the buffer contents always needs writing, so set
     * the changed flag.  Unless in readonly mode: "ls | gview -".
     * When interrupted and 'cpoptions' contains 'i' set changed flag. */
    if ((got_int && vim_strchr(p_cpo, CPO_INTMOD) != NULL)
		|| modified_was_set	/* ":set modified" used in autocmd */
#ifdef FEAT_EVAL
		|| (aborting() && vim_strchr(p_cpo, CPO_INTMOD) != NULL)
#endif
       )
	changed();
    else if (retval == OK && !read_stdin && !read_fifo)
	unchanged(curbuf, FALSE);
    save_file_ff(curbuf);		/* keep this fileformat */

    /* Set last_changedtick to avoid triggering a TextChanged autocommand right
     * after it was added. */
    curbuf->b_last_changedtick = CHANGEDTICK(curbuf);
#ifdef FEAT_INS_EXPAND
    curbuf->b_last_changedtick_pum = CHANGEDTICK(curbuf);
#endif

    /* require "!" to overwrite the file, because it wasn't read completely */
#ifdef FEAT_EVAL
    if (aborting())
#else
    if (got_int)
#endif
	curbuf->b_flags |= BF_READERR;

#ifdef FEAT_FOLDING
    /* Need to update automatic folding.  Do this before the autocommands,
     * they may use the fold info. */
    foldUpdateAll(curwin);
#endif

    /* need to set w_topline, unless some autocommand already did that. */
    if (!(curwin->w_valid & VALID_TOPLINE))
    {
	curwin->w_topline = 1;
#ifdef FEAT_DIFF
	curwin->w_topfill = 0;
#endif
    }
#ifdef FEAT_EVAL
    apply_autocmds_retval(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf, &retval);
#else
    apply_autocmds(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf);
#endif

    if (retval == OK)
    {
	/*
	 * The autocommands may have changed the current buffer.  Apply the
	 * modelines to the correct buffer, if it still exists and is loaded.
	 */
	if (bufref_valid(&old_curbuf) && old_curbuf.br_buf->b_ml.ml_mfp != NULL)
	{
	    aco_save_T	aco;

	    /* Go to the buffer that was opened. */
	    aucmd_prepbuf(&aco, old_curbuf.br_buf);
	    do_modelines(0);
	    curbuf->b_flags &= ~(BF_CHECK_RO | BF_NEVERLOADED);

#ifdef FEAT_EVAL
	    apply_autocmds_retval(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf,
								      &retval);
#else
	    apply_autocmds(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf);
#endif

	    /* restore curwin/curbuf and a few other things */
	    aucmd_restbuf(&aco);
	}
    }

    return retval;
}

/*
 * Store "buf" in "bufref" and set the free count.
 */
    void
set_bufref(bufref_T *bufref, buf_T *buf)
{
    bufref->br_buf = buf;
    bufref->br_fnum = buf == NULL ? 0 : buf->b_fnum;
    bufref->br_buf_free_count = buf_free_count;
}

/*
 * Return TRUE if "bufref->br_buf" points to the same buffer as when
 * set_bufref() was called and it is a valid buffer.
 * Only goes through the buffer list if buf_free_count changed.
 * Also checks if b_fnum is still the same, a :bwipe followed by :new might get
 * the same allocated memory, but it's a different buffer.
 */
    int
bufref_valid(bufref_T *bufref)
{
    return bufref->br_buf_free_count == buf_free_count
	? TRUE : buf_valid(bufref->br_buf)
				  && bufref->br_fnum == bufref->br_buf->b_fnum;
}

/*
 * Return TRUE if "buf" points to a valid buffer (in the buffer list).
 * This can be slow if there are many buffers, prefer using bufref_valid().
 */
    int
buf_valid(buf_T *buf)
{
    buf_T	*bp;

    /* Assume that we more often have a recent buffer, start with the last
     * one. */
    for (bp = lastbuf; bp != NULL; bp = bp->b_prev)
	if (bp == buf)
	    return TRUE;
    return FALSE;
}

/*
 * A hash table used to quickly lookup a buffer by its number.
 */
static hashtab_T buf_hashtab;

    static void
buf_hashtab_add(buf_T *buf)
{
    sprintf((char *)buf->b_key, "%x", buf->b_fnum);
    if (hash_add(&buf_hashtab, buf->b_key) == FAIL)
	EMSG(_("E931: Buffer cannot be registered"));
}

    static void
buf_hashtab_remove(buf_T *buf)
{
    hashitem_T *hi = hash_find(&buf_hashtab, buf->b_key);

    if (!HASHITEM_EMPTY(hi))
	hash_remove(&buf_hashtab, hi);
}

/*
 * Return TRUE when buffer "buf" can be unloaded.
 * Give an error message and return FALSE when the buffer is locked or the
 * screen is being redrawn and the buffer is in a window.
 */
    static int
can_unload_buffer(buf_T *buf)
{
    int	    can_unload = !buf->b_locked;

    if (can_unload && updating_screen)
    {
	win_T	*wp;

	FOR_ALL_WINDOWS(wp)
	    if (wp->w_buffer == buf)
	    {
		can_unload = FALSE;
		break;
	    }
    }
    if (!can_unload)
	EMSG(_("E937: Attempt to delete a buffer that is in use"));
    return can_unload;
}

/*
 * Close the link to a buffer.
 * "action" is used when there is no longer a window for the buffer.
 * It can be:
 * 0			buffer becomes hidden
 * DOBUF_UNLOAD		buffer is unloaded
 * DOBUF_DELETE		buffer is unloaded and removed from buffer list
 * DOBUF_WIPE		buffer is unloaded and really deleted
 * When doing all but the first one on the current buffer, the caller should
 * get a new buffer very soon!
 *
 * The 'bufhidden' option can force freeing and deleting.
 *
 * When "abort_if_last" is TRUE then do not close the buffer if autocommands
 * cause there to be only one window with this buffer.  e.g. when ":quit" is
 * supposed to close the window but autocommands close all other windows.
 */
    void
close_buffer(
    win_T	*win,		/* if not NULL, set b_last_cursor */
    buf_T	*buf,
    int		action,
    int		abort_if_last UNUSED)
{
    int		is_curbuf;
    int		nwindows;
    bufref_T	bufref;
    int		is_curwin = (curwin != NULL && curwin->w_buffer == buf);
    win_T	*the_curwin = curwin;
    tabpage_T	*the_curtab = curtab;
    int		unload_buf = (action != 0);
    int		del_buf = (action == DOBUF_DEL || action == DOBUF_WIPE);
    int		wipe_buf = (action == DOBUF_WIPE);

    /*
     * Force unloading or deleting when 'bufhidden' says so.
     * The caller must take care of NOT deleting/freeing when 'bufhidden' is
     * "hide" (otherwise we could never free or delete a buffer).
     */
    if (buf->b_p_bh[0] == 'd')		/* 'bufhidden' == "delete" */
    {
	del_buf = TRUE;
	unload_buf = TRUE;
    }
    else if (buf->b_p_bh[0] == 'w')	/* 'bufhidden' == "wipe" */
    {
	del_buf = TRUE;
	unload_buf = TRUE;
	wipe_buf = TRUE;
    }
    else if (buf->b_p_bh[0] == 'u')	/* 'bufhidden' == "unload" */
	unload_buf = TRUE;

#ifdef FEAT_TERMINAL
    if (bt_terminal(buf) && (buf->b_nwindows == 1 || del_buf))
    {
	if (term_job_running(buf->b_term))
	{
	    if (wipe_buf || unload_buf)
	    {
		if (!can_unload_buffer(buf))
		    return;

		/* Wiping out or unloading a terminal buffer kills the job. */
		free_terminal(buf);
	    }
	    else
	    {
		/* The job keeps running, hide the buffer. */
		del_buf = FALSE;
		unload_buf = FALSE;
	    }
	}
	else
	{
	    /* A terminal buffer is wiped out if the job has finished. */
	    del_buf = TRUE;
	    unload_buf = TRUE;
	    wipe_buf = TRUE;
	}
    }
#endif

    /* Disallow deleting the buffer when it is locked (already being closed or
     * halfway a command that relies on it). Unloading is allowed. */
    if ((del_buf || wipe_buf) && !can_unload_buffer(buf))
	return;

    /* check no autocommands closed the window */
    if (win != NULL && win_valid_any_tab(win))
    {
	/* Set b_last_cursor when closing the last window for the buffer.
	 * Remember the last cursor position and window options of the buffer.
	 * This used to be only for the current window, but then options like
	 * 'foldmethod' may be lost with a ":only" command. */
	if (buf->b_nwindows == 1)
	    set_last_cursor(win);
	buflist_setfpos(buf, win,
		    win->w_cursor.lnum == 1 ? 0 : win->w_cursor.lnum,
		    win->w_cursor.col, TRUE);
    }

    set_bufref(&bufref, buf);

    /* When the buffer is no longer in a window, trigger BufWinLeave */
    if (buf->b_nwindows == 1)
    {
	++buf->b_locked;
	if (apply_autocmds(EVENT_BUFWINLEAVE, buf->b_fname, buf->b_fname,
								  FALSE, buf)
		&& !bufref_valid(&bufref))
	{
	    /* Autocommands deleted the buffer. */
aucmd_abort:
	    EMSG(_(e_auabort));
	    return;
	}
	--buf->b_locked;
	if (abort_if_last && one_window())
	    /* Autocommands made this the only window. */
	    goto aucmd_abort;

	/* When the buffer becomes hidden, but is not unloaded, trigger
	 * BufHidden */
	if (!unload_buf)
	{
	    ++buf->b_locked;
	    if (apply_autocmds(EVENT_BUFHIDDEN, buf->b_fname, buf->b_fname,
								  FALSE, buf)
		    && !bufref_valid(&bufref))
		/* Autocommands deleted the buffer. */
		goto aucmd_abort;
	    --buf->b_locked;
	    if (abort_if_last && one_window())
		/* Autocommands made this the only window. */
		goto aucmd_abort;
	}
#ifdef FEAT_EVAL
	if (aborting())	    /* autocmds may abort script processing */
	    return;
#endif
    }

    /* If the buffer was in curwin and the window has changed, go back to that
     * window, if it still exists.  This avoids that ":edit x" triggering a
     * "tabnext" BufUnload autocmd leaves a window behind without a buffer. */
    if (is_curwin && curwin != the_curwin &&  win_valid_any_tab(the_curwin))
    {
	block_autocmds();
	goto_tabpage_win(the_curtab, the_curwin);
	unblock_autocmds();
    }

    nwindows = buf->b_nwindows;

    /* decrease the link count from windows (unless not in any window) */
    if (buf->b_nwindows > 0)
	--buf->b_nwindows;

#ifdef FEAT_DIFF
    if (diffopt_hiddenoff() && !unload_buf && buf->b_nwindows == 0)
	diff_buf_delete(buf);	/* Clear 'diff' for hidden buffer. */
#endif

    /* Return when a window is displaying the buffer or when it's not
     * unloaded. */
    if (buf->b_nwindows > 0 || !unload_buf)
	return;

    /* Always remove the buffer when there is no file name. */
    if (buf->b_ffname == NULL)
	del_buf = TRUE;

    /* When closing the current buffer stop Visual mode before freeing
     * anything. */
    if (buf == curbuf && VIsual_active
#if defined(EXITFREE)
	    && !entered_free_all_mem
#endif
	    )
	end_visual_mode();

    /*
     * Free all things allocated for this buffer.
     * Also calls the "BufDelete" autocommands when del_buf is TRUE.
     */
    /* Remember if we are closing the current buffer.  Restore the number of
     * windows, so that autocommands in buf_freeall() don't get confused. */
    is_curbuf = (buf == curbuf);
    buf->b_nwindows = nwindows;

    buf_freeall(buf, (del_buf ? BFA_DEL : 0) + (wipe_buf ? BFA_WIPE : 0));

    /* Autocommands may have deleted the buffer. */
    if (!bufref_valid(&bufref))
	return;
#ifdef FEAT_EVAL
    if (aborting())	    /* autocmds may abort script processing */
	return;
#endif

    /*
     * It's possible that autocommands change curbuf to the one being deleted.
     * This might cause the previous curbuf to be deleted unexpectedly.  But
     * in some cases it's OK to delete the curbuf, because a new one is
     * obtained anyway.  Therefore only return if curbuf changed to the
     * deleted buffer.
     */
    if (buf == curbuf && !is_curbuf)
	return;

    if (win_valid_any_tab(win) && win->w_buffer == buf)
	win->w_buffer = NULL;  /* make sure we don't use the buffer now */

    /* Autocommands may have opened or closed windows for this buffer.
     * Decrement the count for the close we do here. */
    if (buf->b_nwindows > 0)
	--buf->b_nwindows;

    /*
     * Remove the buffer from the list.
     */
    if (wipe_buf)
    {
#ifdef FEAT_SUN_WORKSHOP
	if (usingSunWorkShop)
	    workshop_file_closed_lineno((char *)buf->b_ffname,
			(int)buf->b_last_cursor.lnum);
#endif
	if (buf->b_sfname != buf->b_ffname)
	    VIM_CLEAR(buf->b_sfname);
	else
	    buf->b_sfname = NULL;
	VIM_CLEAR(buf->b_ffname);
	if (buf->b_prev == NULL)
	    firstbuf = buf->b_next;
	else
	    buf->b_prev->b_next = buf->b_next;
	if (buf->b_next == NULL)
	    lastbuf = buf->b_prev;
	else
	    buf->b_next->b_prev = buf->b_prev;
	free_buffer(buf);
    }
    else
    {
	if (del_buf)
	{
	    /* Free all internal variables and reset option values, to make
	     * ":bdel" compatible with Vim 5.7. */
	    free_buffer_stuff(buf, TRUE);

	    /* Make it look like a new buffer. */
	    buf->b_flags = BF_CHECK_RO | BF_NEVERLOADED;

	    /* Init the options when loaded again. */
	    buf->b_p_initialized = FALSE;
	}
	buf_clear_file(buf);
	if (del_buf)
	    buf->b_p_bl = FALSE;
    }
}

/*
 * Make buffer not contain a file.
 */
    void
buf_clear_file(buf_T *buf)
{
    buf->b_ml.ml_line_count = 1;
    unchanged(buf, TRUE);
    buf->b_shortname = FALSE;
    buf->b_p_eol = TRUE;
    buf->b_start_eol = TRUE;
#ifdef FEAT_MBYTE
    buf->b_p_bomb = FALSE;
    buf->b_start_bomb = FALSE;
#endif
    buf->b_ml.ml_mfp = NULL;
    buf->b_ml.ml_flags = ML_EMPTY;		/* empty buffer */
#ifdef FEAT_NETBEANS_INTG
    netbeans_deleted_all_lines(buf);
#endif
}

/*
 * buf_freeall() - free all things allocated for a buffer that are related to
 * the file.  Careful: get here with "curwin" NULL when exiting.
 * flags:
 * BFA_DEL	  buffer is going to be deleted
 * BFA_WIPE	  buffer is going to be wiped out
 * BFA_KEEP_UNDO  do not free undo information
 */
    void
buf_freeall(buf_T *buf, int flags)
{
    int		is_curbuf = (buf == curbuf);
    bufref_T	bufref;
    int		is_curwin = (curwin != NULL && curwin->w_buffer == buf);
    win_T	*the_curwin = curwin;
    tabpage_T	*the_curtab = curtab;

    /* Make sure the buffer isn't closed by autocommands. */
    ++buf->b_locked;
    set_bufref(&bufref, buf);
    if (buf->b_ml.ml_mfp != NULL)
    {
	if (apply_autocmds(EVENT_BUFUNLOAD, buf->b_fname, buf->b_fname,
								  FALSE, buf)
		&& !bufref_valid(&bufref))
	    /* autocommands deleted the buffer */
	    return;
    }
    if ((flags & BFA_DEL) && buf->b_p_bl)
    {
	if (apply_autocmds(EVENT_BUFDELETE, buf->b_fname, buf->b_fname,
								   FALSE, buf)
		&& !bufref_valid(&bufref))
	    /* autocommands deleted the buffer */
	    return;
    }
    if (flags & BFA_WIPE)
    {
	if (apply_autocmds(EVENT_BUFWIPEOUT, buf->b_fname, buf->b_fname,
								  FALSE, buf)
		&& !bufref_valid(&bufref))
	    /* autocommands deleted the buffer */
	    return;
    }
    --buf->b_locked;

    /* If the buffer was in curwin and the window has changed, go back to that
     * window, if it still exists.  This avoids that ":edit x" triggering a
     * "tabnext" BufUnload autocmd leaves a window behind without a buffer. */
    if (is_curwin && curwin != the_curwin &&  win_valid_any_tab(the_curwin))
    {
	block_autocmds();
	goto_tabpage_win(the_curtab, the_curwin);
	unblock_autocmds();
    }

#ifdef FEAT_EVAL
    if (aborting())	    /* autocmds may abort script processing */
	return;
#endif

    /*
     * It's possible that autocommands change curbuf to the one being deleted.
     * This might cause curbuf to be deleted unexpectedly.  But in some cases
     * it's OK to delete the curbuf, because a new one is obtained anyway.
     * Therefore only return if curbuf changed to the deleted buffer.
     */
    if (buf == curbuf && !is_curbuf)
	return;
#ifdef FEAT_DIFF
    diff_buf_delete(buf);	    /* Can't use 'diff' for unloaded buffer. */
#endif
#ifdef FEAT_SYN_HL
    /* Remove any ownsyntax, unless exiting. */
    if (curwin != NULL && curwin->w_buffer == buf)
	reset_synblock(curwin);
#endif

#ifdef FEAT_FOLDING
    /* No folds in an empty buffer. */
    {
	win_T		*win;
	tabpage_T	*tp;

	FOR_ALL_TAB_WINDOWS(tp, win)
	    if (win->w_buffer == buf)
		clearFolding(win);
    }
#endif

#ifdef FEAT_TCL
    tcl_buffer_free(buf);
#endif
    ml_close(buf, TRUE);	    /* close and delete the memline/memfile */
    buf->b_ml.ml_line_count = 0;    /* no lines in buffer */
    if ((flags & BFA_KEEP_UNDO) == 0)
    {
	u_blockfree(buf);	    /* free the memory allocated for undo */
	u_clearall(buf);	    /* reset all undo information */
    }
#ifdef FEAT_SYN_HL
    syntax_clear(&buf->b_s);	    /* reset syntax info */
#endif
#ifdef FEAT_TEXT_PROP
    clear_buf_prop_types(buf);
#endif
    buf->b_flags &= ~BF_READERR;    /* a read error is no longer relevant */
}

/*
 * Free a buffer structure and the things it contains related to the buffer
 * itself (not the file, that must have been done already).
 */
    static void
free_buffer(buf_T *buf)
{
    ++buf_free_count;
    free_buffer_stuff(buf, TRUE);
#ifdef FEAT_EVAL
    /* b:changedtick uses an item in buf_T, remove it now */
    dictitem_remove(buf->b_vars, (dictitem_T *)&buf->b_ct_di);
    unref_var_dict(buf->b_vars);
#endif
#ifdef FEAT_LUA
    lua_buffer_free(buf);
#endif
#ifdef FEAT_MZSCHEME
    mzscheme_buffer_free(buf);
#endif
#ifdef FEAT_PERL
    perl_buf_free(buf);
#endif
#ifdef FEAT_PYTHON
    python_buffer_free(buf);
#endif
#ifdef FEAT_PYTHON3
    python3_buffer_free(buf);
#endif
#ifdef FEAT_RUBY
    ruby_buffer_free(buf);
#endif
#ifdef FEAT_JOB_CHANNEL
    channel_buffer_free(buf);
#endif
#ifdef FEAT_TERMINAL
    free_terminal(buf);
#endif
#ifdef FEAT_JOB_CHANNEL
    vim_free(buf->b_prompt_text);
    free_callback(buf->b_prompt_callback, buf->b_prompt_partial);
#endif

    buf_hashtab_remove(buf);

    aubuflocal_remove(buf);

    if (autocmd_busy)
    {
	/* Do not free the buffer structure while autocommands are executing,
	 * it's still needed. Free it when autocmd_busy is reset. */
	buf->b_next = au_pending_free_buf;
	au_pending_free_buf = buf;
    }
    else
	vim_free(buf);
}

/*
 * Initializes b:changedtick.
 */
    static void
init_changedtick(buf_T *buf)
{
    dictitem_T *di = (dictitem_T *)&buf->b_ct_di;

    di->di_flags = DI_FLAGS_FIX | DI_FLAGS_RO;
    di->di_tv.v_type = VAR_NUMBER;
    di->di_tv.v_lock = VAR_FIXED;
    di->di_tv.vval.v_number = 0;

#ifdef FEAT_EVAL
    STRCPY(buf->b_ct_di.di_key, "changedtick");
    (void)dict_add(buf->b_vars, di);
#endif
}

/*
 * Free stuff in the buffer for ":bdel" and when wiping out the buffer.
 */
    static void
free_buffer_stuff(
    buf_T	*buf,
    int		free_options)		/* free options as well */
{
    if (free_options)
    {
	clear_wininfo(buf);		/* including window-local options */
	free_buf_options(buf, TRUE);
#ifdef FEAT_SPELL
	ga_clear(&buf->b_s.b_langp);
#endif
    }
#ifdef FEAT_EVAL
    {
	varnumber_T tick = CHANGEDTICK(buf);

	vars_clear(&buf->b_vars->dv_hashtab); /* free all buffer variables */
	hash_init(&buf->b_vars->dv_hashtab);
	init_changedtick(buf);
	CHANGEDTICK(buf) = tick;
    }
#endif
#ifdef FEAT_USR_CMDS
    uc_clear(&buf->b_ucmds);		/* clear local user commands */
#endif
#ifdef FEAT_SIGNS
    buf_delete_signs(buf, (char_u *)"*");	// delete any signs */
#endif
#ifdef FEAT_NETBEANS_INTG
    netbeans_file_killed(buf);
#endif
#ifdef FEAT_LOCALMAP
    map_clear_int(buf, MAP_ALL_MODES, TRUE, FALSE);  /* clear local mappings */
    map_clear_int(buf, MAP_ALL_MODES, TRUE, TRUE);   /* clear local abbrevs */
#endif
#ifdef FEAT_MBYTE
    VIM_CLEAR(buf->b_start_fenc);
#endif
}

/*
 * Free the b_wininfo list for buffer "buf".
 */
    static void
clear_wininfo(buf_T *buf)
{
    wininfo_T	*wip;

    while (buf->b_wininfo != NULL)
    {
	wip = buf->b_wininfo;
	buf->b_wininfo = wip->wi_next;
	if (wip->wi_optset)
	{
	    clear_winopt(&wip->wi_opt);
#ifdef FEAT_FOLDING
	    deleteFoldRecurse(&wip->wi_folds);
#endif
	}
	vim_free(wip);
    }
}

/*
 * Go to another buffer.  Handles the result of the ATTENTION dialog.
 */
    void
goto_buffer(
    exarg_T	*eap,
    int		start,
    int		dir,
    int		count)
{
#if defined(HAS_SWAP_EXISTS_ACTION)
    bufref_T	old_curbuf;

    set_bufref(&old_curbuf, curbuf);

    swap_exists_action = SEA_DIALOG;
#endif
    (void)do_buffer(*eap->cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
					     start, dir, count, eap->forceit);
#if defined(HAS_SWAP_EXISTS_ACTION)
    if (swap_exists_action == SEA_QUIT && *eap->cmd == 's')
    {
# if defined(FEAT_EVAL)
	cleanup_T   cs;

	/* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a window. */
	enter_cleanup(&cs);
# endif

	/* Quitting means closing the split window, nothing else. */
	win_close(curwin, TRUE);
	swap_exists_action = SEA_NONE;
	swap_exists_did_quit = TRUE;

# if defined(FEAT_EVAL)
	/* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
	leave_cleanup(&cs);
# endif
    }
    else
	handle_swap_exists(&old_curbuf);
#endif
}

#if defined(HAS_SWAP_EXISTS_ACTION) || defined(PROTO)
/*
 * Handle the situation of swap_exists_action being set.
 * It is allowed for "old_curbuf" to be NULL or invalid.
 */
    void
handle_swap_exists(bufref_T *old_curbuf)
{
# if defined(FEAT_EVAL)
    cleanup_T	cs;
# endif
# ifdef FEAT_SYN_HL
    long	old_tw = curbuf->b_p_tw;
# endif
    buf_T	*buf;

    if (swap_exists_action == SEA_QUIT)
    {
# if defined(FEAT_EVAL)
	/* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a buffer. */
	enter_cleanup(&cs);
# endif

	/* User selected Quit at ATTENTION prompt.  Go back to previous
	 * buffer.  If that buffer is gone or the same as the current one,
	 * open a new, empty buffer. */
	swap_exists_action = SEA_NONE;	/* don't want it again */
	swap_exists_did_quit = TRUE;
	close_buffer(curwin, curbuf, DOBUF_UNLOAD, FALSE);
	if (old_curbuf == NULL || !bufref_valid(old_curbuf)
					      || old_curbuf->br_buf == curbuf)
	    buf = buflist_new(NULL, NULL, 1L, BLN_CURBUF | BLN_LISTED);
	else
	    buf = old_curbuf->br_buf;
	if (buf != NULL)
	{
	    int old_msg_silent = msg_silent;

	    if (shortmess(SHM_FILEINFO))
		msg_silent = 1;  // prevent fileinfo message
	    enter_buffer(buf);
	    // restore msg_silent, so that the command line will be shown
	    msg_silent = old_msg_silent;

# ifdef FEAT_SYN_HL
	    if (old_tw != curbuf->b_p_tw)
		check_colorcolumn(curwin);
# endif
	}
	/* If "old_curbuf" is NULL we are in big trouble here... */

# if defined(FEAT_EVAL)
	/* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
	leave_cleanup(&cs);
# endif
    }
    else if (swap_exists_action == SEA_RECOVER)
    {
# if defined(FEAT_EVAL)
	/* Reset the error/interrupt/exception state here so that
	 * aborting() returns FALSE when closing a buffer. */
	enter_cleanup(&cs);
# endif

	/* User selected Recover at ATTENTION prompt. */
	msg_scroll = TRUE;
	ml_recover();
	MSG_PUTS("\n");	/* don't overwrite the last message */
	cmdline_row = msg_row;
	do_modelines(0);

# if defined(FEAT_EVAL)
	/* Restore the error/interrupt/exception state if not discarded by a
	 * new aborting error, interrupt, or uncaught exception. */
	leave_cleanup(&cs);
# endif
    }
    swap_exists_action = SEA_NONE;
}
#endif

/*
 * do_bufdel() - delete or unload buffer(s)
 *
 * addr_count == 0: ":bdel" - delete current buffer
 * addr_count == 1: ":N bdel" or ":bdel N [N ..]" - first delete
 *		    buffer "end_bnr", then any other arguments.
 * addr_count == 2: ":N,N bdel" - delete buffers in range
 *
 * command can be DOBUF_UNLOAD (":bunload"), DOBUF_WIPE (":bwipeout") or
 * DOBUF_DEL (":bdel")
 *
 * Returns error message or NULL
 */
    char_u *
do_bufdel(
    int		command,
    char_u	*arg,		/* pointer to extra arguments */
    int		addr_count,
    int		start_bnr,	/* first buffer number in a range */
    int		end_bnr,	/* buffer nr or last buffer nr in a range */
    int		forceit)
{
    int		do_current = 0;	/* delete current buffer? */
    int		deleted = 0;	/* number of buffers deleted */
    char_u	*errormsg = NULL; /* return value */
    int		bnr;		/* buffer number */
    char_u	*p;

    if (addr_count == 0)
    {
	(void)do_buffer(command, DOBUF_CURRENT, FORWARD, 0, forceit);
    }
    else
    {
	if (addr_count == 2)
	{
	    if (*arg)		/* both range and argument is not allowed */
		return (char_u *)_(e_trailing);
	    bnr = start_bnr;
	}
	else	/* addr_count == 1 */
	    bnr = end_bnr;

	for ( ;!got_int; ui_breakcheck())
	{
	    /*
	     * delete the current buffer last, otherwise when the
	     * current buffer is deleted, the next buffer becomes
	     * the current one and will be loaded, which may then
	     * also be deleted, etc.
	     */
	    if (bnr == curbuf->b_fnum)
		do_current = bnr;
	    else if (do_buffer(command, DOBUF_FIRST, FORWARD, (int)bnr,
							       forceit) == OK)
		++deleted;

	    /*
	     * find next buffer number to delete/unload
	     */
	    if (addr_count == 2)
	    {
		if (++bnr > end_bnr)
		    break;
	    }
	    else    /* addr_count == 1 */
	    {
		arg = skipwhite(arg);
		if (*arg == NUL)
		    break;
		if (!VIM_ISDIGIT(*arg))
		{
		    p = skiptowhite_esc(arg);
		    bnr = buflist_findpat(arg, p, command == DOBUF_WIPE,
								FALSE, FALSE);
		    if (bnr < 0)	    /* failed */
			break;
		    arg = p;
		}
		else
		    bnr = getdigits(&arg);
	    }
	}
	if (!got_int && do_current && do_buffer(command, DOBUF_FIRST,
					  FORWARD, do_current, forceit) == OK)
	    ++deleted;

	if (deleted == 0)
	{
	    if (command == DOBUF_UNLOAD)
		STRCPY(IObuff, _("E515: No buffers were unloaded"));
	    else if (command == DOBUF_DEL)
		STRCPY(IObuff, _("E516: No buffers were deleted"));
	    else
		STRCPY(IObuff, _("E517: No buffers were wiped out"));
	    errormsg = IObuff;
	}
	else if (deleted >= p_report)
	{
	    if (command == DOBUF_UNLOAD)
		smsg((char_u *)NGETTEXT("%d buffer unloaded",
			    "%d buffers unloaded", deleted), deleted);
	    else if (command == DOBUF_DEL)
		smsg((char_u *)NGETTEXT("%d buffer deleted",
			    "%d buffers deleted", deleted), deleted);
	    else
		smsg((char_u *)NGETTEXT("%d buffer wiped out",
			    "%d buffers wiped out", deleted), deleted);
	}
    }


    return errormsg;
}

/*
 * Make the current buffer empty.
 * Used when it is wiped out and it's the last buffer.
 */
    static int
empty_curbuf(
    int close_others,
    int forceit,
    int action)
{
    int	    retval;
    buf_T   *buf = curbuf;
    bufref_T bufref;

    if (action == DOBUF_UNLOAD)
    {
	EMSG(_("E90: Cannot unload last buffer"));
	return FAIL;
    }

    set_bufref(&bufref, buf);
    if (close_others)
	/* Close any other windows on this buffer, then make it empty. */
	close_windows(buf, TRUE);

    setpcmark();
    retval = do_ecmd(0, NULL, NULL, NULL, ECMD_ONE,
					  forceit ? ECMD_FORCEIT : 0, curwin);

    /*
     * do_ecmd() may create a new buffer, then we have to delete
     * the old one.  But do_ecmd() may have done that already, check
     * if the buffer still exists.
     */
    if (buf != curbuf && bufref_valid(&bufref) && buf->b_nwindows == 0)
	close_buffer(NULL, buf, action, FALSE);
    if (!close_others)
	need_fileinfo = FALSE;
    return retval;
}

/*
 * Implementation of the commands for the buffer list.
 *
 * action == DOBUF_GOTO	    go to specified buffer
 * action == DOBUF_SPLIT    split window and go to specified buffer
 * action == DOBUF_UNLOAD   unload specified buffer(s)
 * action == DOBUF_DEL	    delete specified buffer(s) from buffer list
 * action == DOBUF_WIPE	    delete specified buffer(s) really
 *
 * start == DOBUF_CURRENT   go to "count" buffer from current buffer
 * start == DOBUF_FIRST	    go to "count" buffer from first buffer
 * start == DOBUF_LAST	    go to "count" buffer from last buffer
 * start == DOBUF_MOD	    go to "count" modified buffer from current buffer
 *
 * Return FAIL or OK.
 */
    int
do_buffer(
    int		action,
    int		start,
    int		dir,		/* FORWARD or BACKWARD */
    int		count,		/* buffer number or number of buffers */
    int		forceit)	/* TRUE for :...! */
{
    buf_T	*buf;
    buf_T	*bp;
    int		unload = (action == DOBUF_UNLOAD || action == DOBUF_DEL
						     || action == DOBUF_WIPE);

    switch (start)
    {
	case DOBUF_FIRST:   buf = firstbuf; break;
	case DOBUF_LAST:    buf = lastbuf;  break;
	default:	    buf = curbuf;   break;
    }
    if (start == DOBUF_MOD)	    /* find next modified buffer */
    {
	while (count-- > 0)
	{
	    do
	    {
		buf = buf->b_next;
		if (buf == NULL)
		    buf = firstbuf;
	    }
	    while (buf != curbuf && !bufIsChanged(buf));
	}
	if (!bufIsChanged(buf))
	{
	    EMSG(_("E84: No modified buffer found"));
	    return FAIL;
	}
    }
    else if (start == DOBUF_FIRST && count) /* find specified buffer number */
    {
	while (buf != NULL && buf->b_fnum != count)
	    buf = buf->b_next;
    }
    else
    {
	bp = NULL;
	while (count > 0 || (!unload && !buf->b_p_bl && bp != buf))
	{
	    /* remember the buffer where we start, we come back there when all
	     * buffers are unlisted. */
	    if (bp == NULL)
		bp = buf;
	    if (dir == FORWARD)
	    {
		buf = buf->b_next;
		if (buf == NULL)
		    buf = firstbuf;
	    }
	    else
	    {
		buf = buf->b_prev;
		if (buf == NULL)
		    buf = lastbuf;
	    }
	    /* don't count unlisted buffers */
	    if (unload || buf->b_p_bl)
	    {
		 --count;
		 bp = NULL;	/* use this buffer as new starting point */
	    }
	    if (bp == buf)
	    {
		/* back where we started, didn't find anything. */
		EMSG(_("E85: There is no listed buffer"));
		return FAIL;
	    }
	}
    }

    if (buf == NULL)	    /* could not find it */
    {
	if (start == DOBUF_FIRST)
	{
	    /* don't warn when deleting */
	    if (!unload)
		EMSGN(_(e_nobufnr), count);
	}
	else if (dir == FORWARD)
	    EMSG(_("E87: Cannot go beyond last buffer"));
	else
	    EMSG(_("E88: Cannot go before first buffer"));
	return FAIL;
    }

#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif

    /*
     * delete buffer buf from memory and/or the list
     */
    if (unload)
    {
	int	forward;
	bufref_T bufref;

	if (!can_unload_buffer(buf))
	    return FAIL;

	set_bufref(&bufref, buf);

	/* When unloading or deleting a buffer that's already unloaded and
	 * unlisted: fail silently. */
	if (action != DOBUF_WIPE && buf->b_ml.ml_mfp == NULL && !buf->b_p_bl)
	    return FAIL;

	if (!forceit && bufIsChanged(buf))
	{
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
	    if ((p_confirm || cmdmod.confirm) && p_write)
	    {
		dialog_changed(buf, FALSE);
		if (!bufref_valid(&bufref))
		    /* Autocommand deleted buffer, oops!  It's not changed
		     * now. */
		    return FAIL;
		/* If it's still changed fail silently, the dialog already
		 * mentioned why it fails. */
		if (bufIsChanged(buf))
		    return FAIL;
	    }
	    else
#endif
	    {
		EMSGN(_("E89: No write since last change for buffer %ld (add ! to override)"),
								 buf->b_fnum);
		return FAIL;
	    }
	}

	/* When closing the current buffer stop Visual mode. */
	if (buf == curbuf && VIsual_active)
	    end_visual_mode();

	/*
	 * If deleting the last (listed) buffer, make it empty.
	 * The last (listed) buffer cannot be unloaded.
	 */
	FOR_ALL_BUFFERS(bp)
	    if (bp->b_p_bl && bp != buf)
		break;
	if (bp == NULL && buf == curbuf)
	    return empty_curbuf(TRUE, forceit, action);

	/*
	 * If the deleted buffer is the current one, close the current window
	 * (unless it's the only window).  Repeat this so long as we end up in
	 * a window with this buffer.
	 */
	while (buf == curbuf
		   && !(curwin->w_closing || curwin->w_buffer->b_locked > 0)
		   && (!ONE_WINDOW || first_tabpage->tp_next != NULL))
	{
	    if (win_close(curwin, FALSE) == FAIL)
		break;
	}

	/*
	 * If the buffer to be deleted is not the current one, delete it here.
	 */
	if (buf != curbuf)
	{
	    close_windows(buf, FALSE);
	    if (buf != curbuf && bufref_valid(&bufref) && buf->b_nwindows <= 0)
		    close_buffer(NULL, buf, action, FALSE);
	    return OK;
	}

	/*
	 * Deleting the current buffer: Need to find another buffer to go to.
	 * There should be another, otherwise it would have been handled
	 * above.  However, autocommands may have deleted all buffers.
	 * First use au_new_curbuf.br_buf, if it is valid.
	 * Then prefer the buffer we most recently visited.
	 * Else try to find one that is loaded, after the current buffer,
	 * then before the current buffer.
	 * Finally use any buffer.
	 */
	buf = NULL;	/* selected buffer */
	bp = NULL;	/* used when no loaded buffer found */
	if (au_new_curbuf.br_buf != NULL && bufref_valid(&au_new_curbuf))
	    buf = au_new_curbuf.br_buf;
#ifdef FEAT_JUMPLIST
	else if (curwin->w_jumplistlen > 0)
	{
	    int     jumpidx;

	    jumpidx = curwin->w_jumplistidx - 1;
	    if (jumpidx < 0)
		jumpidx = curwin->w_jumplistlen - 1;

	    forward = jumpidx;
	    while (jumpidx != curwin->w_jumplistidx)
	    {
		buf = buflist_findnr(curwin->w_jumplist[jumpidx].fmark.fnum);
		if (buf != NULL)
		{
		    if (buf == curbuf || !buf->b_p_bl)
			buf = NULL;	/* skip current and unlisted bufs */
		    else if (buf->b_ml.ml_mfp == NULL)
		    {
			/* skip unloaded buf, but may keep it for later */
			if (bp == NULL)
			    bp = buf;
			buf = NULL;
		    }
		}
		if (buf != NULL)   /* found a valid buffer: stop searching */
		    break;
		/* advance to older entry in jump list */
		if (!jumpidx && curwin->w_jumplistidx == curwin->w_jumplistlen)
		    break;
		if (--jumpidx < 0)
		    jumpidx = curwin->w_jumplistlen - 1;
		if (jumpidx == forward)		/* List exhausted for sure */
		    break;
	    }
	}
#endif

	if (buf == NULL)	/* No previous buffer, Try 2'nd approach */
	{
	    forward = TRUE;
	    buf = curbuf->b_next;
	    for (;;)
	    {
		if (buf == NULL)
		{
		    if (!forward)	/* tried both directions */
			break;
		    buf = curbuf->b_prev;
		    forward = FALSE;
		    continue;
		}
		/* in non-help buffer, try to skip help buffers, and vv */
		if (buf->b_help == curbuf->b_help && buf->b_p_bl)
		{
		    if (buf->b_ml.ml_mfp != NULL)   /* found loaded buffer */
			break;
		    if (bp == NULL)	/* remember unloaded buf for later */
			bp = buf;
		}
		if (forward)
		    buf = buf->b_next;
		else
		    buf = buf->b_prev;
	    }
	}
	if (buf == NULL)	/* No loaded buffer, use unloaded one */
	    buf = bp;
	if (buf == NULL)	/* No loaded buffer, find listed one */
	{
	    FOR_ALL_BUFFERS(buf)
		if (buf->b_p_bl && buf != curbuf)
		    break;
	}
	if (buf == NULL)	/* Still no buffer, just take one */
	{
	    if (curbuf->b_next != NULL)
		buf = curbuf->b_next;
	    else
		buf = curbuf->b_prev;
	}
    }

    if (buf == NULL)
    {
	/* Autocommands must have wiped out all other buffers.  Only option
	 * now is to make the current buffer empty. */
	return empty_curbuf(FALSE, forceit, action);
    }

    /*
     * make buf current buffer
     */
    if (action == DOBUF_SPLIT)	    /* split window first */
    {
	/* If 'switchbuf' contains "useopen": jump to first window containing
	 * "buf" if one exists */
	if ((swb_flags & SWB_USEOPEN) && buf_jump_open_win(buf))
	    return OK;
	/* If 'switchbuf' contains "usetab": jump to first window in any tab
	 * page containing "buf" if one exists */
	if ((swb_flags & SWB_USETAB) && buf_jump_open_tab(buf))
	    return OK;
	if (win_split(0, 0) == FAIL)
	    return FAIL;
    }

    /* go to current buffer - nothing to do */
    if (buf == curbuf)
	return OK;

    /*
     * Check if the current buffer may be abandoned.
     */
    if (action == DOBUF_GOTO && !can_abandon(curbuf, forceit))
    {
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
	if ((p_confirm || cmdmod.confirm) && p_write)
	{
	    bufref_T bufref;

	    set_bufref(&bufref, buf);
	    dialog_changed(curbuf, FALSE);
	    if (!bufref_valid(&bufref))
		/* Autocommand deleted buffer, oops! */
		return FAIL;
	}
	if (bufIsChanged(curbuf))
#endif
	{
	    no_write_message();
	    return FAIL;
	}
    }

    /* Go to the other buffer. */
    set_curbuf(buf, action);

    if (action == DOBUF_SPLIT)
    {
	RESET_BINDING(curwin);	/* reset 'scrollbind' and 'cursorbind' */
    }

#if defined(FEAT_EVAL)
    if (aborting())	    /* autocmds may abort script processing */
	return FAIL;
#endif

    return OK;
}

/*
 * Set current buffer to "buf".  Executes autocommands and closes current
 * buffer.  "action" tells how to close the current buffer:
 * DOBUF_GOTO	    free or hide it
 * DOBUF_SPLIT	    nothing
 * DOBUF_UNLOAD	    unload it
 * DOBUF_DEL	    delete it
 * DOBUF_WIPE	    wipe it out
 */
    void
set_curbuf(buf_T *buf, int action)
{
    buf_T	*prevbuf;
    int		unload = (action == DOBUF_UNLOAD || action == DOBUF_DEL
						     || action == DOBUF_WIPE);
#ifdef FEAT_SYN_HL
    long	old_tw = curbuf->b_p_tw;
#endif
    bufref_T	newbufref;
    bufref_T	prevbufref;

    setpcmark();
    if (!cmdmod.keepalt)
	curwin->w_alt_fnum = curbuf->b_fnum; /* remember alternate file */
    buflist_altfpos(curwin);			 /* remember curpos */

    /* Don't restart Select mode after switching to another buffer. */
    VIsual_reselect = FALSE;

    /* close_windows() or apply_autocmds() may change curbuf and wipe out "buf"
     */
    prevbuf = curbuf;
    set_bufref(&prevbufref, prevbuf);
    set_bufref(&newbufref, buf);

    /* Autocommands may delete the curren buffer and/or the buffer we wan to go
     * to.  In those cases don't close the buffer. */
    if (!apply_autocmds(EVENT_BUFLEAVE, NULL, NULL, FALSE, curbuf)
	    || (bufref_valid(&prevbufref)
		&& bufref_valid(&newbufref)
#ifdef FEAT_EVAL
		&& !aborting()
#endif
	       ))
    {
#ifdef FEAT_SYN_HL
	if (prevbuf == curwin->w_buffer)
	    reset_synblock(curwin);
#endif
	if (unload)
	    close_windows(prevbuf, FALSE);
#if defined(FEAT_EVAL)
	if (bufref_valid(&prevbufref) && !aborting())
#else
	if (bufref_valid(&prevbufref))
#endif
	{
	    win_T  *previouswin = curwin;
	    if (prevbuf == curbuf)
		u_sync(FALSE);
	    close_buffer(prevbuf == curwin->w_buffer ? curwin : NULL, prevbuf,
		    unload ? action : (action == DOBUF_GOTO
			&& !buf_hide(prevbuf)
			&& !bufIsChanged(prevbuf)) ? DOBUF_UNLOAD : 0, FALSE);
	    if (curwin != previouswin && win_valid(previouswin))
	      /* autocommands changed curwin, Grr! */
	      curwin = previouswin;
	}
    }
    /* An autocommand may have deleted "buf", already entered it (e.g., when
     * it did ":bunload") or aborted the script processing.
     * If curwin->w_buffer is null, enter_buffer() will make it valid again */
    if ((buf_valid(buf) && buf != curbuf
#ifdef FEAT_EVAL
		&& !aborting()
#endif
	) || curwin->w_buffer == NULL)
    {
	enter_buffer(buf);
#ifdef FEAT_SYN_HL
	if (old_tw != curbuf->b_p_tw)
	    check_colorcolumn(curwin);
#endif
    }
}

/*
 * Enter a new current buffer.
 * Old curbuf must have been abandoned already!  This also means "curbuf" may
 * be pointing to freed memory.
 */
    void
enter_buffer(buf_T *buf)
{
    /* Copy buffer and window local option values.  Not for a help buffer. */
    buf_copy_options(buf, BCO_ENTER | BCO_NOHELP);
    if (!buf->b_help)
	get_winopts(buf);
#ifdef FEAT_FOLDING
    else
	/* Remove all folds in the window. */
	clearFolding(curwin);
    foldUpdateAll(curwin);	/* update folds (later). */
#endif

    /* Get the buffer in the current window. */
    curwin->w_buffer = buf;
    curbuf = buf;
    ++curbuf->b_nwindows;

#ifdef FEAT_DIFF
    if (curwin->w_p_diff)
	diff_buf_add(curbuf);
#endif

#ifdef FEAT_SYN_HL
    curwin->w_s = &(curbuf->b_s);
#endif

    /* Cursor on first line by default. */
    curwin->w_cursor.lnum = 1;
    curwin->w_cursor.col = 0;
#ifdef FEAT_VIRTUALEDIT
    curwin->w_cursor.coladd = 0;
#endif
    curwin->w_set_curswant = TRUE;
    curwin->w_topline_was_set = FALSE;

    /* mark cursor position as being invalid */
    curwin->w_valid = 0;

    buflist_setfpos(curbuf, curwin, curbuf->b_last_cursor.lnum,
					      curbuf->b_last_cursor.col, TRUE);

    /* Make sure the buffer is loaded. */
    if (curbuf->b_ml.ml_mfp == NULL)	/* need to load the file */
    {
	/* If there is no filetype, allow for detecting one.  Esp. useful for
	 * ":ball" used in a autocommand.  If there already is a filetype we
	 * might prefer to keep it. */
	if (*curbuf->b_p_ft == NUL)
	    did_filetype = FALSE;

	open_buffer(FALSE, NULL, 0);
    }
    else
    {
	if (!msg_silent)
	    need_fileinfo = TRUE;	/* display file info after redraw */
	(void)buf_check_timestamp(curbuf, FALSE); /* check if file changed */
	curwin->w_topline = 1;
#ifdef FEAT_DIFF
	curwin->w_topfill = 0;
#endif
	apply_autocmds(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf);
	apply_autocmds(EVENT_BUFWINENTER, NULL, NULL, FALSE, curbuf);
    }

    /* If autocommands did not change the cursor position, restore cursor lnum
     * and possibly cursor col. */
    if (curwin->w_cursor.lnum == 1 && inindent(0))
	buflist_getfpos();

    check_arg_idx(curwin);		/* check for valid arg_idx */
#ifdef FEAT_TITLE
    maketitle();
#endif
	/* when autocmds didn't change it */
    if (curwin->w_topline == 1 && !curwin->w_topline_was_set)
	scroll_cursor_halfway(FALSE);	/* redisplay at correct position */

#ifdef FEAT_NETBEANS_INTG
    /* Send fileOpened event because we've changed buffers. */
    netbeans_file_activated(curbuf);
#endif

    /* Change directories when the 'acd' option is set. */
    DO_AUTOCHDIR;

#ifdef FEAT_KEYMAP
    if (curbuf->b_kmap_state & KEYMAP_INIT)
	(void)keymap_init();
#endif
#ifdef FEAT_SPELL
    /* May need to set the spell language.  Can only do this after the buffer
     * has been properly setup. */
    if (!curbuf->b_help && curwin->w_p_spell && *curwin->w_s->b_p_spl != NUL)
	(void)did_set_spelllang(curwin);
#endif
#ifdef FEAT_VIMINFO
    curbuf->b_last_used = vim_time();
#endif

    redraw_later(NOT_VALID);
}

#if defined(FEAT_AUTOCHDIR) || defined(PROTO)
/*
 * Change to the directory of the current buffer.
 * Don't do this while still starting up.
 */
    void
do_autochdir(void)
{
    if ((starting == 0 || test_autochdir)
	    && curbuf->b_ffname != NULL
	    && vim_chdirfile(curbuf->b_ffname, "auto") == OK)
	shorten_fnames(TRUE);
}
#endif

    void
no_write_message(void)
{
#ifdef FEAT_TERMINAL
    if (term_job_running(curbuf->b_term))
	EMSG(_("E948: Job still running (add ! to end the job)"));
    else
#endif
	EMSG(_("E37: No write since last change (add ! to override)"));
}

    void
no_write_message_nobang(buf_T *buf UNUSED)
{
#ifdef FEAT_TERMINAL
    if (term_job_running(buf->b_term))
	EMSG(_("E948: Job still running"));
    else
#endif
	EMSG(_("E37: No write since last change"));
}

/*
 * functions for dealing with the buffer list
 */

static int  top_file_num = 1;		/* highest file number */

/*
 * Return TRUE if the current buffer is empty, unnamed, unmodified and used in
 * only one window.  That means it can be re-used.
 */
    int
curbuf_reusable(void)
{
    return (curbuf != NULL
	&& curbuf->b_ffname == NULL
	&& curbuf->b_nwindows <= 1
	&& (curbuf->b_ml.ml_mfp == NULL || BUFEMPTY())
	&& !curbufIsChanged());
}

/*
 * Add a file name to the buffer list.  Return a pointer to the buffer.
 * If the same file name already exists return a pointer to that buffer.
 * If it does not exist, or if fname == NULL, a new entry is created.
 * If (flags & BLN_CURBUF) is TRUE, may use current buffer.
 * If (flags & BLN_LISTED) is TRUE, add new buffer to buffer list.
 * If (flags & BLN_DUMMY) is TRUE, don't count it as a real buffer.
 * If (flags & BLN_NEW) is TRUE, don't use an existing buffer.
 * If (flags & BLN_NOOPT) is TRUE, don't copy options from the current buffer
 *				    if the buffer already exists.
 * This is the ONLY way to create a new buffer.
 */
    buf_T *
buflist_new(
    char_u	*ffname_arg,	// full path of fname or relative
    char_u	*sfname_arg,	// short fname or NULL
    linenr_T	lnum,		// preferred cursor line
    int		flags)		// BLN_ defines
{
    char_u	*ffname = ffname_arg;
    char_u	*sfname = sfname_arg;
    buf_T	*buf;
#ifdef UNIX
    stat_T	st;
#endif

    if (top_file_num == 1)
	hash_init(&buf_hashtab);

    fname_expand(curbuf, &ffname, &sfname);	// will allocate ffname

    /*
     * If file name already exists in the list, update the entry.
     */
#ifdef UNIX
    /* On Unix we can use inode numbers when the file exists.  Works better
     * for hard links. */
    if (sfname == NULL || mch_stat((char *)sfname, &st) < 0)
	st.st_dev = (dev_T)-1;
#endif
    if (ffname != NULL && !(flags & (BLN_DUMMY | BLN_NEW)) && (buf =
#ifdef UNIX
		buflist_findname_stat(ffname, &st)
#else
		buflist_findname(ffname)
#endif
		) != NULL)
    {
	vim_free(ffname);
	if (lnum != 0)
	    buflist_setfpos(buf, curwin, lnum, (colnr_T)0, FALSE);

	if ((flags & BLN_NOOPT) == 0)
	    /* copy the options now, if 'cpo' doesn't have 's' and not done
	     * already */
	    buf_copy_options(buf, 0);

	if ((flags & BLN_LISTED) && !buf->b_p_bl)
	{
	    bufref_T bufref;

	    buf->b_p_bl = TRUE;
	    set_bufref(&bufref, buf);
	    if (!(flags & BLN_DUMMY))
	    {
		if (apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, buf)
			&& !bufref_valid(&bufref))
		    return NULL;
	    }
	}
	return buf;
    }

    /*
     * If the current buffer has no name and no contents, use the current
     * buffer.	Otherwise: Need to allocate a new buffer structure.
     *
     * This is the ONLY place where a new buffer structure is allocated!
     * (A spell file buffer is allocated in spell.c, but that's not a normal
     * buffer.)
     */
    buf = NULL;
    if ((flags & BLN_CURBUF) && curbuf_reusable())
    {
	buf = curbuf;
	/* It's like this buffer is deleted.  Watch out for autocommands that
	 * change curbuf!  If that happens, allocate a new buffer anyway. */
	if (curbuf->b_p_bl)
	    apply_autocmds(EVENT_BUFDELETE, NULL, NULL, FALSE, curbuf);
	if (buf == curbuf)
	    apply_autocmds(EVENT_BUFWIPEOUT, NULL, NULL, FALSE, curbuf);
#ifdef FEAT_EVAL
	if (aborting())		/* autocmds may abort script processing */
	    return NULL;
#endif
	if (buf == curbuf)
	{
	    /* Make sure 'bufhidden' and 'buftype' are empty */
	    clear_string_option(&buf->b_p_bh);
	    clear_string_option(&buf->b_p_bt);
	}
    }
    if (buf != curbuf || curbuf == NULL)
    {
	buf = (buf_T *)alloc_clear((unsigned)sizeof(buf_T));
	if (buf == NULL)
	{
	    vim_free(ffname);
	    return NULL;
	}
#ifdef FEAT_EVAL
	/* init b: variables */
	buf->b_vars = dict_alloc();
	if (buf->b_vars == NULL)
	{
	    vim_free(ffname);
	    vim_free(buf);
	    return NULL;
	}
	init_var_dict(buf->b_vars, &buf->b_bufvar, VAR_SCOPE);
#endif
	init_changedtick(buf);
    }

    if (ffname != NULL)
    {
	buf->b_ffname = ffname;
	buf->b_sfname = vim_strsave(sfname);
    }

    clear_wininfo(buf);
    buf->b_wininfo = (wininfo_T *)alloc_clear((unsigned)sizeof(wininfo_T));

    if ((ffname != NULL && (buf->b_ffname == NULL || buf->b_sfname == NULL))
	    || buf->b_wininfo == NULL)
    {
	if (buf->b_sfname != buf->b_ffname)
	    VIM_CLEAR(buf->b_sfname);
	else
	    buf->b_sfname = NULL;
	VIM_CLEAR(buf->b_ffname);
	if (buf != curbuf)
	    free_buffer(buf);
	return NULL;
    }

    if (buf == curbuf)
    {
	/* free all things allocated for this buffer */
	buf_freeall(buf, 0);
	if (buf != curbuf)	 /* autocommands deleted the buffer! */
	    return NULL;
#if defined(FEAT_EVAL)
	if (aborting())		/* autocmds may abort script processing */
	    return NULL;
#endif
	free_buffer_stuff(buf, FALSE);	/* delete local variables et al. */

	/* Init the options. */
	buf->b_p_initialized = FALSE;
	buf_copy_options(buf, BCO_ENTER);

#ifdef FEAT_KEYMAP
	/* need to reload lmaps and set b:keymap_name */
	curbuf->b_kmap_state |= KEYMAP_INIT;
#endif
    }
    else
    {
	/*
	 * put new buffer at the end of the buffer list
	 */
	buf->b_next = NULL;
	if (firstbuf == NULL)		/* buffer list is empty */
	{
	    buf->b_prev = NULL;
	    firstbuf = buf;
	}
	else				/* append new buffer at end of list */
	{
	    lastbuf->b_next = buf;
	    buf->b_prev = lastbuf;
	}
	lastbuf = buf;

	buf->b_fnum = top_file_num++;
	if (top_file_num < 0)		/* wrap around (may cause duplicates) */
	{
	    EMSG(_("W14: Warning: List of file names overflow"));
	    if (emsg_silent == 0)
	    {
		out_flush();
		ui_delay(3000L, TRUE);	/* make sure it is noticed */
	    }
	    top_file_num = 1;
	}
	buf_hashtab_add(buf);

	/*
	 * Always copy the options from the current buffer.
	 */
	buf_copy_options(buf, BCO_ALWAYS);
    }

    buf->b_wininfo->wi_fpos.lnum = lnum;
    buf->b_wininfo->wi_win = curwin;

#ifdef FEAT_SYN_HL
    hash_init(&buf->b_s.b_keywtab);
    hash_init(&buf->b_s.b_keywtab_ic);
#endif

    buf->b_fname = buf->b_sfname;
#ifdef UNIX
    if (st.st_dev == (dev_T)-1)
	buf->b_dev_valid = FALSE;
    else
    {
	buf->b_dev_valid = TRUE;
	buf->b_dev = st.st_dev;
	buf->b_ino = st.st_ino;
    }
#endif
    buf->b_u_synced = TRUE;
    buf->b_flags = BF_CHECK_RO | BF_NEVERLOADED;
    if (flags & BLN_DUMMY)
	buf->b_flags |= BF_DUMMY;
    buf_clear_file(buf);
    clrallmarks(buf);			/* clear marks */
    fmarks_check_names(buf);		/* check file marks for this file */
    buf->b_p_bl = (flags & BLN_LISTED) ? TRUE : FALSE;	/* init 'buflisted' */
    if (!(flags & BLN_DUMMY))
    {
	bufref_T bufref;

	/* Tricky: these autocommands may change the buffer list.  They could
	 * also split the window with re-using the one empty buffer. This may
	 * result in unexpectedly losing the empty buffer. */
	set_bufref(&bufref, buf);
	if (apply_autocmds(EVENT_BUFNEW, NULL, NULL, FALSE, buf)
		&& !bufref_valid(&bufref))
	    return NULL;
	if (flags & BLN_LISTED)
	{
	    if (apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, buf)
		    && !bufref_valid(&bufref))
		return NULL;
	}
#ifdef FEAT_EVAL
	if (aborting())		/* autocmds may abort script processing */
	    return NULL;
#endif
    }

    return buf;
}

/*
 * Free the memory for the options of a buffer.
 * If "free_p_ff" is TRUE also free 'fileformat', 'buftype' and
 * 'fileencoding'.
 */
    void
free_buf_options(
    buf_T	*buf,
    int		free_p_ff)
{
    if (free_p_ff)
    {
#ifdef FEAT_MBYTE
	clear_string_option(&buf->b_p_fenc);
#endif
	clear_string_option(&buf->b_p_ff);
	clear_string_option(&buf->b_p_bh);
	clear_string_option(&buf->b_p_bt);
    }
#ifdef FEAT_FIND_ID
    clear_string_option(&buf->b_p_def);
    clear_string_option(&buf->b_p_inc);
# ifdef FEAT_EVAL
    clear_string_option(&buf->b_p_inex);
# endif
#endif
#if defined(FEAT_CINDENT) && defined(FEAT_EVAL)
    clear_string_option(&buf->b_p_inde);
    clear_string_option(&buf->b_p_indk);
#endif
#if defined(FEAT_BEVAL) && defined(FEAT_EVAL)
    clear_string_option(&buf->b_p_bexpr);
#endif
#if defined(FEAT_CRYPT)
    clear_string_option(&buf->b_p_cm);
#endif
    clear_string_option(&buf->b_p_fp);
#if defined(FEAT_EVAL)
    clear_string_option(&buf->b_p_fex);
#endif
#ifdef FEAT_CRYPT
    clear_string_option(&buf->b_p_key);
#endif
    clear_string_option(&buf->b_p_kp);
    clear_string_option(&buf->b_p_mps);
    clear_string_option(&buf->b_p_fo);
    clear_string_option(&buf->b_p_flp);
    clear_string_option(&buf->b_p_isk);
#ifdef FEAT_VARTABS
    clear_string_option(&buf->b_p_vsts);
    if (buf->b_p_vsts_nopaste)
	vim_free(buf->b_p_vsts_nopaste);
    buf->b_p_vsts_nopaste = NULL;
    if (buf->b_p_vsts_array)
	vim_free(buf->b_p_vsts_array);
    buf->b_p_vsts_array = NULL;
    clear_string_option(&buf->b_p_vts);
    if (buf->b_p_vts_array)
	vim_free(buf->b_p_vts_array);
    buf->b_p_vts_array = NULL;
#endif
#ifdef FEAT_KEYMAP
    clear_string_option(&buf->b_p_keymap);
    keymap_clear(&buf->b_kmap_ga);
    ga_clear(&buf->b_kmap_ga);
#endif
#ifdef FEAT_COMMENTS
    clear_string_option(&buf->b_p_com);
#endif
#ifdef FEAT_FOLDING
    clear_string_option(&buf->b_p_cms);
#endif
    clear_string_option(&buf->b_p_nf);
#ifdef FEAT_SYN_HL
    clear_string_option(&buf->b_p_syn);
    clear_string_option(&buf->b_s.b_syn_isk);
#endif
#ifdef FEAT_SPELL
    clear_string_option(&buf->b_s.b_p_spc);
    clear_string_option(&buf->b_s.b_p_spf);
    vim_regfree(buf->b_s.b_cap_prog);
    buf->b_s.b_cap_prog = NULL;
    clear_string_option(&buf->b_s.b_p_spl);
#endif
#ifdef FEAT_SEARCHPATH
    clear_string_option(&buf->b_p_sua);
#endif
    clear_string_option(&buf->b_p_ft);
#ifdef FEAT_CINDENT
    clear_string_option(&buf->b_p_cink);
    clear_string_option(&buf->b_p_cino);
#endif
#if defined(FEAT_CINDENT) || defined(FEAT_SMARTINDENT)
    clear_string_option(&buf->b_p_cinw);
#endif
#ifdef FEAT_INS_EXPAND
    clear_string_option(&buf->b_p_cpt);
#endif
#ifdef FEAT_COMPL_FUNC
    clear_string_option(&buf->b_p_cfu);
    clear_string_option(&buf->b_p_ofu);
#endif
#ifdef FEAT_QUICKFIX
    clear_string_option(&buf->b_p_gp);
    clear_string_option(&buf->b_p_mp);
    clear_string_option(&buf->b_p_efm);
#endif
    clear_string_option(&buf->b_p_ep);
    clear_string_option(&buf->b_p_path);
    clear_string_option(&buf->b_p_tags);
    clear_string_option(&buf->b_p_tc);
#ifdef FEAT_INS_EXPAND
    clear_string_option(&buf->b_p_dict);
    clear_string_option(&buf->b_p_tsr);
#endif
#ifdef FEAT_TEXTOBJ
    clear_string_option(&buf->b_p_qe);
#endif
    buf->b_p_ar = -1;
    buf->b_p_ul = NO_LOCAL_UNDOLEVEL;
#ifdef FEAT_LISP
    clear_string_option(&buf->b_p_lw);
#endif
    clear_string_option(&buf->b_p_bkc);
#ifdef FEAT_MBYTE
    clear_string_option(&buf->b_p_menc);
#endif
}

/*
 * Get alternate file "n".
 * Set linenr to "lnum" or altfpos.lnum if "lnum" == 0.
 *	Also set cursor column to altfpos.col if 'startofline' is not set.
 * if (options & GETF_SETMARK) call setpcmark()
 * if (options & GETF_ALT) we are jumping to an alternate file.
 * if (options & GETF_SWITCH) respect 'switchbuf' settings when jumping
 *
 * Return FAIL for failure, OK for success.
 */
    int
buflist_getfile(
    int		n,
    linenr_T	lnum,
    int		options,
    int		forceit)
{
    buf_T	*buf;
    win_T	*wp = NULL;
    pos_T	*fpos;
    colnr_T	col;

    buf = buflist_findnr(n);
    if (buf == NULL)
    {
	if ((options & GETF_ALT) && n == 0)
	    EMSG(_(e_noalt));
	else
	    EMSGN(_("E92: Buffer %ld not found"), n);
	return FAIL;
    }

    /* if alternate file is the current buffer, nothing to do */
    if (buf == curbuf)
	return OK;

    if (text_locked())
    {
	text_locked_msg();
	return FAIL;
    }
    if (curbuf_locked())
	return FAIL;

    /* altfpos may be changed by getfile(), get it now */
    if (lnum == 0)
    {
	fpos = buflist_findfpos(buf);
	lnum = fpos->lnum;
	col = fpos->col;
    }
    else
	col = 0;

    if (options & GETF_SWITCH)
    {
	/* If 'switchbuf' contains "useopen": jump to first window containing
	 * "buf" if one exists */
	if (swb_flags & SWB_USEOPEN)
	    wp = buf_jump_open_win(buf);

	/* If 'switchbuf' contains "usetab": jump to first window in any tab
	 * page containing "buf" if one exists */
	if (wp == NULL && (swb_flags & SWB_USETAB))
	    wp = buf_jump_open_tab(buf);

	/* If 'switchbuf' contains "split", "vsplit" or "newtab" and the
	 * current buffer isn't empty: open new tab or window */
	if (wp == NULL && (swb_flags & (SWB_VSPLIT | SWB_SPLIT | SWB_NEWTAB))
							       && !BUFEMPTY())
	{
	    if (swb_flags & SWB_NEWTAB)
		tabpage_new();
	    else if (win_split(0, (swb_flags & SWB_VSPLIT) ? WSP_VERT : 0)
								      == FAIL)
		return FAIL;
	    RESET_BINDING(curwin);
	}
    }

    ++RedrawingDisabled;
    if (GETFILE_SUCCESS(getfile(buf->b_fnum, NULL, NULL,
				     (options & GETF_SETMARK), lnum, forceit)))
    {
	--RedrawingDisabled;

	/* cursor is at to BOL and w_cursor.lnum is checked due to getfile() */
	if (!p_sol && col != 0)
	{
	    curwin->w_cursor.col = col;
	    check_cursor_col();
#ifdef FEAT_VIRTUALEDIT
	    curwin->w_cursor.coladd = 0;
#endif
	    curwin->w_set_curswant = TRUE;
	}
	return OK;
    }
    --RedrawingDisabled;
    return FAIL;
}

/*
 * go to the last know line number for the current buffer
 */
    void
buflist_getfpos(void)
{
    pos_T	*fpos;

    fpos = buflist_findfpos(curbuf);

    curwin->w_cursor.lnum = fpos->lnum;
    check_cursor_lnum();

    if (p_sol)
	curwin->w_cursor.col = 0;
    else
    {
	curwin->w_cursor.col = fpos->col;
	check_cursor_col();
#ifdef FEAT_VIRTUALEDIT
	curwin->w_cursor.coladd = 0;
#endif
	curwin->w_set_curswant = TRUE;
    }
}

#if defined(FEAT_QUICKFIX) || defined(FEAT_EVAL) || defined(PROTO)
/*
 * Find file in buffer list by name (it has to be for the current window).
 * Returns NULL if not found.
 */
    buf_T *
buflist_findname_exp(char_u *fname)
{
    char_u	*ffname;
    buf_T	*buf = NULL;

    /* First make the name into a full path name */
    ffname = FullName_save(fname,
#ifdef UNIX
	    TRUE	    /* force expansion, get rid of symbolic links */
#else
	    FALSE
#endif
	    );
    if (ffname != NULL)
    {
	buf = buflist_findname(ffname);
	vim_free(ffname);
    }
    return buf;
}
#endif

/*
 * Find file in buffer list by name (it has to be for the current window).
 * "ffname" must have a full path.
 * Skips dummy buffers.
 * Returns NULL if not found.
 */
    buf_T *
buflist_findname(char_u *ffname)
{
#ifdef UNIX
    stat_T	st;

    if (mch_stat((char *)ffname, &st) < 0)
	st.st_dev = (dev_T)-1;
    return buflist_findname_stat(ffname, &st);
}

/*
 * Same as buflist_findname(), but pass the stat structure to avoid getting it
 * twice for the same file.
 * Returns NULL if not found.
 */
    static buf_T *
buflist_findname_stat(
    char_u	*ffname,
    stat_T	*stp)
{
#endif
    buf_T	*buf;

    /* Start at the last buffer, expect to find a match sooner. */
    for (buf = lastbuf; buf != NULL; buf = buf->b_prev)
	if ((buf->b_flags & BF_DUMMY) == 0 && !otherfile_buf(buf, ffname
#ifdef UNIX
		    , stp
#endif
		    ))
	    return buf;
    return NULL;
}

/*
 * Find file in buffer list by a regexp pattern.
 * Return fnum of the found buffer.
 * Return < 0 for error.
 */
    int
buflist_findpat(
    char_u	*pattern,
    char_u	*pattern_end,	/* pointer to first char after pattern */
    int		unlisted,	/* find unlisted buffers */
    int		diffmode UNUSED, /* find diff-mode buffers only */
    int		curtab_only)	/* find buffers in current tab only */
{
    buf_T	*buf;
    int		match = -1;
    int		find_listed;
    char_u	*pat;
    char_u	*patend;
    int		attempt;
    char_u	*p;
    int		toggledollar;

    if (pattern_end == pattern + 1 && (*pattern == '%' || *pattern == '#'))
    {
	if (*pattern == '%')
	    match = curbuf->b_fnum;
	else
	    match = curwin->w_alt_fnum;
#ifdef FEAT_DIFF
	if (diffmode && !diff_mode_buf(buflist_findnr(match)))
	    match = -1;
#endif
    }

    /*
     * Try four ways of matching a listed buffer:
     * attempt == 0: without '^' or '$' (at any position)
     * attempt == 1: with '^' at start (only at position 0)
     * attempt == 2: with '$' at end (only match at end)
     * attempt == 3: with '^' at start and '$' at end (only full match)
     * Repeat this for finding an unlisted buffer if there was no matching
     * listed buffer.
     */
    else
    {
	pat = file_pat_to_reg_pat(pattern, pattern_end, NULL, FALSE);
	if (pat == NULL)
	    return -1;
	patend = pat + STRLEN(pat) - 1;
	toggledollar = (patend > pat && *patend == '$');

	/* First try finding a listed buffer.  If not found and "unlisted"
	 * is TRUE, try finding an unlisted buffer. */
	find_listed = TRUE;
	for (;;)
	{
	    for (attempt = 0; attempt <= 3; ++attempt)
	    {
		regmatch_T	regmatch;

		/* may add '^' and '$' */
		if (toggledollar)
		    *patend = (attempt < 2) ? NUL : '$'; /* add/remove '$' */
		p = pat;
		if (*p == '^' && !(attempt & 1))	 /* add/remove '^' */
		    ++p;
		regmatch.regprog = vim_regcomp(p, p_magic ? RE_MAGIC : 0);
		if (regmatch.regprog == NULL)
		{
		    vim_free(pat);
		    return -1;
		}

		for (buf = lastbuf; buf != NULL; buf = buf->b_prev)
		    if (buf->b_p_bl == find_listed
#ifdef FEAT_DIFF
			    && (!diffmode || diff_mode_buf(buf))
#endif
			    && buflist_match(&regmatch, buf, FALSE) != NULL)
		    {
			if (curtab_only)
			{
			    /* Ignore the match if the buffer is not open in
			     * the current tab. */
			    win_T	*wp;

			    FOR_ALL_WINDOWS(wp)
				if (wp->w_buffer == buf)
				    break;
			    if (wp == NULL)
				continue;
			}
			if (match >= 0)		/* already found a match */
			{
			    match = -2;
			    break;
			}
			match = buf->b_fnum;	/* remember first match */
		    }

		vim_regfree(regmatch.regprog);
		if (match >= 0)			/* found one match */
		    break;
	    }

	    /* Only search for unlisted buffers if there was no match with
	     * a listed buffer. */
	    if (!unlisted || !find_listed || match != -1)
		break;
	    find_listed = FALSE;
	}

	vim_free(pat);
    }

    if (match == -2)
	EMSG2(_("E93: More than one match for %s"), pattern);
    else if (match < 0)
	EMSG2(_("E94: No matching buffer for %s"), pattern);
    return match;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)

/*
 * Find all buffer names that match.
 * For command line expansion of ":buf" and ":sbuf".
 * Return OK if matches found, FAIL otherwise.
 */
    int
ExpandBufnames(
    char_u	*pat,
    int		*num_file,
    char_u	***file,
    int		options)
{
    int		count = 0;
    buf_T	*buf;
    int		round;
    char_u	*p;
    int		attempt;
    char_u	*patc;

    *num_file = 0;		    /* return values in case of FAIL */
    *file = NULL;

    /* Make a copy of "pat" and change "^" to "\(^\|[\/]\)". */
    if (*pat == '^')
    {
	patc = alloc((unsigned)STRLEN(pat) + 11);
	if (patc == NULL)
	    return FAIL;
	STRCPY(patc, "\\(^\\|[\\/]\\)");
	STRCPY(patc + 11, pat + 1);
    }
    else
	patc = pat;

    /*
     * attempt == 0: try match with    '\<', match at start of word
     * attempt == 1: try match without '\<', match anywhere
     */
    for (attempt = 0; attempt <= 1; ++attempt)
    {
	regmatch_T	regmatch;

	if (attempt > 0 && patc == pat)
	    break;	/* there was no anchor, no need to try again */
	regmatch.regprog = vim_regcomp(patc + attempt * 11, RE_MAGIC);
	if (regmatch.regprog == NULL)
	{
	    if (patc != pat)
		vim_free(patc);
	    return FAIL;
	}

	/*
	 * round == 1: Count the matches.
	 * round == 2: Build the array to keep the matches.
	 */
	for (round = 1; round <= 2; ++round)
	{
	    count = 0;
	    FOR_ALL_BUFFERS(buf)
	    {
		if (!buf->b_p_bl)	/* skip unlisted buffers */
		    continue;
		p = buflist_match(&regmatch, buf, p_wic);
		if (p != NULL)
		{
		    if (round == 1)
			++count;
		    else
		    {
			if (options & WILD_HOME_REPLACE)
			    p = home_replace_save(buf, p);
			else
			    p = vim_strsave(p);
			(*file)[count++] = p;
		    }
		}
	    }
	    if (count == 0)	/* no match found, break here */
		break;
	    if (round == 1)
	    {
		*file = (char_u **)alloc((unsigned)(count * sizeof(char_u *)));
		if (*file == NULL)
		{
		    vim_regfree(regmatch.regprog);
		    if (patc != pat)
			vim_free(patc);
		    return FAIL;
		}
	    }
	}
	vim_regfree(regmatch.regprog);
	if (count)		/* match(es) found, break here */
	    break;
    }

    if (patc != pat)
	vim_free(patc);

    *num_file = count;
    return (count == 0 ? FAIL : OK);
}

#endif /* FEAT_CMDL_COMPL */

/*
 * Check for a match on the file name for buffer "buf" with regprog "prog".
 */
    static char_u *
buflist_match(
    regmatch_T	*rmp,
    buf_T	*buf,
    int		ignore_case)  /* when TRUE ignore case, when FALSE use 'fic' */
{
    char_u	*match;

    /* First try the short file name, then the long file name. */
    match = fname_match(rmp, buf->b_sfname, ignore_case);
    if (match == NULL)
	match = fname_match(rmp, buf->b_ffname, ignore_case);

    return match;
}

/*
 * Try matching the regexp in "prog" with file name "name".
 * Return "name" when there is a match, NULL when not.
 */
    static char_u *
fname_match(
    regmatch_T	*rmp,
    char_u	*name,
    int		ignore_case)  /* when TRUE ignore case, when FALSE use 'fic' */
{
    char_u	*match = NULL;
    char_u	*p;

    if (name != NULL)
    {
	/* Ignore case when 'fileignorecase' or the argument is set. */
	rmp->rm_ic = p_fic || ignore_case;
	if (vim_regexec(rmp, name, (colnr_T)0))
	    match = name;
	else
	{
	    /* Replace $(HOME) with '~' and try matching again. */
	    p = home_replace_save(NULL, name);
	    if (p != NULL && vim_regexec(rmp, p, (colnr_T)0))
		match = name;
	    vim_free(p);
	}
    }

    return match;
}

/*
 * Find a file in the buffer list by buffer number.
 */
    buf_T *
buflist_findnr(int nr)
{
    char_u	key[VIM_SIZEOF_INT * 2 + 1];
    hashitem_T	*hi;

    if (nr == 0)
	nr = curwin->w_alt_fnum;
    sprintf((char *)key, "%x", nr);
    hi = hash_find(&buf_hashtab, key);

    if (!HASHITEM_EMPTY(hi))
	return (buf_T *)(hi->hi_key
			     - ((unsigned)(curbuf->b_key - (char_u *)curbuf)));
    return NULL;
}

/*
 * Get name of file 'n' in the buffer list.
 * When the file has no name an empty string is returned.
 * home_replace() is used to shorten the file name (used for marks).
 * Returns a pointer to allocated memory, of NULL when failed.
 */
    char_u *
buflist_nr2name(
    int		n,
    int		fullname,
    int		helptail)	/* for help buffers return tail only */
{
    buf_T	*buf;

    buf = buflist_findnr(n);
    if (buf == NULL)
	return NULL;
    return home_replace_save(helptail ? buf : NULL,
				     fullname ? buf->b_ffname : buf->b_fname);
}

/*
 * Set the "lnum" and "col" for the buffer "buf" and the current window.
 * When "copy_options" is TRUE save the local window option values.
 * When "lnum" is 0 only do the options.
 */
    static void
buflist_setfpos(
    buf_T	*buf,
    win_T	*win,
    linenr_T	lnum,
    colnr_T	col,
    int		copy_options)
{
    wininfo_T	*wip;

    for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
	if (wip->wi_win == win)
	    break;
    if (wip == NULL)
    {
	/* allocate a new entry */
	wip = (wininfo_T *)alloc_clear((unsigned)sizeof(wininfo_T));
	if (wip == NULL)
	    return;
	wip->wi_win = win;
	if (lnum == 0)		/* set lnum even when it's 0 */
	    lnum = 1;
    }
    else
    {
	/* remove the entry from the list */
	if (wip->wi_prev)
	    wip->wi_prev->wi_next = wip->wi_next;
	else
	    buf->b_wininfo = wip->wi_next;
	if (wip->wi_next)
	    wip->wi_next->wi_prev = wip->wi_prev;
	if (copy_options && wip->wi_optset)
	{
	    clear_winopt(&wip->wi_opt);
#ifdef FEAT_FOLDING
	    deleteFoldRecurse(&wip->wi_folds);
#endif
	}
    }
    if (lnum != 0)
    {
	wip->wi_fpos.lnum = lnum;
	wip->wi_fpos.col = col;
    }
    if (copy_options)
    {
	/* Save the window-specific option values. */
	copy_winopt(&win->w_onebuf_opt, &wip->wi_opt);
#ifdef FEAT_FOLDING
	wip->wi_fold_manual = win->w_fold_manual;
	cloneFoldGrowArray(&win->w_folds, &wip->wi_folds);
#endif
	wip->wi_optset = TRUE;
    }

    /* insert the entry in front of the list */
    wip->wi_next = buf->b_wininfo;
    buf->b_wininfo = wip;
    wip->wi_prev = NULL;
    if (wip->wi_next)
	wip->wi_next->wi_prev = wip;

    return;
}

#ifdef FEAT_DIFF
/*
 * Return TRUE when "wip" has 'diff' set and the diff is only for another tab
 * page.  That's because a diff is local to a tab page.
 */
    static int
wininfo_other_tab_diff(wininfo_T *wip)
{
    win_T	*wp;

    if (wip->wi_opt.wo_diff)
    {
	FOR_ALL_WINDOWS(wp)
	    /* return FALSE when it's a window in the current tab page, thus
	     * the buffer was in diff mode here */
	    if (wip->wi_win == wp)
		return FALSE;
	return TRUE;
    }
    return FALSE;
}
#endif

/*
 * Find info for the current window in buffer "buf".
 * If not found, return the info for the most recently used window.
 * When "skip_diff_buffer" is TRUE avoid windows with 'diff' set that is in
 * another tab page.
 * Returns NULL when there isn't any info.
 */
    static wininfo_T *
find_wininfo(
    buf_T	*buf,
    int		skip_diff_buffer UNUSED)
{
    wininfo_T	*wip;

    for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
	if (wip->wi_win == curwin
#ifdef FEAT_DIFF
		&& (!skip_diff_buffer || !wininfo_other_tab_diff(wip))
#endif
	   )
	    break;

    /* If no wininfo for curwin, use the first in the list (that doesn't have
     * 'diff' set and is in another tab page). */
    if (wip == NULL)
    {
#ifdef FEAT_DIFF
	if (skip_diff_buffer)
	{
	    for (wip = buf->b_wininfo; wip != NULL; wip = wip->wi_next)
		if (!wininfo_other_tab_diff(wip))
		    break;
	}
	else
#endif
	    wip = buf->b_wininfo;
    }
    return wip;
}

/*
 * Reset the local window options to the values last used in this window.
 * If the buffer wasn't used in this window before, use the values from
 * the most recently used window.  If the values were never set, use the
 * global values for the window.
 */
    void
get_winopts(buf_T *buf)
{
    wininfo_T	*wip;

    clear_winopt(&curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
    clearFolding(curwin);
#endif

    wip = find_wininfo(buf, TRUE);
    if (wip != NULL && wip->wi_win != NULL
	    && wip->wi_win != curwin && wip->wi_win->w_buffer == buf)
    {
	/* The buffer is currently displayed in the window: use the actual
	 * option values instead of the saved (possibly outdated) values. */
	win_T *wp = wip->wi_win;

	copy_winopt(&wp->w_onebuf_opt, &curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
	curwin->w_fold_manual = wp->w_fold_manual;
	curwin->w_foldinvalid = TRUE;
	cloneFoldGrowArray(&wp->w_folds, &curwin->w_folds);
#endif
    }
    else if (wip != NULL && wip->wi_optset)
    {
	/* the buffer was displayed in the current window earlier */
	copy_winopt(&wip->wi_opt, &curwin->w_onebuf_opt);
#ifdef FEAT_FOLDING
	curwin->w_fold_manual = wip->wi_fold_manual;
	curwin->w_foldinvalid = TRUE;
	cloneFoldGrowArray(&wip->wi_folds, &curwin->w_folds);
#endif
    }
    else
	copy_winopt(&curwin->w_allbuf_opt, &curwin->w_onebuf_opt);

#ifdef FEAT_FOLDING
    /* Set 'foldlevel' to 'foldlevelstart' if it's not negative. */
    if (p_fdls >= 0)
	curwin->w_p_fdl = p_fdls;
#endif
#ifdef FEAT_SYN_HL
    check_colorcolumn(curwin);
#endif
}

/*
 * Find the position (lnum and col) for the buffer 'buf' for the current
 * window.
 * Returns a pointer to no_position if no position is found.
 */
    pos_T *
buflist_findfpos(buf_T *buf)
{
    wininfo_T	*wip;
    static pos_T no_position = INIT_POS_T(1, 0, 0);

    wip = find_wininfo(buf, FALSE);
    if (wip != NULL)
	return &(wip->wi_fpos);
    else
	return &no_position;
}

/*
 * Find the lnum for the buffer 'buf' for the current window.
 */
    linenr_T
buflist_findlnum(buf_T *buf)
{
    return buflist_findfpos(buf)->lnum;
}

/*
 * List all known file names (for :files and :buffers command).
 */
    void
buflist_list(exarg_T *eap)
{
    buf_T	*buf;
    int		len;
    int		i;
    int		ro_char;
    int		changed_char;
#ifdef FEAT_TERMINAL
    int		job_running;
    int		job_none_open;
#endif

    for (buf = firstbuf; buf != NULL && !got_int; buf = buf->b_next)
    {
#ifdef FEAT_TERMINAL
	job_running = term_job_running(buf->b_term);
	job_none_open = job_running && term_none_open(buf->b_term);
#endif
	/* skip unlisted buffers, unless ! was used */
	if ((!buf->b_p_bl && !eap->forceit && !vim_strchr(eap->arg, 'u'))
		|| (vim_strchr(eap->arg, 'u') && buf->b_p_bl)
		|| (vim_strchr(eap->arg, '+')
			&& ((buf->b_flags & BF_READERR) || !bufIsChanged(buf)))
		|| (vim_strchr(eap->arg, 'a')
			&& (buf->b_ml.ml_mfp == NULL || buf->b_nwindows == 0))
		|| (vim_strchr(eap->arg, 'h')
			&& (buf->b_ml.ml_mfp == NULL || buf->b_nwindows != 0))
#ifdef FEAT_TERMINAL
		|| (vim_strchr(eap->arg, 'R')
			&& (!job_running || (job_running && job_none_open)))
		|| (vim_strchr(eap->arg, '?')
			&& (!job_running || (job_running && !job_none_open)))
		|| (vim_strchr(eap->arg, 'F')
			&& (job_running || buf->b_term == NULL))
#endif
		|| (vim_strchr(eap->arg, '-') && buf->b_p_ma)
		|| (vim_strchr(eap->arg, '=') && !buf->b_p_ro)
		|| (vim_strchr(eap->arg, 'x') && !(buf->b_flags & BF_READERR))
		|| (vim_strchr(eap->arg, '%') && buf != curbuf)
		|| (vim_strchr(eap->arg, '#')
		      && (buf == curbuf || curwin->w_alt_fnum != buf->b_fnum)))
	    continue;
	if (buf_spname(buf) != NULL)
	    vim_strncpy(NameBuff, buf_spname(buf), MAXPATHL - 1);
	else
	    home_replace(buf, buf->b_fname, NameBuff, MAXPATHL, TRUE);
	if (message_filtered(NameBuff))
	    continue;

	changed_char = (buf->b_flags & BF_READERR) ? 'x'
					     : (bufIsChanged(buf) ? '+' : ' ');
#ifdef FEAT_TERMINAL
	if (term_job_running(buf->b_term))
	{
	    if (term_none_open(buf->b_term))
		ro_char = '?';
	    else
		ro_char = 'R';
	    changed_char = ' ';  /* bufIsChanged() returns TRUE to avoid
				  * closing, but it's not actually changed. */
	}
	else if (buf->b_term != NULL)
	    ro_char = 'F';
	else
#endif
	    ro_char = !buf->b_p_ma ? '-' : (buf->b_p_ro ? '=' : ' ');

	msg_putchar('\n');
	len = vim_snprintf((char *)IObuff, IOSIZE - 20, "%3d%c%c%c%c%c \"%s\"",
		buf->b_fnum,
		buf->b_p_bl ? ' ' : 'u',
		buf == curbuf ? '%' :
			(curwin->w_alt_fnum == buf->b_fnum ? '#' : ' '),
		buf->b_ml.ml_mfp == NULL ? ' ' :
			(buf->b_nwindows == 0 ? 'h' : 'a'),
		ro_char,
		changed_char,
		NameBuff);
	if (len > IOSIZE - 20)
	    len = IOSIZE - 20;

	/* put "line 999" in column 40 or after the file name */
	i = 40 - vim_strsize(IObuff);
	do
	{
	    IObuff[len++] = ' ';
	} while (--i > 0 && len < IOSIZE - 18);
	vim_snprintf((char *)IObuff + len, (size_t)(IOSIZE - len),
		_("line %ld"), buf == curbuf ? curwin->w_cursor.lnum
					       : (long)buflist_findlnum(buf));
	msg_outtrans(IObuff);
	out_flush();	    /* output one line at a time */
	ui_breakcheck();
    }
}

/*
 * Get file name and line number for file 'fnum'.
 * Used by DoOneCmd() for translating '%' and '#'.
 * Used by insert_reg() and cmdline_paste() for '#' register.
 * Return FAIL if not found, OK for success.
 */
    int
buflist_name_nr(
    int		fnum,
    char_u	**fname,
    linenr_T	*lnum)
{
    buf_T	*buf;

    buf = buflist_findnr(fnum);
    if (buf == NULL || buf->b_fname == NULL)
	return FAIL;

    *fname = buf->b_fname;
    *lnum = buflist_findlnum(buf);

    return OK;
}

/*
 * Set the file name for "buf"' to "ffname_arg", short file name to
 * "sfname_arg".
 * The file name with the full path is also remembered, for when :cd is used.
 * Returns FAIL for failure (file name already in use by other buffer)
 *	OK otherwise.
 */
    int
setfname(
    buf_T	*buf,
    char_u	*ffname_arg,
    char_u	*sfname_arg,
    int		message)	/* give message when buffer already exists */
{
    char_u	*ffname = ffname_arg;
    char_u	*sfname = sfname_arg;
    buf_T	*obuf = NULL;
#ifdef UNIX
    stat_T	st;
#endif

    if (ffname == NULL || *ffname == NUL)
    {
	/* Removing the name. */
	if (buf->b_sfname != buf->b_ffname)
	    VIM_CLEAR(buf->b_sfname);
	else
	    buf->b_sfname = NULL;
	VIM_CLEAR(buf->b_ffname);
#ifdef UNIX
	st.st_dev = (dev_T)-1;
#endif
    }
    else
    {
	fname_expand(buf, &ffname, &sfname); /* will allocate ffname */
	if (ffname == NULL)		    /* out of memory */
	    return FAIL;

	/*
	 * if the file name is already used in another buffer:
	 * - if the buffer is loaded, fail
	 * - if the buffer is not loaded, delete it from the list
	 */
#ifdef UNIX
	if (mch_stat((char *)ffname, &st) < 0)
	    st.st_dev = (dev_T)-1;
#endif
	if (!(buf->b_flags & BF_DUMMY))
#ifdef UNIX
	    obuf = buflist_findname_stat(ffname, &st);
#else
	    obuf = buflist_findname(ffname);
#endif
	if (obuf != NULL && obuf != buf)
	{
	    if (obuf->b_ml.ml_mfp != NULL)	/* it's loaded, fail */
	    {
		if (message)
		    EMSG(_("E95: Buffer with this name already exists"));
		vim_free(ffname);
		return FAIL;
	    }
	    /* delete from the list */
	    close_buffer(NULL, obuf, DOBUF_WIPE, FALSE);
	}
	sfname = vim_strsave(sfname);
	if (ffname == NULL || sfname == NULL)
	{
	    vim_free(sfname);
	    vim_free(ffname);
	    return FAIL;
	}
#ifdef USE_FNAME_CASE
# ifdef USE_LONG_FNAME
	if (USE_LONG_FNAME)
# endif
	    fname_case(sfname, 0);    /* set correct case for short file name */
#endif
	if (buf->b_sfname != buf->b_ffname)
	    vim_free(buf->b_sfname);
	vim_free(buf->b_ffname);
	buf->b_ffname = ffname;
	buf->b_sfname = sfname;
    }
    buf->b_fname = buf->b_sfname;
#ifdef UNIX
    if (st.st_dev == (dev_T)-1)
	buf->b_dev_valid = FALSE;
    else
    {
	buf->b_dev_valid = TRUE;
	buf->b_dev = st.st_dev;
	buf->b_ino = st.st_ino;
    }
#endif

    buf->b_shortname = FALSE;

    buf_name_changed(buf);
    return OK;
}

/*
 * Crude way of changing the name of a buffer.  Use with care!
 * The name should be relative to the current directory.
 */
    void
buf_set_name(int fnum, char_u *name)
{
    buf_T	*buf;

    buf = buflist_findnr(fnum);
    if (buf != NULL)
    {
	if (buf->b_sfname != buf->b_ffname)
	    vim_free(buf->b_sfname);
	vim_free(buf->b_ffname);
	buf->b_ffname = vim_strsave(name);
	buf->b_sfname = NULL;
	/* Allocate ffname and expand into full path.  Also resolves .lnk
	 * files on Win32. */
	fname_expand(buf, &buf->b_ffname, &buf->b_sfname);
	buf->b_fname = buf->b_sfname;
    }
}

/*
 * Take care of what needs to be done when the name of buffer "buf" has
 * changed.
 */
    void
buf_name_changed(buf_T *buf)
{
    /*
     * If the file name changed, also change the name of the swapfile
     */
    if (buf->b_ml.ml_mfp != NULL)
	ml_setname(buf);

    if (curwin->w_buffer == buf)
	check_arg_idx(curwin);	/* check file name for arg list */
#ifdef FEAT_TITLE
    maketitle();		/* set window title */
#endif
    status_redraw_all();	/* status lines need to be redrawn */
    fmarks_check_names(buf);	/* check named file marks */
    ml_timestamp(buf);		/* reset timestamp */
}

/*
 * set alternate file name for current window
 *
 * Used by do_one_cmd(), do_write() and do_ecmd().
 * Return the buffer.
 */
    buf_T *
setaltfname(
    char_u	*ffname,
    char_u	*sfname,
    linenr_T	lnum)
{
    buf_T	*buf;

    /* Create a buffer.  'buflisted' is not set if it's a new buffer */
    buf = buflist_new(ffname, sfname, lnum, 0);
    if (buf != NULL && !cmdmod.keepalt)
	curwin->w_alt_fnum = buf->b_fnum;
    return buf;
}

/*
 * Get alternate file name for current window.
 * Return NULL if there isn't any, and give error message if requested.
 */
    char_u  *
getaltfname(
    int		errmsg)		/* give error message */
{
    char_u	*fname;
    linenr_T	dummy;

    if (buflist_name_nr(0, &fname, &dummy) == FAIL)
    {
	if (errmsg)
	    EMSG(_(e_noalt));
	return NULL;
    }
    return fname;
}

/*
 * Add a file name to the buflist and return its number.
 * Uses same flags as buflist_new(), except BLN_DUMMY.
 *
 * used by qf_init(), main() and doarglist()
 */
    int
buflist_add(char_u *fname, int flags)
{
    buf_T	*buf;

    buf = buflist_new(fname, NULL, (linenr_T)0, flags);
    if (buf != NULL)
	return buf->b_fnum;
    return 0;
}

#if defined(BACKSLASH_IN_FILENAME) || defined(PROTO)
/*
 * Adjust slashes in file names.  Called after 'shellslash' was set.
 */
    void
buflist_slash_adjust(void)
{
    buf_T	*bp;

    FOR_ALL_BUFFERS(bp)
    {
	if (bp->b_ffname != NULL)
	    slash_adjust(bp->b_ffname);
	if (bp->b_sfname != NULL)
	    slash_adjust(bp->b_sfname);
    }
}
#endif

/*
 * Set alternate cursor position for the current buffer and window "win".
 * Also save the local window option values.
 */
    void
buflist_altfpos(win_T *win)
{
    buflist_setfpos(curbuf, win, win->w_cursor.lnum, win->w_cursor.col, TRUE);
}

/*
 * Return TRUE if 'ffname' is not the same file as current file.
 * Fname must have a full path (expanded by mch_FullName()).
 */
    int
otherfile(char_u *ffname)
{
    return otherfile_buf(curbuf, ffname
#ifdef UNIX
	    , NULL
#endif
	    );
}

    static int
otherfile_buf(
    buf_T		*buf,
    char_u		*ffname
#ifdef UNIX
    , stat_T		*stp
#endif
    )
{
    /* no name is different */
    if (ffname == NULL || *ffname == NUL || buf->b_ffname == NULL)
	return TRUE;
    if (fnamecmp(ffname, buf->b_ffname) == 0)
	return FALSE;
#ifdef UNIX
    {
	stat_T	    st;

	/* If no stat_T given, get it now */
	if (stp == NULL)
	{
	    if (!buf->b_dev_valid || mch_stat((char *)ffname, &st) < 0)
		st.st_dev = (dev_T)-1;
	    stp = &st;
	}
	/* Use dev/ino to check if the files are the same, even when the names
	 * are different (possible with links).  Still need to compare the
	 * name above, for when the file doesn't exist yet.
	 * Problem: The dev/ino changes when a file is deleted (and created
	 * again) and remains the same when renamed/moved.  We don't want to
	 * mch_stat() each buffer each time, that would be too slow.  Get the
	 * dev/ino again when they appear to match, but not when they appear
	 * to be different: Could skip a buffer when it's actually the same
	 * file. */
	if (buf_same_ino(buf, stp))
	{
	    buf_setino(buf);
	    if (buf_same_ino(buf, stp))
		return FALSE;
	}
    }
#endif
    return TRUE;
}

#if defined(UNIX) || defined(PROTO)
/*
 * Set inode and device number for a buffer.
 * Must always be called when b_fname is changed!.
 */
    void
buf_setino(buf_T *buf)
{
    stat_T	st;

    if (buf->b_fname != NULL && mch_stat((char *)buf->b_fname, &st) >= 0)
    {
	buf->b_dev_valid = TRUE;
	buf->b_dev = st.st_dev;
	buf->b_ino = st.st_ino;
    }
    else
	buf->b_dev_valid = FALSE;
}

/*
 * Return TRUE if dev/ino in buffer "buf" matches with "stp".
 */
    static int
buf_same_ino(
    buf_T	*buf,
    stat_T	*stp)
{
    return (buf->b_dev_valid
	    && stp->st_dev == buf->b_dev
	    && stp->st_ino == buf->b_ino);
}
#endif

/*
 * Print info about the current buffer.
 */
    void
fileinfo(
    int fullname,	    /* when non-zero print full path */
    int shorthelp,
    int	dont_truncate)
{
    char_u	*name;
    int		n;
    char_u	*p;
    char_u	*buffer;
    size_t	len;

    buffer = alloc(IOSIZE);
    if (buffer == NULL)
	return;

    if (fullname > 1)	    /* 2 CTRL-G: include buffer number */
    {
	vim_snprintf((char *)buffer, IOSIZE, "buf %d: ", curbuf->b_fnum);
	p = buffer + STRLEN(buffer);
    }
    else
	p = buffer;

    *p++ = '"';
    if (buf_spname(curbuf) != NULL)
	vim_strncpy(p, buf_spname(curbuf), IOSIZE - (p - buffer) - 1);
    else
    {
	if (!fullname && curbuf->b_fname != NULL)
	    name = curbuf->b_fname;
	else
	    name = curbuf->b_ffname;
	home_replace(shorthelp ? curbuf : NULL, name, p,
					  (int)(IOSIZE - (p - buffer)), TRUE);
    }

    vim_snprintf_add((char *)buffer, IOSIZE, "\"%s%s%s%s%s%s",
	    curbufIsChanged() ? (shortmess(SHM_MOD)
					  ?  " [+]" : _(" [Modified]")) : " ",
	    (curbuf->b_flags & BF_NOTEDITED)
#ifdef FEAT_QUICKFIX
		    && !bt_dontwrite(curbuf)
#endif
					? _("[Not edited]") : "",
	    (curbuf->b_flags & BF_NEW)
#ifdef FEAT_QUICKFIX
		    && !bt_dontwrite(curbuf)
#endif
					? _("[New file]") : "",
	    (curbuf->b_flags & BF_READERR) ? _("[Read errors]") : "",
	    curbuf->b_p_ro ? (shortmess(SHM_RO) ? _("[RO]")
						      : _("[readonly]")) : "",
	    (curbufIsChanged() || (curbuf->b_flags & BF_WRITE_MASK)
							  || curbuf->b_p_ro) ?
								    " " : "");
    /* With 32 bit longs and more than 21,474,836 lines multiplying by 100
     * causes an overflow, thus for large numbers divide instead. */
    if (curwin->w_cursor.lnum > 1000000L)
	n = (int)(((long)curwin->w_cursor.lnum) /
				   ((long)curbuf->b_ml.ml_line_count / 100L));
    else
	n = (int)(((long)curwin->w_cursor.lnum * 100L) /
					    (long)curbuf->b_ml.ml_line_count);
    if (curbuf->b_ml.ml_flags & ML_EMPTY)
	vim_snprintf_add((char *)buffer, IOSIZE, "%s", _(no_lines_msg));
#ifdef FEAT_CMDL_INFO
    else if (p_ru)
	/* Current line and column are already on the screen -- webb */
	vim_snprintf_add((char *)buffer, IOSIZE,
		NGETTEXT("%ld line --%d%%--", "%ld lines --%d%%--",
						   curbuf->b_ml.ml_line_count),
		(long)curbuf->b_ml.ml_line_count, n);
#endif
    else
    {
	vim_snprintf_add((char *)buffer, IOSIZE,
		_("line %ld of %ld --%d%%-- col "),
		(long)curwin->w_cursor.lnum,
		(long)curbuf->b_ml.ml_line_count,
		n);
	validate_virtcol();
	len = STRLEN(buffer);
	col_print(buffer + len, IOSIZE - len,
		   (int)curwin->w_cursor.col + 1, (int)curwin->w_virtcol + 1);
    }

    (void)append_arg_number(curwin, buffer, IOSIZE, !shortmess(SHM_FILE));

    if (dont_truncate)
    {
	/* Temporarily set msg_scroll to avoid the message being truncated.
	 * First call msg_start() to get the message in the right place. */
	msg_start();
	n = msg_scroll;
	msg_scroll = TRUE;
	msg(buffer);
	msg_scroll = n;
    }
    else
    {
	p = msg_trunc_attr(buffer, FALSE, 0);
	if (restart_edit != 0 || (msg_scrolled && !need_wait_return))
	    /* Need to repeat the message after redrawing when:
	     * - When restart_edit is set (otherwise there will be a delay
	     *   before redrawing).
	     * - When the screen was scrolled but there is no wait-return
	     *   prompt. */
	    set_keep_msg(p, 0);
    }

    vim_free(buffer);
}

    void
col_print(
    char_u  *buf,
    size_t  buflen,
    int	    col,
    int	    vcol)
{
    if (col == vcol)
	vim_snprintf((char *)buf, buflen, "%d", col);
    else
	vim_snprintf((char *)buf, buflen, "%d-%d", col, vcol);
}

#if defined(FEAT_TITLE) || defined(PROTO)
static char_u *lasttitle = NULL;
static char_u *lasticon = NULL;

/*
 * Put the file name in the title bar and icon of the window.
 */
    void
maketitle(void)
{
    char_u	*p;
    char_u	*title_str = NULL;
    char_u	*icon_str = NULL;
    int		maxlen = 0;
    int		len;
    int		mustset;
    char_u	buf[IOSIZE];
    int		off;

    if (!redrawing())
    {
	/* Postpone updating the title when 'lazyredraw' is set. */
	need_maketitle = TRUE;
	return;
    }

    need_maketitle = FALSE;
    if (!p_title && !p_icon && lasttitle == NULL && lasticon == NULL)
	return;  // nothing to do

    if (p_title)
    {
	if (p_titlelen > 0)
	{
	    maxlen = p_titlelen * Columns / 100;
	    if (maxlen < 10)
		maxlen = 10;
	}

	title_str = buf;
	if (*p_titlestring != NUL)
	{
#ifdef FEAT_STL_OPT
	    if (stl_syntax & STL_IN_TITLE)
	    {
		int	use_sandbox = FALSE;
		int	save_called_emsg = called_emsg;

# ifdef FEAT_EVAL
		use_sandbox = was_set_insecurely((char_u *)"titlestring", 0);
# endif
		called_emsg = FALSE;
		build_stl_str_hl(curwin, title_str, sizeof(buf),
					      p_titlestring, use_sandbox,
					      0, maxlen, NULL, NULL);
		if (called_emsg)
		    set_string_option_direct((char_u *)"titlestring", -1,
					   (char_u *)"", OPT_FREE, SID_ERROR);
		called_emsg |= save_called_emsg;
	    }
	    else
#endif
		title_str = p_titlestring;
	}
	else
	{
	    /* format: "fname + (path) (1 of 2) - VIM" */

#define SPACE_FOR_FNAME (IOSIZE - 100)
#define SPACE_FOR_DIR   (IOSIZE - 20)
#define SPACE_FOR_ARGNR (IOSIZE - 10)  /* at least room for " - VIM" */
	    if (curbuf->b_fname == NULL)
		vim_strncpy(buf, (char_u *)_("[No Name]"), SPACE_FOR_FNAME);
#ifdef FEAT_TERMINAL
	    else if (curbuf->b_term != NULL)
	    {
		vim_strncpy(buf, term_get_status_text(curbuf->b_term),
							      SPACE_FOR_FNAME);
	    }
#endif
	    else
	    {
		p = transstr(gettail(curbuf->b_fname));
		vim_strncpy(buf, p, SPACE_FOR_FNAME);
		vim_free(p);
	    }

#ifdef FEAT_TERMINAL
	    if (curbuf->b_term == NULL)
#endif
		switch (bufIsChanged(curbuf)
			+ (curbuf->b_p_ro * 2)
			+ (!curbuf->b_p_ma * 4))
		{
		    case 1: STRCAT(buf, " +"); break;
		    case 2: STRCAT(buf, " ="); break;
		    case 3: STRCAT(buf, " =+"); break;
		    case 4:
		    case 6: STRCAT(buf, " -"); break;
		    case 5:
		    case 7: STRCAT(buf, " -+"); break;
		}

	    if (curbuf->b_fname != NULL
#ifdef FEAT_TERMINAL
		    && curbuf->b_term == NULL
#endif
		    )
	    {
		/* Get path of file, replace home dir with ~ */
		off = (int)STRLEN(buf);
		buf[off++] = ' ';
		buf[off++] = '(';
		home_replace(curbuf, curbuf->b_ffname,
					buf + off, SPACE_FOR_DIR - off, TRUE);
#ifdef BACKSLASH_IN_FILENAME
		/* avoid "c:/name" to be reduced to "c" */
		if (isalpha(buf[off]) && buf[off + 1] == ':')
		    off += 2;
#endif
		/* remove the file name */
		p = gettail_sep(buf + off);
		if (p == buf + off)
		{
		    /* must be a help buffer */
		    vim_strncpy(buf + off, (char_u *)_("help"),
					   (size_t)(SPACE_FOR_DIR - off - 1));
		}
		else
		    *p = NUL;

		/* Translate unprintable chars and concatenate.  Keep some
		 * room for the server name.  When there is no room (very long
		 * file name) use (...). */
		if (off < SPACE_FOR_DIR)
		{
		    p = transstr(buf + off);
		    vim_strncpy(buf + off, p, (size_t)(SPACE_FOR_DIR - off));
		    vim_free(p);
		}
		else
		{
		    vim_strncpy(buf + off, (char_u *)"...",
					     (size_t)(SPACE_FOR_ARGNR - off));
		}
		STRCAT(buf, ")");
	    }

	    append_arg_number(curwin, buf, SPACE_FOR_ARGNR, FALSE);

#if defined(FEAT_CLIENTSERVER)
	    if (serverName != NULL)
	    {
		STRCAT(buf, " - ");
		vim_strcat(buf, serverName, IOSIZE);
	    }
	    else
#endif
		STRCAT(buf, " - VIM");

	    if (maxlen > 0)
	    {
		/* make it shorter by removing a bit in the middle */
		if (vim_strsize(buf) > maxlen)
		    trunc_string(buf, buf, maxlen, IOSIZE);
	    }
	}
    }
    mustset = value_changed(title_str, &lasttitle);

    if (p_icon)
    {
	icon_str = buf;
	if (*p_iconstring != NUL)
	{
#ifdef FEAT_STL_OPT
	    if (stl_syntax & STL_IN_ICON)
	    {
		int	use_sandbox = FALSE;
		int	save_called_emsg = called_emsg;

# ifdef FEAT_EVAL
		use_sandbox = was_set_insecurely((char_u *)"iconstring", 0);
# endif
		called_emsg = FALSE;
		build_stl_str_hl(curwin, icon_str, sizeof(buf),
						    p_iconstring, use_sandbox,
						    0, 0, NULL, NULL);
		if (called_emsg)
		    set_string_option_direct((char_u *)"iconstring", -1,
					   (char_u *)"", OPT_FREE, SID_ERROR);
		called_emsg |= save_called_emsg;
	    }
	    else
#endif
		icon_str = p_iconstring;
	}
	else
	{
	    if (buf_spname(curbuf) != NULL)
		p = buf_spname(curbuf);
	    else		    /* use file name only in icon */
		p = gettail(curbuf->b_ffname);
	    *icon_str = NUL;
	    /* Truncate name at 100 bytes. */
	    len = (int)STRLEN(p);
	    if (len > 100)
	    {
		len -= 100;
#ifdef FEAT_MBYTE
		if (has_mbyte)
		    len += (*mb_tail_off)(p, p + len) + 1;
#endif
		p += len;
	    }
	    STRCPY(icon_str, p);
	    trans_characters(icon_str, IOSIZE);
	}
    }

    mustset |= value_changed(icon_str, &lasticon);

    if (mustset)
	resettitle();
}

/*
 * Used for title and icon: Check if "str" differs from "*last".  Set "*last"
 * from "str" if it does.
 * Return TRUE if resettitle() is to be called.
 */
    static int
value_changed(char_u *str, char_u **last)
{
    if ((str == NULL) != (*last == NULL)
	    || (str != NULL && *last != NULL && STRCMP(str, *last) != 0))
    {
	vim_free(*last);
	if (str == NULL)
	{
	    *last = NULL;
	    mch_restore_title(
		  last == &lasttitle ? SAVE_RESTORE_TITLE : SAVE_RESTORE_ICON);
	}
	else
	{
	    *last = vim_strsave(str);
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * Put current window title back (used after calling a shell)
 */
    void
resettitle(void)
{
    mch_settitle(lasttitle, lasticon);
}

# if defined(EXITFREE) || defined(PROTO)
    void
free_titles(void)
{
    vim_free(lasttitle);
    vim_free(lasticon);
}
# endif

#endif /* FEAT_TITLE */

#if defined(FEAT_STL_OPT) || defined(FEAT_GUI_TABLINE) || defined(PROTO)
/*
 * Build a string from the status line items in "fmt".
 * Return length of string in screen cells.
 *
 * Normally works for window "wp", except when working for 'tabline' then it
 * is "curwin".
 *
 * Items are drawn interspersed with the text that surrounds it
 * Specials: %-<wid>(xxx%) => group, %= => middle marker, %< => truncation
 * Item: %-<minwid>.<maxwid><itemch> All but <itemch> are optional
 *
 * If maxwidth is not zero, the string will be filled at any middle marker
 * or truncated if too long, fillchar is used for all whitespace.
 */
    int
build_stl_str_hl(
    win_T	*wp,
    char_u	*out,		/* buffer to write into != NameBuff */
    size_t	outlen,		/* length of out[] */
    char_u	*fmt,
    int		use_sandbox UNUSED, /* "fmt" was set insecurely, use sandbox */
    int		fillchar,
    int		maxwidth,
    struct stl_hlrec *hltab,	/* return: HL attributes (can be NULL) */
    struct stl_hlrec *tabtab)	/* return: tab page nrs (can be NULL) */
{
    char_u	*p;
    char_u	*s;
    char_u	*t;
    int		byteval;
#ifdef FEAT_EVAL
    win_T	*save_curwin;
    buf_T	*save_curbuf;
#endif
    int		empty_line;
    colnr_T	virtcol;
    long	l;
    long	n;
    int		prevchar_isflag;
    int		prevchar_isitem;
    int		itemisflag;
    int		fillable;
    char_u	*str;
    long	num;
    int		width;
    int		itemcnt;
    int		curitem;
    int		group_end_userhl;
    int		group_start_userhl;
    int		groupitem[STL_MAX_ITEM];
    int		groupdepth;
    struct stl_item
    {
	char_u		*start;
	int		minwid;
	int		maxwid;
	enum
	{
	    Normal,
	    Empty,
	    Group,
	    Middle,
	    Highlight,
	    TabPage,
	    Trunc
	}		type;
    }		item[STL_MAX_ITEM];
    int		minwid;
    int		maxwid;
    int		zeropad;
    char_u	base;
    char_u	opt;
#define TMPLEN 70
    char_u	tmp[TMPLEN];
    char_u	*usefmt = fmt;
    struct stl_hlrec *sp;
    int		save_must_redraw = must_redraw;
    int		save_redr_type = curwin->w_redr_type;

#ifdef FEAT_EVAL
    /*
     * When the format starts with "%!" then evaluate it as an expression and
     * use the result as the actual format string.
     */
    if (fmt[0] == '%' && fmt[1] == '!')
    {
	usefmt = eval_to_string_safe(fmt + 2, NULL, use_sandbox);
	if (usefmt == NULL)
	    usefmt = fmt;
    }
#endif

    if (fillchar == 0)
	fillchar = ' ';
#ifdef FEAT_MBYTE
    /* Can't handle a multi-byte fill character yet. */
    else if (mb_char2len(fillchar) > 1)
	fillchar = '-';
#endif

    /* Get line & check if empty (cursorpos will show "0-1").  Note that
     * p will become invalid when getting another buffer line. */
    p = ml_get_buf(wp->w_buffer, wp->w_cursor.lnum, FALSE);
    empty_line = (*p == NUL);

    /* Get the byte value now, in case we need it below. This is more
     * efficient than making a copy of the line. */
    if (wp->w_cursor.col > (colnr_T)STRLEN(p))
	byteval = 0;
    else
#ifdef FEAT_MBYTE
	byteval = (*mb_ptr2char)(p + wp->w_cursor.col);
#else
	byteval = p[wp->w_cursor.col];
#endif

    groupdepth = 0;
    p = out;
    curitem = 0;
    prevchar_isflag = TRUE;
    prevchar_isitem = FALSE;
    for (s = usefmt; *s; )
    {
	if (curitem == STL_MAX_ITEM)
	{
	    /* There are too many items.  Add the error code to the statusline
	     * to give the user a hint about what went wrong. */
	    if (p + 6 < out + outlen)
	    {
		mch_memmove(p, " E541", (size_t)5);
		p += 5;
	    }
	    break;
	}

	if (*s != NUL && *s != '%')
	    prevchar_isflag = prevchar_isitem = FALSE;

	/*
	 * Handle up to the next '%' or the end.
	 */
	while (*s != NUL && *s != '%' && p + 1 < out + outlen)
	    *p++ = *s++;
	if (*s == NUL || p + 1 >= out + outlen)
	    break;

	/*
	 * Handle one '%' item.
	 */
	s++;
	if (*s == NUL)  /* ignore trailing % */
	    break;
	if (*s == '%')
	{
	    if (p + 1 >= out + outlen)
		break;
	    *p++ = *s++;
	    prevchar_isflag = prevchar_isitem = FALSE;
	    continue;
	}
	if (*s == STL_MIDDLEMARK)
	{
	    s++;
	    if (groupdepth > 0)
		continue;
	    item[curitem].type = Middle;
	    item[curitem++].start = p;
	    continue;
	}
	if (*s == STL_TRUNCMARK)
	{
	    s++;
	    item[curitem].type = Trunc;
	    item[curitem++].start = p;
	    continue;
	}
	if (*s == ')')
	{
	    s++;
	    if (groupdepth < 1)
		continue;
	    groupdepth--;

	    t = item[groupitem[groupdepth]].start;
	    *p = NUL;
	    l = vim_strsize(t);
	    if (curitem > groupitem[groupdepth] + 1
		    && item[groupitem[groupdepth]].minwid == 0)
	    {
		/* remove group if all items are empty and highlight group
		 * doesn't change */
		group_start_userhl = group_end_userhl = 0;
		for (n = groupitem[groupdepth] - 1; n >= 0; n--)
		{
		    if (item[n].type == Highlight)
		    {
			group_start_userhl = group_end_userhl = item[n].minwid;
			break;
		    }
		}
		for (n = groupitem[groupdepth] + 1; n < curitem; n++)
		{
		    if (item[n].type == Normal)
			break;
		    if (item[n].type == Highlight)
			group_end_userhl = item[n].minwid;
		}
		if (n == curitem && group_start_userhl == group_end_userhl)
		{
		    p = t;
		    l = 0;
		}
	    }
	    if (l > item[groupitem[groupdepth]].maxwid)
	    {
		/* truncate, remove n bytes of text at the start */
#ifdef FEAT_MBYTE
		if (has_mbyte)
		{
		    /* Find the first character that should be included. */
		    n = 0;
		    while (l >= item[groupitem[groupdepth]].maxwid)
		    {
			l -= ptr2cells(t + n);
			n += (*mb_ptr2len)(t + n);
		    }
		}
		else
#endif
		    n = (long)(p - t) - item[groupitem[groupdepth]].maxwid + 1;

		*t = '<';
		mch_memmove(t + 1, t + n, (size_t)(p - (t + n)));
		p = p - n + 1;
#ifdef FEAT_MBYTE
		/* Fill up space left over by half a double-wide char. */
		while (++l < item[groupitem[groupdepth]].minwid)
		    *p++ = fillchar;
#endif

		/* correct the start of the items for the truncation */
		for (l = groupitem[groupdepth] + 1; l < curitem; l++)
		{
		    item[l].start -= n;
		    if (item[l].start < t)
			item[l].start = t;
		}
	    }
	    else if (abs(item[groupitem[groupdepth]].minwid) > l)
	    {
		/* fill */
		n = item[groupitem[groupdepth]].minwid;
		if (n < 0)
		{
		    /* fill by appending characters */
		    n = 0 - n;
		    while (l++ < n && p + 1 < out + outlen)
			*p++ = fillchar;
		}
		else
		{
		    /* fill by inserting characters */
		    mch_memmove(t + n - l, t, (size_t)(p - t));
		    l = n - l;
		    if (p + l >= out + outlen)
			l = (long)((out + outlen) - p - 1);
		    p += l;
		    for (n = groupitem[groupdepth] + 1; n < curitem; n++)
			item[n].start += l;
		    for ( ; l > 0; l--)
			*t++ = fillchar;
		}
	    }
	    continue;
	}
	minwid = 0;
	maxwid = 9999;
	zeropad = FALSE;
	l = 1;
	if (*s == '0')
	{
	    s++;
	    zeropad = TRUE;
	}
	if (*s == '-')
	{
	    s++;
	    l = -1;
	}
	if (VIM_ISDIGIT(*s))
	{
	    minwid = (int)getdigits(&s);
	    if (minwid < 0)	/* overflow */
		minwid = 0;
	}
	if (*s == STL_USER_HL)
	{
	    item[curitem].type = Highlight;
	    item[curitem].start = p;
	    item[curitem].minwid = minwid > 9 ? 1 : minwid;
	    s++;
	    curitem++;
	    continue;
	}
	if (*s == STL_TABPAGENR || *s == STL_TABCLOSENR)
	{
	    if (*s == STL_TABCLOSENR)
	    {
		if (minwid == 0)
		{
		    /* %X ends the close label, go back to the previously
		     * define tab label nr. */
		    for (n = curitem - 1; n >= 0; --n)
			if (item[n].type == TabPage && item[n].minwid >= 0)
			{
			    minwid = item[n].minwid;
			    break;
			}
		}
		else
		    /* close nrs are stored as negative values */
		    minwid = - minwid;
	    }
	    item[curitem].type = TabPage;
	    item[curitem].start = p;
	    item[curitem].minwid = minwid;
	    s++;
	    curitem++;
	    continue;
	}
	if (*s == '.')
	{
	    s++;
	    if (VIM_ISDIGIT(*s))
	    {
		maxwid = (int)getdigits(&s);
		if (maxwid <= 0)	/* overflow */
		    maxwid = 50;
	    }
	}
	minwid = (minwid > 50 ? 50 : minwid) * l;
	if (*s == '(')
	{
	    groupitem[groupdepth++] = curitem;
	    item[curitem].type = Group;
	    item[curitem].start = p;
	    item[curitem].minwid = minwid;
	    item[curitem].maxwid = maxwid;
	    s++;
	    curitem++;
	    continue;
	}
	if (vim_strchr(STL_ALL, *s) == NULL)
	{
	    s++;
	    continue;
	}
	opt = *s++;

	/* OK - now for the real work */
	base = 'D';
	itemisflag = FALSE;
	fillable = TRUE;
	num = -1;
	str = NULL;
	switch (opt)
	{
	case STL_FILEPATH:
	case STL_FULLPATH:
	case STL_FILENAME:
	    fillable = FALSE;	/* don't change ' ' to fillchar */
	    if (buf_spname(wp->w_buffer) != NULL)
		vim_strncpy(NameBuff, buf_spname(wp->w_buffer), MAXPATHL - 1);
	    else
	    {
		t = (opt == STL_FULLPATH) ? wp->w_buffer->b_ffname
					  : wp->w_buffer->b_fname;
		home_replace(wp->w_buffer, t, NameBuff, MAXPATHL, TRUE);
	    }
	    trans_characters(NameBuff, MAXPATHL);
	    if (opt != STL_FILENAME)
		str = NameBuff;
	    else
		str = gettail(NameBuff);
	    break;

	case STL_VIM_EXPR: /* '{' */
	    itemisflag = TRUE;
	    t = p;
	    while (*s != '}' && *s != NUL && p + 1 < out + outlen)
		*p++ = *s++;
	    if (*s != '}')	/* missing '}' or out of space */
		break;
	    s++;
	    *p = 0;
	    p = t;

#ifdef FEAT_EVAL
	    vim_snprintf((char *)tmp, sizeof(tmp), "%d", curbuf->b_fnum);
	    set_internal_string_var((char_u *)"g:actual_curbuf", tmp);

	    save_curbuf = curbuf;
	    save_curwin = curwin;
	    curwin = wp;
	    curbuf = wp->w_buffer;

	    str = eval_to_string_safe(p, &t, use_sandbox);

	    curwin = save_curwin;
	    curbuf = save_curbuf;
	    do_unlet((char_u *)"g:actual_curbuf", TRUE);

	    if (str != NULL && *str != 0)
	    {
		if (*skipdigits(str) == NUL)
		{
		    num = atoi((char *)str);
		    VIM_CLEAR(str);
		    itemisflag = FALSE;
		}
	    }
#endif
	    break;

	case STL_LINE:
	    num = (wp->w_buffer->b_ml.ml_flags & ML_EMPTY)
		  ? 0L : (long)(wp->w_cursor.lnum);
	    break;

	case STL_NUMLINES:
	    num = wp->w_buffer->b_ml.ml_line_count;
	    break;

	case STL_COLUMN:
	    num = !(State & INSERT) && empty_line
		  ? 0 : (int)wp->w_cursor.col + 1;
	    break;

	case STL_VIRTCOL:
	case STL_VIRTCOL_ALT:
	    /* In list mode virtcol needs to be recomputed */
	    virtcol = wp->w_virtcol;
	    if (wp->w_p_list && lcs_tab1 == NUL)
	    {
		wp->w_p_list = FALSE;
		getvcol(wp, &wp->w_cursor, NULL, &virtcol, NULL);
		wp->w_p_list = TRUE;
	    }
	    ++virtcol;
	    /* Don't display %V if it's the same as %c. */
	    if (opt == STL_VIRTCOL_ALT
		    && (virtcol == (colnr_T)(!(State & INSERT) && empty_line
			    ? 0 : (int)wp->w_cursor.col + 1)))
		break;
	    num = (long)virtcol;
	    break;

	case STL_PERCENTAGE:
	    num = (int)(((long)wp->w_cursor.lnum * 100L) /
			(long)wp->w_buffer->b_ml.ml_line_count);
	    break;

	case STL_ALTPERCENT:
	    str = tmp;
	    get_rel_pos(wp, str, TMPLEN);
	    break;

	case STL_ARGLISTSTAT:
	    fillable = FALSE;
	    tmp[0] = 0;
	    if (append_arg_number(wp, tmp, (int)sizeof(tmp), FALSE))
		str = tmp;
	    break;

	case STL_KEYMAP:
	    fillable = FALSE;
	    if (get_keymap_str(wp, (char_u *)"<%s>", tmp, TMPLEN))
		str = tmp;
	    break;
	case STL_PAGENUM:
#if defined(FEAT_PRINTER) || defined(FEAT_GUI_TABLINE)
	    num = printer_page_num;
#else
	    num = 0;
#endif
	    break;

	case STL_BUFNO:
	    num = wp->w_buffer->b_fnum;
	    break;

	case STL_OFFSET_X:
	    base = 'X';
	    /* FALLTHROUGH */
	case STL_OFFSET:
#ifdef FEAT_BYTEOFF
	    l = ml_find_line_or_offset(wp->w_buffer, wp->w_cursor.lnum, NULL);
	    num = (wp->w_buffer->b_ml.ml_flags & ML_EMPTY) || l < 0 ?
		  0L : l + 1 + (!(State & INSERT) && empty_line ?
				0 : (int)wp->w_cursor.col);
#endif
	    break;

	case STL_BYTEVAL_X:
	    base = 'X';
	    /* FALLTHROUGH */
	case STL_BYTEVAL:
	    num = byteval;
	    if (num == NL)
		num = 0;
	    else if (num == CAR && get_fileformat(wp->w_buffer) == EOL_MAC)
		num = NL;
	    break;

	case STL_ROFLAG:
	case STL_ROFLAG_ALT:
	    itemisflag = TRUE;
	    if (wp->w_buffer->b_p_ro)
		str = (char_u *)((opt == STL_ROFLAG_ALT) ? ",RO" : _("[RO]"));
	    break;

	case STL_HELPFLAG:
	case STL_HELPFLAG_ALT:
	    itemisflag = TRUE;
	    if (wp->w_buffer->b_help)
		str = (char_u *)((opt == STL_HELPFLAG_ALT) ? ",HLP"
							       : _("[Help]"));
	    break;

	case STL_FILETYPE:
	    if (*wp->w_buffer->b_p_ft != NUL
		    && STRLEN(wp->w_buffer->b_p_ft) < TMPLEN - 3)
	    {
		vim_snprintf((char *)tmp, sizeof(tmp), "[%s]",
							wp->w_buffer->b_p_ft);
		str = tmp;
	    }
	    break;

	case STL_FILETYPE_ALT:
	    itemisflag = TRUE;
	    if (*wp->w_buffer->b_p_ft != NUL
		    && STRLEN(wp->w_buffer->b_p_ft) < TMPLEN - 2)
	    {
		vim_snprintf((char *)tmp, sizeof(tmp), ",%s",
							wp->w_buffer->b_p_ft);
		for (t = tmp; *t != 0; t++)
		    *t = TOUPPER_LOC(*t);
		str = tmp;
	    }
	    break;

#if defined(FEAT_QUICKFIX)
	case STL_PREVIEWFLAG:
	case STL_PREVIEWFLAG_ALT:
	    itemisflag = TRUE;
	    if (wp->w_p_pvw)
		str = (char_u *)((opt == STL_PREVIEWFLAG_ALT) ? ",PRV"
							    : _("[Preview]"));
	    break;

	case STL_QUICKFIX:
	    if (bt_quickfix(wp->w_buffer))
		str = (char_u *)(wp->w_llist_ref
			    ? _(msg_loclist)
			    : _(msg_qflist));
	    break;
#endif

	case STL_MODIFIED:
	case STL_MODIFIED_ALT:
	    itemisflag = TRUE;
	    switch ((opt == STL_MODIFIED_ALT)
		    + bufIsChanged(wp->w_buffer) * 2
		    + (!wp->w_buffer->b_p_ma) * 4)
	    {
		case 2: str = (char_u *)"[+]"; break;
		case 3: str = (char_u *)",+"; break;
		case 4: str = (char_u *)"[-]"; break;
		case 5: str = (char_u *)",-"; break;
		case 6: str = (char_u *)"[+-]"; break;
		case 7: str = (char_u *)",+-"; break;
	    }
	    break;

	case STL_HIGHLIGHT:
	    t = s;
	    while (*s != '#' && *s != NUL)
		++s;
	    if (*s == '#')
	    {
		item[curitem].type = Highlight;
		item[curitem].start = p;
		item[curitem].minwid = -syn_namen2id(t, (int)(s - t));
		curitem++;
	    }
	    if (*s != NUL)
		++s;
	    continue;
	}

	item[curitem].start = p;
	item[curitem].type = Normal;
	if (str != NULL && *str)
	{
	    t = str;
	    if (itemisflag)
	    {
		if ((t[0] && t[1])
			&& ((!prevchar_isitem && *t == ',')
			      || (prevchar_isflag && *t == ' ')))
		    t++;
		prevchar_isflag = TRUE;
	    }
	    l = vim_strsize(t);
	    if (l > 0)
		prevchar_isitem = TRUE;
	    if (l > maxwid)
	    {
		while (l >= maxwid)
#ifdef FEAT_MBYTE
		    if (has_mbyte)
		    {
			l -= ptr2cells(t);
			t += (*mb_ptr2len)(t);
		    }
		    else
#endif
			l -= byte2cells(*t++);
		if (p + 1 >= out + outlen)
		    break;
		*p++ = '<';
	    }
	    if (minwid > 0)
	    {
		for (; l < minwid && p + 1 < out + outlen; l++)
		{
		    /* Don't put a "-" in front of a digit. */
		    if (l + 1 == minwid && fillchar == '-' && VIM_ISDIGIT(*t))
			*p++ = ' ';
		    else
			*p++ = fillchar;
		}
		minwid = 0;
	    }
	    else
		minwid *= -1;
	    while (*t && p + 1 < out + outlen)
	    {
		*p++ = *t++;
		/* Change a space by fillchar, unless fillchar is '-' and a
		 * digit follows. */
		if (fillable && p[-1] == ' '
				     && (!VIM_ISDIGIT(*t) || fillchar != '-'))
		    p[-1] = fillchar;
	    }
	    for (; l < minwid && p + 1 < out + outlen; l++)
		*p++ = fillchar;
	}
	else if (num >= 0)
	{
	    int nbase = (base == 'D' ? 10 : (base == 'O' ? 8 : 16));
	    char_u nstr[20];

	    if (p + 20 >= out + outlen)
		break;		/* not sufficient space */
	    prevchar_isitem = TRUE;
	    t = nstr;
	    if (opt == STL_VIRTCOL_ALT)
	    {
		*t++ = '-';
		minwid--;
	    }
	    *t++ = '%';
	    if (zeropad)
		*t++ = '0';
	    *t++ = '*';
	    *t++ = nbase == 16 ? base : (char_u)(nbase == 8 ? 'o' : 'd');
	    *t = 0;

	    for (n = num, l = 1; n >= nbase; n /= nbase)
		l++;
	    if (opt == STL_VIRTCOL_ALT)
		l++;
	    if (l > maxwid)
	    {
		l += 2;
		n = l - maxwid;
		while (l-- > maxwid)
		    num /= nbase;
		*t++ = '>';
		*t++ = '%';
		*t = t[-3];
		*++t = 0;
		vim_snprintf((char *)p, outlen - (p - out), (char *)nstr,
								   0, num, n);
	    }
	    else
		vim_snprintf((char *)p, outlen - (p - out), (char *)nstr,
								 minwid, num);
	    p += STRLEN(p);
	}
	else
	    item[curitem].type = Empty;

	if (opt == STL_VIM_EXPR)
	    vim_free(str);

	if (num >= 0 || (!itemisflag && str && *str))
	    prevchar_isflag = FALSE;	    /* Item not NULL, but not a flag */
	curitem++;
    }
    *p = NUL;
    itemcnt = curitem;

#ifdef FEAT_EVAL
    if (usefmt != fmt)
	vim_free(usefmt);
#endif

    width = vim_strsize(out);
    if (maxwidth > 0 && width > maxwidth)
    {
	/* Result is too long, must truncate somewhere. */
	l = 0;
	if (itemcnt == 0)
	    s = out;
	else
	{
	    for ( ; l < itemcnt; l++)
		if (item[l].type == Trunc)
		{
		    /* Truncate at %< item. */
		    s = item[l].start;
		    break;
		}
	    if (l == itemcnt)
	    {
		/* No %< item, truncate first item. */
		s = item[0].start;
		l = 0;
	    }
	}

	if (width - vim_strsize(s) >= maxwidth)
	{
	    /* Truncation mark is beyond max length */
#ifdef FEAT_MBYTE
	    if (has_mbyte)
	    {
		s = out;
		width = 0;
		for (;;)
		{
		    width += ptr2cells(s);
		    if (width >= maxwidth)
			break;
		    s += (*mb_ptr2len)(s);
		}
		/* Fill up for half a double-wide character. */
		while (++width < maxwidth)
		    *s++ = fillchar;
	    }
	    else
#endif
		s = out + maxwidth - 1;
	    for (l = 0; l < itemcnt; l++)
		if (item[l].start > s)
		    break;
	    itemcnt = l;
	    *s++ = '>';
	    *s = 0;
	}
	else
	{
#ifdef FEAT_MBYTE
	    if (has_mbyte)
	    {
		n = 0;
		while (width >= maxwidth)
		{
		    width -= ptr2cells(s + n);
		    n += (*mb_ptr2len)(s + n);
		}
	    }
	    else
#endif
		n = width - maxwidth + 1;
	    p = s + n;
	    STRMOVE(s + 1, p);
	    *s = '<';

	    /* Fill up for half a double-wide character. */
	    while (++width < maxwidth)
	    {
		s = s + STRLEN(s);
		*s++ = fillchar;
		*s = NUL;
	    }

	    --n;	/* count the '<' */
	    for (; l < itemcnt; l++)
	    {
		if (item[l].start - n >= s)
		    item[l].start -= n;
		else
		    item[l].start = s;
	    }
	}
	width = maxwidth;
    }
    else if (width < maxwidth && STRLEN(out) + maxwidth - width + 1 < outlen)
    {
	/* Apply STL_MIDDLE if any */
	for (l = 0; l < itemcnt; l++)
	    if (item[l].type == Middle)
		break;
	if (l < itemcnt)
	{
	    p = item[l].start + maxwidth - width;
	    STRMOVE(p, item[l].start);
	    for (s = item[l].start; s < p; s++)
		*s = fillchar;
	    for (l++; l < itemcnt; l++)
		item[l].start += maxwidth - width;
	    width = maxwidth;
	}
    }

    /* Store the info about highlighting. */
    if (hltab != NULL)
    {
	sp = hltab;
	for (l = 0; l < itemcnt; l++)
	{
	    if (item[l].type == Highlight)
	    {
		sp->start = item[l].start;
		sp->userhl = item[l].minwid;
		sp++;
	    }
	}
	sp->start = NULL;
	sp->userhl = 0;
    }

    /* Store the info about tab pages labels. */
    if (tabtab != NULL)
    {
	sp = tabtab;
	for (l = 0; l < itemcnt; l++)
	{
	    if (item[l].type == TabPage)
	    {
		sp->start = item[l].start;
		sp->userhl = item[l].minwid;
		sp++;
	    }
	}
	sp->start = NULL;
	sp->userhl = 0;
    }

    /* When inside update_screen we do not want redrawing a stausline, ruler,
     * title, etc. to trigger another redraw, it may cause an endless loop. */
    if (updating_screen)
    {
	must_redraw = save_must_redraw;
	curwin->w_redr_type = save_redr_type;
    }

    return width;
}
#endif /* FEAT_STL_OPT */

#if defined(FEAT_STL_OPT) || defined(FEAT_CMDL_INFO) \
	    || defined(FEAT_GUI_TABLINE) || defined(PROTO)
/*
 * Get relative cursor position in window into "buf[buflen]", in the form 99%,
 * using "Top", "Bot" or "All" when appropriate.
 */
    void
get_rel_pos(
    win_T	*wp,
    char_u	*buf,
    int		buflen)
{
    long	above; /* number of lines above window */
    long	below; /* number of lines below window */

    if (buflen < 3) /* need at least 3 chars for writing */
	return;
    above = wp->w_topline - 1;
#ifdef FEAT_DIFF
    above += diff_check_fill(wp, wp->w_topline) - wp->w_topfill;
    if (wp->w_topline == 1 && wp->w_topfill >= 1)
	above = 0;  /* All buffer lines are displayed and there is an
		     * indication of filler lines, that can be considered
		     * seeing all lines. */
#endif
    below = wp->w_buffer->b_ml.ml_line_count - wp->w_botline + 1;
    if (below <= 0)
	vim_strncpy(buf, (char_u *)(above == 0 ? _("All") : _("Bot")),
							(size_t)(buflen - 1));
    else if (above <= 0)
	vim_strncpy(buf, (char_u *)_("Top"), (size_t)(buflen - 1));
    else
	vim_snprintf((char *)buf, (size_t)buflen, "%2d%%", above > 1000000L
				    ? (int)(above / ((above + below) / 100L))
				    : (int)(above * 100L / (above + below)));
}
#endif

/*
 * Append (file 2 of 8) to "buf[buflen]", if editing more than one file.
 * Return TRUE if it was appended.
 */
    static int
append_arg_number(
    win_T	*wp,
    char_u	*buf,
    int		buflen,
    int		add_file)	/* Add "file" before the arg number */
{
    char_u	*p;

    if (ARGCOUNT <= 1)		/* nothing to do */
	return FALSE;

    p = buf + STRLEN(buf);	/* go to the end of the buffer */
    if (p - buf + 35 >= buflen)	/* getting too long */
	return FALSE;
    *p++ = ' ';
    *p++ = '(';
    if (add_file)
    {
	STRCPY(p, "file ");
	p += 5;
    }
    vim_snprintf((char *)p, (size_t)(buflen - (p - buf)),
		wp->w_arg_idx_invalid ? "(%d) of %d)"
				  : "%d of %d)", wp->w_arg_idx + 1, ARGCOUNT);
    return TRUE;
}

/*
 * If fname is not a full path, make it a full path.
 * Returns pointer to allocated memory (NULL for failure).
 */
    char_u  *
fix_fname(char_u  *fname)
{
    /*
     * Force expanding the path always for Unix, because symbolic links may
     * mess up the full path name, even though it starts with a '/'.
     * Also expand when there is ".." in the file name, try to remove it,
     * because "c:/src/../README" is equal to "c:/README".
     * Similarly "c:/src//file" is equal to "c:/src/file".
     * For MS-Windows also expand names like "longna~1" to "longname".
     */
#ifdef UNIX
    return FullName_save(fname, TRUE);
#else
    if (!vim_isAbsName(fname)
	    || strstr((char *)fname, "..") != NULL
	    || strstr((char *)fname, "//") != NULL
# ifdef BACKSLASH_IN_FILENAME
	    || strstr((char *)fname, "\\\\") != NULL
# endif
# if defined(MSWIN)
	    || vim_strchr(fname, '~') != NULL
# endif
	    )
	return FullName_save(fname, FALSE);

    fname = vim_strsave(fname);

# ifdef USE_FNAME_CASE
#  ifdef USE_LONG_FNAME
    if (USE_LONG_FNAME)
#  endif
    {
	if (fname != NULL)
	    fname_case(fname, 0);	/* set correct case for file name */
    }
# endif

    return fname;
#endif
}

/*
 * Make "*ffname" a full file name, set "*sfname" to "*ffname" if not NULL.
 * "*ffname" becomes a pointer to allocated memory (or NULL).
 * When resolving a link both "*sfname" and "*ffname" will point to the same
 * allocated memory.
 * The "*ffname" and "*sfname" pointer values on call will not be freed.
 * Note that the resulting "*ffname" pointer should be considered not allocaed.
 */
    void
fname_expand(
    buf_T	*buf UNUSED,
    char_u	**ffname,
    char_u	**sfname)
{
    if (*ffname == NULL)	    // no file name given, nothing to do
	return;
    if (*sfname == NULL)	    // no short file name given, use ffname
	*sfname = *ffname;
    *ffname = fix_fname(*ffname);   // expand to full path

#ifdef FEAT_SHORTCUT
    if (!buf->b_p_bin)
    {
	char_u  *rfname;

	// If the file name is a shortcut file, use the file it links to.
	rfname = mch_resolve_shortcut(*ffname);
	if (rfname != NULL)
	{
	    vim_free(*ffname);
	    *ffname = rfname;
	    *sfname = rfname;
	}
    }
#endif
}

/*
 * Get the file name for an argument list entry.
 */
    char_u *
alist_name(aentry_T *aep)
{
    buf_T	*bp;

    /* Use the name from the associated buffer if it exists. */
    bp = buflist_findnr(aep->ae_fnum);
    if (bp == NULL || bp->b_fname == NULL)
	return aep->ae_fname;
    return bp->b_fname;
}

/*
 * do_arg_all(): Open up to 'count' windows, one for each argument.
 */
    void
do_arg_all(
    int	count,
    int	forceit,		/* hide buffers in current windows */
    int keep_tabs)		/* keep current tabs, for ":tab drop file" */
{
    int		i;
    win_T	*wp, *wpnext;
    char_u	*opened;	/* Array of weight for which args are open:
				 *  0: not opened
				 *  1: opened in other tab
				 *  2: opened in curtab
				 *  3: opened in curtab and curwin
				 */
    int		opened_len;	/* length of opened[] */
    int		use_firstwin = FALSE;	/* use first window for arglist */
    int		split_ret = OK;
    int		p_ea_save;
    alist_T	*alist;		/* argument list to be used */
    buf_T	*buf;
    tabpage_T	*tpnext;
    int		had_tab = cmdmod.tab;
    win_T	*old_curwin, *last_curwin;
    tabpage_T	*old_curtab, *last_curtab;
    win_T	*new_curwin = NULL;
    tabpage_T	*new_curtab = NULL;

    if (ARGCOUNT <= 0)
    {
	/* Don't give an error message.  We don't want it when the ":all"
	 * command is in the .vimrc. */
	return;
    }
    setpcmark();

    opened_len = ARGCOUNT;
    opened = alloc_clear((unsigned)opened_len);
    if (opened == NULL)
	return;

    /* Autocommands may do anything to the argument list.  Make sure it's not
     * freed while we are working here by "locking" it.  We still have to
     * watch out for its size to be changed. */
    alist = curwin->w_alist;
    ++alist->al_refcount;

    old_curwin = curwin;
    old_curtab = curtab;

# ifdef FEAT_GUI
    need_mouse_correct = TRUE;
# endif

    /*
     * Try closing all windows that are not in the argument list.
     * Also close windows that are not full width;
     * When 'hidden' or "forceit" set the buffer becomes hidden.
     * Windows that have a changed buffer and can't be hidden won't be closed.
     * When the ":tab" modifier was used do this for all tab pages.
     */
    if (had_tab > 0)
	goto_tabpage_tp(first_tabpage, TRUE, TRUE);
    for (;;)
    {
	tpnext = curtab->tp_next;
	for (wp = firstwin; wp != NULL; wp = wpnext)
	{
	    wpnext = wp->w_next;
	    buf = wp->w_buffer;
	    if (buf->b_ffname == NULL
		    || (!keep_tabs && (buf->b_nwindows > 1
			    || wp->w_width != Columns)))
		i = opened_len;
	    else
	    {
		/* check if the buffer in this window is in the arglist */
		for (i = 0; i < opened_len; ++i)
		{
		    if (i < alist->al_ga.ga_len
			    && (AARGLIST(alist)[i].ae_fnum == buf->b_fnum
				|| fullpathcmp(alist_name(&AARGLIST(alist)[i]),
					      buf->b_ffname, TRUE) & FPC_SAME))
		    {
			int weight = 1;

			if (old_curtab == curtab)
			{
			    ++weight;
			    if (old_curwin == wp)
				++weight;
			}

			if (weight > (int)opened[i])
			{
			    opened[i] = (char_u)weight;
			    if (i == 0)
			    {
				if (new_curwin != NULL)
				    new_curwin->w_arg_idx = opened_len;
				new_curwin = wp;
				new_curtab = curtab;
			    }
			}
			else if (keep_tabs)
			    i = opened_len;

			if (wp->w_alist != alist)
			{
			    /* Use the current argument list for all windows
			     * containing a file from it. */
			    alist_unlink(wp->w_alist);
			    wp->w_alist = alist;
			    ++wp->w_alist->al_refcount;
			}
			break;
		    }
		}
	    }
	    wp->w_arg_idx = i;

	    if (i == opened_len && !keep_tabs)/* close this window */
	    {
		if (buf_hide(buf) || forceit || buf->b_nwindows > 1
							|| !bufIsChanged(buf))
		{
		    /* If the buffer was changed, and we would like to hide it,
		     * try autowriting. */
		    if (!buf_hide(buf) && buf->b_nwindows <= 1
							 && bufIsChanged(buf))
		    {
			bufref_T    bufref;

			set_bufref(&bufref, buf);

			(void)autowrite(buf, FALSE);

			/* check if autocommands removed the window */
			if (!win_valid(wp) || !bufref_valid(&bufref))
			{
			    wpnext = firstwin;	/* start all over... */
			    continue;
			}
		    }
		    /* don't close last window */
		    if (ONE_WINDOW
			    && (first_tabpage->tp_next == NULL || !had_tab))
			use_firstwin = TRUE;
		    else
		    {
			win_close(wp, !buf_hide(buf) && !bufIsChanged(buf));

			/* check if autocommands removed the next window */
			if (!win_valid(wpnext))
			    wpnext = firstwin;	/* start all over... */
		    }
		}
	    }
	}

	/* Without the ":tab" modifier only do the current tab page. */
	if (had_tab == 0 || tpnext == NULL)
	    break;

	/* check if autocommands removed the next tab page */
	if (!valid_tabpage(tpnext))
	    tpnext = first_tabpage;	/* start all over...*/

	goto_tabpage_tp(tpnext, TRUE, TRUE);
    }

    /*
     * Open a window for files in the argument list that don't have one.
     * ARGCOUNT may change while doing this, because of autocommands.
     */
    if (count > opened_len || count <= 0)
	count = opened_len;

    /* Don't execute Win/Buf Enter/Leave autocommands here. */
    ++autocmd_no_enter;
    ++autocmd_no_leave;
    last_curwin = curwin;
    last_curtab = curtab;
    win_enter(lastwin, FALSE);
    /* ":drop all" should re-use an empty window to avoid "--remote-tab"
     * leaving an empty tab page when executed locally. */
    if (keep_tabs && BUFEMPTY() && curbuf->b_nwindows == 1
			    && curbuf->b_ffname == NULL && !curbuf->b_changed)
	use_firstwin = TRUE;

    for (i = 0; i < count && i < opened_len && !got_int; ++i)
    {
	if (alist == &global_alist && i == global_alist.al_ga.ga_len - 1)
	    arg_had_last = TRUE;
	if (opened[i] > 0)
	{
	    /* Move the already present window to below the current window */
	    if (curwin->w_arg_idx != i)
	    {
		for (wpnext = firstwin; wpnext != NULL; wpnext = wpnext->w_next)
		{
		    if (wpnext->w_arg_idx == i)
		    {
			if (keep_tabs)
			{
			    new_curwin = wpnext;
			    new_curtab = curtab;
			}
			else
			    win_move_after(wpnext, curwin);
			break;
		    }
		}
	    }
	}
	else if (split_ret == OK)
	{
	    if (!use_firstwin)		/* split current window */
	    {
		p_ea_save = p_ea;
		p_ea = TRUE;		/* use space from all windows */
		split_ret = win_split(0, WSP_ROOM | WSP_BELOW);
		p_ea = p_ea_save;
		if (split_ret == FAIL)
		    continue;
	    }
	    else    /* first window: do autocmd for leaving this buffer */
		--autocmd_no_leave;

	    /*
	     * edit file "i"
	     */
	    curwin->w_arg_idx = i;
	    if (i == 0)
	    {
		new_curwin = curwin;
		new_curtab = curtab;
	    }
	    (void)do_ecmd(0, alist_name(&AARGLIST(alist)[i]), NULL, NULL,
		      ECMD_ONE,
		      ((buf_hide(curwin->w_buffer)
			   || bufIsChanged(curwin->w_buffer)) ? ECMD_HIDE : 0)
						       + ECMD_OLDBUF, curwin);
	    if (use_firstwin)
		++autocmd_no_leave;
	    use_firstwin = FALSE;
	}
	ui_breakcheck();

	/* When ":tab" was used open a new tab for a new window repeatedly. */
	if (had_tab > 0 && tabpage_index(NULL) <= p_tpm)
	    cmdmod.tab = 9999;
    }

    /* Remove the "lock" on the argument list. */
    alist_unlink(alist);

    --autocmd_no_enter;

    /* restore last referenced tabpage's curwin */
    if (last_curtab != new_curtab)
    {
	if (valid_tabpage(last_curtab))
	    goto_tabpage_tp(last_curtab, TRUE, TRUE);
	if (win_valid(last_curwin))
	    win_enter(last_curwin, FALSE);
    }
    /* to window with first arg */
    if (valid_tabpage(new_curtab))
	goto_tabpage_tp(new_curtab, TRUE, TRUE);
    if (win_valid(new_curwin))
	win_enter(new_curwin, FALSE);

    --autocmd_no_leave;
    vim_free(opened);
}

/*
 * Open a window for a number of buffers.
 */
    void
ex_buffer_all(exarg_T *eap)
{
    buf_T	*buf;
    win_T	*wp, *wpnext;
    int		split_ret = OK;
    int		p_ea_save;
    int		open_wins = 0;
    int		r;
    int		count;		/* Maximum number of windows to open. */
    int		all;		/* When TRUE also load inactive buffers. */
    int		had_tab = cmdmod.tab;
    tabpage_T	*tpnext;

    if (eap->addr_count == 0)	/* make as many windows as possible */
	count = 9999;
    else
	count = eap->line2;	/* make as many windows as specified */
    if (eap->cmdidx == CMD_unhide || eap->cmdidx == CMD_sunhide)
	all = FALSE;
    else
	all = TRUE;

    setpcmark();

#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif

    /*
     * Close superfluous windows (two windows for the same buffer).
     * Also close windows that are not full-width.
     */
    if (had_tab > 0)
	goto_tabpage_tp(first_tabpage, TRUE, TRUE);
    for (;;)
    {
	tpnext = curtab->tp_next;
	for (wp = firstwin; wp != NULL; wp = wpnext)
	{
	    wpnext = wp->w_next;
	    if ((wp->w_buffer->b_nwindows > 1
		    || ((cmdmod.split & WSP_VERT)
			? wp->w_height + wp->w_status_height < Rows - p_ch
							    - tabline_height()
			: wp->w_width != Columns)
		    || (had_tab > 0 && wp != firstwin)) && !ONE_WINDOW
			     && !(wp->w_closing || wp->w_buffer->b_locked > 0))
	    {
		win_close(wp, FALSE);
		wpnext = firstwin;	/* just in case an autocommand does
					   something strange with windows */
		tpnext = first_tabpage;	/* start all over... */
		open_wins = 0;
	    }
	    else
		++open_wins;
	}

	/* Without the ":tab" modifier only do the current tab page. */
	if (had_tab == 0 || tpnext == NULL)
	    break;
	goto_tabpage_tp(tpnext, TRUE, TRUE);
    }

    /*
     * Go through the buffer list.  When a buffer doesn't have a window yet,
     * open one.  Otherwise move the window to the right position.
     * Watch out for autocommands that delete buffers or windows!
     */
    /* Don't execute Win/Buf Enter/Leave autocommands here. */
    ++autocmd_no_enter;
    win_enter(lastwin, FALSE);
    ++autocmd_no_leave;
    for (buf = firstbuf; buf != NULL && open_wins < count; buf = buf->b_next)
    {
	/* Check if this buffer needs a window */
	if ((!all && buf->b_ml.ml_mfp == NULL) || !buf->b_p_bl)
	    continue;

	if (had_tab != 0)
	{
	    /* With the ":tab" modifier don't move the window. */
	    if (buf->b_nwindows > 0)
		wp = lastwin;	    /* buffer has a window, skip it */
	    else
		wp = NULL;
	}
	else
	{
	    /* Check if this buffer already has a window */
	    FOR_ALL_WINDOWS(wp)
		if (wp->w_buffer == buf)
		    break;
	    /* If the buffer already has a window, move it */
	    if (wp != NULL)
		win_move_after(wp, curwin);
	}

	if (wp == NULL && split_ret == OK)
	{
	    bufref_T	bufref;

	    set_bufref(&bufref, buf);

	    /* Split the window and put the buffer in it */
	    p_ea_save = p_ea;
	    p_ea = TRUE;		/* use space from all windows */
	    split_ret = win_split(0, WSP_ROOM | WSP_BELOW);
	    ++open_wins;
	    p_ea = p_ea_save;
	    if (split_ret == FAIL)
		continue;

	    /* Open the buffer in this window. */
#if defined(HAS_SWAP_EXISTS_ACTION)
	    swap_exists_action = SEA_DIALOG;
#endif
	    set_curbuf(buf, DOBUF_GOTO);
	    if (!bufref_valid(&bufref))
	    {
		/* autocommands deleted the buffer!!! */
#if defined(HAS_SWAP_EXISTS_ACTION)
		swap_exists_action = SEA_NONE;
#endif
		break;
	    }
#if defined(HAS_SWAP_EXISTS_ACTION)
	    if (swap_exists_action == SEA_QUIT)
	    {
# if defined(FEAT_EVAL)
		cleanup_T   cs;

		/* Reset the error/interrupt/exception state here so that
		 * aborting() returns FALSE when closing a window. */
		enter_cleanup(&cs);
# endif

		/* User selected Quit at ATTENTION prompt; close this window. */
		win_close(curwin, TRUE);
		--open_wins;
		swap_exists_action = SEA_NONE;
		swap_exists_did_quit = TRUE;

# if defined(FEAT_EVAL)
		/* Restore the error/interrupt/exception state if not
		 * discarded by a new aborting error, interrupt, or uncaught
		 * exception. */
		leave_cleanup(&cs);
# endif
	    }
	    else
		handle_swap_exists(NULL);
#endif
	}

	ui_breakcheck();
	if (got_int)
	{
	    (void)vgetc();	/* only break the file loading, not the rest */
	    break;
	}
#ifdef FEAT_EVAL
	/* Autocommands deleted the buffer or aborted script processing!!! */
	if (aborting())
	    break;
#endif
	/* When ":tab" was used open a new tab for a new window repeatedly. */
	if (had_tab > 0 && tabpage_index(NULL) <= p_tpm)
	    cmdmod.tab = 9999;
    }
    --autocmd_no_enter;
    win_enter(firstwin, FALSE);		/* back to first window */
    --autocmd_no_leave;

    /*
     * Close superfluous windows.
     */
    for (wp = lastwin; open_wins > count; )
    {
	r = (buf_hide(wp->w_buffer) || !bufIsChanged(wp->w_buffer)
				     || autowrite(wp->w_buffer, FALSE) == OK);
	if (!win_valid(wp))
	{
	    /* BufWrite Autocommands made the window invalid, start over */
	    wp = lastwin;
	}
	else if (r)
	{
	    win_close(wp, !buf_hide(wp->w_buffer));
	    --open_wins;
	    wp = lastwin;
	}
	else
	{
	    wp = wp->w_prev;
	    if (wp == NULL)
		break;
	}
    }
}


static int  chk_modeline(linenr_T, int);

/*
 * do_modelines() - process mode lines for the current file
 *
 * "flags" can be:
 * OPT_WINONLY	    only set options local to window
 * OPT_NOWIN	    don't set options local to window
 *
 * Returns immediately if the "ml" option isn't set.
 */
    void
do_modelines(int flags)
{
    linenr_T	lnum;
    int		nmlines;
    static int	entered = 0;

    if (!curbuf->b_p_ml || (nmlines = (int)p_mls) == 0)
	return;

    /* Disallow recursive entry here.  Can happen when executing a modeline
     * triggers an autocommand, which reloads modelines with a ":do". */
    if (entered)
	return;

    ++entered;
    for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count && lnum <= nmlines;
								       ++lnum)
	if (chk_modeline(lnum, flags) == FAIL)
	    nmlines = 0;

    for (lnum = curbuf->b_ml.ml_line_count; lnum > 0 && lnum > nmlines
		       && lnum > curbuf->b_ml.ml_line_count - nmlines; --lnum)
	if (chk_modeline(lnum, flags) == FAIL)
	    nmlines = 0;
    --entered;
}

#include "version.h"		/* for version number */

/*
 * chk_modeline() - check a single line for a mode string
 * Return FAIL if an error encountered.
 */
    static int
chk_modeline(
    linenr_T	lnum,
    int		flags)		/* Same as for do_modelines(). */
{
    char_u	*s;
    char_u	*e;
    char_u	*linecopy;		/* local copy of any modeline found */
    int		prev;
    int		vers;
    int		end;
    int		retval = OK;
    char_u	*save_sourcing_name;
    linenr_T	save_sourcing_lnum;
#ifdef FEAT_EVAL
    sctx_T	save_current_sctx;
#endif

    prev = -1;
    for (s = ml_get(lnum); *s != NUL; ++s)
    {
	if (prev == -1 || vim_isspace(prev))
	{
	    if ((prev != -1 && STRNCMP(s, "ex:", (size_t)3) == 0)
		    || STRNCMP(s, "vi:", (size_t)3) == 0)
		break;
	    /* Accept both "vim" and "Vim". */
	    if ((s[0] == 'v' || s[0] == 'V') && s[1] == 'i' && s[2] == 'm')
	    {
		if (s[3] == '<' || s[3] == '=' || s[3] == '>')
		    e = s + 4;
		else
		    e = s + 3;
		vers = getdigits(&e);
		if (*e == ':'
			&& (s[0] != 'V'
				  || STRNCMP(skipwhite(e + 1), "set", 3) == 0)
			&& (s[3] == ':'
			    || (VIM_VERSION_100 >= vers && isdigit(s[3]))
			    || (VIM_VERSION_100 < vers && s[3] == '<')
			    || (VIM_VERSION_100 > vers && s[3] == '>')
			    || (VIM_VERSION_100 == vers && s[3] == '=')))
		    break;
	    }
	}
	prev = *s;
    }

    if (*s)
    {
	do				/* skip over "ex:", "vi:" or "vim:" */
	    ++s;
	while (s[-1] != ':');

	s = linecopy = vim_strsave(s);	/* copy the line, it will change */
	if (linecopy == NULL)
	    return FAIL;

	save_sourcing_lnum = sourcing_lnum;
	save_sourcing_name = sourcing_name;
	sourcing_lnum = lnum;		/* prepare for emsg() */
	sourcing_name = (char_u *)"modelines";

	end = FALSE;
	while (end == FALSE)
	{
	    s = skipwhite(s);
	    if (*s == NUL)
		break;

	    /*
	     * Find end of set command: ':' or end of line.
	     * Skip over "\:", replacing it with ":".
	     */
	    for (e = s; *e != ':' && *e != NUL; ++e)
		if (e[0] == '\\' && e[1] == ':')
		    STRMOVE(e, e + 1);
	    if (*e == NUL)
		end = TRUE;

	    /*
	     * If there is a "set" command, require a terminating ':' and
	     * ignore the stuff after the ':'.
	     * "vi:set opt opt opt: foo" -- foo not interpreted
	     * "vi:opt opt opt: foo" -- foo interpreted
	     * Accept "se" for compatibility with Elvis.
	     */
	    if (STRNCMP(s, "set ", (size_t)4) == 0
		    || STRNCMP(s, "se ", (size_t)3) == 0)
	    {
		if (*e != ':')		/* no terminating ':'? */
		    break;
		end = TRUE;
		s = vim_strchr(s, ' ') + 1;
	    }
	    *e = NUL;			/* truncate the set command */

	    if (*s != NUL)		/* skip over an empty "::" */
	    {
		int secure_save = secure;
#ifdef FEAT_EVAL
		save_current_sctx = current_sctx;
		current_sctx.sc_sid = SID_MODELINE;
		current_sctx.sc_seq = 0;
		current_sctx.sc_lnum = 0;
#endif
		// Make sure no risky things are executed as a side effect.
		++secure;

		retval = do_set(s, OPT_MODELINE | OPT_LOCAL | flags);

		secure = secure_save;
#ifdef FEAT_EVAL
		current_sctx = save_current_sctx;
#endif
		if (retval == FAIL)		/* stop if error found */
		    break;
	    }
	    s = e + 1;			/* advance to next part */
	}

	sourcing_lnum = save_sourcing_lnum;
	sourcing_name = save_sourcing_name;

	vim_free(linecopy);
    }
    return retval;
}

#if defined(FEAT_VIMINFO) || defined(PROTO)
    int
read_viminfo_bufferlist(
    vir_T	*virp,
    int		writing)
{
    char_u	*tab;
    linenr_T	lnum;
    colnr_T	col;
    buf_T	*buf;
    char_u	*sfname;
    char_u	*xline;

    /* Handle long line and escaped characters. */
    xline = viminfo_readstring(virp, 1, FALSE);

    /* don't read in if there are files on the command-line or if writing: */
    if (xline != NULL && !writing && ARGCOUNT == 0
				       && find_viminfo_parameter('%') != NULL)
    {
	/* Format is: <fname> Tab <lnum> Tab <col>.
	 * Watch out for a Tab in the file name, work from the end. */
	lnum = 0;
	col = 0;
	tab = vim_strrchr(xline, '\t');
	if (tab != NULL)
	{
	    *tab++ = '\0';
	    col = (colnr_T)atoi((char *)tab);
	    tab = vim_strrchr(xline, '\t');
	    if (tab != NULL)
	    {
		*tab++ = '\0';
		lnum = atol((char *)tab);
	    }
	}

	/* Expand "~/" in the file name at "line + 1" to a full path.
	 * Then try shortening it by comparing with the current directory */
	expand_env(xline, NameBuff, MAXPATHL);
	sfname = shorten_fname1(NameBuff);

	buf = buflist_new(NameBuff, sfname, (linenr_T)0, BLN_LISTED);
	if (buf != NULL)	/* just in case... */
	{
	    buf->b_last_cursor.lnum = lnum;
	    buf->b_last_cursor.col = col;
	    buflist_setfpos(buf, curwin, lnum, col, FALSE);
	}
    }
    vim_free(xline);

    return viminfo_readline(virp);
}

    void
write_viminfo_bufferlist(FILE *fp)
{
    buf_T	*buf;
    win_T	*win;
    tabpage_T	*tp;
    char_u	*line;
    int		max_buffers;

    if (find_viminfo_parameter('%') == NULL)
	return;

    /* Without a number -1 is returned: do all buffers. */
    max_buffers = get_viminfo_parameter('%');

    /* Allocate room for the file name, lnum and col. */
#define LINE_BUF_LEN (MAXPATHL + 40)
    line = alloc(LINE_BUF_LEN);
    if (line == NULL)
	return;

    FOR_ALL_TAB_WINDOWS(tp, win)
	set_last_cursor(win);

    fputs(_("\n# Buffer list:\n"), fp);
    FOR_ALL_BUFFERS(buf)
    {
	if (buf->b_fname == NULL
		|| !buf->b_p_bl
#ifdef FEAT_QUICKFIX
		|| bt_quickfix(buf)
#endif
#ifdef FEAT_TERMINAL
		|| bt_terminal(buf)
#endif
		|| removable(buf->b_ffname))
	    continue;

	if (max_buffers-- == 0)
	    break;
	putc('%', fp);
	home_replace(NULL, buf->b_ffname, line, MAXPATHL, TRUE);
	vim_snprintf_add((char *)line, LINE_BUF_LEN, "\t%ld\t%d",
			(long)buf->b_last_cursor.lnum,
			buf->b_last_cursor.col);
	viminfo_writestring(fp, line);
    }
    vim_free(line);
}
#endif

/*
 * Return TRUE if "buf" is a normal buffer, 'buftype' is empty.
 */
    int
bt_normal(buf_T *buf)
{
    return buf != NULL && buf->b_p_bt[0] == NUL;
}

/*
 * Return TRUE if "buf" is the quickfix buffer.
 */
    int
bt_quickfix(buf_T *buf)
{
    return buf != NULL && buf->b_p_bt[0] == 'q';
}

/*
 * Return TRUE if "buf" is a terminal buffer.
 */
    int
bt_terminal(buf_T *buf)
{
    return buf != NULL && buf->b_p_bt[0] == 't';
}

/*
 * Return TRUE if "buf" is a help buffer.
 */
    int
bt_help(buf_T *buf)
{
    return buf != NULL && buf->b_help;
}

/*
 * Return TRUE if "buf" is a prompt buffer.
 */
    int
bt_prompt(buf_T *buf)
{
    return buf != NULL && buf->b_p_bt[0] == 'p';
}

/*
 * Return TRUE if "buf" is a "nofile", "acwrite", "terminal" or "prompt"
 * buffer.  This means the buffer name is not a file name.
 */
    int
bt_nofile(buf_T *buf)
{
    return buf != NULL && ((buf->b_p_bt[0] == 'n' && buf->b_p_bt[2] == 'f')
	    || buf->b_p_bt[0] == 'a'
	    || buf->b_p_bt[0] == 't'
	    || buf->b_p_bt[0] == 'p');
}

/*
 * Return TRUE if "buf" is a "nowrite", "nofile", "terminal" or "prompt"
 * buffer.
 */
    int
bt_dontwrite(buf_T *buf)
{
    return buf != NULL && (buf->b_p_bt[0] == 'n'
	         || buf->b_p_bt[0] == 't'
	         || buf->b_p_bt[0] == 'p');
}

    int
bt_dontwrite_msg(buf_T *buf)
{
    if (bt_dontwrite(buf))
    {
	EMSG(_("E382: Cannot write, 'buftype' option is set"));
	return TRUE;
    }
    return FALSE;
}

/*
 * Return TRUE if the buffer should be hidden, according to 'hidden', ":hide"
 * and 'bufhidden'.
 */
    int
buf_hide(buf_T *buf)
{
    /* 'bufhidden' overrules 'hidden' and ":hide", check it first */
    switch (buf->b_p_bh[0])
    {
	case 'u':		    /* "unload" */
	case 'w':		    /* "wipe" */
	case 'd': return FALSE;	    /* "delete" */
	case 'h': return TRUE;	    /* "hide" */
    }
    return (p_hid || cmdmod.hide);
}

/*
 * Return special buffer name.
 * Returns NULL when the buffer has a normal file name.
 */
    char_u *
buf_spname(buf_T *buf)
{
#if defined(FEAT_QUICKFIX)
    if (bt_quickfix(buf))
    {
	win_T	    *win;
	tabpage_T   *tp;

	/*
	 * For location list window, w_llist_ref points to the location list.
	 * For quickfix window, w_llist_ref is NULL.
	 */
	if (find_win_for_buf(buf, &win, &tp) == OK && win->w_llist_ref != NULL)
	    return (char_u *)_(msg_loclist);
	else
	    return (char_u *)_(msg_qflist);
    }
#endif

    /* There is no _file_ when 'buftype' is "nofile", b_sfname
     * contains the name as specified by the user. */
    if (bt_nofile(buf))
    {
#ifdef FEAT_TERMINAL
	if (buf->b_term != NULL)
	    return term_get_status_text(buf->b_term);
#endif
	if (buf->b_fname != NULL)
	    return buf->b_fname;
#ifdef FEAT_JOB_CHANNEL
	if (bt_prompt(buf))
	    return (char_u *)_("[Prompt]");
#endif
	return (char_u *)_("[Scratch]");
    }

    if (buf->b_fname == NULL)
	return (char_u *)_("[No Name]");
    return NULL;
}

#if defined(FEAT_JOB_CHANNEL) \
	|| defined(FEAT_PYTHON) || defined(FEAT_PYTHON3) \
	|| defined(PROTO)
# define SWITCH_TO_WIN

/*
 * Find a window that contains "buf" and switch to it.
 * If there is no such window, use the current window and change "curbuf".
 * Caller must initialize save_curbuf to NULL.
 * restore_win_for_buf() MUST be called later!
 */
    void
switch_to_win_for_buf(
    buf_T	*buf,
    win_T	**save_curwinp,
    tabpage_T	**save_curtabp,
    bufref_T	*save_curbuf)
{
    win_T	*wp;
    tabpage_T	*tp;

    if (find_win_for_buf(buf, &wp, &tp) == FAIL)
	switch_buffer(save_curbuf, buf);
    else if (switch_win(save_curwinp, save_curtabp, wp, tp, TRUE) == FAIL)
    {
	restore_win(*save_curwinp, *save_curtabp, TRUE);
	switch_buffer(save_curbuf, buf);
    }
}

    void
restore_win_for_buf(
    win_T	*save_curwin,
    tabpage_T	*save_curtab,
    bufref_T	*save_curbuf)
{
    if (save_curbuf->br_buf == NULL)
	restore_win(save_curwin, save_curtab, TRUE);
    else
	restore_buffer(save_curbuf);
}
#endif

#if defined(FEAT_QUICKFIX) || defined(SWITCH_TO_WIN) || defined(PROTO)
/*
 * Find a window for buffer "buf".
 * If found OK is returned and "wp" and "tp" are set to the window and tabpage.
 * If not found FAIL is returned.
 */
    int
find_win_for_buf(
    buf_T     *buf,
    win_T     **wp,
    tabpage_T **tp)
{
    FOR_ALL_TAB_WINDOWS(*tp, *wp)
	if ((*wp)->w_buffer == buf)
	    goto win_found;
    return FAIL;
win_found:
    return OK;
}
#endif

#if defined(FEAT_SIGNS) || defined(PROTO)
static hashtab_T	sg_table;	// sign group (signgroup_T) hashtable

/*
 * A new sign in group 'groupname' is added. If the group is not present,
 * create it. Otherwise reference the group.
 */
    static signgroup_T *
sign_group_ref(char_u *groupname)
{
    static int		initialized = FALSE;
    hash_T		hash;
    hashitem_T		*hi;
    signgroup_T		*group;

    if (!initialized)
    {
	initialized = TRUE;
	hash_init(&sg_table);
    }

    hash = hash_hash(groupname);
    hi = hash_lookup(&sg_table, groupname, hash);
    if (HASHITEM_EMPTY(hi))
    {
	// new group
	group = (signgroup_T *)alloc(
		(unsigned)(sizeof(signgroup_T) + STRLEN(groupname)));
	if (group == NULL)
	    return NULL;
	STRCPY(group->sg_name, groupname);
	group->refcount = 1;
	hash_add_item(&sg_table, hi, group->sg_name, hash);
    }
    else
    {
	// existing group
	group = HI2SG(hi);
	group->refcount++;
    }

    return group;
}

/*
 * A sign in group 'groupname' is removed. If all the signs in this group are
 * removed, then remove the group.
 */
    static void
sign_group_unref(char_u *groupname)
{
    hashitem_T		*hi;
    signgroup_T		*group;

    hi = hash_find(&sg_table, groupname);
    if (!HASHITEM_EMPTY(hi))
    {
	group = HI2SG(hi);
	group->refcount--;
	if (group->refcount == 0)
	{
	    // All the signs in this group are removed
	    hash_remove(&sg_table, hi);
	    vim_free(group);
	}
    }
}

/*
 * Insert a new sign into the signlist for buffer 'buf' between the 'prev' and
 * 'next' signs.
 */
    static void
insert_sign(
    buf_T	*buf,		// buffer to store sign in
    signlist_T	*prev,		// previous sign entry
    signlist_T	*next,		// next sign entry
    int		id,		// sign ID
    char_u	*group,		// sign group; NULL for global group
    int		prio,		// sign priority
    linenr_T	lnum,		// line number which gets the mark
    int		typenr)		// typenr of sign we are adding
{
    signlist_T	*newsign;

    newsign = (signlist_T *)lalloc_id((long_u)sizeof(signlist_T), FALSE,
							aid_insert_sign);
    if (newsign != NULL)
    {
	newsign->id = id;
	newsign->lnum = lnum;
	newsign->typenr = typenr;
	if (group != NULL)
	{
	    newsign->group = sign_group_ref(group);
	    if (newsign->group == NULL)
	    {
		vim_free(newsign);
		return;
	    }
	}
	else
	    newsign->group = NULL;
	newsign->priority = prio;
	newsign->next = next;
	newsign->prev = prev;
	if (next != NULL)
	    next->prev = newsign;

	if (prev == NULL)
	{
	    // When adding first sign need to redraw the windows to create the
	    // column for signs.
	    if (buf->b_signlist == NULL)
	    {
		redraw_buf_later(buf, NOT_VALID);
		changed_cline_bef_curs();
	    }

	    // first sign in signlist
	    buf->b_signlist = newsign;
#ifdef FEAT_NETBEANS_INTG
	    if (netbeans_active())
		buf->b_has_sign_column = TRUE;
#endif
	}
	else
	    prev->next = newsign;
    }
}

/*
 * Insert a new sign sorted by line number and sign priority.
 */
    static void
insert_sign_by_lnum_prio(
    buf_T	*buf,		// buffer to store sign in
    signlist_T	*prev,		// previous sign entry
    int		id,		// sign ID
    char_u	*group,		// sign group; NULL for global group
    int		prio,		// sign priority
    linenr_T	lnum,		// line number which gets the mark
    int		typenr)		// typenr of sign we are adding
{
    signlist_T	*sign;

    // keep signs sorted by lnum and by priority: insert new sign at
    // the proper position in the list for this lnum.
    while (prev != NULL && prev->lnum == lnum && prev->priority <= prio)
	prev = prev->prev;
    if (prev == NULL)
	sign = buf->b_signlist;
    else
	sign = prev->next;

    insert_sign(buf, prev, sign, id, group, prio, lnum, typenr);
}

/*
 * Returns TRUE if 'sign' is in 'group'.
 * A sign can either be in the global group (sign->group == NULL)
 * or in a named group. If 'group' is '*', then the sign is part of the group.
 */
    int
sign_in_group(signlist_T *sign, char_u *group)
{
    return ((group != NULL && STRCMP(group, "*") == 0) ||
	    (group == NULL && sign->group == NULL) ||
	    (group != NULL && sign->group != NULL &&
				STRCMP(group, sign->group->sg_name) == 0));
}

/*
 * Return information about a sign in a Dict
 */
    dict_T *
sign_get_info(signlist_T *sign)
{
    dict_T	*d;

    if ((d = dict_alloc_id(aid_sign_getinfo)) == NULL)
	return NULL;
    dict_add_number(d, "id", sign->id);
    dict_add_string(d, "group", (sign->group == NULL) ?
					(char_u *)"" : sign->group->sg_name);
    dict_add_number(d, "lnum", sign->lnum);
    dict_add_string(d, "name", sign_typenr2name(sign->typenr));
    dict_add_number(d, "priority", sign->priority);

    return d;
}

/*
 * Add the sign into the signlist. Find the right spot to do it though.
 */
    void
buf_addsign(
    buf_T	*buf,		// buffer to store sign in
    int		id,		// sign ID
    char_u	*groupname,	// sign group
    int		prio,		// sign priority
    linenr_T	lnum,		// line number which gets the mark
    int		typenr)		// typenr of sign we are adding
{
    signlist_T	*sign;		// a sign in the signlist
    signlist_T	*prev;		// the previous sign

    prev = NULL;
    FOR_ALL_SIGNS_IN_BUF(buf)
    {
	if (lnum == sign->lnum && id == sign->id &&
		sign_in_group(sign, groupname))
	{
	    // Update an existing sign
	    sign->typenr = typenr;
	    return;
	}
	else if (lnum < sign->lnum)
	{
	    insert_sign_by_lnum_prio(buf, prev, id, groupname, prio,
								lnum, typenr);
	    return;
	}
	prev = sign;
    }

    insert_sign_by_lnum_prio(buf, prev, id, groupname, prio, lnum, typenr);
    return;
}

/*
 * For an existing, placed sign "markId" change the type to "typenr".
 * Returns the line number of the sign, or zero if the sign is not found.
 */
    linenr_T
buf_change_sign_type(
    buf_T	*buf,		// buffer to store sign in
    int		markId,		// sign ID
    char_u	*group,		// sign group
    int		typenr)		// typenr of sign we are adding
{
    signlist_T	*sign;		// a sign in the signlist

    FOR_ALL_SIGNS_IN_BUF(buf)
    {
	if (sign->id == markId && sign_in_group(sign, group))
	{
	    sign->typenr = typenr;
	    return sign->lnum;
	}
    }

    return (linenr_T)0;
}

/*
 * Return the type number of the sign at line number 'lnum' in buffer 'buf'
 * which has the attribute specifed by 'type'. Returns 0 if a sign is not found
 * at the line number or it doesn't have the specified attribute.
 */
    int
buf_getsigntype(
    buf_T	*buf,
    linenr_T	lnum,
    int		type)	/* SIGN_ICON, SIGN_TEXT, SIGN_ANY, SIGN_LINEHL */
{
    signlist_T	*sign;		/* a sign in a b_signlist */

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->lnum == lnum
		&& (type == SIGN_ANY
# ifdef FEAT_SIGN_ICONS
		    || (type == SIGN_ICON
			&& sign_get_image(sign->typenr) != NULL)
# endif
		    || (type == SIGN_TEXT
			&& sign_get_text(sign->typenr) != NULL)
		    || (type == SIGN_LINEHL
			&& sign_get_attr(sign->typenr, TRUE) != 0)))
	    return sign->typenr;
    return 0;
}

/*
 * Delete sign 'id' in group 'group' from buffer 'buf'.
 * If 'id' is zero, then delete all the signs in group 'group'. Otherwise
 * delete only the specified sign.
 * If 'group' is '*', then delete the sign in all the groups. If 'group' is
 * NULL, then delete the sign in the global group. Otherwise delete the sign in
 * the specified group.
 * Returns the line number of the deleted sign. If multiple signs are deleted,
 * then returns the line number of the last sign deleted.
 */
    linenr_T
buf_delsign(
    buf_T	*buf,		// buffer sign is stored in
    int		id,		// sign id
    char_u	*group)		// sign group
{
    signlist_T	**lastp;	// pointer to pointer to current sign
    signlist_T	*sign;		// a sign in a b_signlist
    signlist_T	*next;		// the next sign in a b_signlist
    linenr_T	lnum;		// line number whose sign was deleted

    lastp = &buf->b_signlist;
    lnum = 0;
    for (sign = buf->b_signlist; sign != NULL; sign = next)
    {
	next = sign->next;
	if ((id == 0 || sign->id == id) && sign_in_group(sign, group))

	{
	    *lastp = next;
	    if (next != NULL)
		next->prev = sign->prev;
	    lnum = sign->lnum;
	    if (sign->group != NULL)
		sign_group_unref(sign->group->sg_name);
	    vim_free(sign);
	    // Check whether only one sign needs to be deleted
	    if (group == NULL || (*group != '*' && id != 0))
		break;
	}
	else
	    lastp = &sign->next;
    }

    // When deleted the last sign need to redraw the windows to remove the
    // sign column.
    if (buf->b_signlist == NULL)
    {
	redraw_buf_later(buf, NOT_VALID);
	changed_cline_bef_curs();
    }

    return lnum;
}


/*
 * Find the line number of the sign with the requested id in group 'group'. If
 * the sign does not exist, return 0 as the line number. This will still let
 * the correct file get loaded.
 */
    int
buf_findsign(
    buf_T	*buf,		// buffer to store sign in
    int		id,		// sign ID
    char_u	*group)		// sign group
{
    signlist_T	*sign;		// a sign in the signlist

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->id == id && sign_in_group(sign, group))
	    return sign->lnum;

    return 0;
}

/*
 * Return the sign at line 'lnum' in buffer 'buf'. Returns NULL if a sign is
 * not found at the line.
 */
    static signlist_T *
buf_getsign_at_line(
    buf_T	*buf,		// buffer whose sign we are searching for
    linenr_T	lnum)		// line number of sign
{
    signlist_T	*sign;		// a sign in the signlist

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->lnum == lnum)
	    return sign;

    return NULL;
}

/*
 * Return the sign with identifier 'id' in group 'group' placed in buffer 'buf'
 */
    signlist_T *
buf_getsign_with_id(
    buf_T	*buf,		// buffer whose sign we are searching for
    int		id,		// sign identifier
    char_u	*group)		// sign group
{
    signlist_T	*sign;		// a sign in the signlist

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->id == id && sign_in_group(sign, group))
	    return sign;

    return NULL;
}

/*
 * Return the identifier of the sign at line number 'lnum' in buffer 'buf'.
 */
    int
buf_findsign_id(
    buf_T	*buf,		// buffer whose sign we are searching for
    linenr_T	lnum)		// line number of sign
{
    signlist_T	*sign;		// a sign in the signlist

    sign = buf_getsign_at_line(buf, lnum);
    if (sign != NULL)
	return sign->id;

    return 0;
}

# if defined(FEAT_NETBEANS_INTG) || defined(PROTO)
/*
 * See if a given type of sign exists on a specific line.
 */
    int
buf_findsigntype_id(
    buf_T	*buf,		/* buffer whose sign we are searching for */
    linenr_T	lnum,		/* line number of sign */
    int		typenr)		/* sign type number */
{
    signlist_T	*sign;		/* a sign in the signlist */

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->lnum == lnum && sign->typenr == typenr)
	    return sign->id;

    return 0;
}


#  if defined(FEAT_SIGN_ICONS) || defined(PROTO)
/*
 * Return the number of icons on the given line.
 */
    int
buf_signcount(buf_T *buf, linenr_T lnum)
{
    signlist_T	*sign;		// a sign in the signlist
    int		count = 0;

    FOR_ALL_SIGNS_IN_BUF(buf)
	if (sign->lnum == lnum)
	    if (sign_get_image(sign->typenr) != NULL)
		count++;

    return count;
}
#  endif /* FEAT_SIGN_ICONS */
# endif /* FEAT_NETBEANS_INTG */

/*
 * Delete signs in group 'group' in buffer "buf". If 'group' is '*', then
 * delete all the signs.
 */
    void
buf_delete_signs(buf_T *buf, char_u *group)
{
    signlist_T	*sign;
    signlist_T	**lastp;	// pointer to pointer to current sign
    signlist_T	*next;

    // When deleting the last sign need to redraw the windows to remove the
    // sign column. Not when curwin is NULL (this means we're exiting).
    if (buf->b_signlist != NULL && curwin != NULL)
    {
	redraw_buf_later(buf, NOT_VALID);
	changed_cline_bef_curs();
    }

    lastp = &buf->b_signlist;
    for (sign = buf->b_signlist; sign != NULL; sign = next)
    {
	next = sign->next;
	if (sign_in_group(sign, group))
	{
	    *lastp = next;
	    if (next != NULL)
		next->prev = sign->prev;
	    if (sign->group != NULL)
		sign_group_unref(sign->group->sg_name);
	    vim_free(sign);
	}
	else
	    lastp = &sign->next;
    }
}

/*
 * Delete all signs in all buffers.
 */
    void
buf_delete_all_signs(void)
{
    buf_T	*buf;		/* buffer we are checking for signs */

    FOR_ALL_BUFFERS(buf)
	if (buf->b_signlist != NULL)
	    buf_delete_signs(buf, (char_u *)"*");
}

/*
 * List placed signs for "rbuf".  If "rbuf" is NULL do it for all buffers.
 */
    void
sign_list_placed(buf_T *rbuf, char_u *sign_group)
{
    buf_T	*buf;
    signlist_T	*sign;
    char	lbuf[BUFSIZ];
    char	group[BUFSIZ];

    MSG_PUTS_TITLE(_("\n--- Signs ---"));
    msg_putchar('\n');
    if (rbuf == NULL)
	buf = firstbuf;
    else
	buf = rbuf;
    while (buf != NULL && !got_int)
    {
	if (buf->b_signlist != NULL)
	{
	    vim_snprintf(lbuf, BUFSIZ, _("Signs for %s:"), buf->b_fname);
	    MSG_PUTS_ATTR(lbuf, HL_ATTR(HLF_D));
	    msg_putchar('\n');
	}
	FOR_ALL_SIGNS_IN_BUF(buf)
	{
	    if (got_int)
		break;
	    if (!sign_in_group(sign, sign_group))
		continue;
	    if (sign->group != NULL)
		vim_snprintf(group, BUFSIZ, "  group=%s",
							sign->group->sg_name);
	    else
		group[0] = '\0';
	    vim_snprintf(lbuf, BUFSIZ, _("    line=%ld  id=%d%s  name=%s "
							"priority=%d"),
			   (long)sign->lnum, sign->id, group,
			   sign_typenr2name(sign->typenr), sign->priority);
	    MSG_PUTS(lbuf);
	    msg_putchar('\n');
	}
	if (rbuf != NULL)
	    break;
	buf = buf->b_next;
    }
}

/*
 * Adjust a placed sign for inserted/deleted lines.
 */
    void
sign_mark_adjust(
    linenr_T	line1,
    linenr_T	line2,
    long	amount,
    long	amount_after)
{
    signlist_T	*sign;		/* a sign in a b_signlist */

    FOR_ALL_SIGNS_IN_BUF(curbuf)
    {
	if (sign->lnum >= line1 && sign->lnum <= line2)
	{
	    if (amount == MAXLNUM)
		sign->lnum = line1;
	    else
		sign->lnum += amount;
	}
	else if (sign->lnum > line2)
	    sign->lnum += amount_after;
    }
}
#endif /* FEAT_SIGNS */

/*
 * Set 'buflisted' for curbuf to "on" and trigger autocommands if it changed.
 */
    void
set_buflisted(int on)
{
    if (on != curbuf->b_p_bl)
    {
	curbuf->b_p_bl = on;
	if (on)
	    apply_autocmds(EVENT_BUFADD, NULL, NULL, FALSE, curbuf);
	else
	    apply_autocmds(EVENT_BUFDELETE, NULL, NULL, FALSE, curbuf);
    }
}

/*
 * Read the file for "buf" again and check if the contents changed.
 * Return TRUE if it changed or this could not be checked.
 */
    int
buf_contents_changed(buf_T *buf)
{
    buf_T	*newbuf;
    int		differ = TRUE;
    linenr_T	lnum;
    aco_save_T	aco;
    exarg_T	ea;

    /* Allocate a buffer without putting it in the buffer list. */
    newbuf = buflist_new(NULL, NULL, (linenr_T)1, BLN_DUMMY);
    if (newbuf == NULL)
	return TRUE;

    /* Force the 'fileencoding' and 'fileformat' to be equal. */
    if (prep_exarg(&ea, buf) == FAIL)
    {
	wipe_buffer(newbuf, FALSE);
	return TRUE;
    }

    /* set curwin/curbuf to buf and save a few things */
    aucmd_prepbuf(&aco, newbuf);

    if (ml_open(curbuf) == OK
	    && readfile(buf->b_ffname, buf->b_fname,
				  (linenr_T)0, (linenr_T)0, (linenr_T)MAXLNUM,
					    &ea, READ_NEW | READ_DUMMY) == OK)
    {
	/* compare the two files line by line */
	if (buf->b_ml.ml_line_count == curbuf->b_ml.ml_line_count)
	{
	    differ = FALSE;
	    for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count; ++lnum)
		if (STRCMP(ml_get_buf(buf, lnum, FALSE), ml_get(lnum)) != 0)
		{
		    differ = TRUE;
		    break;
		}
	}
    }
    vim_free(ea.cmd);

    /* restore curwin/curbuf and a few other things */
    aucmd_restbuf(&aco);

    if (curbuf != newbuf)	/* safety check */
	wipe_buffer(newbuf, FALSE);

    return differ;
}

/*
 * Wipe out a buffer and decrement the last buffer number if it was used for
 * this buffer.  Call this to wipe out a temp buffer that does not contain any
 * marks.
 */
    void
wipe_buffer(
    buf_T	*buf,
    int		aucmd UNUSED)	    /* When TRUE trigger autocommands. */
{
    if (buf->b_fnum == top_file_num - 1)
	--top_file_num;

    if (!aucmd)		    /* Don't trigger BufDelete autocommands here. */
	block_autocmds();

    close_buffer(NULL, buf, DOBUF_WIPE, FALSE);

    if (!aucmd)
	unblock_autocmds();
}
/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Implements communication through a socket or any file handle.
 */

#include "vim.h"

#if defined(FEAT_JOB_CHANNEL) || defined(PROTO)

/* TRUE when netbeans is running with a GUI. */
#ifdef FEAT_GUI
# define CH_HAS_GUI (gui.in_use || gui.starting)
#endif

/* Note: when making changes here also adjust configure.ac. */
#ifdef WIN32
/* WinSock API is separated from C API, thus we can't use read(), write(),
 * errno... */
# define SOCK_ERRNO errno = WSAGetLastError()
# undef ECONNREFUSED
# define ECONNREFUSED WSAECONNREFUSED
# undef EWOULDBLOCK
# define EWOULDBLOCK WSAEWOULDBLOCK
# undef EINPROGRESS
# define EINPROGRESS WSAEINPROGRESS
# ifdef EINTR
#  undef EINTR
# endif
# define EINTR WSAEINTR
# define sock_write(sd, buf, len) send((SOCKET)sd, buf, len, 0)
# define sock_read(sd, buf, len) recv((SOCKET)sd, buf, len, 0)
# define sock_close(sd) closesocket((SOCKET)sd)
#else
# include <netdb.h>
# include <netinet/in.h>

# include <sys/socket.h>
# ifdef HAVE_LIBGEN_H
#  include <libgen.h>
# endif
# define SOCK_ERRNO
# define sock_write(sd, buf, len) write(sd, buf, len)
# define sock_read(sd, buf, len) read(sd, buf, len)
# define sock_close(sd) close(sd)
# define fd_read(fd, buf, len) read(fd, buf, len)
# define fd_write(sd, buf, len) write(sd, buf, len)
# define fd_close(sd) close(sd)
#endif

static void channel_read(channel_T *channel, ch_part_T part, char *func);

/* Whether a redraw is needed for appending a line to a buffer. */
static int channel_need_redraw = FALSE;

/* Whether we are inside channel_parse_messages() or another situation where it
 * is safe to invoke callbacks. */
static int safe_to_invoke_callback = 0;

static char *part_names[] = {"sock", "out", "err", "in"};

#ifdef WIN32
    static int
fd_read(sock_T fd, char *buf, size_t len)
{
    HANDLE h = (HANDLE)fd;
    DWORD nread;

    if (!ReadFile(h, buf, (DWORD)len, &nread, NULL))
	return -1;
    return (int)nread;
}

    static int
fd_write(sock_T fd, char *buf, size_t len)
{
    HANDLE h = (HANDLE)fd;
    DWORD nwrite;

    if (!WriteFile(h, buf, (DWORD)len, &nwrite, NULL))
	return -1;
    return (int)nwrite;
}

    static void
fd_close(sock_T fd)
{
    HANDLE h = (HANDLE)fd;

    CloseHandle(h);
}
#endif

/* Log file opened with ch_logfile(). */
static FILE *log_fd = NULL;
#ifdef FEAT_RELTIME
static proftime_T log_start;
#endif

    void
ch_logfile(char_u *fname, char_u *opt)
{
    FILE   *file = NULL;

    if (log_fd != NULL)
	fclose(log_fd);

    if (*fname != NUL)
    {
	file = fopen((char *)fname, *opt == 'w' ? "w" : "a");
	if (file == NULL)
	{
	    EMSG2(_(e_notopen), fname);
	    return;
	}
    }
    log_fd = file;

    if (log_fd != NULL)
    {
	fprintf(log_fd, "==== start log session ====\n");
#ifdef FEAT_RELTIME
	profile_start(&log_start);
#endif
    }
}

    int
ch_log_active(void)
{
    return log_fd != NULL;
}

    static void
ch_log_lead(const char *what, channel_T *ch, ch_part_T part)
{
    if (log_fd != NULL)
    {
#ifdef FEAT_RELTIME
	proftime_T log_now;

	profile_start(&log_now);
	profile_sub(&log_now, &log_start);
	fprintf(log_fd, "%s ", profile_msg(&log_now));
#endif
	if (ch != NULL)
	{
	    if (part < PART_COUNT)
		fprintf(log_fd, "%son %d(%s): ",
					   what, ch->ch_id, part_names[part]);
	    else
		fprintf(log_fd, "%son %d: ", what, ch->ch_id);
	}
	else
	    fprintf(log_fd, "%s: ", what);
    }
}

static int did_log_msg = TRUE;

#ifndef PROTO  /* prototype is in vim.h */
    void
ch_log(channel_T *ch, const char *fmt, ...)
{
    if (log_fd != NULL)
    {
	va_list ap;

	ch_log_lead("", ch, PART_COUNT);
	va_start(ap, fmt);
	vfprintf(log_fd, fmt, ap);
	va_end(ap);
	fputc('\n', log_fd);
	fflush(log_fd);
	did_log_msg = TRUE;
    }
}
#endif

    static void
ch_error(channel_T *ch, const char *fmt, ...)
#ifdef USE_PRINTF_FORMAT_ATTRIBUTE
    __attribute__((format(printf, 2, 3)))
#endif
    ;

    static void
ch_error(channel_T *ch, const char *fmt, ...)
{
    if (log_fd != NULL)
    {
	va_list ap;

	ch_log_lead("ERR ", ch, PART_COUNT);
	va_start(ap, fmt);
	vfprintf(log_fd, fmt, ap);
	va_end(ap);
	fputc('\n', log_fd);
	fflush(log_fd);
	did_log_msg = TRUE;
    }
}

#ifdef _WIN32
# undef PERROR
# define PERROR(msg) (void)emsg3((char_u *)"%s: %s", \
	(char_u *)msg, (char_u *)strerror_win32(errno))

    static char *
strerror_win32(int eno)
{
    static LPVOID msgbuf = NULL;
    char_u *ptr;

    if (msgbuf)
    {
	LocalFree(msgbuf);
	msgbuf = NULL;
    }
    FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM |
	FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL,
	eno,
	MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
	(LPTSTR) &msgbuf,
	0,
	NULL);
    if (msgbuf != NULL)
	/* chomp \r or \n */
	for (ptr = (char_u *)msgbuf; *ptr; ptr++)
	    switch (*ptr)
	    {
		case '\r':
		    STRMOVE(ptr, ptr + 1);
		    ptr--;
		    break;
		case '\n':
		    if (*(ptr + 1) == '\0')
			*ptr = '\0';
		    else
			*ptr = ' ';
		    break;
	    }
    return msgbuf;
}
#endif

/*
 * The list of all allocated channels.
 */
static channel_T *first_channel = NULL;
static int next_ch_id = 0;

/*
 * Allocate a new channel.  The refcount is set to 1.
 * The channel isn't actually used until it is opened.
 * Returns NULL if out of memory.
 */
    channel_T *
add_channel(void)
{
    ch_part_T	part;
    channel_T	*channel = (channel_T *)alloc_clear((int)sizeof(channel_T));

    if (channel == NULL)
	return NULL;

    channel->ch_id = next_ch_id++;
    ch_log(channel, "Created channel");

    for (part = PART_SOCK; part < PART_COUNT; ++part)
    {
	channel->ch_part[part].ch_fd = INVALID_FD;
#ifdef FEAT_GUI_X11
	channel->ch_part[part].ch_inputHandler = (XtInputId)NULL;
#endif
#ifdef FEAT_GUI_GTK
	channel->ch_part[part].ch_inputHandler = 0;
#endif
	channel->ch_part[part].ch_timeout = 2000;
    }

    if (first_channel != NULL)
    {
	first_channel->ch_prev = channel;
	channel->ch_next = first_channel;
    }
    first_channel = channel;

    channel->ch_refcount = 1;
    return channel;
}

    int
has_any_channel(void)
{
    return first_channel != NULL;
}

/*
 * Called when the refcount of a channel is zero.
 * Return TRUE if "channel" has a callback and the associated job wasn't
 * killed.
 */
    static int
channel_still_useful(channel_T *channel)
{
    int has_sock_msg;
    int	has_out_msg;
    int	has_err_msg;

    /* If the job was killed the channel is not expected to work anymore. */
    if (channel->ch_job_killed && channel->ch_job == NULL)
	return FALSE;

    /* If there is a close callback it may still need to be invoked. */
    if (channel->ch_close_cb != NULL)
	return TRUE;

    /* If reading from or a buffer it's still useful. */
    if (channel->ch_part[PART_IN].ch_bufref.br_buf != NULL)
	return TRUE;

    /* If there is no callback then nobody can get readahead.  If the fd is
     * closed and there is no readahead then the callback won't be called. */
    has_sock_msg = channel->ch_part[PART_SOCK].ch_fd != INVALID_FD
		|| channel->ch_part[PART_SOCK].ch_head.rq_next != NULL
		|| channel->ch_part[PART_SOCK].ch_json_head.jq_next != NULL;
    has_out_msg = channel->ch_part[PART_OUT].ch_fd != INVALID_FD
		  || channel->ch_part[PART_OUT].ch_head.rq_next != NULL
		  || channel->ch_part[PART_OUT].ch_json_head.jq_next != NULL;
    has_err_msg = channel->ch_part[PART_ERR].ch_fd != INVALID_FD
		  || channel->ch_part[PART_ERR].ch_head.rq_next != NULL
		  || channel->ch_part[PART_ERR].ch_json_head.jq_next != NULL;
    return (channel->ch_callback != NULL && (has_sock_msg
		|| has_out_msg || has_err_msg))
	    || ((channel->ch_part[PART_OUT].ch_callback != NULL
		       || channel->ch_part[PART_OUT].ch_bufref.br_buf != NULL)
		    && has_out_msg)
	    || ((channel->ch_part[PART_ERR].ch_callback != NULL
		       || channel->ch_part[PART_ERR].ch_bufref.br_buf != NULL)
		    && has_err_msg);
}

/*
 * Return TRUE if "channel" is closeable (i.e. all readable fds are closed).
 */
    static int
channel_can_close(channel_T *channel)
{
    return channel->ch_to_be_closed == 0;
}

/*
 * Close a channel and free all its resources.
 */
    static void
channel_free_contents(channel_T *channel)
{
    channel_close(channel, TRUE);
    channel_clear(channel);
    ch_log(channel, "Freeing channel");
}

    static void
channel_free_channel(channel_T *channel)
{
    if (channel->ch_next != NULL)
	channel->ch_next->ch_prev = channel->ch_prev;
    if (channel->ch_prev == NULL)
	first_channel = channel->ch_next;
    else
	channel->ch_prev->ch_next = channel->ch_next;
    vim_free(channel);
}

    static void
channel_free(channel_T *channel)
{
    if (!in_free_unref_items)
    {
	if (safe_to_invoke_callback == 0)
	    channel->ch_to_be_freed = TRUE;
	else
	{
	    channel_free_contents(channel);
	    channel_free_channel(channel);
	}
    }
}

/*
 * Close a channel and free all its resources if there is no further action
 * possible, there is no callback to be invoked or the associated job was
 * killed.
 * Return TRUE if the channel was freed.
 */
    static int
channel_may_free(channel_T *channel)
{
    if (!channel_still_useful(channel))
    {
	channel_free(channel);
	return TRUE;
    }
    return FALSE;
}

/*
 * Decrement the reference count on "channel" and maybe free it when it goes
 * down to zero.  Don't free it if there is a pending action.
 * Returns TRUE when the channel is no longer referenced.
 */
    int
channel_unref(channel_T *channel)
{
    if (channel != NULL && --channel->ch_refcount <= 0)
	return channel_may_free(channel);
    return FALSE;
}

    int
free_unused_channels_contents(int copyID, int mask)
{
    int		did_free = FALSE;
    channel_T	*ch;

    /* This is invoked from the garbage collector, which only runs at a safe
     * point. */
    ++safe_to_invoke_callback;

    for (ch = first_channel; ch != NULL; ch = ch->ch_next)
	if (!channel_still_useful(ch)
				 && (ch->ch_copyID & mask) != (copyID & mask))
	{
	    /* Free the channel and ordinary items it contains, but don't
	     * recurse into Lists, Dictionaries etc. */
	    channel_free_contents(ch);
	    did_free = TRUE;
	}

    --safe_to_invoke_callback;
    return did_free;
}

    void
free_unused_channels(int copyID, int mask)
{
    channel_T	*ch;
    channel_T	*ch_next;

    for (ch = first_channel; ch != NULL; ch = ch_next)
    {
	ch_next = ch->ch_next;
	if (!channel_still_useful(ch)
				 && (ch->ch_copyID & mask) != (copyID & mask))
	{
	    /* Free the channel struct itself. */
	    channel_free_channel(ch);
	}
    }
}

#if defined(FEAT_GUI) || defined(PROTO)

#if defined(FEAT_GUI_X11) || defined(FEAT_GUI_GTK)
    static void
channel_read_fd(int fd)
{
    channel_T	*channel;
    ch_part_T	part;

    channel = channel_fd2channel(fd, &part);
    if (channel == NULL)
	ch_error(NULL, "Channel for fd %d not found", fd);
    else
	channel_read(channel, part, "channel_read_fd");
}
#endif

/*
 * Read a command from netbeans.
 */
#ifdef FEAT_GUI_X11
    static void
messageFromServerX11(XtPointer clientData,
		  int *unused1 UNUSED,
		  XtInputId *unused2 UNUSED)
{
    channel_read_fd((int)(long)clientData);
}
#endif

#ifdef FEAT_GUI_GTK
# if GTK_CHECK_VERSION(3,0,0)
    static gboolean
messageFromServerGtk3(GIOChannel *unused1 UNUSED,
		  GIOCondition unused2 UNUSED,
		  gpointer clientData)
{
    channel_read_fd(GPOINTER_TO_INT(clientData));
    return TRUE; /* Return FALSE instead in case the event source is to
		  * be removed after this function returns. */
}
# else
    static void
messageFromServerGtk2(gpointer clientData,
		  gint unused1 UNUSED,
		  GdkInputCondition unused2 UNUSED)
{
    channel_read_fd((int)(long)clientData);
}
# endif
#endif

    static void
channel_gui_register_one(channel_T *channel, ch_part_T part)
{
    if (!CH_HAS_GUI)
	return;

    /* gets stuck in handling events for a not connected channel */
    if (channel->ch_keep_open)
	return;

# ifdef FEAT_GUI_X11
    /* Tell notifier we are interested in being called when there is input on
     * the editor connection socket. */
    if (channel->ch_part[part].ch_inputHandler == (XtInputId)NULL)
    {
	ch_log(channel, "Registering part %s with fd %d",
		part_names[part], channel->ch_part[part].ch_fd);

	channel->ch_part[part].ch_inputHandler = XtAppAddInput(
		(XtAppContext)app_context,
		channel->ch_part[part].ch_fd,
		(XtPointer)(XtInputReadMask + XtInputExceptMask),
		messageFromServerX11,
		(XtPointer)(long)channel->ch_part[part].ch_fd);
    }
# else
#  ifdef FEAT_GUI_GTK
    /* Tell gdk we are interested in being called when there is input on the
     * editor connection socket. */
    if (channel->ch_part[part].ch_inputHandler == 0)
    {
	ch_log(channel, "Registering part %s with fd %d",
		part_names[part], channel->ch_part[part].ch_fd);
#   if GTK_CHECK_VERSION(3,0,0)
	GIOChannel *chnnl = g_io_channel_unix_new(
		(gint)channel->ch_part[part].ch_fd);

	channel->ch_part[part].ch_inputHandler = g_io_add_watch(
		chnnl,
		G_IO_IN|G_IO_HUP|G_IO_ERR|G_IO_PRI,
		messageFromServerGtk3,
		GINT_TO_POINTER(channel->ch_part[part].ch_fd));

	g_io_channel_unref(chnnl);
#   else
	channel->ch_part[part].ch_inputHandler = gdk_input_add(
		(gint)channel->ch_part[part].ch_fd,
		(GdkInputCondition)
			     ((int)GDK_INPUT_READ + (int)GDK_INPUT_EXCEPTION),
		messageFromServerGtk2,
		(gpointer)(long)channel->ch_part[part].ch_fd);
#   endif
    }
#  endif
# endif
}

    static void
channel_gui_register(channel_T *channel)
{
    if (channel->CH_SOCK_FD != INVALID_FD)
	channel_gui_register_one(channel, PART_SOCK);
    if (channel->CH_OUT_FD != INVALID_FD
	    && channel->CH_OUT_FD != channel->CH_SOCK_FD)
	channel_gui_register_one(channel, PART_OUT);
    if (channel->CH_ERR_FD != INVALID_FD
	    && channel->CH_ERR_FD != channel->CH_SOCK_FD
	    && channel->CH_ERR_FD != channel->CH_OUT_FD)
	channel_gui_register_one(channel, PART_ERR);
}

/*
 * Register any of our file descriptors with the GUI event handling system.
 * Called when the GUI has started.
 */
    void
channel_gui_register_all(void)
{
    channel_T *channel;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	channel_gui_register(channel);
}

    static void
channel_gui_unregister_one(channel_T *channel, ch_part_T part)
{
# ifdef FEAT_GUI_X11
    if (channel->ch_part[part].ch_inputHandler != (XtInputId)NULL)
    {
	ch_log(channel, "Unregistering part %s", part_names[part]);
	XtRemoveInput(channel->ch_part[part].ch_inputHandler);
	channel->ch_part[part].ch_inputHandler = (XtInputId)NULL;
    }
# else
#  ifdef FEAT_GUI_GTK
    if (channel->ch_part[part].ch_inputHandler != 0)
    {
	ch_log(channel, "Unregistering part %s", part_names[part]);
#   if GTK_CHECK_VERSION(3,0,0)
	g_source_remove(channel->ch_part[part].ch_inputHandler);
#   else
	gdk_input_remove(channel->ch_part[part].ch_inputHandler);
#   endif
	channel->ch_part[part].ch_inputHandler = 0;
    }
#  endif
# endif
}

    static void
channel_gui_unregister(channel_T *channel)
{
    ch_part_T	part;

    for (part = PART_SOCK; part < PART_IN; ++part)
	channel_gui_unregister_one(channel, part);
}

#endif

static char *e_cannot_connect = N_("E902: Cannot connect to port");

/*
 * Open a socket channel to "hostname":"port".
 * "waittime" is the time in msec to wait for the connection.
 * When negative wait forever.
 * Returns the channel for success.
 * Returns NULL for failure.
 */
    channel_T *
channel_open(
	char *hostname,
	int port_in,
	int waittime,
	void (*nb_close_cb)(void))
{
    int			sd = -1;
    struct sockaddr_in	server;
    struct hostent	*host;
#ifdef WIN32
    u_short		port = port_in;
    u_long		val = 1;
#else
    int			port = port_in;
#endif
    channel_T		*channel;
    int			ret;

#ifdef WIN32
    channel_init_winsock();
#endif

    channel = add_channel();
    if (channel == NULL)
    {
	ch_error(NULL, "Cannot allocate channel.");
	return NULL;
    }

    /* Get the server internet address and put into addr structure */
    /* fill in the socket address structure and connect to server */
    vim_memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if ((host = gethostbyname(hostname)) == NULL)
    {
	ch_error(channel, "in gethostbyname() in channel_open()");
	PERROR(_("E901: gethostbyname() in channel_open()"));
	channel_free(channel);
	return NULL;
    }
    {
	char		*p;

	/* When using host->h_addr_list[0] directly ubsan warns for it to not
	 * be aligned.  First copy the pointer to avoid that. */
	memcpy(&p, &host->h_addr_list[0], sizeof(p));
	memcpy((char *)&server.sin_addr, p, host->h_length);
    }

    /* On Mac and Solaris a zero timeout almost never works.  At least wait
     * one millisecond. Let's do it for all systems, because we don't know why
     * this is needed. */
    if (waittime == 0)
	waittime = 1;

    /*
     * For Unix we need to call connect() again after connect() failed.
     * On Win32 one time is sufficient.
     */
    while (TRUE)
    {
	long	elapsed_msec = 0;
	int	waitnow;

	if (sd >= 0)
	    sock_close(sd);
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
	    ch_error(channel, "in socket() in channel_open().");
	    PERROR(_("E898: socket() in channel_open()"));
	    channel_free(channel);
	    return NULL;
	}

	if (waittime >= 0)
	{
	    /* Make connect() non-blocking. */
	    if (
#ifdef _WIN32
		ioctlsocket(sd, FIONBIO, &val) < 0
#else
		fcntl(sd, F_SETFL, O_NONBLOCK) < 0
#endif
	       )
	    {
		SOCK_ERRNO;
		ch_error(channel,
			 "channel_open: Connect failed with errno %d", errno);
		sock_close(sd);
		channel_free(channel);
		return NULL;
	    }
	}

	/* Try connecting to the server. */
	ch_log(channel, "Connecting to %s port %d", hostname, port);
	ret = connect(sd, (struct sockaddr *)&server, sizeof(server));

	if (ret == 0)
	    /* The connection could be established. */
	    break;

	SOCK_ERRNO;
	if (waittime < 0 || (errno != EWOULDBLOCK
		&& errno != ECONNREFUSED
#ifdef EINPROGRESS
		&& errno != EINPROGRESS
#endif
		))
	{
	    ch_error(channel,
			 "channel_open: Connect failed with errno %d", errno);
	    PERROR(_(e_cannot_connect));
	    sock_close(sd);
	    channel_free(channel);
	    return NULL;
	}

	/* Limit the waittime to 50 msec.  If it doesn't work within this
	 * time we close the socket and try creating it again. */
	waitnow = waittime > 50 ? 50 : waittime;

	/* If connect() didn't finish then try using select() to wait for the
	 * connection to be made. For Win32 always use select() to wait. */
#ifndef WIN32
	if (errno != ECONNREFUSED)
#endif
	{
	    struct timeval	tv;
	    fd_set		rfds;
	    fd_set		wfds;
#ifndef WIN32
	    int			so_error = 0;
	    socklen_t		so_error_len = sizeof(so_error);
	    struct timeval	start_tv;
	    struct timeval	end_tv;
#endif
	    FD_ZERO(&rfds);
	    FD_SET(sd, &rfds);
	    FD_ZERO(&wfds);
	    FD_SET(sd, &wfds);

	    tv.tv_sec = waitnow / 1000;
	    tv.tv_usec = (waitnow % 1000) * 1000;
#ifndef WIN32
	    gettimeofday(&start_tv, NULL);
#endif
	    ch_log(channel,
		    "Waiting for connection (waiting %d msec)...", waitnow);
	    ret = select((int)sd + 1, &rfds, &wfds, NULL, &tv);

	    if (ret < 0)
	    {
		SOCK_ERRNO;
		ch_error(channel,
			"channel_open: Connect failed with errno %d", errno);
		PERROR(_(e_cannot_connect));
		sock_close(sd);
		channel_free(channel);
		return NULL;
	    }

#ifdef WIN32
	    /* On Win32: select() is expected to work and wait for up to
	     * "waitnow" msec for the socket to be open. */
	    if (FD_ISSET(sd, &wfds))
		break;
	    elapsed_msec = waitnow;
	    if (waittime > 1 && elapsed_msec < waittime)
	    {
		waittime -= elapsed_msec;
		continue;
	    }
#else
	    /* On Linux-like systems: See socket(7) for the behavior
	     * After putting the socket in non-blocking mode, connect() will
	     * return EINPROGRESS, select() will not wait (as if writing is
	     * possible), need to use getsockopt() to check if the socket is
	     * actually able to connect.
	     * We detect a failure to connect when either read and write fds
	     * are set.  Use getsockopt() to find out what kind of failure. */
	    if (FD_ISSET(sd, &rfds) || FD_ISSET(sd, &wfds))
	    {
		ret = getsockopt(sd,
			      SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
		if (ret < 0 || (so_error != 0
			&& so_error != EWOULDBLOCK
			&& so_error != ECONNREFUSED
# ifdef EINPROGRESS
			&& so_error != EINPROGRESS
# endif
			))
		{
		    ch_error(channel,
			    "channel_open: Connect failed with errno %d",
			    so_error);
		    PERROR(_(e_cannot_connect));
		    sock_close(sd);
		    channel_free(channel);
		    return NULL;
		}
	    }

	    if (FD_ISSET(sd, &wfds) && so_error == 0)
		/* Did not detect an error, connection is established. */
		break;

	    gettimeofday(&end_tv, NULL);
	    elapsed_msec = (end_tv.tv_sec - start_tv.tv_sec) * 1000
			     + (end_tv.tv_usec - start_tv.tv_usec) / 1000;
#endif
	}

#ifndef WIN32
	if (waittime > 1 && elapsed_msec < waittime)
	{
	    /* The port isn't ready but we also didn't get an error.
	     * This happens when the server didn't open the socket
	     * yet.  Select() may return early, wait until the remaining
	     * "waitnow"  and try again. */
	    waitnow -= elapsed_msec;
	    waittime -= elapsed_msec;
	    if (waitnow > 0)
	    {
		mch_delay((long)waitnow, TRUE);
		ui_breakcheck();
		waittime -= waitnow;
	    }
	    if (!got_int)
	    {
		if (waittime <= 0)
		    /* give it one more try */
		    waittime = 1;
		continue;
	    }
	    /* we were interrupted, behave as if timed out */
	}
#endif

	/* We timed out. */
	ch_error(channel, "Connection timed out");
	sock_close(sd);
	channel_free(channel);
	return NULL;
    }

    ch_log(channel, "Connection made");

    if (waittime >= 0)
    {
#ifdef _WIN32
	val = 0;
	ioctlsocket(sd, FIONBIO, &val);
#else
	(void)fcntl(sd, F_SETFL, 0);
#endif
    }

    channel->CH_SOCK_FD = (sock_T)sd;
    channel->ch_nb_close_cb = nb_close_cb;
    channel->ch_hostname = (char *)vim_strsave((char_u *)hostname);
    channel->ch_port = port_in;
    channel->ch_to_be_closed |= (1U << PART_SOCK);

#ifdef FEAT_GUI
    channel_gui_register_one(channel, PART_SOCK);
#endif

    return channel;
}

/*
 * Implements ch_open().
 */
    channel_T *
channel_open_func(typval_T *argvars)
{
    char_u	*address;
    char_u	*p;
    char	*rest;
    int		port;
    jobopt_T    opt;
    channel_T	*channel = NULL;

    address = tv_get_string(&argvars[0]);
    if (argvars[1].v_type != VAR_UNKNOWN
	 && (argvars[1].v_type != VAR_DICT || argvars[1].vval.v_dict == NULL))
    {
	EMSG(_(e_invarg));
	return NULL;
    }

    /* parse address */
    p = vim_strchr(address, ':');
    if (p == NULL)
    {
	EMSG2(_(e_invarg2), address);
	return NULL;
    }
    *p++ = NUL;
    port = strtol((char *)p, &rest, 10);
    if (*address == NUL || port <= 0 || *rest != NUL)
    {
	p[-1] = ':';
	EMSG2(_(e_invarg2), address);
	return NULL;
    }

    /* parse options */
    clear_job_options(&opt);
    opt.jo_mode = MODE_JSON;
    opt.jo_timeout = 2000;
    if (get_job_options(&argvars[1], &opt,
	    JO_MODE_ALL + JO_CB_ALL + JO_WAITTIME + JO_TIMEOUT_ALL, 0) == FAIL)
	goto theend;
    if (opt.jo_timeout < 0)
    {
	EMSG(_(e_invarg));
	goto theend;
    }

    channel = channel_open((char *)address, port, opt.jo_waittime, NULL);
    if (channel != NULL)
    {
	opt.jo_set = JO_ALL;
	channel_set_options(channel, &opt);
    }
theend:
    free_job_options(&opt);
    return channel;
}

    static void
ch_close_part(channel_T *channel, ch_part_T part)
{
    sock_T *fd = &channel->ch_part[part].ch_fd;

    if (*fd != INVALID_FD)
    {
	if (part == PART_SOCK)
	    sock_close(*fd);
	else
	{
	    /* When using a pty the same FD is set on multiple parts, only
	     * close it when the last reference is closed. */
	    if ((part == PART_IN || channel->CH_IN_FD != *fd)
		    && (part == PART_OUT || channel->CH_OUT_FD != *fd)
		    && (part == PART_ERR || channel->CH_ERR_FD != *fd))
	    {
#ifdef WIN32
		if (channel->ch_named_pipe)
		    DisconnectNamedPipe((HANDLE)fd);
#endif
		fd_close(*fd);
	    }
	}
	*fd = INVALID_FD;

	/* channel is closed, may want to end the job if it was the last */
	channel->ch_to_be_closed &= ~(1U << part);
    }
}

    void
channel_set_pipes(channel_T *channel, sock_T in, sock_T out, sock_T err)
{
    if (in != INVALID_FD)
    {
	ch_close_part(channel, PART_IN);
	channel->CH_IN_FD = in;
# if defined(UNIX)
	/* Do not end the job when all output channels are closed, wait until
	 * the job ended. */
	if (isatty(in))
	    channel->ch_to_be_closed |= (1U << PART_IN);
# endif
    }
    if (out != INVALID_FD)
    {
# if defined(FEAT_GUI)
	channel_gui_unregister_one(channel, PART_OUT);
# endif
	ch_close_part(channel, PART_OUT);
	channel->CH_OUT_FD = out;
	channel->ch_to_be_closed |= (1U << PART_OUT);
# if defined(FEAT_GUI)
	channel_gui_register_one(channel, PART_OUT);
# endif
    }
    if (err != INVALID_FD)
    {
# if defined(FEAT_GUI)
	channel_gui_unregister_one(channel, PART_ERR);
# endif
	ch_close_part(channel, PART_ERR);
	channel->CH_ERR_FD = err;
	channel->ch_to_be_closed |= (1U << PART_ERR);
# if defined(FEAT_GUI)
	channel_gui_register_one(channel, PART_ERR);
# endif
    }
}

/*
 * Sets the job the channel is associated with and associated options.
 * This does not keep a refcount, when the job is freed ch_job is cleared.
 */
    void
channel_set_job(channel_T *channel, job_T *job, jobopt_T *options)
{
    channel->ch_job = job;

    channel_set_options(channel, options);

    if (job->jv_in_buf != NULL)
    {
	chanpart_T *in_part = &channel->ch_part[PART_IN];

	set_bufref(&in_part->ch_bufref, job->jv_in_buf);
	ch_log(channel, "reading from buffer '%s'",
				 (char *)in_part->ch_bufref.br_buf->b_ffname);
	if (options->jo_set & JO_IN_TOP)
	{
	    if (options->jo_in_top == 0 && !(options->jo_set & JO_IN_BOT))
	    {
		/* Special mode: send last-but-one line when appending a line
		 * to the buffer. */
		in_part->ch_bufref.br_buf->b_write_to_channel = TRUE;
		in_part->ch_buf_append = TRUE;
		in_part->ch_buf_top =
			    in_part->ch_bufref.br_buf->b_ml.ml_line_count + 1;
	    }
	    else
		in_part->ch_buf_top = options->jo_in_top;
	}
	else
	    in_part->ch_buf_top = 1;
	if (options->jo_set & JO_IN_BOT)
	    in_part->ch_buf_bot = options->jo_in_bot;
	else
	    in_part->ch_buf_bot = in_part->ch_bufref.br_buf->b_ml.ml_line_count;
    }
}

/*
 * Find a buffer matching "name" or create a new one.
 * Returns NULL if there is something very wrong (error already reported).
 */
    static buf_T *
find_buffer(char_u *name, int err, int msg)
{
    buf_T *buf = NULL;
    buf_T *save_curbuf = curbuf;

    if (name != NULL && *name != NUL)
    {
	buf = buflist_findname(name);
	if (buf == NULL)
	    buf = buflist_findname_exp(name);
    }
    if (buf == NULL)
    {
	buf = buflist_new(name == NULL || *name == NUL ? NULL : name,
				     NULL, (linenr_T)0, BLN_LISTED | BLN_NEW);
	if (buf == NULL)
	    return NULL;
	buf_copy_options(buf, BCO_ENTER);
	curbuf = buf;
#ifdef FEAT_QUICKFIX
	set_option_value((char_u *)"bt", 0L, (char_u *)"nofile", OPT_LOCAL);
	set_option_value((char_u *)"bh", 0L, (char_u *)"hide", OPT_LOCAL);
#endif
	if (curbuf->b_ml.ml_mfp == NULL)
	    ml_open(curbuf);
	if (msg)
	    ml_replace(1, (char_u *)(err ? "Reading from channel error..."
				   : "Reading from channel output..."), TRUE);
	changed_bytes(1, 0);
	curbuf = save_curbuf;
    }

    return buf;
}

    static void
set_callback(
	char_u **cbp,
	partial_T **pp,
	char_u *callback,
	partial_T *partial)
{
    free_callback(*cbp, *pp);
    if (callback != NULL && *callback != NUL)
    {
	if (partial != NULL)
	    *cbp = partial_name(partial);
	else
	{
	    *cbp = vim_strsave(callback);
	    func_ref(*cbp);
	}
    }
    else
	*cbp = NULL;
    *pp = partial;
    if (partial != NULL)
	++partial->pt_refcount;
}

/*
 * Set various properties from an "opt" argument.
 */
    void
channel_set_options(channel_T *channel, jobopt_T *opt)
{
    ch_part_T	part;

    if (opt->jo_set & JO_MODE)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	    channel->ch_part[part].ch_mode = opt->jo_mode;
    if (opt->jo_set & JO_IN_MODE)
	channel->ch_part[PART_IN].ch_mode = opt->jo_in_mode;
    if (opt->jo_set & JO_OUT_MODE)
	channel->ch_part[PART_OUT].ch_mode = opt->jo_out_mode;
    if (opt->jo_set & JO_ERR_MODE)
	channel->ch_part[PART_ERR].ch_mode = opt->jo_err_mode;
    channel->ch_nonblock = opt->jo_noblock;

    if (opt->jo_set & JO_TIMEOUT)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	    channel->ch_part[part].ch_timeout = opt->jo_timeout;
    if (opt->jo_set & JO_OUT_TIMEOUT)
	channel->ch_part[PART_OUT].ch_timeout = opt->jo_out_timeout;
    if (opt->jo_set & JO_ERR_TIMEOUT)
	channel->ch_part[PART_ERR].ch_timeout = opt->jo_err_timeout;
    if (opt->jo_set & JO_BLOCK_WRITE)
	channel->ch_part[PART_IN].ch_block_write = 1;

    if (opt->jo_set & JO_CALLBACK)
	set_callback(&channel->ch_callback, &channel->ch_partial,
					   opt->jo_callback, opt->jo_partial);
    if (opt->jo_set & JO_OUT_CALLBACK)
	set_callback(&channel->ch_part[PART_OUT].ch_callback,
		&channel->ch_part[PART_OUT].ch_partial,
		opt->jo_out_cb, opt->jo_out_partial);
    if (opt->jo_set & JO_ERR_CALLBACK)
	set_callback(&channel->ch_part[PART_ERR].ch_callback,
		&channel->ch_part[PART_ERR].ch_partial,
		opt->jo_err_cb, opt->jo_err_partial);
    if (opt->jo_set & JO_CLOSE_CALLBACK)
	set_callback(&channel->ch_close_cb, &channel->ch_close_partial,
		opt->jo_close_cb, opt->jo_close_partial);
    channel->ch_drop_never = opt->jo_drop_never;

    if ((opt->jo_set & JO_OUT_IO) && opt->jo_io[PART_OUT] == JIO_BUFFER)
    {
	buf_T *buf;

	/* writing output to a buffer. Default mode is NL. */
	if (!(opt->jo_set & JO_OUT_MODE))
	    channel->ch_part[PART_OUT].ch_mode = MODE_NL;
	if (opt->jo_set & JO_OUT_BUF)
	{
	    buf = buflist_findnr(opt->jo_io_buf[PART_OUT]);
	    if (buf == NULL)
		EMSGN(_(e_nobufnr), (long)opt->jo_io_buf[PART_OUT]);
	}
	else
	{
	    int msg = TRUE;

	    if (opt->jo_set2 & JO2_OUT_MSG)
		msg = opt->jo_message[PART_OUT];
	    buf = find_buffer(opt->jo_io_name[PART_OUT], FALSE, msg);
	}
	if (buf != NULL)
	{
	    if (opt->jo_set & JO_OUT_MODIFIABLE)
		channel->ch_part[PART_OUT].ch_nomodifiable =
						!opt->jo_modifiable[PART_OUT];

	    if (!buf->b_p_ma && !channel->ch_part[PART_OUT].ch_nomodifiable)
	    {
		EMSG(_(e_modifiable));
	    }
	    else
	    {
		ch_log(channel, "writing out to buffer '%s'",
						       (char *)buf->b_ffname);
		set_bufref(&channel->ch_part[PART_OUT].ch_bufref, buf);
	    }
	}
    }

    if ((opt->jo_set & JO_ERR_IO) && (opt->jo_io[PART_ERR] == JIO_BUFFER
	 || (opt->jo_io[PART_ERR] == JIO_OUT && (opt->jo_set & JO_OUT_IO)
				       && opt->jo_io[PART_OUT] == JIO_BUFFER)))
    {
	buf_T *buf;

	/* writing err to a buffer. Default mode is NL. */
	if (!(opt->jo_set & JO_ERR_MODE))
	    channel->ch_part[PART_ERR].ch_mode = MODE_NL;
	if (opt->jo_io[PART_ERR] == JIO_OUT)
	    buf = channel->ch_part[PART_OUT].ch_bufref.br_buf;
	else if (opt->jo_set & JO_ERR_BUF)
	{
	    buf = buflist_findnr(opt->jo_io_buf[PART_ERR]);
	    if (buf == NULL)
		EMSGN(_(e_nobufnr), (long)opt->jo_io_buf[PART_ERR]);
	}
	else
	{
	    int msg = TRUE;

	    if (opt->jo_set2 & JO2_ERR_MSG)
		msg = opt->jo_message[PART_ERR];
	    buf = find_buffer(opt->jo_io_name[PART_ERR], TRUE, msg);
	}
	if (buf != NULL)
	{
	    if (opt->jo_set & JO_ERR_MODIFIABLE)
		channel->ch_part[PART_ERR].ch_nomodifiable =
						!opt->jo_modifiable[PART_ERR];
	    if (!buf->b_p_ma && !channel->ch_part[PART_ERR].ch_nomodifiable)
	    {
		EMSG(_(e_modifiable));
	    }
	    else
	    {
		ch_log(channel, "writing err to buffer '%s'",
						       (char *)buf->b_ffname);
		set_bufref(&channel->ch_part[PART_ERR].ch_bufref, buf);
	    }
	}
    }

    channel->ch_part[PART_OUT].ch_io = opt->jo_io[PART_OUT];
    channel->ch_part[PART_ERR].ch_io = opt->jo_io[PART_ERR];
    channel->ch_part[PART_IN].ch_io = opt->jo_io[PART_IN];
}

/*
 * Set the callback for "channel"/"part" for the response with "id".
 */
    void
channel_set_req_callback(
	channel_T   *channel,
	ch_part_T   part,
	char_u	    *callback,
	partial_T   *partial,
	int	    id)
{
    cbq_T *head = &channel->ch_part[part].ch_cb_head;
    cbq_T *item = (cbq_T *)alloc((int)sizeof(cbq_T));

    if (item != NULL)
    {
	item->cq_partial = partial;
	if (partial != NULL)
	{
	    ++partial->pt_refcount;
	    item->cq_callback = callback;
	}
	else
	{
	    item->cq_callback = vim_strsave(callback);
	    func_ref(item->cq_callback);
	}
	item->cq_seq_nr = id;
	item->cq_prev = head->cq_prev;
	head->cq_prev = item;
	item->cq_next = NULL;
	if (item->cq_prev == NULL)
	    head->cq_next = item;
	else
	    item->cq_prev->cq_next = item;
    }
}

    static void
write_buf_line(buf_T *buf, linenr_T lnum, channel_T *channel)
{
    char_u  *line = ml_get_buf(buf, lnum, FALSE);
    int	    len = (int)STRLEN(line);
    char_u  *p;
    int	    i;

    /* Need to make a copy to be able to append a NL. */
    if ((p = alloc(len + 2)) == NULL)
	return;
    memcpy((char *)p, (char *)line, len);

    if (channel->ch_write_text_mode)
	p[len] = CAR;
    else
    {
	for (i = 0; i < len; ++i)
	    if (p[i] == NL)
		p[i] = NUL;

	p[len] = NL;
    }
    p[len + 1] = NUL;
    channel_send(channel, PART_IN, p, len + 1, "write_buf_line");
    vim_free(p);
}

/*
 * Return TRUE if "channel" can be written to.
 * Returns FALSE if the input is closed or the write would block.
 */
    static int
can_write_buf_line(channel_T *channel)
{
    chanpart_T *in_part = &channel->ch_part[PART_IN];

    if (in_part->ch_fd == INVALID_FD)
	return FALSE;  /* pipe was closed */

    /* for testing: block every other attempt to write */
    if (in_part->ch_block_write == 1)
	in_part->ch_block_write = -1;
    else if (in_part->ch_block_write == -1)
	in_part->ch_block_write = 1;

    /* TODO: Win32 implementation, probably using WaitForMultipleObjects() */
#ifndef WIN32
    {
# if defined(HAVE_SELECT)
	struct timeval	tval;
	fd_set		wfds;
	int		ret;

	FD_ZERO(&wfds);
	FD_SET((int)in_part->ch_fd, &wfds);
	tval.tv_sec = 0;
	tval.tv_usec = 0;
	for (;;)
	{
	    ret = select((int)in_part->ch_fd + 1, NULL, &wfds, NULL, &tval);
#  ifdef EINTR
	    SOCK_ERRNO;
	    if (ret == -1 && errno == EINTR)
		continue;
#  endif
	    if (ret <= 0 || in_part->ch_block_write == 1)
	    {
		if (ret > 0)
		    ch_log(channel, "FAKED Input not ready for writing");
		else
		    ch_log(channel, "Input not ready for writing");
		return FALSE;
	    }
	    break;
	}
# else
	struct pollfd	fds;

	fds.fd = in_part->ch_fd;
	fds.events = POLLOUT;
	if (poll(&fds, 1, 0) <= 0)
	{
	    ch_log(channel, "Input not ready for writing");
	    return FALSE;
	}
	if (in_part->ch_block_write == 1)
	{
	    ch_log(channel, "FAKED Input not ready for writing");
	    return FALSE;
	}
# endif
    }
#endif
    return TRUE;
}

/*
 * Write any buffer lines to the input channel.
 */
    static void
channel_write_in(channel_T *channel)
{
    chanpart_T *in_part = &channel->ch_part[PART_IN];
    linenr_T    lnum;
    buf_T	*buf = in_part->ch_bufref.br_buf;
    int		written = 0;

    if (buf == NULL || in_part->ch_buf_append)
	return;  /* no buffer or using appending */
    if (!bufref_valid(&in_part->ch_bufref) || buf->b_ml.ml_mfp == NULL)
    {
	/* buffer was wiped out or unloaded */
	ch_log(channel, "input buffer has been wiped out");
	in_part->ch_bufref.br_buf = NULL;
	return;
    }

    for (lnum = in_part->ch_buf_top; lnum <= in_part->ch_buf_bot
				   && lnum <= buf->b_ml.ml_line_count; ++lnum)
    {
	if (!can_write_buf_line(channel))
	    break;
	write_buf_line(buf, lnum, channel);
	++written;
    }

    if (written == 1)
	ch_log(channel, "written line %d to channel", (int)lnum - 1);
    else if (written > 1)
	ch_log(channel, "written %d lines to channel", written);

    in_part->ch_buf_top = lnum;
    if (lnum > buf->b_ml.ml_line_count || lnum > in_part->ch_buf_bot)
    {
#if defined(FEAT_TERMINAL)
	/* Send CTRL-D or "eof_chars" to close stdin on MS-Windows. */
	if (channel->ch_job != NULL)
	    term_send_eof(channel);
#endif

	/* Writing is done, no longer need the buffer. */
	in_part->ch_bufref.br_buf = NULL;
	ch_log(channel, "Finished writing all lines to channel");

	/* Close the pipe/socket, so that the other side gets EOF. */
	ch_close_part(channel, PART_IN);
    }
    else
	ch_log(channel, "Still %ld more lines to write",
				   (long)(buf->b_ml.ml_line_count - lnum + 1));
}

/*
 * Handle buffer "buf" being freed, remove it from any channels.
 */
    void
channel_buffer_free(buf_T *buf)
{
    channel_T	*channel;
    ch_part_T	part;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	for (part = PART_SOCK; part < PART_COUNT; ++part)
	{
	    chanpart_T  *ch_part = &channel->ch_part[part];

	    if (ch_part->ch_bufref.br_buf == buf)
	    {
		ch_log(channel, "%s buffer has been wiped out",
							    part_names[part]);
		ch_part->ch_bufref.br_buf = NULL;
	    }
	}
}

/*
 * Write any lines waiting to be written to "channel".
 */
    static void
channel_write_input(channel_T *channel)
{
    chanpart_T	*in_part = &channel->ch_part[PART_IN];

    if (in_part->ch_writeque.wq_next != NULL)
	channel_send(channel, PART_IN, (char_u *)"", 0, "channel_write_input");
    else if (in_part->ch_bufref.br_buf != NULL)
    {
	if (in_part->ch_buf_append)
	    channel_write_new_lines(in_part->ch_bufref.br_buf);
	else
	    channel_write_in(channel);
    }
}

/*
 * Write any lines waiting to be written to a channel.
 */
    void
channel_write_any_lines(void)
{
    channel_T	*channel;

    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
	channel_write_input(channel);
}

/*
 * Write appended lines above the last one in "buf" to the channel.
 */
    void
channel_write_new_lines(buf_T *buf)
{
    channel_T	*channel;
    int		found_one = FALSE;

    /* There could be more than one channel for the buffer, loop over all of
     * them. */
    for (channel = first_channel; channel != NULL; channel = channel->ch_next)
    {
	chanpart_T  *in_part = &channel->ch_part[PART_IN];
	linenr_T    lnum;
	int	    written = 0;

	if (in_part->ch_bufref.br_buf == buf && in_part->ch_buf_append)
	{
	    if (in_part->ch_fd == INVALID_FD)
		continue;  /* pipe was closed */
	    found_one = TRUE;
	    for (lnum = in_part->ch_buf_bot; lnum < buf->b_ml.ml_line_count;
								       ++lnum)
	    {
		if (!can_write_buf_line(channel))
		    break;
		write_buf_line(buf, lnum, channel);
		++written;
	    }

	    if (written == 1)
		ch_log(channel, "written line %d to channel", (int)lnum - 1);
	    else if (written > 1)
		ch_log(channel, "written %d lines to channel", written);
	    if (lnum < buf->b_ml.ml_line_count)
		ch_log(channel, "Still %ld more lines to write",
				       (long)(buf->b_ml.ml_line_count - lnum));

	    in_part->ch_buf_bot = lnum;
	}
    }
    if (!found_one)
	buf->b_write_to_channel = FALSE;
}

/*
 * Invoke the "callback" on channel "channel".
 * This does not redraw but sets channel_need_redraw;
 */
    static void
invoke_callback(channel_T *channel, char_u *callback, partial_T *partial,
							       typval_T *argv)
{
    typval_T	rettv;
    int		dummy;

    if (safe_to_invoke_callback == 0)
	IEMSG("INTERNAL: Invoking callback when it is not safe");

    argv[0].v_type = VAR_CHANNEL;
    argv[0].vval.v_channel = channel;

    call_func(callback, (int)STRLEN(callback), &rettv, 2, argv, NULL,
					  0L, 0L, &dummy, TRUE, partial, NULL);
    clear_tv(&rettv);
    channel_need_redraw = TRUE;
}

/*
 * Return the first node from "channel"/"part" without removing it.
 * Returns NULL if there is nothing.
 */
    readq_T *
channel_peek(channel_T *channel, ch_part_T part)
{
    readq_T *head = &channel->ch_part[part].ch_head;

    return head->rq_next;
}

/*
 * Return a pointer to the first NL in "node".
 * Skips over NUL characters.
 * Returns NULL if there is no NL.
 */
    char_u *
channel_first_nl(readq_T *node)
{
    char_u  *buffer = node->rq_buffer;
    long_u  i;

    for (i = 0; i < node->rq_buflen; ++i)
	if (buffer[i] == NL)
	    return buffer + i;
    return NULL;
}

/*
 * Return the first buffer from channel "channel"/"part" and remove it.
 * The caller must free it.
 * Returns NULL if there is nothing.
 */
    char_u *
channel_get(channel_T *channel, ch_part_T part)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    char_u *p;

    if (node == NULL)
	return NULL;
    /* dispose of the node but keep the buffer */
    p = node->rq_buffer;
    head->rq_next = node->rq_next;
    if (node->rq_next == NULL)
	head->rq_prev = NULL;
    else
	node->rq_next->rq_prev = NULL;
    vim_free(node);
    return p;
}

/*
 * Returns the whole buffer contents concatenated for "channel"/"part".
 * Replaces NUL bytes with NL.
 */
    static char_u *
channel_get_all(channel_T *channel, ch_part_T part)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    long_u  len = 0;
    char_u  *res;
    char_u  *p;

    /* If there is only one buffer just get that one. */
    if (head->rq_next == NULL || head->rq_next->rq_next == NULL)
	return channel_get(channel, part);

    /* Concatenate everything into one buffer. */
    for (node = head->rq_next; node != NULL; node = node->rq_next)
	len += node->rq_buflen;
    res = lalloc(len + 1, TRUE);
    if (res == NULL)
	return NULL;
    p = res;
    for (node = head->rq_next; node != NULL; node = node->rq_next)
    {
	mch_memmove(p, node->rq_buffer, node->rq_buflen);
	p += node->rq_buflen;
    }
    *p = NUL;

    /* Free all buffers */
    do
    {
	p = channel_get(channel, part);
	vim_free(p);
    } while (p != NULL);

    /* turn all NUL into NL */
    while (len > 0)
    {
	--len;
	if (res[len] == NUL)
	    res[len] = NL;
    }

    return res;
}

/*
 * Consume "len" bytes from the head of "node".
 * Caller must check these bytes are available.
 */
    void
channel_consume(channel_T *channel, ch_part_T part, int len)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    char_u *buf = node->rq_buffer;

    mch_memmove(buf, buf + len, node->rq_buflen - len);
    node->rq_buflen -= len;
}

/*
 * Collapses the first and second buffer for "channel"/"part".
 * Returns FAIL if that is not possible.
 * When "want_nl" is TRUE collapse more buffers until a NL is found.
 */
    int
channel_collapse(channel_T *channel, ch_part_T part, int want_nl)
{
    readq_T *head = &channel->ch_part[part].ch_head;
    readq_T *node = head->rq_next;
    readq_T *last_node;
    readq_T *n;
    char_u  *newbuf;
    char_u  *p;
    long_u len;

    if (node == NULL || node->rq_next == NULL)
	return FAIL;

    last_node = node->rq_next;
    len = node->rq_buflen + last_node->rq_buflen + 1;
    if (want_nl)
	while (last_node->rq_next != NULL
		&& channel_first_nl(last_node) == NULL)
	{
	    last_node = last_node->rq_next;
	    len += last_node->rq_buflen;
	}

    p = newbuf = alloc(len);
    if (newbuf == NULL)
	return FAIL;	    /* out of memory */
    mch_memmove(p, node->rq_buffer, node->rq_buflen);
    p += node->rq_buflen;
    vim_free(node->rq_buffer);
    node->rq_buffer = newbuf;
    for (n = node; n != last_node; )
    {
	n = n->rq_next;
	mch_memmove(p, n->rq_buffer, n->rq_buflen);
	p += n->rq_buflen;
	vim_free(n->rq_buffer);
    }
    node->rq_buflen = (long_u)(p - newbuf);

    /* dispose of the collapsed nodes and their buffers */
    for (n = node->rq_next; n != last_node; )
    {
	n = n->rq_next;
	vim_free(n->rq_prev);
