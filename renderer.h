#ifndef RENDERER_H
#define RENDERER_H

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long long uint64_t;

#include "ecs.h"

// -------------------------------------------------------------
// Rendering Structures
// -------------------------------------------------------------
typedef struct {
  float x, y;
  float u, v;
  float r, g, b, a;
} Vertex;

typedef struct {
  float pan_x;
  float pan_y;
  float zoom;
  float pad1;
  float screen_width;
  float screen_height;
  float pad2[2];
} Uniforms;

typedef struct {
  int pipeline_id;
  int texture_id;
  unsigned int index_start;
  unsigned int index_count;
} DrawBatch;

// -------------------------------------------------------------
// Constants
// -------------------------------------------------------------
#define MAX_VERTICES 8200000
#define MAX_INDICES 12300000
#define MAX_BATCHES 5000

// -------------------------------------------------------------
// Shared Global State (defined in renderer.c)
// -------------------------------------------------------------
extern Vertex vertex_buffer[MAX_VERTICES];
extern uint32_t index_buffer[MAX_INDICES];
extern int vertex_count;
extern int index_count;

extern Uniforms uniforms;

extern DrawBatch batches[MAX_BATCHES];
extern int batch_count;
extern unsigned int last_batch_index_start;

extern int vertex_buffer_id;
extern int index_buffer_id;
extern int uniform_buffer_id;
extern int texture_id;

extern float current_time_ms;
extern int needs_redraw;

extern int active_stroke_node_idx;

// -------------------------------------------------------------
// Standard Runtime replacements
// -------------------------------------------------------------
int strlen(const char *str);
char *strcpy(char *dest, const char *src);
void *memcpy(void *dest, const void *src, int n);
void *memset(void *dest, int val, int n);
float float_sqrt(float val);
void log_str(const char *str);

// -------------------------------------------------------------
// Drawing Primitives and Helpers
// -------------------------------------------------------------
uint32_t decode_utf8(const char **str);
void push_vertex(float x, float y, float u, float v, float r, float g, float b, float a);
void push_index(uint32_t idx);
void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void draw_line(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a);
void draw_char(uint32_t codepoint, float x, float y, float w, float h, float r, float g, float b, float a);
void flush_batch(int pipeline_id, int bind_texture_id);
float get_char_advance(uint32_t codepoint);
void mark_dirty();
void upload_uniforms_if_changed();

// -------------------------------------------------------------
// JS/WebGPU imports (declared with WASM_IMPORT in canvas.c/renderer.c)
// -------------------------------------------------------------
#define WASM_IMPORT(name) __attribute__((import_module("env"), import_name(name)))

WASM_IMPORT("js_console_log") void js_console_log(const char *ptr, int len);
WASM_IMPORT("js_wgpu_write_buffer") void js_wgpu_write_buffer(int buffer_id, int offset, const void *data_ptr, int size);

#endif // RENDERER_H
