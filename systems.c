#include "systems.h"

#define SOLID_U 0.046875f
#define SOLID_V 0.015625f

// External declarations from canvas.c
typedef struct {
  float pan_x;
  float pan_y;
  float zoom;
  float pad1;
  float screen_width;
  float screen_height;
  float pad2[2];
} Uniforms;

extern Uniforms uniforms;
extern int vertex_count;

extern void push_vertex(float x, float y, float u, float v, float r, float g, float b, float a);
extern void push_index(unsigned int idx);
extern void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);
extern void draw_line(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a);
extern void draw_char(unsigned int codepoint, float x, float y, float w, float h, float r, float g, float b, float a);
extern void flush_batch(int pipeline_id, int bind_texture_id);
extern float get_char_advance(unsigned int codepoint);
extern unsigned int decode_utf8(const char **str);
extern float float_sqrt(float val);

// Math helpers
static float float_cos(float val) { return __builtin_cosf(val); }
static float float_sin(float val) { return __builtin_sinf(val); }

static int local_strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

// Math/Layout helpers
float get_shape_border_offset(Entity e, float ux, float uy) {
  if (!ecs_has_component(e, COMP_TRANSFORM)) return 0.0f;
  TransformComponent *t = &transform_components[e];
  float abs_ux = (ux < 0.0f) ? -ux : ux;
  float abs_uy = (uy < 0.0f) ? -uy : uy;
  
  float dist_x = (abs_ux > 0.0001f) ? (t->w / 2.0f) / abs_ux : 999999.0f;
  float dist_y = (abs_uy > 0.0001f) ? (t->h / 2.0f) / abs_uy : 999999.0f;
  
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

// Drawing helpers
static void draw_oval(float cx, float cy, float rx, float ry, float r, float g, float b, float a) {
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

static void draw_filled_circle(float cx, float cy, float radius, float r, float g, float b, float a) {
  int segments = 8;
  unsigned int center_idx = vertex_count;
  push_vertex(cx, cy, SOLID_U, SOLID_V, r, g, b, a);
  
  for (int i = 0; i < segments; i++) {
    float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
    float x = cx + float_cos(angle) * radius;
    float y = cy + float_sin(angle) * radius;
    push_vertex(x, y, SOLID_U, SOLID_V, r, g, b, a);
  }
  
  for (int i = 0; i < segments; i++) {
    push_index(center_idx);
    push_index(center_idx + 1 + i);
    push_index(center_idx + 1 + ((i + 1) % segments));
  }
}

static void draw_oval_border(float cx, float cy, float rx, float ry, float thickness, float r, float g, float b, float a) {
  int segments = 64;
  float prev_x = cx + rx;
  float prev_y = cy;
  for (int i = 1; i <= segments; i++) {
    float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
    float x = cx + float_cos(angle) * rx;
    float y = cy + float_sin(angle) * ry;
    draw_line(prev_x, prev_y, x, y, thickness, r, g, b, a);
    draw_filled_circle(prev_x, prev_y, thickness / 2.0f, r, g, b, a);
    prev_x = x;
    prev_y = y;
  }
}

static void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, float r, float g, float b, float a) {
  unsigned int start_idx = vertex_count;
  push_vertex(x1, y1, SOLID_U, SOLID_V, r, g, b, a);
  push_vertex(x2, y2, SOLID_U, SOLID_V, r, g, b, a);
  push_vertex(x3, y3, SOLID_U, SOLID_V, r, g, b, a);
  
  push_index(start_idx + 0);
  push_index(start_idx + 1);
  push_index(start_idx + 2);
}

static void draw_triangle_border(float x1, float y1, float x2, float y2, float x3, float y3, float thickness, float r, float g, float b, float a) {
  draw_line(x1, y1, x2, y2, thickness, r, g, b, a);
  draw_line(x2, y2, x3, y3, thickness, r, g, b, a);
  draw_line(x3, y3, x1, y1, thickness, r, g, b, a);
  
  draw_filled_circle(x1, y1, thickness / 2.0f, r, g, b, a);
  draw_filled_circle(x2, y2, thickness / 2.0f, r, g, b, a);
  draw_filled_circle(x3, y3, thickness / 2.0f, r, g, b, a);
}

static void draw_image_rect(float x, float y, float w, float h, float alpha) {
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

static void draw_widget_text_content(Entity e, float tx_offset_y, float char_w, float char_h, float tr, float tg, float tb, float ta) {
  if (!ecs_has_component(e, COMP_TEXT)) return;
  const char *text = text_components[e].text;
  int len = local_strlen(text);
  if (len <= 0) return;

  float cell_size = char_w * (64.0f / 38.0f);
  float line_spacing = char_h * 0.2f;

  int line_starts[32];
  int line_lens[32];
  int num_lines = 0;

  int curr_start = 0;
  int curr_len = 0;
  for (int j = 0; ; j++) {
    char c = text[j];
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

  TransformComponent *t = &transform_components[e];
  float total_text_h = char_h + (num_lines - 1) * (char_h + line_spacing);
  float start_ty = t->y + (t->h - total_text_h) / 2.0f + tx_offset_y;

  for (int l = 0; l < num_lines; l++) {
    int l_start = line_starts[l];
    int l_len = line_lens[l];
    
    // Calculate total proportional width of this line
    float total_w = 0.0f;
    const char *ptr = text + l_start;
    const char *line_end = ptr + l_len;
    while (ptr < line_end) {
      unsigned int cp = decode_utf8(&ptr);
      if (cp != '\r') {
        total_w += get_char_advance(cp) * char_w;
      }
    }
    
    float tx = t->x + (t->w - total_w) / 2.0f;
    float ty = start_ty + l * (char_h + line_spacing);

    // Draw each character in the range using proportional offset
    float cur_x = tx;
    ptr = text + l_start;
    while (ptr < line_end) {
      unsigned int cp = decode_utf8(&ptr);
      if (cp != '\r') {
        float advance = get_char_advance(cp) * char_w;
        float x_pos = cur_x - cell_size * (24.0f / 64.0f);
        float y_pos = ty - cell_size * (46.0f / 64.0f) + char_h * 0.75f;
        draw_char(cp, x_pos, y_pos, cell_size, cell_size, tr, tg, tb, ta);
        cur_x += advance;
      }
    }
  }
}

static void draw_entity(Entity e, int is_editing, int default_texture_id) {
  if (!ecs_has_component(e, COMP_RENDER) || !ecs_has_component(e, COMP_TRANSFORM)) return;
  RenderComponent *r = &render_components[e];
  TransformComponent *t = &transform_components[e];
  InteractionComponent *inter = ecs_has_component(e, COMP_INTERACTION) ? &interaction_components[e] : NULL;



  // 2. Draw Widget Body
  if (r->type == WIDGET_STICKY) {
    if (r->bg_a > 0.001f) {
      draw_rect(t->x, t->y, t->w, t->h, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
    }
    if (r->border_a > 0.001f) {
      draw_rect(t->x, t->y, t->w, t->h, r->border_r, r->border_g, r->border_b, r->border_a);
      if (r->bg_a > 0.001f) {
        draw_rect(t->x + 2.0f, t->y + 2.0f, t->w - 4.0f, t->h - 4.0f, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
      }
    }
  } else if (r->type == WIDGET_RECT) {
    if (r->border_a > 0.001f) {
      draw_rect(t->x, t->y, t->w, t->h, r->border_r, r->border_g, r->border_b, r->border_a);
    }
    if (r->bg_a > 0.001f) {
      float tr = (r->border_a > 0.001f) ? 2.0f : 0.0f;
      draw_rect(t->x + tr, t->y + tr, t->w - tr * 2.0f, t->h - tr * 2.0f, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
    }
  } else if (r->type == WIDGET_OVAL) {
    if (r->bg_a > 0.001f) {
      draw_oval(t->x + t->w/2.0f, t->y + t->h/2.0f, t->w/2.0f, t->h/2.0f, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
    }
    if (r->border_a > 0.001f) {
      draw_oval_border(t->x + t->w/2.0f, t->y + t->h/2.0f, t->w/2.0f, t->h/2.0f, 2.0f, r->border_r, r->border_g, r->border_b, r->border_a);
    }
  } else if (r->type == WIDGET_TRIANGLE) {
    float x1 = t->x + t->w / 2.0f;
    float y1 = t->y;
    float x2 = t->x;
    float y2 = t->y + t->h;
    float x3 = t->x + t->w;
    float y3 = t->y + t->h;
    if (r->bg_a > 0.001f) {
      draw_triangle(x1, y1, x2, y2, x3, y3, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
    }
    if (r->border_a > 0.001f) {
      draw_triangle_border(x1, y1, x2, y2, x3, y3, 2.0f, r->border_r, r->border_g, r->border_b, r->border_a);
    }
  } else if (r->type == WIDGET_IMAGE) {
    if (r->texture_id != -1) {
      flush_batch(0, default_texture_id);
      draw_image_rect(t->x, t->y, t->w, t->h, 1.0f);
      flush_batch(1, r->texture_id);
    }
  } else if (r->type == WIDGET_PATH) {
    if (ecs_has_component(e, COMP_PATH)) {
      PathDrawingComponent *p = &path_components[e];
      float thickness = 3.5f;
      float radius = thickness / 2.0f;
      for (int i = 0; i < p->path_point_len - 1; i++) {
        PathPoint p1 = path_points[p->path_start_idx + i];
        PathPoint p2 = path_points[p->path_start_idx + i + 1];
        
        float x1 = t->x + p1.x;
        float y1 = t->y + p1.y;
        float x2 = t->x + p2.x;
        float y2 = t->y + p2.y;
        
        draw_line(x1, y1, x2, y2, thickness, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
        draw_filled_circle(x1, y1, radius, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
        if (i == p->path_point_len - 2) {
          draw_filled_circle(x2, y2, radius, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
        }
      }
    }
  }

  // 3. Draw Text Content using MSDF characters directly in pipeline 0
  if (!is_editing && r->type != WIDGET_IMAGE && r->type != WIDGET_PATH && r->type != WIDGET_ARROW) {
    float tr = r->r;
    float tg = r->g;
    float tb = r->b;
    if (r->type == WIDGET_TEXT) {
      tr = r->bg_r;
      tg = r->bg_g;
      tb = r->bg_b;
    }
    float char_h = r->font_size;
    float char_w = r->font_size;
    
    draw_widget_text_content(e, 0.0f, char_w, char_h, tr, tg, tb, 1.0f);
  }
}

static void draw_connection_arrow(Entity e, int editing_node_idx) {
  if (!ecs_has_component(e, COMP_CONNECTION) || !ecs_has_component(e, COMP_RENDER)) return;
  ConnectionComponent *conn = &connection_components[e];
  RenderComponent *r = &render_components[e];
  InteractionComponent *inter = ecs_has_component(e, COMP_INTERACTION) ? &interaction_components[e] : NULL;

  int from = conn->from_entity;
  int to = conn->to_entity;

  if (from >= 0 && from < (int)entity_count && to >= 0 && to < (int)entity_count) {
    TransformComponent *t_from = &transform_components[from];
    TransformComponent *t_to = &transform_components[to];

    float x1 = t_from->x + t_from->w / 2.0f;
    float y1 = t_from->y + t_from->h / 2.0f;
    float x2 = t_to->x + t_to->w / 2.0f;
    float y2 = t_to->y + t_to->h / 2.0f;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = float_sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
      float ux = dx / len;
      float uy = dy / len;
      
      float offset_A = get_shape_border_offset(from, ux, uy);
      float offset_B = get_shape_border_offset(to, ux, uy);
      
      float sx = x1 + ux * offset_A;
      float sy = y1 + uy * offset_A;
      float ex = x2 - ux * offset_B;
      float ey = y2 - uy * offset_B;
      
      float head_len = 12.0f;
      float shaft_end_offset = head_len * 0.7f;
      float line_ex = ex - ux * shaft_end_offset;
      float line_ey = ey - uy * shaft_end_offset;
      
      if (inter && inter->selected) {
        draw_line(sx, sy, line_ex, line_ey, 7.0f, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.4f);
      }
      
      draw_line(sx, sy, line_ex, line_ey, 3.0f, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
      
      float rx1 = -ux * 0.866f - -uy * 0.5f;
      float ry1 = -ux * 0.5f + -uy * 0.866f;
      float rx2 = -ux * 0.866f - -uy * -0.5f;
      float ry2 = -ux * -0.5f + -uy * 0.866f;
      
      if (inter && inter->selected) {
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
      push_vertex(ex, ey, SOLID_U, SOLID_V, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
      push_vertex(ex + rx1 * head_len, ey + ry1 * head_len, SOLID_U, SOLID_V, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
      push_vertex(ex + rx2 * head_len, ey + ry2 * head_len, SOLID_U, SOLID_V, r->bg_r, r->bg_g, r->bg_b, r->bg_a);
      
      push_index(start_idx + 0);
      push_index(start_idx + 1);
      push_index(start_idx + 2);
    }
  }
}

// ECS Systems
static void draw_selection_outline_and_handles(Entity e) {
  if (!ecs_has_component(e, COMP_RENDER) || !ecs_has_component(e, COMP_TRANSFORM)) return;
  RenderComponent *r = &render_components[e];
  TransformComponent *t = &transform_components[e];

  float pr = 94.0f / 255.0f;
  float pg = 106.0f / 255.0f;
  float pb = 210.0f / 255.0f;
  float alpha = (r->type == WIDGET_TEXT) ? 0.6f : 1.0f;

  // 1. Draw Selection Contour
  if (r->type == WIDGET_OVAL) {
    draw_oval_border(t->x + t->w/2.0f, t->y + t->h/2.0f, t->w/2.0f, t->h/2.0f, 1.5f, pr, pg, pb, alpha);
  } else {
    float x1 = t->x;
    float y1 = t->y;
    float x2 = t->x + t->w;
    float y2 = t->y + t->h;

    draw_line(x1, y1, x2, y1, 1.5f, pr, pg, pb, alpha);
    draw_line(x2, y1, x2, y2, 1.5f, pr, pg, pb, alpha);
    draw_line(x2, y2, x1, y2, 1.5f, pr, pg, pb, alpha);
    draw_line(x1, y2, x1, y1, 1.5f, pr, pg, pb, alpha);

    // Smooth corners for clean contour rectangle
    draw_filled_circle(x1, y1, 0.75f, pr, pg, pb, alpha);
    draw_filled_circle(x2, y1, 0.75f, pr, pg, pb, alpha);
    draw_filled_circle(x2, y2, 0.75f, pr, pg, pb, alpha);
    draw_filled_circle(x1, y2, 0.75f, pr, pg, pb, alpha);
  }

  // 2. Draw Resize Handles (only for non-path widgets)
  if (r->type != WIDGET_PATH) {
    float hs = 6.0f / uniforms.zoom;
    if (hs < 3.5f) hs = 3.5f;
    if (hs > 10.0f) hs = 10.0f;

    float x_coords[8] = {
      t->x, t->x + t->w, t->x, t->x + t->w,             // Corners
      t->x + t->w / 2.0f, t->x + t->w / 2.0f,          // Top & Bottom middles
      t->x, t->x + t->w                                // Left & Right middles
    };
    float y_coords[8] = {
      t->y, t->y, t->y + t->h, t->y + t->h,             // Corners
      t->y, t->y + t->h,                               // Top & Bottom middles
      t->y + t->h / 2.0f, t->y + t->h / 2.0f           // Left & Right middle
    };

    float inner_size = hs - 2.0f;
    if (inner_size < 1.0f) inner_size = 1.0f;

    for (int i = 0; i < 8; i++) {
      // Draw purple border square
      draw_rect(x_coords[i] - hs / 2.0f, y_coords[i] - hs / 2.0f, hs, hs, pr, pg, pb, 1.0f);
      // Draw inner white square
      draw_rect(x_coords[i] - inner_size / 2.0f, y_coords[i] - inner_size / 2.0f, inner_size, inner_size, 1.0f, 1.0f, 1.0f, 1.0f);
    }
  }
}

void render_system(float left, float right, float top, float bottom, int editing_node_idx, int texture_id) {
  // Pass 1: Render all widget bodies and arrows
  for (unsigned int i = 0; i < entity_count; i++) {
    if (!ecs_has_component(i, COMP_RENDER)) continue;

    RenderComponent *r = &render_components[i];

    // Culling system check
    if (r->type == WIDGET_ARROW) {
      if (ecs_has_component(i, COMP_CONNECTION)) {
        ConnectionComponent *conn = &connection_components[i];
        int from = conn->from_entity;
        int to = conn->to_entity;
        if (from >= 0 && from < (int)entity_count && to >= 0 && to < (int)entity_count) {
          TransformComponent *t_from = &transform_components[from];
          TransformComponent *t_to = &transform_components[to];
          float ax1 = t_from->x;
          float ax2 = t_to->x;
          float ay1 = t_from->y;
          float ay2 = t_to->y;
          float min_x = (ax1 < ax2) ? ax1 : ax2;
          float max_x = (ax1 > ax2) ? ax1 : ax2;
          float min_y = (ay1 < ay2) ? ay1 : ay2;
          float max_y = (ay1 > ay2) ? ay1 : ay2;
          if (max_x < left || min_x > right || max_y < top || min_y > bottom) {
            continue;
          }
        }
      }
    } else if (r->type != WIDGET_PATH) {
      if (ecs_has_component(i, COMP_TRANSFORM)) {
        TransformComponent *t = &transform_components[i];
        if (t->x + t->w < left || t->x > right || t->y + t->h < top || t->y > bottom) {
          continue;
        }
        if (t->w * uniforms.zoom < 2.0f) {
          continue;
        }
      }
    }

    // Delegate rendering
    if (r->type == WIDGET_ARROW) {
      draw_connection_arrow(i, editing_node_idx);
    } else {
      draw_entity(i, (int)i == editing_node_idx, texture_id);
    }
  }

  // Pass 2: Render selection contours and handles on top of everything
  for (unsigned int i = 0; i < entity_count; i++) {
    if (!ecs_has_component(i, COMP_RENDER) || !ecs_has_component(i, COMP_TRANSFORM)) continue;

    RenderComponent *r = &render_components[i];
    if (r->type == WIDGET_ARROW) continue;

    // Culling system check for contour/handles
    TransformComponent *t = &transform_components[i];
    if (t->x + t->w < left || t->x > right || t->y + t->h < top || t->y > bottom) {
      continue;
    }

    InteractionComponent *inter = ecs_has_component(i, COMP_INTERACTION) ? &interaction_components[i] : NULL;
    if (inter && inter->selected) {
      draw_selection_outline_and_handles(i);
    }
  }
}

void drag_system(float dx, float dy) {
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_INTERACTION) && ecs_has_component(i, COMP_TRANSFORM)) {
      if (interaction_components[i].is_dragging) {
        transform_components[i].x += dx;
        transform_components[i].y += dy;
      }
    }
  }
}

void marquee_system(float draw_x, float draw_y, float draw_w, float draw_h, int *selected_node_idx) {
  *selected_node_idx = -1;
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_INTERACTION)) {
      InteractionComponent *inter = &interaction_components[i];
      if (ecs_has_component(i, COMP_RENDER) && render_components[i].type != WIDGET_ARROW && render_components[i].type != WIDGET_PATH) {
        TransformComponent *t = &transform_components[i];
        int overlap = (t->x < draw_x + draw_w && t->x + t->w > draw_x &&
                       t->y < draw_y + draw_h && t->y + t->h > draw_y);
        inter->selected = overlap;
        if (overlap) {
          *selected_node_idx = (int)i;
        }
      } else {
        inter->selected = 0;
      }
    }
  }
}
