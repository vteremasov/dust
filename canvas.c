// WebGPU Infinite Canvas (Miro Clone) in Pure C (No Emscripten)
// Compiled directly to target wasm32 using clang

#define WASM_IMPORT(name)                                                      \
  __attribute__((import_module("env"), import_name(name)))

// Custom JS interface imports
WASM_IMPORT("js_console_log") void js_console_log(const char *ptr, int len);
WASM_IMPORT("js_console_error") void js_console_error(const char *ptr, int len);
WASM_IMPORT("js_random_float") float js_random_float();
WASM_IMPORT("js_log_click_delta") void js_log_click_delta(float delta);
WASM_IMPORT("js_update_stats")
void js_update_stats(float pan_x, float pan_y, float zoom, int node_count);
WASM_IMPORT("js_set_editing_state")
void js_set_editing_state(int is_editing, float x, float y, float w, float h,
                          const char *text_ptr, int max_len, int idx);
WASM_IMPORT("js_init_node_texture")
void js_init_node_texture(int idx, const char *text_ptr, int type, float w,
                          float h);

// Thin WebGPU wrapper imports
WASM_IMPORT("js_wgpu_init") int js_wgpu_init(int width, int height);
WASM_IMPORT("js_wgpu_create_buffer")
int js_wgpu_create_buffer(int size, int usage);
WASM_IMPORT("js_wgpu_create_texture")
int js_wgpu_create_texture(int width, int height);
WASM_IMPORT("js_wgpu_write_buffer")
void js_wgpu_write_buffer(int buffer_id, int offset, const void *data_ptr,
                          int size);
WASM_IMPORT("js_wgpu_write_texture")
void js_wgpu_write_texture(int texture_id, int width, int height,
                           const void *data_ptr, int size);
WASM_IMPORT("js_wgpu_begin_render_pass") void js_wgpu_begin_render_pass();
WASM_IMPORT("js_wgpu_set_pipeline") void js_wgpu_set_pipeline(int pipeline_id);
WASM_IMPORT("js_wgpu_set_bind_group")
void js_wgpu_set_bind_group(int bind_group_id, int uniform_buffer_id,
                            int texture_id);
WASM_IMPORT("js_wgpu_draw_indexed")
void js_wgpu_draw_indexed(int index_count, int index_start, int index_buffer_id,
                          int vertex_buffer_id);
WASM_IMPORT("js_wgpu_end_render_pass") void js_wgpu_end_render_pass();

// Standard type definitions since we build with -nostdlib
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long long uint64_t;

#define NULL ((void *)0)

// Standard C runtime helper replacements
int strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

char *strcpy(char *dest, const char *src) {
  int i = 0;
  while (src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
  return dest;
}

void *memcpy(void *dest, const void *src, int n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  for (int i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *memset(void *dest, int val, int n) {
  uint8_t *d = (uint8_t *)dest;
  for (int i = 0; i < n; i++) {
    d[i] = (uint8_t)val;
  }
  return dest;
}

float float_sqrt(float val) { return __builtin_sqrtf(val); }

void log_str(const char *str) { js_console_log(str, strlen(str)); }

// -------------------------------------------------------------
// Font Atlas Builder
// -------------------------------------------------------------
#include "font.generated.h"
#include "widgets.h"

void add_widget_wasm(int type, float x, float y, int texture_id, int img_w,
                     int img_h);
void flush_batch(int pipeline_id, int bind_texture_id);

// UV coord for solid block (character index 1 in 64x64 cell layout)
#define SOLID_U 0.046875f
#define SOLID_V 0.015625f

// Grid size for 64x64 cell atlas layout
#define GRID_SIZE 32

uint32_t decode_utf8(const char **str);
const GlyphInfo* lookup_glyph(uint32_t codepoint);
float get_char_advance(uint32_t codepoint);

uint32_t decode_utf8(const char **str) {
  const unsigned char *s = (const unsigned char *)*str;
  if (*s == '\0') {
    return 0;
  }

  uint32_t codepoint = 0;
  int bytes = 0;

  unsigned char c = *s;
  if (c < 0x80) {
    codepoint = c;
    bytes = 1;
  } else if ((c & 0xE0) == 0xC0) {
    codepoint = c & 0x1F;
    bytes = 2;
  } else if ((c & 0xF0) == 0xE0) {
    codepoint = c & 0x0F;
    bytes = 3;
  } else if ((c & 0xF8) == 0xF0) {
    codepoint = c & 0x07;
    bytes = 4;
  } else {
    codepoint = c;
    bytes = 1;
  }

  for (int i = 1; i < bytes; i++) {
    if (s[i] == '\0' || (s[i] & 0xC0) != 0x80) {
      bytes = 1;
      codepoint = c;
      break;
    }
    codepoint = (codepoint << 6) | (s[i] & 0x3F);
  }

  *str += bytes;
  return codepoint;
}

const GlyphInfo* lookup_glyph(uint32_t codepoint) {
  int low = 0;
  int high = FONT_GLYPHS_COUNT - 1;
  while (low <= high) {
    int mid = low + (high - low) / 2;
    if (font_glyphs[mid].codepoint == codepoint) {
      return &font_glyphs[mid];
    } else if (font_glyphs[mid].codepoint < codepoint) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return NULL;
}

// -------------------------------------------------------------
// Immediate Mode Rendering Pipeline structures
// -------------------------------------------------------------
typedef struct {
  float x, y;
  float u, v;
  float r, g, b, a;
} Vertex;

#define MAX_VERTICES 30000
#define MAX_INDICES 45000

Vertex vertex_buffer[MAX_VERTICES];
uint32_t index_buffer[MAX_INDICES];

int vertex_count = 0;
int index_count = 0;

void push_vertex(float x, float y, float u, float v, float r, float g, float b,
                 float a) {
  if (vertex_count < MAX_VERTICES) {
    vertex_buffer[vertex_count] = (Vertex){x, y, u, v, r, g, b, a};
    vertex_count++;
  }
}

void push_index(uint32_t idx) {
  if (index_count < MAX_INDICES) {
    index_buffer[index_count] = idx;
    index_count++;
  }
}

void draw_rect(float x, float y, float w, float h, float r, float g, float b,
               float a) {
  uint32_t start_idx = vertex_count;

  push_vertex(x, y, SOLID_U, SOLID_V, r, g, b, a);         // TL
  push_vertex(x + w, y, SOLID_U, SOLID_V, r, g, b, a);     // TR
  push_vertex(x, y + h, SOLID_U, SOLID_V, r, g, b, a);     // BL
  push_vertex(x + w, y + h, SOLID_U, SOLID_V, r, g, b, a); // BR

  push_index(start_idx + 0);
  push_index(start_idx + 2);
  push_index(start_idx + 1);

  push_index(start_idx + 1);
  push_index(start_idx + 2);
  push_index(start_idx + 3);
}

void draw_line(float x1, float y1, float x2, float y2, float thickness, float r,
               float g, float b, float a) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float len = float_sqrt(dx * dx + dy * dy);
  if (len < 0.0001f)
    return;

  float px = -dy / len * (thickness / 2.0f);
  float py = dx / len * (thickness / 2.0f);

  uint32_t start_idx = vertex_count;

  push_vertex(x1 - px, y1 - py, SOLID_U, SOLID_V, r, g, b, a);
  push_vertex(x1 + px, y1 + py, SOLID_U, SOLID_V, r, g, b, a);
  push_vertex(x2 - px, y2 - py, SOLID_U, SOLID_V, r, g, b, a);
  push_vertex(x2 + px, y2 + py, SOLID_U, SOLID_V, r, g, b, a);

  push_index(start_idx + 0);
  push_index(start_idx + 1);
  push_index(start_idx + 2);

  push_index(start_idx + 2);
  push_index(start_idx + 1);
  push_index(start_idx + 3);
}

void draw_char(uint32_t codepoint, float x, float y, float w, float h, float r, float g,
               float b, float a) {
  const GlyphInfo *g_info = lookup_glyph(codepoint);
  uint16_t atlas_index = 0; // Default fallback to empty cell
  if (g_info) {
    atlas_index = g_info->atlas_index;
  } else if (codepoint == 32 || codepoint == 160) {
    // Space or NBSP: just advance without drawing
    return;
  }

  int col = atlas_index % GRID_SIZE;
  int row = atlas_index / GRID_SIZE;

  float inset = 1.0f / 2048.0f;
  float u0 = (float)col / (float)GRID_SIZE + inset;
  float v0 = (float)row / (float)GRID_SIZE + inset;
  float u1 = (float)(col + 1) / (float)GRID_SIZE - inset;
  float v1 = (float)(row + 1) / (float)GRID_SIZE - inset;

  uint32_t start_idx = vertex_count;

  push_vertex(x, y, u0, v0, r, g, b, a);         // TL
  push_vertex(x + w, y, u1, v0, r, g, b, a);     // TR
  push_vertex(x, y + h, u0, v1, r, g, b, a);     // BL
  push_vertex(x + w, y + h, u1, v1, r, g, b, a); // BR

  push_index(start_idx + 0);
  push_index(start_idx + 2);
  push_index(start_idx + 1);

  push_index(start_idx + 1);
  push_index(start_idx + 2);
  push_index(start_idx + 3);
}

void draw_text(const char *text, float x, float y, float char_w, float char_h,
               float r, float g, float b, float a) {
  float cell_size = char_w * (64.0f / 38.0f);
  float cur_x = x;
  const char *ptr = text;
  while (*ptr != '\0') {
    uint32_t cp = decode_utf8(&ptr);
    float advance = get_char_advance(cp) * char_w;
    float x_pos = cur_x - cell_size * (24.0f / 64.0f);
    float y_pos = y - cell_size * (46.0f / 64.0f) + char_h * 0.75f;
    draw_char(cp, x_pos, y_pos, cell_size, cell_size, r, g, b, a);
    cur_x += advance;
  }
}

// -------------------------------------------------------------
// Uniform Block Configuration
// -------------------------------------------------------------
typedef struct {
  float pan_x;
  float pan_y;
  float zoom;
  float pad1;
  float screen_width;
  float screen_height;
  float pad2[2];
} Uniforms;

Uniforms uniforms;

// -------------------------------------------------------------
// Canvas Interactive State
// -------------------------------------------------------------
#define MAX_NODES 2700000
#define MAX_CONNECTIONS 100

typedef struct {
  int from;
  int to;
} Connection;

Node nodes[MAX_NODES];
int node_count = 0;
extern int selected_node_idx;
extern int editing_node_idx;

#define MAX_PATH_POINTS 20000
PathPoint path_points[MAX_PATH_POINTS];
int path_point_count = 0;

Connection connections[MAX_CONNECTIONS];
int connection_count = 0;

typedef struct {
  float r, g, b;
} PastelColor;

PastelColor colors[6] = {
    {1.0f, 0.8f, 0.8f},  // Pastel Pink
    {0.8f, 0.9f, 1.0f},  // Pastel Blue
    {0.8f, 1.0f, 0.8f},  // Pastel Green
    {1.0f, 0.95f, 0.7f}, // Pastel Yellow
    {0.9f, 0.8f, 1.0f},  // Pastel Purple
    {1.0f, 0.85f, 0.7f}  // Pastel Orange
};

static unsigned int rng_state = 123456789;
static float local_random_float() {
  rng_state = rng_state * 1664525 + 1013904223;
  return (float)(rng_state & 0xFFFFFF) / 16777216.0f;
}

void add_node_full(WidgetType type, float world_x, float world_y, float w,
                   float h, const char *text, int texture_id) {
  if (node_count >= MAX_NODES)
    return;

  Node *n = &nodes[node_count];
  n->type = type;
  n->x = world_x;
  n->y = world_y;
  n->w = w;
  n->h = h;
  strcpy(n->text, text);
  n->texture_id = texture_id;

  int color_idx = (int)(local_random_float() * 6.0f);
  if (color_idx < 0)
    color_idx = 0;
  if (color_idx > 5)
    color_idx = 5;
  n->bg_r = colors[color_idx].r;
  n->bg_g = colors[color_idx].g;
  n->bg_b = colors[color_idx].b;

  if (type == WIDGET_TEXT) {
    n->bg_a = 0.0f;
    n->r = n->bg_r;
    n->g = n->bg_g;
    n->b = n->bg_b;
  } else {
    n->bg_a = 0.9f;
    if (n->bg_r < 0.5f) {
      n->r = 25.0f / 255.0f;
    } else {
      n->r = 1.0f;
    }
    if (n->bg_g < 0.5f) {
      n->g = 25.0f / 255.0f;
    } else {
      n->g = 1.0f;
    }
    if (n->bg_b < 0.5f) {
      n->b = 25.0f / 255.0f;
    } else {
      n->b = 1.0f;
    }
  }

  n->border_r = n->bg_r * 0.7f;
  n->border_g = n->bg_g * 0.7f;
  n->border_b = n->bg_b * 0.7f;
  if (type == WIDGET_RECT || type == WIDGET_OVAL || type == WIDGET_TRIANGLE) {
    n->border_a = 1.0f;
  } else {
    n->border_a = 0.0f;
  }

  n->font_size = 18.0f;
  n->selected = 0;
  n->is_dragging = 0;

  node_count++;
}

void add_node(float world_x, float world_y, const char *text) {
  add_node_full(WIDGET_STICKY, world_x, world_y, 150.0f, 150.0f, text, -1);
}

void add_connection(int from, int to) {
  if (from == to)
    return;
  if (from < 0 || from >= node_count || to < 0 || to >= node_count)
    return;

  // Check duplicate WIDGET_ARROW connections
  for (int i = 0; i < node_count; i++) {
    Node *n = &nodes[i];
    if (n->type == WIDGET_ARROW) {
      if ((n->path_start_idx == from && n->path_point_len == to) ||
          (n->path_start_idx == to && n->path_point_len == from)) {
        return; // Connection already exists
      }
    }
  }

  // Create WIDGET_ARROW node
  if (node_count < MAX_NODES) {
    Node *n = &nodes[node_count];
    n->type = WIDGET_ARROW;
    n->x = 0.0f;
    n->y = 0.0f;
    n->w = 0.0f;
    n->h = 0.0f;
    n->text[0] = '\0';
    n->texture_id = -1;
    n->path_start_idx = from;
    n->path_point_len = to;

    // Default connection color: Indigo accent
    n->r = 94.0f / 255.0f;
    n->g = 106.0f / 255.0f;
    n->b = 210.0f / 255.0f;

    n->bg_r = n->r;
    n->bg_g = n->g;
    n->bg_b = n->b;
    n->bg_a = 1.0f; // line alpha

    n->border_r = n->r;
    n->border_g = n->g;
    n->border_b = n->b;
    n->border_a = 1.0f;

    n->selected = 0;
    n->is_dragging = 0;

    node_count++;
  }
}

void delete_node(int index) {
  if (index < 0 || index >= node_count)
    return;

  // Iterate through nodes and delete WIDGET_ARROW connections referencing this
  // node
  for (int i = 0; i < node_count;) {
    Node *n = &nodes[i];
    if (n->type == WIDGET_ARROW) {
      if (n->path_start_idx == index || n->path_point_len == index) {
        // Shift nodes to delete
        for (int k = i; k < node_count - 1; k++) {
          nodes[k] = nodes[k + 1];
        }
        node_count--;
        if (i < index) {
          index--;
        }
        continue;
      }
    }
    i++;
  }

  // Shift target index references inside WIDGET_ARROW nodes
  for (int i = 0; i < node_count; i++) {
    Node *n = &nodes[i];
    if (n->type == WIDGET_ARROW) {
      if (n->path_start_idx > index) {
        n->path_start_idx--;
      }
      if (n->path_point_len > index) {
        n->path_point_len--;
      }
    }
  }

  // Move nodes
  for (int i = index; i < node_count - 1; i++) {
    nodes[i] = nodes[i + 1];
  }
  node_count--;

  // Update selection/editing index mappings
  if (selected_node_idx == index) {
    selected_node_idx = -1;
  } else if (selected_node_idx > index) {
    selected_node_idx--;
  }

  if (editing_node_idx == index) {
    editing_node_idx = -1;
  } else if (editing_node_idx > index) {
    editing_node_idx--;
  }
}

// -------------------------------------------------------------
// Mouse & Keyboard state trackers
// -------------------------------------------------------------
float mouse_world_x = 0.0f;
float mouse_world_y = 0.0f;

float last_mouse_screen_x = 0.0f;
float last_mouse_screen_y = 0.0f;

float last_mouse_world_x = 0.0f;
float last_mouse_world_y = 0.0f;
float mouse_down_world_x = 0.0f;
float mouse_down_world_y = 0.0f;

int is_panning = 0;
int selected_node_idx = -1;
int is_dragging_node = 0;
int is_resizing_node = 0;
int arrow_tool_active = 0;
float drag_offset_x = 0.0f;
float drag_offset_y = 0.0f;
float resize_initial_w = 150.0f;
float resize_initial_h = 150.0f;

int is_selecting_marquee = 0;
float marquee_start_x = 0.0f;
float marquee_start_y = 0.0f;

int space_pressed = 0;
int shift_pressed = 0;
int editing_node_idx = -1;

float last_click_timestamp = 0.0f;

// -------------------------------------------------------------
// WebGPU Resource IDs
// -------------------------------------------------------------
int vertex_buffer_id = -1;
int index_buffer_id = -1;
int uniform_buffer_id = -1;
int texture_id = -1;

// -------------------------------------------------------------
// WebAssembly exported handlers
// -------------------------------------------------------------

__attribute__((visibility("default"))) int init_app(int width, int height) {
  // Initial matrices
  uniforms.screen_width = (float)width;
  uniforms.screen_height = (float)height;
  uniforms.zoom = 1.0f;
  uniforms.pan_x = 0.0f;
  uniforms.pan_y = 0.0f;

  // Advances copy omitted as it's now dynamically resolved via font_glyphs table

  int success = js_wgpu_init(width, height);
  if (!success) {
    return 0;
  }

  vertex_buffer_id = js_wgpu_create_buffer(sizeof(Vertex) * MAX_VERTICES, 1);
  index_buffer_id = js_wgpu_create_buffer(sizeof(uint32_t) * MAX_INDICES, 2);
  uniform_buffer_id = js_wgpu_create_buffer(sizeof(Uniforms), 3);
  texture_id = js_wgpu_create_texture(2048, 2048);

  js_wgpu_write_texture(texture_id, 2048, 2048, NULL, 0);

  // Add default sample notes - BLITZ Infographic
  // 1. Left Panel Background (Dark Slate)
  add_node_full(WIDGET_RECT, 100.0f, 100.0f, 300.0f, 550.0f, "", -1);
  int left_bg = node_count - 1;
  nodes[left_bg].bg_r = 15.0f / 255.0f;
  nodes[left_bg].bg_g = 18.0f / 255.0f;
  nodes[left_bg].bg_b = 25.0f / 255.0f;
  nodes[left_bg].bg_a = 1.0f;
  nodes[left_bg].border_a = 0.0f;

  // 2. Right Panel Background (Sleek Dark Slate)
  add_node_full(WIDGET_RECT, 400.0f, 100.0f, 700.0f, 550.0f, "", -1);
  int right_bg = node_count - 1;
  nodes[right_bg].bg_r = 20.0f / 255.0f;
  nodes[right_bg].bg_g = 24.0f / 255.0f;
  nodes[right_bg].bg_b = 33.0f / 255.0f;
  nodes[right_bg].bg_a = 1.0f;
  nodes[right_bg].border_a = 0.0f;

  // 3. Title Text "DUST" in White
  add_node_full(WIDGET_TEXT, 130.0f, 130.0f, 240.0f, 60.0f, "DUST", -1);
  int title_idx = node_count - 1;
  nodes[title_idx].bg_r = 226.0f / 255.0f;
  nodes[title_idx].bg_g = 232.0f / 255.0f;
  nodes[title_idx].bg_b = 240.0f / 255.0f;
  nodes[title_idx].font_size = 48.0f;

  // 4. Subtitle Text "WEBGPU WHITEBOARD" in Accent Indigo
  add_node_full(WIDGET_TEXT, 130.0f, 190.0f, 240.0f, 30.0f, "WEBGPU WHITEBOARD",
                -1);
  int sub_idx = node_count - 1;
  nodes[sub_idx].bg_r = 94.0f / 255.0f;
  nodes[sub_idx].bg_g = 106.0f / 255.0f;
  nodes[sub_idx].bg_b = 210.0f / 255.0f;
  nodes[sub_idx].font_size = 14.0f;

  // 5. Left Panel Section header "WHITEBOARD LAYERS" in Muted Text
  add_node_full(WIDGET_TEXT, 130.0f, 270.0f, 240.0f, 30.0f, "WHITEBOARD LAYERS",
                -1);
  int rm_header = node_count - 1;
  nodes[rm_header].bg_r = 148.0f / 255.0f;
  nodes[rm_header].bg_g = 163.0f / 255.0f;
  nodes[rm_header].bg_b = 184.0f / 255.0f;
  nodes[rm_header].font_size = 14.0f;

  // 6. Left Panel list items
  add_node_full(WIDGET_TEXT, 130.0f, 310.0f, 240.0f, 30.0f, "Shapes", -1);
  int item1 = node_count - 1;
  nodes[item1].bg_r = 226.0f / 255.0f;
  nodes[item1].bg_g = 232.0f / 255.0f;
  nodes[item1].bg_b = 240.0f / 255.0f;
  nodes[item1].font_size = 18.0f;

  add_node_full(WIDGET_TEXT, 130.0f, 350.0f, 240.0f, 30.0f, "Text", -1);
  int item2 = node_count - 1;
  nodes[item2].bg_r = 226.0f / 255.0f;
  nodes[item2].bg_g = 232.0f / 255.0f;
  nodes[item2].bg_b = 240.0f / 255.0f;
  nodes[item2].font_size = 18.0f;

  add_node_full(WIDGET_TEXT, 130.0f, 390.0f, 240.0f, 30.0f, "Arrows", -1);
  int item3 = node_count - 1;
  nodes[item3].bg_r = 226.0f / 255.0f;
  nodes[item3].bg_g = 232.0f / 255.0f;
  nodes[item3].bg_b = 240.0f / 255.0f;
  nodes[item3].font_size = 18.0f;

  // 7. Left Panel footer "Wasm Core" and languages
  add_node_full(WIDGET_TEXT, 130.0f, 510.0f, 240.0f, 30.0f, "Wasm Core", -1);
  int ft1 = node_count - 1;
  nodes[ft1].bg_r = 148.0f / 255.0f;
  nodes[ft1].bg_g = 163.0f / 255.0f;
  nodes[ft1].bg_b = 184.0f / 255.0f;
  nodes[ft1].font_size = 16.0f;

  add_node_full(WIDGET_TEXT, 130.0f, 550.0f, 240.0f, 30.0f,
                "Pure C · WebGPU · JS", -1);
  int ft2 = node_count - 1;
  nodes[ft2].bg_r = 94.0f / 255.0f;
  nodes[ft2].bg_g = 106.0f / 255.0f;
  nodes[ft2].bg_b = 210.0f / 255.0f;
  nodes[ft2].font_size = 13.0f;

  // 8. Right Panel big statement
  add_node_full(WIDGET_TEXT, 440.0f, 130.0f, 600.0f, 80.0f,
                "Pure C Engine.\nWebGPU Pipelines.", -1);
  int right_title = node_count - 1;
  nodes[right_title].bg_r = 226.0f / 255.0f;
  nodes[right_title].bg_g = 232.0f / 255.0f;
  nodes[right_title].bg_b = 240.0f / 255.0f;
  nodes[right_title].font_size = 32.0f;

  add_node_full(
      WIDGET_TEXT, 440.0f, 220.0f, 600.0f, 40.0f,
      "Infinite collaborative canvas built natively on raw WebGPU primitives.",
      -1);
  int right_sub = node_count - 1;
  nodes[right_sub].bg_r = 148.0f / 255.0f;
  nodes[right_sub].bg_g = 163.0f / 255.0f;
  nodes[right_sub].bg_b = 184.0f / 255.0f;
  nodes[right_sub].font_size = 15.0f;

  // 9. Card 1 "01 WASM Core"
  add_node_full(WIDGET_RECT, 440.0f, 280.0f, 290.0f, 100.0f,
                "01   WASM Core\nPure C whiteboard code compiles directly to "
                "WebAssembly.",
                -1);
  int card1 = node_count - 1;
  nodes[card1].bg_r = 30.0f / 255.0f;
  nodes[card1].bg_g = 35.0f / 255.0f;
  nodes[card1].bg_b = 48.0f / 255.0f;
  nodes[card1].bg_a = 1.0f;
  nodes[card1].border_r = 94.0f / 255.0f;
  nodes[card1].border_g = 106.0f / 255.0f;
  nodes[card1].border_b = 210.0f / 255.0f;
  nodes[card1].border_a = 1.0f;
  nodes[card1].font_size = 13.0f;

  // 10. Card 2 "02 Smooth MSAA"
  add_node_full(WIDGET_RECT, 750.0f, 280.0f, 290.0f, 100.0f,
                "02   Smooth MSAA\nHardware MSAA 4x rendering for crisp text, "
                "ovals & lines.",
                -1);
  int card2 = node_count - 1;
  nodes[card2].bg_r = 30.0f / 255.0f;
  nodes[card2].bg_g = 35.0f / 255.0f;
  nodes[card2].bg_b = 48.0f / 255.0f;
  nodes[card2].bg_a = 1.0f;
  nodes[card2].border_r = 94.0f / 255.0f;
  nodes[card2].border_g = 106.0f / 255.0f;
  nodes[card2].border_b = 210.0f / 255.0f;
  nodes[card2].border_a = 1.0f;
  nodes[card2].font_size = 13.0f;

  // 11. Frame Composition Title
  add_node_full(WIDGET_TEXT, 440.0f, 410.0f, 600.0f, 30.0f,
                "ENGINE COMPOSITION", -1);
  int fc_title = node_count - 1;
  nodes[fc_title].bg_r = 148.0f / 255.0f;
  nodes[fc_title].bg_g = 163.0f / 255.0f;
  nodes[fc_title].bg_b = 184.0f / 255.0f;
  nodes[fc_title].font_size = 14.0f;

  // 12. Frame Composition Rows
  // Row 1: C Geometry
  add_node_full(WIDGET_TEXT, 440.0f, 450.0f, 150.0f, 30.0f, "C Geometry", -1);
  int geom_lbl = node_count - 1;
  nodes[geom_lbl].bg_r = 148.0f / 255.0f;
  nodes[geom_lbl].bg_g = 163.0f / 255.0f;
  nodes[geom_lbl].bg_b = 184.0f / 255.0f;
  nodes[geom_lbl].font_size = 14.0f;

  // Row 1 Bar Background
  add_node_full(WIDGET_RECT, 600.0f, 455.0f, 400.0f, 20.0f, "", -1);
  int geom_bar_bg = node_count - 1;
  nodes[geom_bar_bg].bg_r = 30.0f / 255.0f;
  nodes[geom_bar_bg].bg_g = 35.0f / 255.0f;
  nodes[geom_bar_bg].bg_b = 48.0f / 255.0f;
  nodes[geom_bar_bg].bg_a = 1.0f;
  nodes[geom_bar_bg].border_a = 0.0f;

  // Row 1 Bar Value (Indigo Accent)
  add_node_full(WIDGET_RECT, 600.0f, 455.0f, 300.0f, 20.0f, "", -1);
  int geom_bar_val = node_count - 1;
  nodes[geom_bar_val].bg_r = 94.0f / 255.0f;
  nodes[geom_bar_val].bg_g = 106.0f / 255.0f;
  nodes[geom_bar_val].bg_b = 210.0f / 255.0f;
  nodes[geom_bar_val].bg_a = 1.0f;
  nodes[geom_bar_val].border_a = 0.0f;

  // Row 2: Wasm Textures
  add_node_full(WIDGET_TEXT, 440.0f, 490.0f, 150.0f, 30.0f, "Wasm Textures",
                -1);
  int typo_lbl = node_count - 1;
  nodes[typo_lbl].bg_r = 148.0f / 255.0f;
  nodes[typo_lbl].bg_g = 163.0f / 255.0f;
  nodes[typo_lbl].bg_b = 184.0f / 255.0f;
  nodes[typo_lbl].font_size = 14.0f;

  // Row 2 Bar Background
  add_node_full(WIDGET_RECT, 600.0f, 495.0f, 400.0f, 20.0f, "", -1);
  int typo_bar_bg = node_count - 1;
  nodes[typo_bar_bg].bg_r = 30.0f / 255.0f;
  nodes[typo_bar_bg].bg_g = 35.0f / 255.0f;
  nodes[typo_bar_bg].bg_b = 48.0f / 255.0f;
  nodes[typo_bar_bg].bg_a = 1.0f;
  nodes[typo_bar_bg].border_a = 0.0f;

  // Row 2 Bar Value (Cyan Harmony)
  add_node_full(WIDGET_RECT, 600.0f, 495.0f, 240.0f, 20.0f, "", -1);
  int typo_bar_val = node_count - 1;
  nodes[typo_bar_val].bg_r = 6.0f / 255.0f;
  nodes[typo_bar_val].bg_g = 182.0f / 255.0f;
  nodes[typo_bar_val].bg_b = 212.0f / 255.0f;
  nodes[typo_bar_val].bg_a = 1.0f;
  nodes[typo_bar_val].border_a = 0.0f;

  // Row 3: JS WebGPU
  add_node_full(WIDGET_TEXT, 440.0f, 530.0f, 150.0f, 30.0f, "JS WebGPU", -1);
  int int_lbl = node_count - 1;
  nodes[int_lbl].bg_r = 148.0f / 255.0f;
  nodes[int_lbl].bg_g = 163.0f / 255.0f;
  nodes[int_lbl].bg_b = 184.0f / 255.0f;
  nodes[int_lbl].font_size = 14.0f;

  // Row 3 Bar Background
  add_node_full(WIDGET_RECT, 600.0f, 535.0f, 400.0f, 20.0f, "", -1);
  int int_bar_bg = node_count - 1;
  nodes[int_bar_bg].bg_r = 30.0f / 255.0f;
  nodes[int_bar_bg].bg_g = 35.0f / 255.0f;
  nodes[int_bar_bg].bg_b = 48.0f / 255.0f;
  nodes[int_bar_bg].bg_a = 1.0f;
  nodes[int_bar_bg].border_a = 0.0f;

  // Row 3 Bar Value (Purple Harmony)
  add_node_full(WIDGET_RECT, 600.0f, 535.0f, 180.0f, 20.0f, "", -1);
  int int_bar_val = node_count - 1;
  nodes[int_bar_val].bg_r = 139.0f / 255.0f;
  nodes[int_bar_val].bg_g = 92.0f / 255.0f;
  nodes[int_bar_val].bg_b = 246.0f / 255.0f;
  nodes[int_bar_val].bg_a = 1.0f;
  nodes[int_bar_val].border_a = 0.0f;

  // 13. Create connection arrows linking Card 1 to Geometry and Card 2 to
  // Typography
  add_connection(card1, geom_lbl);
  add_connection(card2, typo_lbl);

  log_str("Canvas loaded successfully.");
  return 1;
}

__attribute__((visibility("default"))) void on_resize(int width, int height) {
  uniforms.screen_width = (float)width;
  uniforms.screen_height = (float)height;
}

void clamp_node_size_with_aspect(Node *n, float *w, float *h) {
  if (n->type == WIDGET_STICKY) {
    if (*w < 40.0f) {
      *w = 40.0f;
    }
    *h = *w;
  } else if (n->type == WIDGET_IMAGE) {
    if (n->h > 0.0f) {
      float aspect = n->w / n->h;
      if (*w < 40.0f) {
        *w = 40.0f;
        *h = 40.0f / aspect;
      }
      if (*h < 20.0f) {
        *h = 20.0f;
        *w = 20.0f * aspect;
      }
    }
  } else {
    if (*w < 40.0f)
      *w = 40.0f;
    if (*h < 20.0f)
      *h = 20.0f;
  }
}

__attribute__((visibility("default"))) void
on_mouse_down(int button, float x, float y, int shift, int ctrl) {
  // Determine world coordinates
  float world_x = (x - uniforms.pan_x) / uniforms.zoom;
  float world_y = (y - uniforms.pan_y) / uniforms.zoom;
  mouse_world_x = world_x;
  mouse_world_y = world_y;
  last_mouse_world_x = world_x;
  last_mouse_world_y = world_y;

  // Simple double-click timing check
  static float prev_click_time = 0.0f;
  extern float current_time_ms; // set in tick_app
  float now = current_time_ms;
  float delta = now - prev_click_time;
  js_log_click_delta(delta);
  int is_double_click = (delta < 300.0f);
  prev_click_time = now;

  // Check if we hit the resize handle of the selected node
  int hit_resize_handle = 0;
  if (selected_node_idx != -1) {
    Node *sn = &nodes[selected_node_idx];
    float handle_x = sn->x + sn->w;
    float handle_y = sn->y + sn->h;
    float dist_x = world_x - handle_x;
    float dist_y = world_y - handle_y;
    float dist = float_sqrt(dist_x * dist_x + dist_y * dist_y);
    float click_tolerance = 12.0f / uniforms.zoom;
    if (dist <= click_tolerance) {
      hit_resize_handle = 1;
    }
  }

  if (hit_resize_handle) {
    is_resizing_node = 1;
    drag_offset_x = world_x - nodes[selected_node_idx].w;
    drag_offset_y = world_y - nodes[selected_node_idx].h;
    resize_initial_w = nodes[selected_node_idx].w;
    resize_initial_h = nodes[selected_node_idx].h;
    is_panning = 0;
    is_dragging_node = 0;
    return;
  }

  // Check if we hit a node
  int hit_node_idx = -1;
  float distance_to_segment(float px, float py, float x1, float y1, float x2,
                            float y2);
  for (int i = node_count - 1; i >= 0; i--) {
    Node *n = &nodes[i];
    if (n->type == WIDGET_ARROW) {
      int from = n->path_start_idx;
      int to = n->path_point_len;
      if (from >= 0 && from < node_count && to >= 0 && to < node_count) {
        float x1 = nodes[from].x + nodes[from].w / 2.0f;
        float y1 = nodes[from].y + nodes[from].h / 2.0f;
        float x2 = nodes[to].x + nodes[to].w / 2.0f;
        float y2 = nodes[to].y + nodes[to].h / 2.0f;
        float dist = distance_to_segment(world_x, world_y, x1, y1, x2, y2);
        float tolerance = 8.0f / uniforms.zoom;
        if (dist <= tolerance) {
          hit_node_idx = i;
          break;
        }
      }
    } else if (n->type == WIDGET_PATH) {
      extern PathPoint path_points[];
      float tolerance = 8.0f / uniforms.zoom;
      int hit = 0;
      for (int k = 0; k < n->path_point_len - 1; k++) {
        PathPoint p1 = path_points[n->path_start_idx + k];
        PathPoint p2 = path_points[n->path_start_idx + k + 1];
        float x1 = n->x + p1.x;
        float y1 = n->y + p1.y;
        float x2 = n->x + p2.x;
        float y2 = n->y + p2.y;
        float dist = distance_to_segment(world_x, world_y, x1, y1, x2, y2);
        if (dist <= tolerance) {
          hit = 1;
          break;
        }
      }
      if (hit) {
        hit_node_idx = i;
        break;
      }
    } else {
      if (world_x >= n->x && world_x <= n->x + n->w && world_y >= n->y &&
          world_y <= n->y + n->h) {
        hit_node_idx = i;
        break;
      }
    }
  }

  if (hit_node_idx != -1) {
    if (is_double_click) {
      // Trigger text editing
      editing_node_idx = hit_node_idx;
      selected_node_idx = hit_node_idx;

      // Convert node position to screen coordinates
      float sx = nodes[hit_node_idx].x * uniforms.zoom + uniforms.pan_x;
      float sy = nodes[hit_node_idx].y * uniforms.zoom + uniforms.pan_y;

      // Adjust to center within the note card (JS handles zoom scaling)
      js_set_editing_state(1, sx, sy, nodes[hit_node_idx].w,
                           nodes[hit_node_idx].h, nodes[hit_node_idx].text, 120,
                           hit_node_idx);

      is_dragging_node = 0;
      is_panning = 0;
      return;
    }

    // Single Click Node
    if (shift || shift_pressed) {
      // Connect nodes
      if (selected_node_idx != -1 && selected_node_idx != hit_node_idx) {
        add_connection(selected_node_idx, hit_node_idx);
      }
      selected_node_idx = hit_node_idx;
    } else {
      if (ctrl) {
        // Toggle selection
        nodes[hit_node_idx].selected = !nodes[hit_node_idx].selected;
        if (nodes[hit_node_idx].selected) {
          selected_node_idx = hit_node_idx;
        } else {
          if (selected_node_idx == hit_node_idx) {
            selected_node_idx = -1;
            for (int i = 0; i < node_count; i++) {
              if (nodes[i].selected) {
                selected_node_idx = i;
                break;
              }
            }
          }
        }
      } else {
        // If the clicked node is NOT already selected, clear previous
        // selections and select only this
        if (!nodes[hit_node_idx].selected) {
          for (int i = 0; i < node_count; i++) {
            nodes[i].selected = (i == hit_node_idx);
          }
          selected_node_idx = hit_node_idx;
        }
      }

      // Start dragging all selected nodes
      is_dragging_node = 1;
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          nodes[i].is_dragging = 1;
        }
      }
      drag_offset_x = world_x - nodes[hit_node_idx].x;
      drag_offset_y = world_y - nodes[hit_node_idx].y;
    }
  } else {
    // Hit Background
    if (is_double_click) {
      add_widget_wasm(WIDGET_STICKY, world_x, world_y, -1, 0, 0);
    } else {
      // Start panning or marquee selection
      if (button == 2 || space_pressed || button == 1) {
        is_panning = 1;
      } else {
        // Clear selection
        selected_node_idx = -1;
        for (int i = 0; i < node_count; i++) {
          nodes[i].selected = 0;
        }

        // Start marquee select
        is_selecting_marquee = 1;
        marquee_start_x = world_x;
        marquee_start_y = world_y;
      }
    }
  }

  mouse_down_world_x = world_x;
  mouse_down_world_y = world_y;
  last_mouse_screen_x = x;
  last_mouse_screen_y = y;
}

__attribute__((visibility("default"))) void on_mouse_move(float x, float y) {
  mouse_world_x = (x - uniforms.pan_x) / uniforms.zoom;
  mouse_world_y = (y - uniforms.pan_y) / uniforms.zoom;

  if (is_panning) {
    uniforms.pan_x += (x - last_mouse_screen_x);
    uniforms.pan_y += (y - last_mouse_screen_y);
  } else if (is_selecting_marquee) {
    float draw_x =
        (marquee_start_x < mouse_world_x) ? marquee_start_x : mouse_world_x;
    float draw_y =
        (marquee_start_y < mouse_world_y) ? marquee_start_y : mouse_world_y;
    float draw_w = (marquee_start_x < mouse_world_x)
                       ? (mouse_world_x - marquee_start_x)
                       : (marquee_start_x - mouse_world_x);
    float draw_h = (marquee_start_y < mouse_world_y)
                       ? (mouse_world_y - marquee_start_y)
                       : (marquee_start_y - mouse_world_y);

    selected_node_idx = -1;
    for (int i = 0; i < node_count; i++) {
      Node *n = &nodes[i];
      if (n->type != WIDGET_ARROW && n->type != WIDGET_PATH) {
        int overlap = (n->x < draw_x + draw_w && n->x + n->w > draw_x &&
                       n->y < draw_y + draw_h && n->y + n->h > draw_y);
        n->selected = overlap;
        if (overlap) {
          selected_node_idx = i;
        }
      }
    }
  } else if (is_dragging_node) {
    float world_dx = mouse_world_x - last_mouse_world_x;
    float world_dy = mouse_world_y - last_mouse_world_y;
    for (int i = 0; i < node_count; i++) {
      if (nodes[i].is_dragging) {
        nodes[i].x += world_dx;
        nodes[i].y += world_dy;
      }
    }
  } else if (is_resizing_node && selected_node_idx != -1) {
    Node *sn = &nodes[selected_node_idx];
    float new_w = mouse_world_x - drag_offset_x;
    float new_h = mouse_world_y - drag_offset_y;
    if (sn->type == WIDGET_STICKY || sn->type == WIDGET_IMAGE) {
      if (resize_initial_w > 0.0f && resize_initial_h > 0.0f) {
        float scale_w = new_w / resize_initial_w;
        float scale_h = new_h / resize_initial_h;
        float scale = (scale_w + scale_h) / 2.0f;
        float min_scale_w = 40.0f / resize_initial_w;
        float min_scale_h = (sn->type == WIDGET_STICKY) ? (40.0f / resize_initial_h) : (20.0f / resize_initial_h);
        float min_scale = (min_scale_w > min_scale_h) ? min_scale_w : min_scale_h;
        if (scale < min_scale) {
          scale = min_scale;
        }
        new_w = resize_initial_w * scale;
        new_h = resize_initial_h * scale;
      }
    } else {
      if (new_w < 40.0f)
        new_w = 40.0f;
      if (new_h < 20.0f)
        new_h = 20.0f;
    }
    sn->w = new_w;
    sn->h = new_h;
    if (sn->type != WIDGET_IMAGE && sn->type != WIDGET_PATH) {
      js_init_node_texture(selected_node_idx, sn->text, sn->type, sn->w, sn->h);
    }
  }

  last_mouse_screen_x = x;
  last_mouse_screen_y = y;
  last_mouse_world_x = mouse_world_x;
  last_mouse_world_y = mouse_world_y;
}

__attribute__((visibility("default"))) void on_mouse_up(int button, float x,
                                                        float y) {
  is_panning = 0;
  is_selecting_marquee = 0;

  float world_x = (x - uniforms.pan_x) / uniforms.zoom;
  float world_y = (y - uniforms.pan_y) / uniforms.zoom;
  float dx = world_x - mouse_down_world_x;
  float dy = world_y - mouse_down_world_y;
  float dist = float_sqrt(dx * dx + dy * dy);

  if (is_dragging_node) {
    for (int i = 0; i < node_count; i++) {
      nodes[i].is_dragging = 0;
    }

    // If it was a simple click (not dragged, not shift, not ctrl), select only
    // the hit node
    if (dist < 3.0f && selected_node_idx != -1) {
      int hit_node_idx = -1;
      for (int i = node_count - 1; i >= 0; i--) {
        Node *n = &nodes[i];
        if (n->type != WIDGET_ARROW && n->type != WIDGET_PATH) {
          if (mouse_down_world_x >= n->x && mouse_down_world_x <= n->x + n->w &&
              mouse_down_world_y >= n->y && mouse_down_world_y <= n->y + n->h) {
            hit_node_idx = i;
            break;
          }
        }
      }
      if (hit_node_idx != -1) {
        for (int i = 0; i < node_count; i++) {
          nodes[i].selected = (i == hit_node_idx);
        }
        selected_node_idx = hit_node_idx;
      }
    }
  }

  is_dragging_node = 0;
  is_resizing_node = 0;
}

__attribute__((visibility("default"))) void on_mouse_wheel(float delta_y,
                                                           float x, float y) {
  float factor = (delta_y < 0.0f) ? 1.08f : 0.9259f;

  float old_zoom = uniforms.zoom;
  uniforms.zoom *= factor;

  // Constraint bounds
  if (uniforms.zoom < 0.08f)
    uniforms.zoom = 0.08f;
  if (uniforms.zoom > 4.0f)
    uniforms.zoom = 4.0f;

  // Zoom centered at cursor position
  float wx = (x - uniforms.pan_x) / old_zoom;
  float wy = (y - uniforms.pan_y) / old_zoom;

  uniforms.pan_x = x - wx * uniforms.zoom;
  uniforms.pan_y = y - wy * uniforms.zoom;
}

__attribute__((visibility("default"))) void on_key_down(int key) {
  if (key == 16) { // Shift key
    shift_pressed = 1;
  } else if (key == 32) { // Space bar
    space_pressed = 1;
  } else if (key == 8 || key == 46) { // Backspace or Delete
    for (int i = node_count - 1; i >= 0; i--) {
      if (nodes[i].selected) {
        delete_node(i);
      }
    }
    selected_node_idx = -1;
  }
}

__attribute__((visibility("default"))) void on_key_up(int key) {
  if (key == 16) {
    shift_pressed = 0;
  } else if (key == 32) {
    space_pressed = 0;
  }
}

__attribute__((visibility("default"))) int get_selected_node_idx() {
  return selected_node_idx;
}

__attribute__((visibility("default"))) int get_node_count() {
  return node_count;
}

__attribute__((visibility("default"))) int get_node_type(int idx) {
  if (idx < 0 || idx > node_count)
    return -1;
  return nodes[idx].type;
}

__attribute__((visibility("default"))) float get_node_width(int idx) {
  if (idx < 0 || idx > node_count)
    return 0.0f;
  return nodes[idx].w;
}

__attribute__((visibility("default"))) float get_node_height(int idx) {
  if (idx < 0 || idx > node_count)
    return 0.0f;
  return nodes[idx].h;
}

__attribute__((visibility("default"))) float get_node_x(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].x;
}

__attribute__((visibility("default"))) float get_node_y(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].y;
}

__attribute__((visibility("default"))) int get_node_texture_id(int idx) {
  if (idx < 0 || idx > node_count)
    return -1;
  return nodes[idx].texture_id;
}

__attribute__((visibility("default"))) void set_node_texture_id(int idx,
                                                                int tex_id) {
  if (idx >= 0 && idx <= node_count) {
    nodes[idx].texture_id = tex_id;
  }
}

__attribute__((visibility("default"))) float get_node_bg_r(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].bg_r;
}
__attribute__((visibility("default"))) float get_node_bg_g(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].bg_g;
}
__attribute__((visibility("default"))) float get_node_bg_b(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].bg_b;
}
__attribute__((visibility("default"))) float get_node_bg_a(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].bg_a;
}

__attribute__((visibility("default"))) float get_node_border_r(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].border_r;
}
__attribute__((visibility("default"))) float get_node_border_g(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].border_g;
}
__attribute__((visibility("default"))) float get_node_border_b(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].border_b;
}
__attribute__((visibility("default"))) float get_node_border_a(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  return nodes[idx].border_a;
}

__attribute__((visibility("default"))) float get_node_text_r(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  if (nodes[idx].type == WIDGET_TEXT) {
    return nodes[idx].bg_r;
  }
  return nodes[idx].r;
}
__attribute__((visibility("default"))) float get_node_text_g(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  if (nodes[idx].type == WIDGET_TEXT) {
    return nodes[idx].bg_g;
  }
  return nodes[idx].g;
}
__attribute__((visibility("default"))) float get_node_text_b(int idx) {
  if (idx < 0 || idx >= node_count)
    return 0.0f;
  if (nodes[idx].type == WIDGET_TEXT) {
    return nodes[idx].bg_b;
  }
  return nodes[idx].b;
}

__attribute__((visibility("default"))) void
set_node_text_color(int idx, float r, float g, float b) {
  if (idx >= 0 && idx < node_count) {
    if (nodes[idx].selected) {
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          float tr = r;
          float tg = g;
          float tb = b;
          if (nodes[i].bg_a > 0.001f && nodes[i].bg_r < 0.01f &&
              nodes[i].bg_g < 0.01f && nodes[i].bg_b < 0.01f) {
            tr = 1.0f;
            tg = 1.0f;
            tb = 1.0f;
          }
          nodes[i].r = tr;
          nodes[i].g = tg;
          nodes[i].b = tb;
          if (nodes[i].type == WIDGET_TEXT) {
            nodes[i].bg_r = tr;
            nodes[i].bg_g = tg;
            nodes[i].bg_b = tb;
          }
          if (nodes[i].type != WIDGET_IMAGE && nodes[i].type != WIDGET_PATH) {
            js_init_node_texture(i, nodes[i].text, nodes[i].type, nodes[i].w,
                                 nodes[i].h);
          }
        }
      }
    } else {
      float tr = r;
      float tg = g;
      float tb = b;
      if (nodes[idx].bg_a > 0.001f && nodes[idx].bg_r < 0.01f &&
          nodes[idx].bg_g < 0.01f && nodes[idx].bg_b < 0.01f) {
        tr = 1.0f;
        tg = 1.0f;
        tb = 1.0f;
      }
      nodes[idx].r = tr;
      nodes[idx].g = tg;
      nodes[idx].b = tb;
      if (nodes[idx].type == WIDGET_TEXT) {
        nodes[idx].bg_r = tr;
        nodes[idx].bg_g = tg;
        nodes[idx].bg_b = tb;
      }
      if (nodes[idx].type != WIDGET_IMAGE && nodes[idx].type != WIDGET_PATH) {
        js_init_node_texture(idx, nodes[idx].text, nodes[idx].type,
                             nodes[idx].w, nodes[idx].h);
      }
    }
  }
}

__attribute__((visibility("default"))) void
set_node_bg_color(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < node_count) {
    if (nodes[idx].selected) {
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          nodes[i].bg_r = r;
          nodes[i].bg_g = g;
          nodes[i].bg_b = b;
          nodes[i].bg_a = a;
          if (a > 0.001f && r < 0.01f && g < 0.01f && b < 0.01f) {
            nodes[i].r = 1.0f;
            nodes[i].g = 1.0f;
            nodes[i].b = 1.0f;
          }
          if (nodes[i].type != WIDGET_IMAGE && nodes[i].type != WIDGET_PATH) {
            js_init_node_texture(i, nodes[i].text, nodes[i].type, nodes[i].w,
                                 nodes[i].h);
          }
        }
      }
    } else {
      nodes[idx].bg_r = r;
      nodes[idx].bg_g = g;
      nodes[idx].bg_b = b;
      nodes[idx].bg_a = a;
      if (a > 0.001f && r < 0.01f && g < 0.01f && b < 0.01f) {
        nodes[idx].r = 1.0f;
        nodes[idx].g = 1.0f;
        nodes[idx].b = 1.0f;
      }
      if (nodes[idx].type != WIDGET_IMAGE && nodes[idx].type != WIDGET_PATH) {
        js_init_node_texture(idx, nodes[idx].text, nodes[idx].type,
                             nodes[idx].w, nodes[idx].h);
      }
    }
  }
}

__attribute__((visibility("default"))) void
set_node_border_color(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < node_count) {
    if (nodes[idx].selected) {
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          nodes[i].border_r = r;
          nodes[i].border_g = g;
          nodes[i].border_b = b;
          nodes[i].border_a = a;
        }
      }
    } else {
      nodes[idx].border_r = r;
      nodes[idx].border_g = g;
      nodes[idx].border_b = b;
      nodes[idx].border_a = a;
    }
  }
}

__attribute__((visibility("default"))) float get_node_font_size(int idx) {
  if (idx < 0 || idx >= node_count)
    return 18.0f;
  return nodes[idx].font_size;
}

__attribute__((visibility("default"))) void set_node_font_size(int idx,
                                                               float size) {
  if (idx >= 0 && idx < node_count) {
    if (size < 6.0f)
      size = 6.0f;
    if (size > 120.0f)
      size = 120.0f;
    if (nodes[idx].selected) {
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          nodes[i].font_size = size;
          if (nodes[i].type != WIDGET_IMAGE && nodes[i].type != WIDGET_PATH) {
            js_init_node_texture(i, nodes[i].text, nodes[i].type, nodes[i].w,
                                 nodes[i].h);
          }
        }
      }
    } else {
      nodes[idx].font_size = size;
      if (nodes[idx].type != WIDGET_IMAGE && nodes[idx].type != WIDGET_PATH) {
        js_init_node_texture(idx, nodes[idx].text, nodes[idx].type,
                             nodes[idx].w, nodes[idx].h);
      }
    }
  }
}

__attribute__((visibility("default"))) void set_char_advance(int c,
                                                             float advance) {
  // No-op (statically stored inside font_glyphs mapping)
}

float get_char_advance(uint32_t codepoint) {
  if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
    return 0.0f;
  }
  const GlyphInfo *g = lookup_glyph(codepoint);
  if (g) {
    return g->advance;
  }
  return 0.55f;
}

__attribute__((visibility("default"))) void set_node_size(int idx, float w,
                                                          float h) {
  if (idx >= 0 && idx < node_count) {
    int width_changed = (w != nodes[idx].w);
    int height_changed = (h != nodes[idx].h);

    if (nodes[idx].selected) {
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].selected) {
          float final_w = w;
          float final_h = h;
          if (nodes[i].type == WIDGET_STICKY) {
            if (height_changed && !width_changed) {
              final_w = h;
              final_h = h;
            } else {
              final_w = w;
              final_h = w;
            }
          } else if (nodes[i].type == WIDGET_IMAGE) {
            if (nodes[i].h > 0.0f) {
              float aspect = nodes[i].w / nodes[i].h;
              if (height_changed && !width_changed) {
                final_h = h;
                final_w = h * aspect;
              } else {
                final_w = w;
                final_h = w / aspect;
              }
            }
          }
          clamp_node_size_with_aspect(&nodes[i], &final_w, &final_h);
          nodes[i].w = final_w;
          nodes[i].h = final_h;
          if (nodes[i].type != WIDGET_IMAGE && nodes[i].type != WIDGET_PATH) {
            js_init_node_texture(i, nodes[i].text, nodes[i].type, final_w, final_h);
          }
        }
      }
    } else {
      float final_w = w;
      float final_h = h;
      if (nodes[idx].type == WIDGET_STICKY) {
        if (height_changed && !width_changed) {
          final_w = h;
          final_h = h;
        } else {
          final_w = w;
          final_h = w;
        }
      } else if (nodes[idx].type == WIDGET_IMAGE) {
        if (nodes[idx].h > 0.0f) {
          float aspect = nodes[idx].w / nodes[idx].h;
          if (height_changed && !width_changed) {
            final_h = h;
            final_w = h * aspect;
          } else {
            final_w = w;
            final_h = w / aspect;
          }
        }
      }
      clamp_node_size_with_aspect(&nodes[idx], &final_w, &final_h);
      nodes[idx].w = final_w;
      nodes[idx].h = final_h;
      if (nodes[idx].type != WIDGET_IMAGE && nodes[idx].type != WIDGET_PATH) {
        js_init_node_texture(idx, nodes[idx].text, nodes[idx].type, final_w, final_h);
      }
    }
  }
}

__attribute__((visibility("default"))) void shift_node(int from, int to) {
  if (from == to || from < 0 || from >= node_count || to < 0 ||
      to >= node_count)
    return;

  Node temp = nodes[from];

  if (from < to) {
    for (int i = from; i < to; i++) {
      nodes[i] = nodes[i + 1];
    }
  } else {
    for (int i = from; i > to; i--) {
      nodes[i] = nodes[i - 1];
    }
  }
  nodes[to] = temp;

  for (int i = 0; i < node_count; i++) {
    Node *n = &nodes[i];
    if (n->type == WIDGET_ARROW) {
      if (n->path_start_idx == from) {
        n->path_start_idx = to;
      } else if (from < to) {
        if (n->path_start_idx > from && n->path_start_idx <= to) {
          n->path_start_idx--;
        }
      } else {
        if (n->path_start_idx < from && n->path_start_idx >= to) {
          n->path_start_idx++;
        }
      }

      if (n->path_point_len == from) {
        n->path_point_len = to;
      } else if (from < to) {
        if (n->path_point_len > from && n->path_point_len <= to) {
          n->path_point_len--;
        }
      } else {
        if (n->path_point_len < from && n->path_point_len >= to) {
          n->path_point_len++;
        }
      }
    }
  }

  if (selected_node_idx == from) {
    selected_node_idx = to;
  } else if (from < to) {
    if (selected_node_idx > from && selected_node_idx <= to) {
      selected_node_idx--;
    }
  } else {
    if (selected_node_idx < from && selected_node_idx >= to) {
      selected_node_idx++;
    }
  }

  if (editing_node_idx == from) {
    editing_node_idx = to;
  } else if (from < to) {
    if (editing_node_idx > from && editing_node_idx <= to) {
      editing_node_idx--;
    }
  } else {
    if (editing_node_idx < from && editing_node_idx >= to) {
      editing_node_idx++;
    }
  }
}

__attribute__((visibility("default"))) void bring_to_front_wasm(int idx) {
  shift_node(idx, node_count - 1);
}

__attribute__((visibility("default"))) void send_to_back_wasm(int idx) {
  shift_node(idx, 0);
}

__attribute__((visibility("default"))) void move_forward_wasm(int idx) {
  if (idx < node_count - 1) {
    shift_node(idx, idx + 1);
  }
}

__attribute__((visibility("default"))) void move_backward_wasm(int idx) {
  if (idx > 0) {
    shift_node(idx, idx - 1);
  }
}

__attribute__((visibility("default"))) void set_arrow_tool(int active) {
  arrow_tool_active = active;
}

__attribute__((visibility("default"))) void on_text_commit() {
  // Text committed from JavaScript text box directly into the C pointer
  if (editing_node_idx != -1) {
    Node *n = &nodes[editing_node_idx];
    js_init_node_texture(editing_node_idx, n->text, n->type, n->w, n->h);
  }
  editing_node_idx = -1;
}

__attribute__((visibility("default"))) void on_text_cancel() {
  editing_node_idx = -1;
}

__attribute__((visibility("default"))) void
add_widget_wasm(int type, float x, float y, int texture_id, int img_w,
                int img_h) {
  if (node_count >= MAX_NODES)
    return;

  float w = 150.0f;
  float h = 150.0f;
  const char *default_text = "";

  if (type == WIDGET_STICKY) {
    default_text = "Sticky Note";
  } else if (type == WIDGET_RECT) {
    default_text = "Rectangle";
  } else if (type == WIDGET_OVAL) {
    default_text = "Oval";
  } else if (type == WIDGET_TEXT) {
    default_text = "Text";
    h = 30.0f;
  } else if (type == WIDGET_TRIANGLE) {
    default_text = "Triangle";
  } else if (type == WIDGET_IMAGE) {
    // Keep aspect ratio for the image, max size 200px
    float aspect = (float)img_w / (float)img_h;
    if (img_w > img_h) {
      w = 200.0f;
      h = 200.0f / aspect;
    } else {
      h = 200.0f;
      w = 200.0f * aspect;
    }
  }

  // Center it on coordinate (x, y)
  add_node_full(type, x - w / 2.0f, y - h / 2.0f, w, h, default_text,
                texture_id);

  // Select it
  selected_node_idx = node_count - 1;
  for (int i = 0; i < node_count; i++) {
    nodes[i].selected = (i == selected_node_idx);
  }
}

__attribute__((visibility("default"))) void on_btn_add_click() {
  float cx = (uniforms.screen_width / 2.0f - uniforms.pan_x) / uniforms.zoom;
  float cy = (uniforms.screen_height / 2.0f - uniforms.pan_y) / uniforms.zoom;
  add_widget_wasm(WIDGET_STICKY, cx, cy, -1, 0, 0);
}

__attribute__((visibility("default"))) void on_btn_clear_click() {
  node_count = 0;
  selected_node_idx = -1;
  editing_node_idx = -1;
  path_point_count = 0;
}

int active_stroke_node_idx = -1;

__attribute__((visibility("default"))) void
start_stroke(float world_x, float world_y, float r, float g, float b) {
  if (node_count >= MAX_NODES)
    return;

  Node *n = &nodes[node_count];
  n->type = WIDGET_PATH;
  n->path_start_idx = path_point_count;
  n->path_point_len = 0;
  n->selected = 0;
  n->is_dragging = 0;
  n->texture_id = -1;
  n->text[0] = '\0';

  n->r = r;
  n->g = g;
  n->b = b;
  n->bg_r = r;
  n->bg_g = g;
  n->bg_b = b;
  n->bg_a = 1.0f;

  active_stroke_node_idx = node_count;
  node_count++;

  // Add first point
  if (path_point_count < MAX_PATH_POINTS) {
    path_points[path_point_count] = (PathPoint){world_x, world_y};
    path_point_count++;
    n->path_point_len++;
  }
}

__attribute__((visibility("default"))) void add_stroke_point(float world_x,
                                                             float world_y) {
  if (active_stroke_node_idx == -1)
    return;
  Node *n = &nodes[active_stroke_node_idx];

  if (path_point_count < MAX_PATH_POINTS) {
    // Prevent adding identical consecutive points (optimizes buffer space)
    if (n->path_point_len > 0) {
      PathPoint last = path_points[path_point_count - 1];
      float dx = world_x - last.x;
      float dy = world_y - last.y;
      if (dx * dx + dy * dy < 1.0f) { // If mouse moved less than 1 pixel, skip
        return;
      }
    }

    path_points[path_point_count] = (PathPoint){world_x, world_y};
    path_point_count++;
    n->path_point_len++;
  }
}

__attribute__((visibility("default"))) void end_stroke() {
  if (active_stroke_node_idx == -1)
    return;
  Node *n = &nodes[active_stroke_node_idx];

  if (n->path_point_len <= 1) {
    // Delete the stroke
    path_point_count -= n->path_point_len;
    node_count--;
  } else {
    // Calculate bounding box
    float min_x = path_points[n->path_start_idx].x;
    float max_x = min_x;
    float min_y = path_points[n->path_start_idx].y;
    float max_y = min_y;

    for (int i = 1; i < n->path_point_len; i++) {
      float px = path_points[n->path_start_idx + i].x;
      float py = path_points[n->path_start_idx + i].y;
      if (px < min_x)
        min_x = px;
      if (px > max_x)
        max_x = px;
      if (py < min_y)
        min_y = py;
      if (py > max_y)
        max_y = py;
    }

    n->x = min_x;
    n->y = min_y;
    n->w = max_x - min_x;
    n->h = max_y - min_y;

    // Convert to relative coordinates
    for (int i = 0; i < n->path_point_len; i++) {
      path_points[n->path_start_idx + i].x -= min_x;
      path_points[n->path_start_idx + i].y -= min_y;
    }
  }

  active_stroke_node_idx = -1;
}

// -------------------------------------------------------------
// Frame Loop & Render Compiler
// -------------------------------------------------------------
float current_time_ms = 0.0f;

typedef struct {
  int pipeline_id;
  int texture_id;
  unsigned int index_start;
  unsigned int index_count;
} DrawBatch;

#define MAX_BATCHES 500
DrawBatch batches[MAX_BATCHES];
int batch_count = 0;
unsigned int last_batch_index_start = 0;

__attribute__((visibility("default"))) void tick_app(float timestamp) {
  current_time_ms = timestamp;

  // Clear dynamic draw buffers
  vertex_count = 0;
  index_count = 0;
  batch_count = 0;
  last_batch_index_start = 0;

  // 1. Compile Infinite Grid lines
  float left = -uniforms.pan_x / uniforms.zoom;
  float right = (uniforms.screen_width - uniforms.pan_x) / uniforms.zoom;
  float top = -uniforms.pan_y / uniforms.zoom;
  float bottom = (uniforms.screen_height - uniforms.pan_y) / uniforms.zoom;

  float grid_size = 100.0f;
  if (uniforms.zoom < 0.25f)
    grid_size = 400.0f;
  if (uniforms.zoom < 0.08f)
    grid_size = 1600.0f;

  float start_x = ((int)(left / grid_size) - 1) * grid_size;
  float end_x = ((int)(right / grid_size) + 1) * grid_size;
  float start_y = ((int)(top / grid_size) - 1) * grid_size;
  float end_y = ((int)(bottom / grid_size) + 1) * grid_size;

  float grid_r = 35.0f / 255.0f;
  float grid_g = 39.0f / 255.0f;
  float grid_b = 49.0f / 255.0f;
  float grid_a = 0.5f;

  // Vertical grid lines
  for (float x = start_x; x <= end_x; x += grid_size) {
    draw_line(x, top, x, bottom, 1.2f / uniforms.zoom, grid_r, grid_g, grid_b,
              grid_a);
  }

  // Horizontal grid lines
  for (float y = start_y; y <= end_y; y += grid_size) {
    draw_line(left, y, right, y, 1.2f / uniforms.zoom, grid_r, grid_g, grid_b,
              grid_a);
  }

  // 3. Compile Connection Draft (if Shift is held down or arrow tool active,
  // and node selected)
  if (selected_node_idx != -1 && (shift_pressed || arrow_tool_active) &&
      editing_node_idx == -1) {
    Node *s = &nodes[selected_node_idx];
    float x1 = s->x + s->w / 2.0f;
    float y1 = s->y + s->h / 2.0f;
    float x2 = mouse_world_x;
    float y2 = mouse_world_y;

    // Draw draft line
    draw_line(x1, y1, x2, y2, 2.5f, 0.1f, 0.8f, 0.9f, 0.7f);

    // Draw draft arrow head at mouse cursor
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = float_sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
      float ux = dx / len;
      float uy = dy / len;
      float head_len = 10.0f;
      float rx1 = -ux * 0.866f - -uy * 0.5f;
      float ry1 = -ux * 0.5f + -uy * 0.866f;
      float rx2 = -ux * 0.866f - -uy * -0.5f;
      float ry2 = -ux * -0.5f + -uy * 0.866f;

      draw_line(x2, y2, x2 + rx1 * head_len, y2 + ry1 * head_len, 2.5f, 0.1f,
                0.8f, 0.9f, 0.7f);
      draw_line(x2, y2, x2 + rx2 * head_len, y2 + ry2 * head_len, 2.5f, 0.1f,
                0.8f, 0.9f, 0.7f);
    }
  }

  // 5. Compile and draw widgets with culling
  int shapes_count = 0;
  for (int i = 0; i < node_count; i++) {
    Node *n = &nodes[i];

    // Count shapes
    if (n->type == WIDGET_STICKY || n->type == WIDGET_RECT ||
        n->type == WIDGET_OVAL || n->type == WIDGET_TRIANGLE) {
      shapes_count++;
    }

    // Frustum and LOD Culling
    if (n->type == WIDGET_ARROW) {
      int from = n->path_start_idx;
      int to = n->path_point_len;
      if (from >= 0 && from < node_count && to >= 0 && to < node_count) {
        Node *n_from = &nodes[from];
        Node *n_to = &nodes[to];
        float ax1 = n_from->x;
        float ax2 = n_to->x;
        float ay1 = n_from->y;
        float ay2 = n_to->y;
        float min_x = (ax1 < ax2) ? ax1 : ax2;
        float max_x = (ax1 > ax2) ? ax1 : ax2;
        float min_y = (ay1 < ay2) ? ay1 : ay2;
        float max_y = (ay1 > ay2) ? ay1 : ay2;
        if (max_x < left || min_x > right || max_y < top || min_y > bottom) {
          continue;
        }
      }
    } else if (n->type != WIDGET_PATH) {
      if (n->x + n->w < left || n->x > right || n->y + n->h < top ||
          n->y > bottom) {
        continue;
      }
      if (n->w * uniforms.zoom < 2.0f) {
        continue;
      }
    }

    draw_node_widget(n, i == editing_node_idx, texture_id);
  }

  if (is_selecting_marquee) {
    float w = mouse_world_x - marquee_start_x;
    float h = mouse_world_y - marquee_start_y;
    float draw_x = (w < 0.0f) ? marquee_start_x + w : marquee_start_x;
    float draw_y = (h < 0.0f) ? marquee_start_y + h : marquee_start_y;
    float draw_w = (w < 0.0f) ? -w : w;
    float draw_h = (h < 0.0f) ? -h : h;

    // Translucent fill
    draw_rect(draw_x, draw_y, draw_w, draw_h, 94.0f / 255.0f, 106.0f / 255.0f,
              210.0f / 255.0f, 0.15f);
    // Borders
    draw_line(draw_x, draw_y, draw_x + draw_w, draw_y, 1.0f, 94.0f / 255.0f,
              106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x + draw_w, draw_y, draw_x + draw_w, draw_y + draw_h, 1.0f,
              94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x + draw_w, draw_y + draw_h, draw_x, draw_y + draw_h, 1.0f,
              94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x, draw_y + draw_h, draw_x, draw_y, 1.0f, 94.0f / 255.0f,
              106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
  }

  // Flush any remaining accumulated geometry
  flush_batch(0, texture_id);

  // Write all accumulated vertex and index buffer data to the GPU in a single
  // operation
  if (index_count > 0) {
    js_wgpu_write_buffer(vertex_buffer_id, 0, vertex_buffer,
                         vertex_count * sizeof(Vertex));
    js_wgpu_write_buffer(index_buffer_id, 0, index_buffer,
                         index_count * sizeof(uint32_t));
  }

  // 4. Begin WebGPU render pass
  js_wgpu_begin_render_pass();

  // Submit uniforms (constant for the entire frame)
  js_wgpu_write_buffer(uniform_buffer_id, 0, &uniforms, sizeof(Uniforms));

  // Loop through recorded batches and submit their draw calls
  for (int b = 0; b < batch_count; b++) {
    DrawBatch *batch = &batches[b];
    js_wgpu_set_pipeline(batch->pipeline_id);
    js_wgpu_set_bind_group(0, uniform_buffer_id, batch->texture_id);
    js_wgpu_draw_indexed(batch->index_count, batch->index_start,
                         index_buffer_id, vertex_buffer_id);
  }

  js_wgpu_end_render_pass();

  // Report stats back to HTML overlay
  js_update_stats(uniforms.pan_x, uniforms.pan_y, uniforms.zoom, shapes_count);
}

void flush_batch(int pipeline_id, int bind_texture_id) {
  if (index_count == last_batch_index_start)
    return;

  if (batch_count < MAX_BATCHES) {
    batches[batch_count] =
        (DrawBatch){pipeline_id, bind_texture_id, last_batch_index_start,
                    index_count - last_batch_index_start};
    batch_count++;
  }
  last_batch_index_start = index_count;
}

__attribute__((visibility("default"))) void create_100k_infographics() {
  node_count = 0;
  selected_node_idx = -1;
  editing_node_idx = -1;

  int cols = 400;
  int rows = 250;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      float x_offset = col * 1500.0f;
      float y_offset = row * 800.0f;

      // 1. Left Panel Background (Dark Slate)
      add_node_full(WIDGET_RECT, 100.0f + x_offset, 100.0f + y_offset, 300.0f,
                    550.0f, "", -1);
      int left_bg = node_count - 1;
      nodes[left_bg].bg_r = 15.0f / 255.0f;
      nodes[left_bg].bg_g = 18.0f / 255.0f;
      nodes[left_bg].bg_b = 25.0f / 255.0f;
      nodes[left_bg].bg_a = 1.0f;
      nodes[left_bg].border_a = 0.0f;

      // 2. Right Panel Background (Sleek Dark Slate)
      add_node_full(WIDGET_RECT, 400.0f + x_offset, 100.0f + y_offset, 700.0f,
                    550.0f, "", -1);
      int right_bg = node_count - 1;
      nodes[right_bg].bg_r = 20.0f / 255.0f;
      nodes[right_bg].bg_g = 24.0f / 255.0f;
      nodes[right_bg].bg_b = 33.0f / 255.0f;
      nodes[right_bg].bg_a = 1.0f;
      nodes[right_bg].border_a = 0.0f;

      // 3. Title Text "DUST" in White
      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 130.0f + y_offset, 240.0f,
                    60.0f, "DUST", -1);
      int title_idx = node_count - 1;
      nodes[title_idx].bg_r = 226.0f / 255.0f;
      nodes[title_idx].bg_g = 232.0f / 255.0f;
      nodes[title_idx].bg_b = 240.0f / 255.0f;
      nodes[title_idx].font_size = 48.0f;

      // 4. Subtitle Text "WEBGPU WHITEBOARD" in Accent Indigo
      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 190.0f + y_offset, 240.0f,
                    30.0f, "WEBGPU WHITEBOARD", -1);
      int sub_idx = node_count - 1;
      nodes[sub_idx].bg_r = 94.0f / 255.0f;
      nodes[sub_idx].bg_g = 106.0f / 255.0f;
      nodes[sub_idx].bg_b = 210.0f / 255.0f;
      nodes[sub_idx].font_size = 14.0f;

      // 5. Left Panel Section header "WHITEBOARD LAYERS" in Muted Text
      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 270.0f + y_offset, 240.0f,
                    30.0f, "WHITEBOARD LAYERS", -1);
      int rm_header = node_count - 1;
      nodes[rm_header].bg_r = 148.0f / 255.0f;
      nodes[rm_header].bg_g = 163.0f / 255.0f;
      nodes[rm_header].bg_b = 184.0f / 255.0f;
      nodes[rm_header].font_size = 14.0f;

      // 6. Left Panel list items
      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 310.0f + y_offset, 240.0f,
                    30.0f, "Shapes", -1);
      int item1 = node_count - 1;
      nodes[item1].bg_r = 226.0f / 255.0f;
      nodes[item1].bg_g = 232.0f / 255.0f;
      nodes[item1].bg_b = 240.0f / 255.0f;
      nodes[item1].font_size = 18.0f;

      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 350.0f + y_offset, 240.0f,
                    30.0f, "Text", -1);
      int item2 = node_count - 1;
      nodes[item2].bg_r = 226.0f / 255.0f;
      nodes[item2].bg_g = 232.0f / 255.0f;
      nodes[item2].bg_b = 240.0f / 255.0f;
      nodes[item2].font_size = 18.0f;

      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 390.0f + y_offset, 240.0f,
                    30.0f, "Arrows", -1);
      int item3 = node_count - 1;
      nodes[item3].bg_r = 226.0f / 255.0f;
      nodes[item3].bg_g = 232.0f / 255.0f;
      nodes[item3].bg_b = 240.0f / 255.0f;
      nodes[item3].font_size = 18.0f;

      // 7. Left Panel footer "Wasm Core" and languages
      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 510.0f + y_offset, 240.0f,
                    30.0f, "Wasm Core", -1);
      int ft1 = node_count - 1;
      nodes[ft1].bg_r = 148.0f / 255.0f;
      nodes[ft1].bg_g = 163.0f / 255.0f;
      nodes[ft1].bg_b = 184.0f / 255.0f;
      nodes[ft1].font_size = 16.0f;

      add_node_full(WIDGET_TEXT, 130.0f + x_offset, 550.0f + y_offset, 240.0f,
                    30.0f, "Pure C · WebGPU · JS", -1);
      int ft2 = node_count - 1;
      nodes[ft2].bg_r = 94.0f / 255.0f;
      nodes[ft2].bg_g = 106.0f / 255.0f;
      nodes[ft2].bg_b = 210.0f / 255.0f;
      nodes[ft2].font_size = 13.0f;

      // 8. Right Panel big statement
      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 130.0f + y_offset, 600.0f,
                    80.0f, "Pure C Engine.\nWebGPU Pipelines.", -1);
      int right_title = node_count - 1;
      nodes[right_title].bg_r = 226.0f / 255.0f;
      nodes[right_title].bg_g = 232.0f / 255.0f;
      nodes[right_title].bg_b = 240.0f / 255.0f;
      nodes[right_title].font_size = 32.0f;

      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 220.0f + y_offset, 600.0f,
                    40.0f,
                    "Infinite collaborative canvas built natively on raw "
                    "WebGPU primitives.",
                    -1);
      int right_sub = node_count - 1;
      nodes[right_sub].bg_r = 148.0f / 255.0f;
      nodes[right_sub].bg_g = 163.0f / 255.0f;
      nodes[right_sub].bg_b = 184.0f / 255.0f;
      nodes[right_sub].font_size = 15.0f;

      // 9. Card 1 "01 WASM Core"
      add_node_full(WIDGET_RECT, 440.0f + x_offset, 280.0f + y_offset, 290.0f,
                    100.0f,
                    "01   WASM Core\nPure C whiteboard code compiles directly "
                    "to WebAssembly.",
                    -1);
      int card1 = node_count - 1;
      nodes[card1].bg_r = 30.0f / 255.0f;
      nodes[card1].bg_g = 35.0f / 255.0f;
      nodes[card1].bg_b = 48.0f / 255.0f;
      nodes[card1].bg_a = 1.0f;
      nodes[card1].border_r = 94.0f / 255.0f;
      nodes[card1].border_g = 106.0f / 255.0f;
      nodes[card1].border_b = 210.0f / 255.0f;
      nodes[card1].border_a = 1.0f;
      nodes[card1].font_size = 13.0f;

      // 10. Card 2 "02 Smooth MSAA"
      add_node_full(WIDGET_RECT, 750.0f + x_offset, 280.0f + y_offset, 290.0f,
                    100.0f,
                    "02   Smooth MSAA\nHardware MSAA 4x rendering for crisp "
                    "text, ovals & lines.",
                    -1);
      int card2 = node_count - 1;
      nodes[card2].bg_r = 30.0f / 255.0f;
      nodes[card2].bg_g = 35.0f / 255.0f;
      nodes[card2].bg_b = 48.0f / 255.0f;
      nodes[card2].bg_a = 1.0f;
      nodes[card2].border_r = 94.0f / 255.0f;
      nodes[card2].border_g = 106.0f / 255.0f;
      nodes[card2].border_b = 210.0f / 255.0f;
      nodes[card2].border_a = 1.0f;
      nodes[card2].font_size = 13.0f;

      // 11. Frame Composition Title
      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 410.0f + y_offset, 600.0f,
                    30.0f, "ENGINE COMPOSITION", -1);
      int fc_title = node_count - 1;
      nodes[fc_title].bg_r = 148.0f / 255.0f;
      nodes[fc_title].bg_g = 163.0f / 255.0f;
      nodes[fc_title].bg_b = 184.0f / 255.0f;
      nodes[fc_title].font_size = 14.0f;

      // 12. Frame Composition Rows
      // Row 1: C Geometry
      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 450.0f + y_offset, 150.0f,
                    30.0f, "C Geometry", -1);
      int geom_lbl = node_count - 1;
      nodes[geom_lbl].bg_r = 148.0f / 255.0f;
      nodes[geom_lbl].bg_g = 163.0f / 255.0f;
      nodes[geom_lbl].bg_b = 184.0f / 255.0f;
      nodes[geom_lbl].font_size = 14.0f;

      // Row 1 Bar Background
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 455.0f + y_offset, 400.0f,
                    20.0f, "", -1);
      int geom_bar_bg = node_count - 1;
      nodes[geom_bar_bg].bg_r = 30.0f / 255.0f;
      nodes[geom_bar_bg].bg_g = 35.0f / 255.0f;
      nodes[geom_bar_bg].bg_b = 48.0f / 255.0f;
      nodes[geom_bar_bg].bg_a = 1.0f;
      nodes[geom_bar_bg].border_a = 0.0f;

      // Row 1 Bar Value (Indigo Accent)
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 455.0f + y_offset, 300.0f,
                    20.0f, "", -1);
      int geom_bar_val = node_count - 1;
      nodes[geom_bar_val].bg_r = 94.0f / 255.0f;
      nodes[geom_bar_val].bg_g = 106.0f / 255.0f;
      nodes[geom_bar_val].bg_b = 210.0f / 255.0f;
      nodes[geom_bar_val].bg_a = 1.0f;
      nodes[geom_bar_val].border_a = 0.0f;

      // Row 2: Wasm Textures
      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 490.0f + y_offset, 150.0f,
                    30.0f, "Wasm Textures", -1);
      int typo_lbl = node_count - 1;
      nodes[typo_lbl].bg_r = 148.0f / 255.0f;
      nodes[typo_lbl].bg_g = 163.0f / 255.0f;
      nodes[typo_lbl].bg_b = 184.0f / 255.0f;
      nodes[typo_lbl].font_size = 14.0f;

      // Row 2 Bar Background
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 495.0f + y_offset, 400.0f,
                    20.0f, "", -1);
      int typo_bar_bg = node_count - 1;
      nodes[typo_bar_bg].bg_r = 30.0f / 255.0f;
      nodes[typo_bar_bg].bg_g = 35.0f / 255.0f;
      nodes[typo_bar_bg].bg_b = 48.0f / 255.0f;
      nodes[typo_bar_bg].bg_a = 1.0f;
      nodes[typo_bar_bg].border_a = 0.0f;

      // Row 2 Bar Value (Cyan Harmony)
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 495.0f + y_offset, 240.0f,
                    20.0f, "", -1);
      int typo_bar_val = node_count - 1;
      nodes[typo_bar_val].bg_r = 6.0f / 255.0f;
      nodes[typo_bar_val].bg_g = 182.0f / 255.0f;
      nodes[typo_bar_val].bg_b = 212.0f / 255.0f;
      nodes[typo_bar_val].bg_a = 1.0f;
      nodes[typo_bar_val].border_a = 0.0f;

      // Row 3: JS WebGPU
      add_node_full(WIDGET_TEXT, 440.0f + x_offset, 530.0f + y_offset, 150.0f,
                    30.0f, "JS WebGPU", -1);
      int int_lbl = node_count - 1;
      nodes[int_lbl].bg_r = 148.0f / 255.0f;
      nodes[int_lbl].bg_g = 163.0f / 255.0f;
      nodes[int_lbl].bg_b = 184.0f / 255.0f;
      nodes[int_lbl].font_size = 14.0f;

      // Row 3 Bar Background
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 535.0f + y_offset, 400.0f,
                    20.0f, "", -1);
      int int_bar_bg = node_count - 1;
      nodes[int_bar_bg].bg_r = 30.0f / 255.0f;
      nodes[int_bar_bg].bg_g = 35.0f / 255.0f;
      nodes[int_bar_bg].bg_b = 48.0f / 255.0f;
      nodes[int_bar_bg].bg_a = 1.0f;
      nodes[int_bar_bg].border_a = 0.0f;

      // Row 3 Bar Value (Purple Harmony)
      add_node_full(WIDGET_RECT, 600.0f + x_offset, 535.0f + y_offset, 180.0f,
                    20.0f, "", -1);
      int int_bar_val = node_count - 1;
      nodes[int_bar_val].bg_r = 139.0f / 255.0f;
      nodes[int_bar_val].bg_g = 92.0f / 255.0f;
      nodes[int_bar_val].bg_b = 246.0f / 255.0f;
      nodes[int_bar_val].bg_a = 1.0f;
      nodes[int_bar_val].border_a = 0.0f;

      // 13. Create connection arrows linking Card 1 to Geometry and Card 2 to
      // Typography
      add_connection(card1, geom_lbl);
      add_connection(card2, typo_lbl);
    }
  }
}

#include "widgets.c"
