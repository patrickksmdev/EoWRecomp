// The player's own shader list (shader_prep.h): every variant this player's game builds that the shipped list doesn't
// have, kept in cache/shader_seen.txt and prepared after the shipped list on the next start.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "nvn.h"
#include "platform.h"
#include "shader_prep.h"

#define SEEN_FILE "cache/shader_seen.txt"
#define SEEN_MAX 20000

bool shader_seen_on(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("EOWRECOMP_SHADER_SEEN");
        v = !(e && *e && !atoi(e));
    }
    return v;
}

typedef struct { uint64_t *t; size_t cap, n; } HSet;

static uint64_t line_hash(const char *l, size_t n) {
    uint64_t h = shader_code_hash((const uint8_t *)l, n);
    return h ? h : 1;
}

static bool hs_add(HSet *s, uint64_t h) {
    if (s->n * 2 >= s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 4096;
        uint64_t *t = calloc(cap, sizeof *t);
        for (size_t i = 0; i < s->cap; i++)
            if (s->t[i]) {
                size_t j = s->t[i] & (cap - 1);
                while (t[j]) j = (j + 1) & (cap - 1);
                t[j] = s->t[i];
            }
        free(s->t);
        s->t = t, s->cap = cap;
    }
    size_t j = h & (s->cap - 1);
    while (s->t[j]) {
        if (s->t[j] == h) return false;
        j = (j + 1) & (s->cap - 1);
    }
    s->t[j] = h;
    s->n++;
    return true;
}

static bool hs_has(const HSet *s, uint64_t h) {
    if (!s->cap) return false;
    for (size_t j = h & (s->cap - 1); s->t[j]; j = (j + 1) & (s->cap - 1))
        if (s->t[j] == h) return true;
    return false;
}

static void each_record(const char *text, void (*fn)(const char *line, size_t n, void *ctx), void *ctx) {
    for (const char *p = text; p && *p;) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        size_t m = n;
        while (m && (p[m - 1] == '\r' || p[m - 1] == ' ')) m--;
        if (m && (p[0] == 'G' || p[0] == 'C')) fn(p, m, ctx);
        p += n + (e != NULL);
    }
}

static void add_line(const char *l, size_t n, void *s) { hs_add(s, line_hash(l, n)); }

static HSet shipped;
static void shipped_load_once(void) { each_record(shader_prep_shipped_list(), add_line, &shipped); }
static void shipped_load(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, shipped_load_once);
}

bool shader_seen_shipped(const char *line) {
    shipped_load();
    return hs_has(&shipped, line_hash(line, strlen(line)));
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *d = n >= 0 ? calloc(1, (size_t)n + 1) : NULL;
    if (d && n > 0 && fread(d, 1, (size_t)n, f) != (size_t)n) d[0] = 0;
    fclose(f);
    return d;
}

static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER;
static HSet q_seen;
static char **q_tab;
static size_t q_n, q_cap;
static bool writer_started;

static HSet known;
static bool known_loaded;
static size_t file_n;
static uint64_t game_id;

static void known_load(void) {
    if (known_loaded) return;
    known_loaded = true;
    shipped_load();
    for (size_t i = 0; i < shipped.cap; i++)
        if (shipped.t[i]) hs_add(&known, shipped.t[i]);
    char *d = read_all(SEEN_FILE);
    size_t before = known.n;
    each_record(d, add_line, &known);
    file_n = known.n - before;
    free(d);
}

static void write_batch(void) {
    pthread_mutex_lock(&q_lock);
    char **tab = q_tab;
    size_t n = q_n;
    q_tab = NULL, q_n = q_cap = 0;
    pthread_mutex_unlock(&q_lock);
    if (!n) return;
    known_load();
    FILE *f = NULL;
    for (size_t i = 0; i < n; i++) {
        uint64_t h = line_hash(tab[i], strlen(tab[i]));
        if (file_n < SEEN_MAX && !hs_has(&known, h)) {
            if (!f) {
                struct stat sb;
                mkdir("cache", 0755);
                bool fresh = stat(SEEN_FILE, &sb) != 0 || sb.st_size == 0;
                f = fopen(SEEN_FILE, "ab");
                if (f && fresh && game_id) fprintf(f, "# shader_seen game %016llx\n", (unsigned long long)game_id);
            }
            if (f && fprintf(f, "%s\n", tab[i]) > 0) { hs_add(&known, h); file_n++; }
            if (file_n == SEEN_MAX) LOG("[shader] seen list full (%d records)\n", SEEN_MAX);
        }
        free(tab[i]);
    }
    free(tab);
    if (f) fclose(f);
}

static void *writer(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&q_lock);
        while (!q_n) pthread_cond_wait(&q_cond, &q_lock);
        pthread_mutex_unlock(&q_lock);
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&f_lock);
        write_batch();
        pthread_mutex_unlock(&f_lock);
    }
    return NULL;
}

static void at_exit(void) {
    if (pthread_mutex_trylock(&f_lock)) return;
    write_batch();
    pthread_mutex_unlock(&f_lock);
}

void shader_seen_note(const ShaderRec *r) {
    if (!shader_seen_on() || !r->vs || (r->kind == 'G' && !r->fs)) return;
    char line[2048];
    shader_rec_format(r, line, sizeof line);
    pthread_mutex_lock(&q_lock);
    if (hs_add(&q_seen, line_hash(line, strlen(line)))) {
        if (!shader_seen_shipped(line)) LOG("[shader] record %s\n", line);
        if (q_n == q_cap) q_tab = realloc(q_tab, sizeof *q_tab * (q_cap = q_cap ? q_cap * 2 : 256));
        q_tab[q_n++] = strdup(line);
        if (!writer_started) {
            writer_started = true;
            pthread_t t;
            if (pthread_create(&t, NULL, writer, NULL) == 0) pthread_detach(t);
            atexit(at_exit);
        }
        pthread_cond_signal(&q_cond);
    }
    pthread_mutex_unlock(&q_lock);
}

char *shader_seen_read(uint64_t *file_game) {
    *file_game = 0;
    if (!shader_seen_on()) return NULL;
    pthread_mutex_lock(&f_lock);
    char *d = read_all(SEEN_FILE);
    pthread_mutex_unlock(&f_lock);
    unsigned long long g;
    if (d && sscanf(d, "# shader_seen game %llx", &g) == 1) *file_game = g;
    return d;
}

void shader_seen_prep_done(uint64_t game, const char *keep) {
    if (!shader_seen_on()) return;
    pthread_mutex_lock(&f_lock);
    game_id = game;
    if (keep) {
        size_t n = strlen(keep);
        char *d = malloc(n + 64);
        int o = snprintf(d, 64, "# shader_seen game %016llx\n", (unsigned long long)game);
        memcpy(d + o, keep, n);
        shader_cache_write(SEEN_FILE, d, (size_t)o + n);
        free(d);
        if (known_loaded) {
            HSet k = {0};
            each_record(keep, add_line, &k);
            file_n = k.n;
            free(k.t);
        }
    }
    pthread_mutex_unlock(&f_lock);
}
