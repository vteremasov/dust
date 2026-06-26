#include "renderer.h"
#include "font.generated.h"

// -------------------------------------------------------------
// Font Atlas Builder Constants
// -------------------------------------------------------------
// UV coord for solid block (character index 1 in 64x64 cell layout)
#define SOLID_U 0.046875f
#define SOLID_V 0.015625f

// Grid size for 64x64 cell atlas layout
#define GRID_SIZE 32

// -------------------------------------------------------------
// Shared Global State Definitions
// -------------------------------------------------------------
Vertex vertex_buffer[MAX_VERTICES];
uint32_t index_buffer[MAX_INDICES];
int vertex_count = 0;
int index_count = 0;

Uniforms uniforms;

DrawBatch batches[MAX_BATCHES];
int batch_count = 0;
unsigned int last_batch_index_start = 0;

int vertex_buffer_id = -1;
int index_buffer_id = -1;
int uniform_buffer_id = -1;
int texture_id = -1;

float current_time_ms = 0.0f;
int needs_redraw = 1;

int active_stroke_node_idx = -1;

// -------------------------------------------------------------
// Standard Runtime replacements
// -------------------------------------------------------------
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
// Font Atlas Helper & Cache Lookup
// -------------------------------------------------------------
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

static const GlyphInfo *ascii_glyph_cache[256];
static int ascii_glyph_cache_initialized = 0;

static const GlyphInfo *lookup_glyph(uint32_t codepoint) {
  if (codepoint < 256) {
    if (!ascii_glyph_cache_initialized) {
      for (int i = 0; i < 256; i++) {
        ascii_glyph_cache[i] = NULL;
      }
      for (int i = 0; i < FONT_GLYPHS_COUNT; i++) {
        uint32_t cp = font_glyphs[i].codepoint;
        if (cp < 256) {
          ascii_glyph_cache[cp] = &font_glyphs[i];
        }
      }
      ascii_glyph_cache_initialized = 1;
    }
    return ascii_glyph_cache[codepoint];
  }

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
// Immediate Mode Drawing Pipeline implementations
// -------------------------------------------------------------
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

void draw_char(uint32_t codepoint, float x, float y, float w, float h, float r,
               float g, float b, float a) {
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

void mark_dirty() { needs_redraw = 1; }

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

static Uniforms last_uploaded_uniforms;
static int first_uniform_upload = 1;

void upload_uniforms_if_changed() {
  int changed = first_uniform_upload ||
                uniforms.pan_x != last_uploaded_uniforms.pan_x ||
                uniforms.pan_y != last_uploaded_uniforms.pan_y ||
                uniforms.zoom != last_uploaded_uniforms.zoom ||
                uniforms.screen_width != last_uploaded_uniforms.screen_width ||
                uniforms.screen_height != last_uploaded_uniforms.screen_height;
  if (changed) {
    js_wgpu_write_buffer(uniform_buffer_id, 0, &uniforms, sizeof(Uniforms));
    last_uploaded_uniforms = uniforms;
    first_uniform_upload = 0;
  }
}
