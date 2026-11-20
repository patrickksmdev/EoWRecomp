// Shader preparation (first launch): the game's shader variants are translated, compiled and built into pipelines
// before the game starts, so a first visit anywhere has no compile hitch or pop-in. The approach follows
// LASRecomp / SuperMarioWonderRecomp's ahead-of-time shaders.
//
// What ships is only a list of variant records (runtime/nvn/shader_variants.txt): pipeline / compute state in NVN terms
// (formats, vertex layout, blend state, ...) plus each shader's identity as a one-way hash of its code bytes. No
// shader code ships in any form: the code is found by that hash in the player's own game files (the BNSH shader
// containers in the romfs' shader archives, effects and UI archives, and in the main module's data), and the backend
// builds the variants on the player's machine through the same code its draws use.
//
// EOWRECOMP_RECORD_KEYS=<file> (dev): every program / pipeline / compute variant the game builds is appended to <file>.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { SR_FLIP_Y = 1, SR_DEPTH_MINUS_ONE = 2, SR_FLIP_X = 4 };

typedef struct {
    char kind;               // 'G' graphics pipeline, 'C' compute program
    uint64_t vs, fs;         // code hashes (shader_code_hash); compute: vs
    uint8_t flags;           // SR_*: lower-left window origin, NVN depth mode [-1, 1], mirrored composite
    uint32_t vs_stencil;     // ShaderKey.stencil_tex for VS
    uint32_t fs_stencil;     // ShaderKey.stencil_tex for FS
    uint8_t nattrib, nstream, ncolor;
    uint32_t attrib[16];     // NVN vertex attribute state: format | stream << 8 | offset << 13
    uint32_t stream[16][2];  // vertex stream stride, divisor
    uint32_t color[8];       // render target texture formats (0: no target)
    uint32_t cview[8];       // render target view formats (0: the texture's own)
    uint32_t depth;          // depth / stencil target format (0: none)
    uint32_t samples;        // samples of the targets (1: single-sampled)
    uint32_t channel_mask;   // color write masks, 4 bits per target
    uint8_t blend_on;        // blending enabled, 1 bit per target
    uint8_t blend[8][8];     // NVN blend state per target (zero where blending is off)
    uint8_t a2c;             // alpha to coverage
    uint8_t topo;            // primitive class: 0 points, 1 lines, 2 triangles
    uint8_t poly_fill;       // filled polygons (0: lines)
    uint32_t local[3];       // compute: local size
} ShaderRec;

// The record's text form (one line, no newline) and back
void shader_rec_format(const ShaderRec *r, char *out, size_t n);
bool shader_rec_parse(const char *line, ShaderRec *r);

// One-way hash of a shader's code bytes: what identifies a shader in the records
uint64_t shader_code_hash(const uint8_t *code, size_t n);
struct NvnShaderStage;
uint64_t shader_stage_hash(const struct NvnShaderStage *s);
uint8_t shader_topo_class(uint32_t mode);

// Stale-program detector
typedef struct ShaderFp {
    uint64_t hash;
    uint32_t len;
    uint32_t w[4];
    uint8_t state;
} ShaderFp;
void shader_fp_check(ShaderFp *fp, const struct NvnShaderStage *s, const char *stage);

// EOWRECOMP_RECORD_KEYS: note a variant the game builds
bool shader_rec_recording(void);
void shader_rec_note(const ShaderRec *r);

// Player's own list (cache/shader_seen.txt)
bool shader_seen_on(void);
void shader_seen_note(const ShaderRec *r);
bool shader_seen_shipped(const char *line);
char *shader_seen_read(uint64_t *file_game);
void shader_seen_prep_done(uint64_t game, const char *keep);
const char *shader_prep_shipped_list(void);

// First visits
void shader_first_visit(const char *what, uint64_t a, uint64_t b, uint64_t id);

// ---- preparation
typedef struct {
    const uint8_t *code;
    uint32_t size;
    const uint8_t *control;
    uint32_t const_offset, const_size;
} ShaderBlob;

bool renderer_prepare_begin(void);
bool renderer_prepare(const ShaderRec *r, const ShaderBlob *vs, const ShaderBlob *fs);
int renderer_prepare_pending(void);
void renderer_prepare_end(void);

bool shader_prep_run(const char *romfs, void (*progress)(int done, int total), const volatile int *cancel);
void shader_prep_boot(const char *romfs);

#define SHADER_CACHE_VERSION 1
void shader_cache_tag(char *out, size_t n, const char *gpu);
bool shader_cache_tag_ok(const char *tag_path, const char *tag);
void shader_cache_tag_write(const char *tag_path, const char *tag);
bool shader_cache_write(const char *path, const void *data, size_t n);
