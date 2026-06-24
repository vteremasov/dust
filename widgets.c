#include "widgets.h"

// Forward declarations of drawing functions from canvas.c
void push_vertex(float x, float y, float u, float v, float r, float g, float b, float a);
void push_index(unsigned int idx);
void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void draw_line(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a);
void draw_char(uint32_t codepoint, float x, float y, float w, float h, float r, float g, float b, float a);
int strlen(const char *str);
void flush_batch(int pipeline_id, int bind_texture_id);
uint32_t decode_utf8(const char **str);

#define SOLID_U 0.046875f
#define SOLID_V 0.015625f

// Math helpers
static float float_cos(float val) { return __builtin_cosf(val); }
static float float_sin(float val) { return __builtin_sinf(val); }

float get_shape_border_offset(Node *n, float ux, float uy) {
  float abs_ux = (ux < 0.0f) ? -ux : ux;
  float abs_uy = (uy < 0.0f) ? -uy : uy;
  
  float dist_x = (abs_ux > 0.0001f) ? (n->w / 2.0f) / abs_ux : 999999.0f;
  float dist_y = (abs_uy > 0.0001f) ? (n->h / 2.0f) / abs_uy : 999999.0f;
  
  return (dist_x < dist_y) ? dist_x : dist_y;
}

float distance_to_segment(float px, float py, float x1, float y1, float x2, float y2) {
  float l2 = (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
  if (l2 < 0.0001f) {
    float dx = px - x1;
    float dy = py - y1;
    return float_sqrt(dx * dx + dy * dy);
  }
  float t = ((px - x1) * (x2 - x1) + (py - y1) * (y2 - y1)) / l2;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  float proj_x = x1 + t * (x2 - x1);
  float proj_y = y1 + t * (y2 - y1);
  float dx = px - proj_x;
  float dy = py - proj_y;
  return float_sqrt(dx * dx + dy * dy);
}

extern int vertex_count;

void draw_oval(float cx, float cy, float rx, float ry, float r, float g, float b, float a) {
  int segments = 64;
  unsigned int center_idx = vertex_count;
  push_vertex(cx, cy, SOLID_U, SOLID_V, r, g, b, a);
  
  for (int i = 0; i <= segments; i++) {
    float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
    float x = cx + float_cos(angle) * rx;
    float y = cy + float_sin(angle) * ry;
    push_vertex(x, y, SOLID_U, SOLID_V, r, g, b, a);
  }
  
  for (int i = 1; i <= segments; i++) {
    push_index(center_idx);
    push_index(center_idx + i);
    push_index(center_idx + i + 1);
  }
}

void draw_oval_border(float cx, float cy, float rx, float ry, float thickness, float r, float g, float b, float a) {
  int segments = 64;
  float prev_x = cx + rx;
  float prev_y = cy;
  for (int i = 1; i <= segments; i++) {
    float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
    float x = cx + float_cos(angle) * rx;
    float y = cy + float_sin(angle) * ry;
    draw_line(prev_x, prev_y, x, y, thickness, r, g, b, a);
    prev_x = x;
    prev_y = y;
  }
}

void draw_image_rect(float x, float y, float w, float h, float alpha) {
  unsigned int start_idx = vertex_count;

  push_vertex(x, y, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, alpha);         // TL
  push_vertex(x + w, y, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, alpha);     // TR
  push_vertex(x, y + h, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, alpha);     // BL
  push_vertex(x + w, y + h, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, alpha); // BR

  push_index(start_idx + 0);
  push_index(start_idx + 2);
  push_index(start_idx + 1);

  push_index(start_idx + 1);
  push_index(start_idx + 2);
  push_index(start_idx + 3);
}

float get_char_advance(uint32_t codepoint);

void draw_widget_text_content(Node *n, float tx_offset_y, float char_w, float char_h, float tr, float tg, float tb, float ta) {
  int len = strlen(n->text);
  if (len <= 0) return;

  // Since the characters in the 64x64 cell are generated at 38px,
  // we scale the rendered quad to n->font_size * (64/38) to render them at the true logical font size.
  float cell_size = char_w * (64.0f / 38.0f);
  float line_spacing = char_h * 0.2f;

  int line_starts[32];
  int line_lens[32];
  int num_lines = 0;

  int curr_start = 0;
  int curr_len = 0;
  for (int j = 0; ; j++) {
    char c = n->text[j];
    if (c == '\n' || c == '\0') {
      if (num_lines < 32) {
        line_starts[num_lines] = curr_start;
        line_lens[num_lines] = curr_len;
        num_lines++;
      }
      if (c == '\0') {
        break;
      }
      curr_start = j + 1;
      curr_len = 0;
    } else if (c != '\r') {
      curr_len++;
    }
  }

  float total_text_h = char_h + (num_lines - 1) * (char_h + line_spacing);
  float start_ty = n->y + (n->h - total_text_h) / 2.0f + tx_offset_y;

  for (int l = 0; l < num_lines; l++) {
    int l_start = line_starts[l];
    int l_len = line_lens[l];
    
    // Calculate total proportional width of this line
    float total_w = 0.0f;
    const char *ptr = n->text + l_start;
    const char *line_end = ptr + l_len;
    while (ptr < line_end) {
      uint32_t cp = decode_utf8(&ptr);
      if (cp != '\r') {
        total_w += get_char_advance(cp) * char_w;
      }
    }
    
    float tx = n->x + (n->w - total_w) / 2.0f;
    float ty = start_ty + l * (char_h + line_spacing);

    // Draw each character in the range using proportional offset
    float cur_x = tx;
    ptr = n->text + l_start;
    while (ptr < line_end) {
      uint32_t cp = decode_utf8(&ptr);
      if (cp != '\r') {
        float advance = get_char_advance(cp) * char_w;
        // Position quad so glyph origin (at cell x=24) aligns with cur_x,
        // and glyph baseline (at cell y=46) aligns with the baseline (75% of char_h)
        float x_pos = cur_x - cell_size * (24.0f / 64.0f);
        float y_pos = ty - cell_size * (46.0f / 64.0f) + char_h * 0.75f;
        draw_char(cp, x_pos, y_pos, cell_size, cell_size, tr, tg, tb, ta);
        cur_x += advance;
      }
    }
  }
}

void draw_node_widget(Node *n, int is_editing, int default_texture_id) {
  float border = 4.0f;
  
  // 1. Draw Selection Outline
  if (n->selected) {
    if (n->type == WIDGET_OVAL) {
      draw_oval(n->x + n->w/2.0f, n->y + n->h/2.0f, n->w/2.0f + border, n->h/2.0f + border, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.35f);
    } else if (n->type != WIDGET_TEXT) {
      draw_rect(n->x - border, n->y - border, n->w + border * 2.0f, n->h + border * 2.0f, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.35f);
    } else {
      // For text, just a subtle dashed outline
      draw_rect(n->x - border, n->y - border, n->w + border * 2.0f, n->h + border * 2.0f, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.15f);
    }

    // Draw Resize Handle
    if (n->type != WIDGET_PATH) {
      extern Uniforms uniforms;
      float hs = 8.0f / uniforms.zoom;
      if (hs < 5.0f) hs = 5.0f;
      if (hs > 20.0f) hs = 20.0f;

      // Handle outline
      draw_rect(n->x + n->w - hs/2.0f - 1.0f, n->y + n->h - hs/2.0f - 1.0f, hs + 2.0f, hs + 2.0f, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 1.0f);
      // Handle fill
      draw_rect(n->x + n->w - hs/2.0f, n->y + n->h - hs/2.0f, hs, hs, 1.0f, 1.0f, 1.0f, 1.0f);
    }
  }

  // 2. Draw Widget Body
  if (n->type == WIDGET_STICKY) {
    if (n->bg_a > 0.001f) {
      draw_rect(n->x, n->y, n->w, n->h, n->bg_r, n->bg_g, n->bg_b, n->bg_a);
    }
    if (n->border_a > 0.001f) {
      draw_rect(n->x, n->y, n->w, n->h, n->border_r, n->border_g, n->border_b, n->border_a);
      if (n->bg_a > 0.001f) {
        draw_rect(n->x + 2.0f, n->y + 2.0f, n->w - 4.0f, n->h - 4.0f, n->bg_r, n->bg_g, n->bg_b, n->bg_a);
      }
    }
  } else if (n->type == WIDGET_RECT) {
    if (n->border_a > 0.001f) {
      draw_rect(n->x, n->y, n->w, n->h, n->border_r, n->border_g, n->border_b, n->border_a);
    }
    if (n->bg_a > 0.001f) {
      float t = (n->border_a > 0.001f) ? 2.0f : 0.0f;
      draw_rect(n->x + t, n->y + t, n->w - t*2.0f, n->h - t*2.0f, n->bg_r, n->bg_g, n->bg_b, n->bg_a);
    }
  } else if (n->type == WIDGET_OVAL) {
    if (n->bg_a > 0.001f) {
      draw_oval(n->x + n->w/2.0f, n->y + n->h/2.0f, n->w/2.0f, n->h/2.0f, n->bg_r, n->bg_g, n->bg_b, n->bg_a);
    }
    if (n->border_a > 0.001f) {
      draw_oval_border(n->x + n->w/2.0f, n->y + n->h/2.0f, n->w/2.0f, n->h/2.0f, 2.0f, n->border_r, n->border_g, n->border_b, n->border_a);
    }
  } else if (n->type == WIDGET_IMAGE) {
    // Image shape!
    if (n->texture_id != -1) {
      // Flush any existing batch before drawing this image
      flush_batch(0, default_texture_id);
      
      // Draw image geometry
      draw_image_rect(n->x, n->y, n->w, n->h, 1.0f);
      
      // Flush the image batch using pipeline 1
      flush_batch(1, n->texture_id);
    }
  } else if (n->type == WIDGET_PATH) {
    extern PathPoint path_points[];
    for (int i = 0; i < n->path_point_len - 1; i++) {
      PathPoint p1 = path_points[n->path_start_idx + i];
      PathPoint p2 = path_points[n->path_start_idx + i + 1];
      
      float x1 = n->x + p1.x;
      float y1 = n->y + p1.y;
      float x2 = n->x + p2.x;
      float y2 = n->y + p2.y;
      
      draw_line(x1, y1, x2, y2, 3.5f, n->bg_r, n->bg_g, n->bg_b, n->bg_a);
    }
  } else if (n->type == WIDGET_ARROW) {
    int from = n->path_start_idx;
    int to = n->path_point_len;
    extern Node nodes[];
    extern int node_count;
    
    if (from >= 0 && from < node_count && to >= 0 && to < node_count) {
      float x1 = nodes[from].x + nodes[from].w / 2.0f;
      float y1 = nodes[from].y + nodes[from].h / 2.0f;
      float x2 = nodes[to].x + nodes[to].w / 2.0f;
      float y2 = nodes[to].y + nodes[to].h / 2.0f;
      
      float dx = x2 - x1;
      float dy = y2 - y1;
      float len = float_sqrt(dx * dx + dy * dy);
      if (len > 1.0f) {
        float ux = dx / len;
        float uy = dy / len;
        
        float offset_A = get_shape_border_offset(&nodes[from], ux, uy);
        float offset_B = get_shape_border_offset(&nodes[to], ux, uy);
        
        float sx = x1 + ux * offset_A;
        float sy = y1 + uy * offset_A;
        float ex = x2 - ux * offset_B;
        float ey = y2 - uy * offset_B;
        
        float r = n->bg_r;
        float g = n->bg_g;
        float b = n->bg_b;
        float a = n->bg_a;
        
        float head_len = 12.0f;
        float shaft_end_offset = head_len * 0.7f;
        float line_ex = ex - ux * shaft_end_offset;
        float line_ey = ey - uy * shaft_end_offset;
        
        if (n->selected) {
          draw_line(sx, sy, line_ex, line_ey, 7.0f, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.4f);
        }
        
        draw_line(sx, sy, line_ex, line_ey, 3.0f, r, g, b, a);
        
        float rx1 = -ux * 0.866f - -uy * 0.5f;
        float ry1 = -ux * 0.5f + -uy * 0.866f;
        float rx2 = -ux * 0.866f - -uy * -0.5f;
        float ry2 = -ux * -0.5f + -uy * 0.866f;
        
        if (n->selected) {
          float scale_sel = 1.35f;
          unsigned int start_idx_sel = vertex_count;
          push_vertex(ex + ux * 2.0f, ey + uy * 2.0f, SOLID_U, SOLID_V, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.4f);
          push_vertex(ex + rx1 * (head_len * scale_sel), ey + ry1 * (head_len * scale_sel), SOLID_U, SOLID_V, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.4f);
          push_vertex(ex + rx2 * (head_len * scale_sel), ey + ry2 * (head_len * scale_sel), SOLID_U, SOLID_V, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.4f);
          
          push_index(start_idx_sel + 0);
          push_index(start_idx_sel + 1);
          push_index(start_idx_sel + 2);
        }
        
        unsigned int start_idx = vertex_count;
        push_vertex(ex, ey, SOLID_U, SOLID_V, r, g, b, a);
        push_vertex(ex + rx1 * head_len, ey + ry1 * head_len, SOLID_U, SOLID_V, r, g, b, a);
        push_vertex(ex + rx2 * head_len, ey + ry2 * head_len, SOLID_U, SOLID_V, r, g, b, a);
        
        push_index(start_idx + 0);
        push_index(start_idx + 1);
        push_index(start_idx + 2);
      }
    }
  }

  // 3. Draw Text Content using MSDF characters directly in pipeline 0
  if (!is_editing && n->type != WIDGET_IMAGE && n->type != WIDGET_PATH && n->type != WIDGET_ARROW) {
    float tr = n->r;
    float tg = n->g;
    float tb = n->b;
    if (n->type == WIDGET_TEXT) {
      tr = n->bg_r;
      tg = n->bg_g;
      tb = n->bg_b;
    }
    float char_h = n->font_size;
    float char_w = n->font_size;
    
    draw_widget_text_content(n, 0.0f, char_w, char_h, tr, tg, tb, 1.0f);
  }
}
