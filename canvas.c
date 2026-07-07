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
WASM_IMPORT("js_on_entity_shifted")
void js_on_entity_shifted(int from_idx, int to_idx);

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

#include "renderer.h"
#include "initial_content.h"
#include "systems.h"

void add_widget_wasm(int type, float x, float y, int texture_id, int img_w, int img_h);
const char* get_node_text_ptr(int idx);

#define NULL ((void *)0)

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

typedef enum {
  RESIZE_NONE = 0,
  RESIZE_TL,
  RESIZE_TR,
  RESIZE_BL,
  RESIZE_BR,
  RESIZE_T,
  RESIZE_B,
  RESIZE_L,
  RESIZE_R
} ResizeHandle;

ResizeHandle active_resize_handle = RESIZE_NONE;
float resize_initial_x = 0.0f;
float resize_initial_y = 0.0f;
float resize_initial_w = 150.0f;
float resize_initial_h = 150.0f;
float resize_start_mouse_x = 0.0f;
float resize_start_mouse_y = 0.0f;

int is_selecting_marquee = 0;
float marquee_start_x = 0.0f;
float marquee_start_y = 0.0f;

int space_pressed = 0;
int shift_pressed = 0;
int editing_node_idx = -1;

float last_click_timestamp = 0.0f;


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

  int success = js_wgpu_init(width, height);
  if (!success) {
    return 0;
  }

  vertex_buffer_id = js_wgpu_create_buffer(sizeof(Vertex) * MAX_VERTICES, 1);
  index_buffer_id = js_wgpu_create_buffer(sizeof(uint32_t) * MAX_INDICES, 2);
  uniform_buffer_id = js_wgpu_create_buffer(sizeof(Uniforms), 3);
  texture_id = js_wgpu_create_texture(2048, 2048);

  js_wgpu_write_texture(texture_id, 2048, 2048, NULL, 0);

  ecs_init();
  generate_initial_content();

  log_str("Canvas loaded successfully.");
  return 1;
}

__attribute__((visibility("default"))) void on_resize(int width, int height) {
  uniforms.screen_width = (float)width;
  uniforms.screen_height = (float)height;
  mark_dirty();
}

void clamp_node_size_with_aspect_idx(int idx, float *w, float *h) {
  WidgetType type = render_components[idx].type;
  if (type == WIDGET_STICKY) {
    if (*w < 40.0f) {
      *w = 40.0f;
    }
    *h = *w;
  } else if (type == WIDGET_IMAGE) {
    if (transform_components[idx].h > 0.0f) {
      float aspect = transform_components[idx].w / transform_components[idx].h;
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
  mark_dirty();
  float world_x = (x - uniforms.pan_x) / uniforms.zoom;
  float world_y = (y - uniforms.pan_y) / uniforms.zoom;
  mouse_world_x = world_x;
  mouse_world_y = world_y;
  last_mouse_world_x = world_x;
  last_mouse_world_y = world_y;

  static float prev_click_time = 0.0f;
  extern float current_time_ms;
  float now = current_time_ms;
  float delta = now - prev_click_time;
  js_log_click_delta(delta);
  int is_double_click = (delta < 300.0f);
  prev_click_time = now;

  // Check if we hit any resize handle of the selected node
  ResizeHandle hit_handle = RESIZE_NONE;
  if (selected_node_idx != -1) {
    TransformComponent *sn = &transform_components[selected_node_idx];
    float click_tolerance = 12.0f / uniforms.zoom;

    float x_l = sn->x;
    float x_r = sn->x + sn->w;
    float y_t = sn->y;
    float y_b = sn->y + sn->h;

    // Check corners first (high priority)
    float dist_tl_x = world_x - x_l;
    float dist_tl_y = world_y - y_t;
    if (float_sqrt(dist_tl_x * dist_tl_x + dist_tl_y * dist_tl_y) <=
        click_tolerance) {
      hit_handle = RESIZE_TL;
    } else if (float_sqrt((world_x - x_r) * (world_x - x_r) +
                          (world_y - y_t) * (world_y - y_t)) <=
               click_tolerance) {
      hit_handle = RESIZE_TR;
    } else if (float_sqrt((world_x - x_l) * (world_x - x_l) +
                          (world_y - y_b) * (world_y - y_b)) <=
               click_tolerance) {
      hit_handle = RESIZE_BL;
    } else if (float_sqrt((world_x - x_r) * (world_x - x_r) +
                          (world_y - y_b) * (world_y - y_b)) <=
               click_tolerance) {
      hit_handle = RESIZE_BR;
    }
    // Check edges
    else if (distance_to_segment(world_x, world_y, x_l, y_t, x_r, y_t) <=
             click_tolerance) {
      hit_handle = RESIZE_T;
    } else if (distance_to_segment(world_x, world_y, x_l, y_b, x_r, y_b) <=
               click_tolerance) {
      hit_handle = RESIZE_B;
    } else if (distance_to_segment(world_x, world_y, x_l, y_t, x_l, y_b) <=
               click_tolerance) {
      hit_handle = RESIZE_L;
    } else if (distance_to_segment(world_x, world_y, x_r, y_t, x_r, y_b) <=
               click_tolerance) {
      hit_handle = RESIZE_R;
    }
  }

  if (hit_handle != RESIZE_NONE) {
    is_resizing_node = 1;
    active_resize_handle = hit_handle;
    resize_initial_x = transform_components[selected_node_idx].x;
    resize_initial_y = transform_components[selected_node_idx].y;
    resize_initial_w = transform_components[selected_node_idx].w;
    resize_initial_h = transform_components[selected_node_idx].h;
    resize_start_mouse_x = world_x;
    resize_start_mouse_y = world_y;
    is_panning = 0;
    is_dragging_node = 0;
    return;
  }

  // Hit detection loop
  int hit_node_idx = -1;
  for (int i = (int)entity_count - 1; i >= 0; i--) {
    if (ecs_has_component(i, COMP_CONNECTION)) {
      ConnectionComponent *conn = &connection_components[i];
      int from = conn->from_entity;
      int to = conn->to_entity;
      if (from >= 0 && from < (int)entity_count && to >= 0 &&
          to < (int)entity_count) {
        float x1 =
            transform_components[from].x + transform_components[from].w / 2.0f;
        float y1 =
            transform_components[from].y + transform_components[from].h / 2.0f;
        float x2 =
            transform_components[to].x + transform_components[to].w / 2.0f;
        float y2 =
            transform_components[to].y + transform_components[to].h / 2.0f;
        float dist = distance_to_segment(world_x, world_y, x1, y1, x2, y2);
        float tolerance = 8.0f / uniforms.zoom;
        if (dist <= tolerance) {
          hit_node_idx = i;
          break;
        }
      }
    } else if (ecs_has_component(i, COMP_PATH)) {
      PathDrawingComponent *p = &path_components[i];
      float tolerance = 8.0f / uniforms.zoom;
      int hit = 0;
      for (int k = 0; k < p->path_point_len - 1; k++) {
        PathPoint p1 = path_points[p->path_start_idx + k];
        PathPoint p2 = path_points[p->path_start_idx + k + 1];
        float x1 = transform_components[i].x + p1.x;
        float y1 = transform_components[i].y + p1.y;
        float x2 = transform_components[i].x + p2.x;
        float y2 = transform_components[i].y + p2.y;
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
    } else if (ecs_has_component(i, COMP_TRANSFORM)) {
      TransformComponent *n = &transform_components[i];
      if (world_x >= n->x && world_x <= n->x + n->w && world_y >= n->y &&
          world_y <= n->y + n->h) {
        hit_node_idx = i;
        break;
      }
    }
  }

  if (hit_node_idx != -1) {
    if (is_double_click) {
      editing_node_idx = hit_node_idx;
      selected_node_idx = hit_node_idx;

      float sx =
          transform_components[hit_node_idx].x * uniforms.zoom + uniforms.pan_x;
      float sy =
          transform_components[hit_node_idx].y * uniforms.zoom + uniforms.pan_y;

      js_set_editing_state(1, sx, sy, transform_components[hit_node_idx].w,
                           transform_components[hit_node_idx].h,
                           get_node_text_ptr(hit_node_idx), 100000,
                           hit_node_idx);

      is_dragging_node = 0;
      is_panning = 0;
      return;
    }

    if (shift || shift_pressed) {
      if (selected_node_idx != -1 && selected_node_idx != hit_node_idx) {
        add_connection_entity(selected_node_idx, hit_node_idx);
      }
      selected_node_idx = hit_node_idx;
    } else {
      if (ctrl) {
        interaction_components[hit_node_idx].selected =
            !interaction_components[hit_node_idx].selected;
        if (interaction_components[hit_node_idx].selected) {
          selected_node_idx = hit_node_idx;
        } else {
          if (selected_node_idx == hit_node_idx) {
            selected_node_idx = -1;
            for (unsigned int i = 0; i < entity_count; i++) {
              if (ecs_has_component(i, COMP_INTERACTION) &&
                  interaction_components[i].selected) {
                selected_node_idx = (int)i;
                break;
              }
            }
          }
        }
      } else {
        if (!interaction_components[hit_node_idx].selected) {
          for (unsigned int i = 0; i < entity_count; i++) {
            if (ecs_has_component(i, COMP_INTERACTION)) {
              interaction_components[i].selected = ((int)i == hit_node_idx);
            }
          }
          selected_node_idx = hit_node_idx;
        }
      }

      is_dragging_node = 1;
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          interaction_components[i].is_dragging = 1;
        }
      }
      drag_offset_x = world_x - transform_components[hit_node_idx].x;
      drag_offset_y = world_y - transform_components[hit_node_idx].y;
    }
  } else {
    // Hit Background
    if (is_double_click) {
      add_widget_wasm(WIDGET_STICKY, world_x, world_y, -1, 0, 0);
    } else {
      if (button == 2 || space_pressed || button == 1 || button == 4) {
        is_panning = 1;
      } else {
        selected_node_idx = -1;
        for (unsigned int i = 0; i < entity_count; i++) {
          if (ecs_has_component(i, COMP_INTERACTION)) {
            interaction_components[i].selected = 0;
          }
        }

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
    mark_dirty();
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

    marquee_system(draw_x, draw_y, draw_w, draw_h, &selected_node_idx);
  } else if (is_dragging_node) {
    mark_dirty();
    float world_dx = mouse_world_x - last_mouse_world_x;
    float world_dy = mouse_world_y - last_mouse_world_y;
    drag_system(world_dx, world_dy);
  } else if (is_resizing_node && selected_node_idx != -1) {
    mark_dirty();
    TransformComponent *sn = &transform_components[selected_node_idx];
    RenderComponent *rn = &render_components[selected_node_idx];

    float dx = mouse_world_x - resize_start_mouse_x;
    float dy = mouse_world_y - resize_start_mouse_y;

    float new_x = resize_initial_x;
    float new_y = resize_initial_y;
    float new_w = resize_initial_w;
    float new_h = resize_initial_h;

    // Calculate unconstrained dimensions
    if (active_resize_handle == RESIZE_R || active_resize_handle == RESIZE_TR ||
        active_resize_handle == RESIZE_BR) {
      new_w = resize_initial_w + dx;
    } else if (active_resize_handle == RESIZE_L ||
               active_resize_handle == RESIZE_TL ||
               active_resize_handle == RESIZE_BL) {
      new_w = resize_initial_w - dx;
    }

    if (active_resize_handle == RESIZE_B || active_resize_handle == RESIZE_BL ||
        active_resize_handle == RESIZE_BR) {
      new_h = resize_initial_h + dy;
    } else if (active_resize_handle == RESIZE_T ||
               active_resize_handle == RESIZE_TL ||
               active_resize_handle == RESIZE_TR) {
      new_h = resize_initial_h - dy;
    }

    if (rn->type == WIDGET_STICKY || rn->type == WIDGET_IMAGE) {
      if (resize_initial_w > 0.0f && resize_initial_h > 0.0f) {
        float scale = 1.0f;
        if (active_resize_handle == RESIZE_L ||
            active_resize_handle == RESIZE_R) {
          scale = new_w / resize_initial_w;
        } else if (active_resize_handle == RESIZE_T ||
                   active_resize_handle == RESIZE_B) {
          scale = new_h / resize_initial_h;
        } else { // TL, TR, BL, BR
          float scale_w = new_w / resize_initial_w;
          float scale_h = new_h / resize_initial_h;
          scale = (scale_w + scale_h) / 2.0f;
        }

        float min_scale_w = 2.0f / resize_initial_w;
        float min_scale_h = 2.0f / resize_initial_h;
        float min_scale =
            (min_scale_w > min_scale_h) ? min_scale_w : min_scale_h;
        if (scale < min_scale) {
          scale = min_scale;
        }

        new_w = resize_initial_w * scale;
        new_h = resize_initial_h * scale;
      }
    } else {
      if (new_w < 2.0f)
        new_w = 2.0f;
      if (new_h < 2.0f)
        new_h = 2.0f;
    }

    // Apply anchors for Left and Top handles
    if (active_resize_handle == RESIZE_L || active_resize_handle == RESIZE_TL ||
        active_resize_handle == RESIZE_BL) {
      new_x = resize_initial_x + resize_initial_w - new_w;
    }
    if (active_resize_handle == RESIZE_T || active_resize_handle == RESIZE_TL ||
        active_resize_handle == RESIZE_TR) {
      new_y = resize_initial_y + resize_initial_h - new_h;
    }

    sn->x = new_x;
    sn->y = new_y;
    sn->w = new_w;
    sn->h = new_h;

    if (rn->type != WIDGET_IMAGE && rn->type != WIDGET_PATH) {
      js_init_node_texture(selected_node_idx,
                           text_components[selected_node_idx].text, rn->type,
                           sn->w, sn->h);
    }
  }

  last_mouse_screen_x = x;
  last_mouse_screen_y = y;
  last_mouse_world_x = mouse_world_x;
  last_mouse_world_y = mouse_world_y;

  // If drafting a connection line, we need to redraw to follow the mouse
  if (selected_node_idx != -1 && (shift_pressed || arrow_tool_active) &&
      editing_node_idx == -1) {
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void on_mouse_up(int button, float x,
                                                        float y) {
  mark_dirty();
  is_panning = 0;
  is_selecting_marquee = 0;

  float world_x = (x - uniforms.pan_x) / uniforms.zoom;
  float world_y = (y - uniforms.pan_y) / uniforms.zoom;
  float dx = world_x - mouse_down_world_x;
  float dy = world_y - mouse_down_world_y;
  float dist = float_sqrt(dx * dx + dy * dy);

  if (is_dragging_node) {
    for (unsigned int i = 0; i < entity_count; i++) {
      if (ecs_has_component(i, COMP_INTERACTION)) {
        interaction_components[i].is_dragging = 0;
      }
    }

    if (dist < 3.0f && selected_node_idx != -1) {
      int hit_node_idx = -1;
      for (int i = (int)entity_count - 1; i >= 0; i--) {
        if (ecs_has_component(i, COMP_TRANSFORM) &&
            render_components[i].type != WIDGET_ARROW &&
            render_components[i].type != WIDGET_PATH) {
          TransformComponent *n = &transform_components[i];
          if (mouse_down_world_x >= n->x && mouse_down_world_x <= n->x + n->w &&
              mouse_down_world_y >= n->y && mouse_down_world_y <= n->y + n->h) {
            hit_node_idx = i;
            break;
          }
        }
      }
      if (hit_node_idx != -1) {
        for (unsigned int i = 0; i < entity_count; i++) {
          if (ecs_has_component(i, COMP_INTERACTION)) {
            interaction_components[i].selected = ((int)i == hit_node_idx);
          }
        }
        selected_node_idx = hit_node_idx;
      }
    }
  }

  is_dragging_node = 0;
  is_resizing_node = 0;
  active_resize_handle = RESIZE_NONE;
}

__attribute__((visibility("default"))) void on_mouse_wheel(float delta_y,
                                                           float x, float y) {
  mark_dirty();
  float factor = (delta_y < 0.0f) ? 1.08f : 0.9259f;

  float old_zoom = uniforms.zoom;
  uniforms.zoom *= factor;

  if (uniforms.zoom < 0.002f)
    uniforms.zoom = 0.002f;
  if (uniforms.zoom > 32.0f)
    uniforms.zoom = 32.0f;

  float wx = (x - uniforms.pan_x) / old_zoom;
  float wy = (y - uniforms.pan_y) / old_zoom;

  uniforms.pan_x = x - wx * uniforms.zoom;
  uniforms.pan_y = y - wy * uniforms.zoom;
}

__attribute__((visibility("default"))) void pan_canvas(float dx, float dy) {
  uniforms.pan_x += dx;
  uniforms.pan_y += dy;
  mark_dirty();
}

__attribute__((visibility("default"))) void on_key_down(int key) {
  mark_dirty();
  if (key == 16) { // Shift key
    shift_pressed = 1;
  } else if (key == 32) { // Space bar
    space_pressed = 1;
  } else if (key == 8 || key == 46) { // Backspace or Delete
    for (int i = (int)entity_count - 1; i >= 0; i--) {
      if (ecs_has_component(i, COMP_INTERACTION) &&
          interaction_components[i].selected) {
        ecs_delete_entity(i);
      }
    }
    selected_node_idx = -1;
  }
}

__attribute__((visibility("default"))) void on_key_up(int key) {
  mark_dirty();
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
  return (int)entity_count;
}

__attribute__((visibility("default"))) int get_node_type(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return -1;
  return render_components[idx].type;
}

__attribute__((visibility("default"))) float get_node_width(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return transform_components[idx].w;
}

__attribute__((visibility("default"))) float get_node_height(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return transform_components[idx].h;
}

__attribute__((visibility("default"))) float get_node_x(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return transform_components[idx].x;
}

__attribute__((visibility("default"))) float get_node_y(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return transform_components[idx].y;
}

__attribute__((visibility("default"))) int get_node_texture_id(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return -1;
  return render_components[idx].texture_id;
}

static char edit_buffer[100005];

__attribute__((visibility("default"))) const char* get_node_text_ptr(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return NULL;
  const char *src = text_components[idx].text;
  if (!src) src = "";
  unsigned int i = 0;
  while (src[i] != '\0' && i < 100000) {
    edit_buffer[i] = src[i];
    i++;
  }
  edit_buffer[i] = '\0';
  return edit_buffer;
}

__attribute__((visibility("default"))) void set_node_text(int idx, const char *text) {
  if (idx >= 0 && idx < (int)entity_count && ecs_has_component(idx, COMP_TEXT)) {
    if (!text) text = "";
    unsigned int len = strlen(text);
    text_components[idx].text = allocate_text(text, len);
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_node_texture_id(int idx,
                                                                int tex_id) {
  if (idx >= 0 && idx < (int)entity_count) {
    render_components[idx].texture_id = tex_id;
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void mark_dirty_wasm() {
  mark_dirty();
}

__attribute__((visibility("default"))) float get_node_bg_r(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].bg_r;
}
__attribute__((visibility("default"))) float get_node_bg_g(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].bg_g;
}
__attribute__((visibility("default"))) float get_node_bg_b(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].bg_b;
}
__attribute__((visibility("default"))) float get_node_bg_a(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].bg_a;
}

__attribute__((visibility("default"))) float get_node_border_r(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].border_r;
}
__attribute__((visibility("default"))) float get_node_border_g(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].border_g;
}
__attribute__((visibility("default"))) float get_node_border_b(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].border_b;
}
__attribute__((visibility("default"))) float get_node_border_a(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  return render_components[idx].border_a;
}

__attribute__((visibility("default"))) float get_node_text_r(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  if (render_components[idx].type == WIDGET_TEXT) {
    return render_components[idx].bg_r;
  }
  return render_components[idx].r;
}
__attribute__((visibility("default"))) float get_node_text_g(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  if (render_components[idx].type == WIDGET_TEXT) {
    return render_components[idx].bg_g;
  }
  return render_components[idx].g;
}
__attribute__((visibility("default"))) float get_node_text_b(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 0.0f;
  if (render_components[idx].type == WIDGET_TEXT) {
    return render_components[idx].bg_b;
  }
  return render_components[idx].b;
}

__attribute__((visibility("default"))) void
set_node_text_color(int idx, float r, float g, float b) {
  if (idx >= 0 && idx < (int)entity_count) {
    if (interaction_components[idx].selected) {
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          float tr = r;
          float tg = g;
          float tb = b;
          if (render_components[i].bg_a > 0.001f &&
              render_components[i].bg_r < 0.01f &&
              render_components[i].bg_g < 0.01f &&
              render_components[i].bg_b < 0.01f) {
            tr = 1.0f;
            tg = 1.0f;
            tb = 1.0f;
          }
          render_components[i].r = tr;
          render_components[i].g = tg;
          render_components[i].b = tb;
          if (render_components[i].type == WIDGET_TEXT) {
            render_components[i].bg_r = tr;
            render_components[i].bg_g = tg;
            render_components[i].bg_b = tb;
          }
          if (render_components[i].type != WIDGET_IMAGE &&
              render_components[i].type != WIDGET_PATH) {
            js_init_node_texture(
                i, text_components[i].text, render_components[i].type,
                transform_components[i].w, transform_components[i].h);
          }
        }
      }
    } else {
      float tr = r;
      float tg = g;
      float tb = b;
      if (render_components[idx].bg_a > 0.001f &&
          render_components[idx].bg_r < 0.01f &&
          render_components[idx].bg_g < 0.01f &&
          render_components[idx].bg_b < 0.01f) {
        tr = 1.0f;
        tg = 1.0f;
        tb = 1.0f;
      }
      render_components[idx].r = tr;
      render_components[idx].g = tg;
      render_components[idx].b = tb;
      if (render_components[idx].type == WIDGET_TEXT) {
        render_components[idx].bg_r = tr;
        render_components[idx].bg_g = tg;
        render_components[idx].bg_b = tb;
      }
      if (render_components[idx].type != WIDGET_IMAGE &&
          render_components[idx].type != WIDGET_PATH) {
        js_init_node_texture(
            idx, text_components[idx].text, render_components[idx].type,
            transform_components[idx].w, transform_components[idx].h);
      }
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void
set_node_bg_color(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < (int)entity_count) {
    if (interaction_components[idx].selected) {
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          render_components[i].bg_r = r;
          render_components[i].bg_g = g;
          render_components[i].bg_b = b;
          render_components[i].bg_a = a;
          if (a > 0.001f && r < 0.01f && g < 0.01f && b < 0.01f) {
            render_components[i].r = 1.0f;
            render_components[i].g = 1.0f;
            render_components[i].b = 1.0f;
          }
          if (render_components[i].type != WIDGET_IMAGE &&
              render_components[i].type != WIDGET_PATH) {
            js_init_node_texture(
                i, text_components[i].text, render_components[i].type,
                transform_components[i].w, transform_components[i].h);
          }
        }
      }
    } else {
      render_components[idx].bg_r = r;
      render_components[idx].bg_g = g;
      render_components[idx].bg_b = b;
      render_components[idx].bg_a = a;
      if (a > 0.001f && r < 0.01f && g < 0.01f && b < 0.01f) {
        render_components[idx].r = 1.0f;
        render_components[idx].g = 1.0f;
        render_components[idx].b = 1.0f;
      }
      if (render_components[idx].type != WIDGET_IMAGE &&
          render_components[idx].type != WIDGET_PATH) {
        js_init_node_texture(
            idx, text_components[idx].text, render_components[idx].type,
            transform_components[idx].w, transform_components[idx].h);
      }
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void
set_node_border_color(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < (int)entity_count) {
    if (interaction_components[idx].selected) {
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          render_components[i].border_r = r;
          render_components[i].border_g = g;
          render_components[i].border_b = b;
          render_components[i].border_a = a;
        }
      }
    } else {
      render_components[idx].border_r = r;
      render_components[idx].border_g = g;
      render_components[idx].border_b = b;
      render_components[idx].border_a = a;
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) float get_node_font_size(int idx) {
  if (idx < 0 || idx >= (int)entity_count)
    return 18.0f;
  return render_components[idx].font_size;
}

__attribute__((visibility("default"))) void set_node_font_size(int idx,
                                                               float size) {
  if (idx >= 0 && idx < (int)entity_count) {
    if (size < 6.0f)
      size = 6.0f;
    if (size > 120.0f)
      size = 120.0f;
    if (interaction_components[idx].selected) {
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          render_components[i].font_size = size;
          if (render_components[i].type != WIDGET_IMAGE &&
              render_components[i].type != WIDGET_PATH) {
            js_init_node_texture(
                i, text_components[i].text, render_components[i].type,
                transform_components[i].w, transform_components[i].h);
          }
        }
      }
    } else {
      render_components[idx].font_size = size;
      if (render_components[idx].type != WIDGET_IMAGE &&
          render_components[idx].type != WIDGET_PATH) {
        js_init_node_texture(
            idx, text_components[idx].text, render_components[idx].type,
            transform_components[idx].w, transform_components[idx].h);
      }
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_char_advance(int c,
                                                             float advance) {
  // No-op (statically stored inside font_glyphs mapping)
}

__attribute__((visibility("default"))) void set_node_size(int idx, float w,
                                                          float h) {
  if (idx >= 0 && idx < (int)entity_count) {
    int width_changed = (w != transform_components[idx].w);
    int height_changed = (h != transform_components[idx].h);

    if (interaction_components[idx].selected) {
      for (unsigned int i = 0; i < entity_count; i++) {
        if (ecs_has_component(i, COMP_INTERACTION) &&
            interaction_components[i].selected) {
          float final_w = w;
          float final_h = h;
          WidgetType t = render_components[i].type;
          if (t == WIDGET_STICKY) {
            if (height_changed && !width_changed) {
              final_w = h;
              final_h = h;
            } else {
              final_w = w;
              final_h = w;
            }
          } else if (t == WIDGET_IMAGE) {
            if (transform_components[i].h > 0.0f) {
              float aspect =
                  transform_components[i].w / transform_components[i].h;
              if (height_changed && !width_changed) {
                final_h = h;
                final_w = h * aspect;
              } else {
                final_w = w;
                final_h = w / aspect;
              }
            }
          }
          clamp_node_size_with_aspect_idx(i, &final_w, &final_h);
          transform_components[i].w = final_w;
          transform_components[i].h = final_h;
          if (t != WIDGET_IMAGE && t != WIDGET_PATH) {
            js_init_node_texture(i, text_components[i].text, t, final_w,
                                 final_h);
          }
        }
      }
    } else {
      float final_w = w;
      float final_h = h;
      WidgetType t = render_components[idx].type;
      if (t == WIDGET_STICKY) {
        if (height_changed && !width_changed) {
          final_w = h;
          final_h = h;
        } else {
          final_w = w;
          final_h = w;
        }
      } else if (t == WIDGET_IMAGE) {
        if (transform_components[idx].h > 0.0f) {
          float aspect =
              transform_components[idx].w / transform_components[idx].h;
          if (height_changed && !width_changed) {
            final_h = h;
            final_w = h * aspect;
          } else {
            final_w = w;
            final_h = w / aspect;
          }
        }
      }
      clamp_node_size_with_aspect_idx(idx, &final_w, &final_h);
      transform_components[idx].w = final_w;
      transform_components[idx].h = final_h;
      if (t != WIDGET_IMAGE && t != WIDGET_PATH) {
        js_init_node_texture(idx, text_components[idx].text, t, final_w,
                             final_h);
      }
    }
  }
  mark_dirty();
}

__attribute__((visibility("default"))) void shift_node(int from, int to) {
  if (from == to || from < 0 || from >= (int)entity_count || to < 0 ||
      to >= (int)entity_count)
    return;

  js_on_entity_shifted(from, to);

  ComponentMask temp_mask = entity_masks[from];
  TransformComponent temp_transform = transform_components[from];
  RenderComponent temp_render = render_components[from];
  TextComponent temp_text = text_components[from];
  PathDrawingComponent temp_path = path_components[from];
  ConnectionComponent temp_connection = connection_components[from];
  InteractionComponent temp_interaction = interaction_components[from];

  if (from < to) {
    for (int i = from; i < to; i++) {
      entity_masks[i] = entity_masks[i + 1];
      transform_components[i] = transform_components[i + 1];
      render_components[i] = render_components[i + 1];
      text_components[i] = text_components[i + 1];
      path_components[i] = path_components[i + 1];
      connection_components[i] = connection_components[i + 1];
      interaction_components[i] = interaction_components[i + 1];
    }
  } else {
    for (int i = from; i > to; i--) {
      entity_masks[i] = entity_masks[i - 1];
      transform_components[i] = transform_components[i - 1];
      render_components[i] = render_components[i - 1];
      text_components[i] = text_components[i - 1];
      path_components[i] = path_components[i - 1];
      connection_components[i] = connection_components[i - 1];
      interaction_components[i] = interaction_components[i - 1];
    }
  }
  entity_masks[to] = temp_mask;
  transform_components[to] = temp_transform;
  render_components[to] = temp_render;
  text_components[to] = temp_text;
  path_components[to] = temp_path;
  connection_components[to] = temp_connection;
  interaction_components[to] = temp_interaction;

  // Fix connections
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_CONNECTION)) {
      ConnectionComponent *n = &connection_components[i];
      if (n->from_entity == from) {
        n->from_entity = to;
      } else if (from < to) {
        if (n->from_entity > from && n->from_entity <= to) {
          n->from_entity--;
        }
      } else {
        if (n->from_entity < from && n->from_entity >= to) {
          n->from_entity++;
        }
      }

      if (n->to_entity == from) {
        n->to_entity = to;
      } else if (from < to) {
        if (n->to_entity > from && n->to_entity <= to) {
          n->to_entity--;
        }
      } else {
        if (n->to_entity < from && n->to_entity >= to) {
          n->to_entity++;
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
  mark_dirty();
}

__attribute__((visibility("default"))) void bring_to_front_wasm(int idx) {
  shift_node(idx, (int)entity_count - 1);
}

__attribute__((visibility("default"))) void send_to_back_wasm(int idx) {
  shift_node(idx, 0);
}

__attribute__((visibility("default"))) void move_forward_wasm(int idx) {
  if (idx < (int)entity_count - 1) {
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
  mark_dirty();
}

__attribute__((visibility("default"))) void on_text_commit() {
  if (editing_node_idx != -1) {
    unsigned int len = strlen(edit_buffer);
    text_components[editing_node_idx].text = allocate_text(edit_buffer, len);
    js_init_node_texture(editing_node_idx,
                         text_components[editing_node_idx].text,
                         render_components[editing_node_idx].type,
                         transform_components[editing_node_idx].w,
                         transform_components[editing_node_idx].h);
  }
  editing_node_idx = -1;
  mark_dirty();
}

__attribute__((visibility("default"))) void on_text_cancel() {
  editing_node_idx = -1;
  mark_dirty();
}

__attribute__((visibility("default"))) void
add_widget_wasm(int type, float x, float y, int texture_id, int img_w,
                int img_h) {
  if (entity_count >= MAX_ENTITIES)
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
  } else if (type == WIDGET_CODE) {
    default_text = "int main() {\n  return 0;\n}";
    w = 250.0f;
    h = 180.0f;
  } else if (type == WIDGET_IMAGE) {
    float aspect = (float)img_w / (float)img_h;
    if (img_w > img_h) {
      w = 200.0f;
      h = 200.0f / aspect;
    } else {
      h = 200.0f;
      w = 200.0f * aspect;
    }
  }

  Entity e = add_entity_full(type, x - w / 2.0f, y - h / 2.0f, w, h,
                             default_text, texture_id);

  selected_node_idx = (int)e;
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_INTERACTION)) {
      interaction_components[i].selected = (i == e);
    }
  }
  mark_dirty();
}

__attribute__((visibility("default"))) void on_btn_add_click() {
  float cx = (uniforms.screen_width / 2.0f - uniforms.pan_x) / uniforms.zoom;
  float cy = (uniforms.screen_height / 2.0f - uniforms.pan_y) / uniforms.zoom;
  add_widget_wasm(WIDGET_STICKY, cx, cy, -1, 0, 0);
}

__attribute__((visibility("default"))) void on_btn_clear_click() {
  ecs_init();
  selected_node_idx = -1;
  editing_node_idx = -1;
  mark_dirty();
}

// active_stroke_node_idx is defined in renderer.c

__attribute__((visibility("default"))) void
start_stroke(float world_x, float world_y, float r, float g, float b) {
  if (entity_count >= MAX_ENTITIES)
    return;

  Entity e = ecs_create_entity();
  if (e == (Entity)-1)
    return;

  ecs_add_component(e, COMP_RENDER);
  RenderComponent *rc = &render_components[e];
  rc->type = WIDGET_PATH;
  rc->r = r;
  rc->g = g;
  rc->b = b;
  rc->bg_r = r;
  rc->bg_g = g;
  rc->bg_b = b;
  rc->bg_a = 1.0f;
  rc->texture_id = -1;

  ecs_add_component(e, COMP_PATH);
  path_components[e].path_start_idx = path_point_count;
  path_components[e].path_point_len = 0;

  ecs_add_component(e, COMP_TRANSFORM);
  transform_components[e].x = 0.0f;
  transform_components[e].y = 0.0f;
  transform_components[e].w = 0.0f;
  transform_components[e].h = 0.0f;

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  active_stroke_node_idx = (int)e;

  if (path_point_count < MAX_PATH_POINTS) {
    path_points[path_point_count] = (PathPoint){world_x, world_y};
    path_point_count++;
    path_components[e].path_point_len++;
  }
  mark_dirty();
}

__attribute__((visibility("default"))) void add_stroke_point(float world_x,
                                                             float world_y) {
  if (active_stroke_node_idx == -1)
    return;
  Entity e = (Entity)active_stroke_node_idx;
  PathDrawingComponent *p = &path_components[e];

  if (path_point_count < MAX_PATH_POINTS) {
    if (p->path_point_len > 0) {
      PathPoint last = path_points[path_point_count - 1];
      float dx = world_x - last.x;
      float dy = world_y - last.y;
      if (dx * dx + dy * dy < 1.0f) {
        return;
      }
    }

    path_points[path_point_count] = (PathPoint){world_x, world_y};
    path_point_count++;
    p->path_point_len++;
  }
  mark_dirty();
}

__attribute__((visibility("default"))) void end_stroke() {
  if (active_stroke_node_idx == -1)
    return;
  Entity e = (Entity)active_stroke_node_idx;
  PathDrawingComponent *p = &path_components[e];

  if (p->path_point_len <= 1) {
    path_point_count -= p->path_point_len;
    ecs_delete_entity(e);
  } else {
    float min_x = path_points[p->path_start_idx].x;
    float max_x = min_x;
    float min_y = path_points[p->path_start_idx].y;
    float max_y = min_y;

    for (int i = 1; i < p->path_point_len; i++) {
      float px = path_points[p->path_start_idx + i].x;
      float py = path_points[p->path_start_idx + i].y;
      if (px < min_x)
        min_x = px;
      if (px > max_x)
        max_x = px;
      if (py < min_y)
        min_y = py;
      if (py > max_y)
        max_y = py;
    }

    ecs_add_component(e, COMP_TRANSFORM);
    transform_components[e].x = min_x;
    transform_components[e].y = min_y;
    transform_components[e].w = max_x - min_x;
    transform_components[e].h = max_y - min_y;

    for (int i = 0; i < p->path_point_len; i++) {
      path_points[p->path_start_idx + i].x -= min_x;
      path_points[p->path_start_idx + i].y -= min_y;
    }
  }

  active_stroke_node_idx = -1;
  mark_dirty();
}

// Rendering frame states and batches are defined in renderer.c

__attribute__((visibility("default"))) void tick_app(float timestamp) {
  current_time_ms = timestamp;

  if (!needs_redraw) {
    js_wgpu_begin_render_pass();
    upload_uniforms_if_changed();

    for (int b = 0; b < batch_count; b++) {
      DrawBatch *batch = &batches[b];
      js_wgpu_set_pipeline(batch->pipeline_id);
      js_wgpu_set_bind_group(0, uniform_buffer_id, batch->texture_id);
      js_wgpu_draw_indexed(batch->index_count, batch->index_start,
                           index_buffer_id, vertex_buffer_id);
    }

    js_wgpu_end_render_pass();

    int shapes_count = 0;
    for (unsigned int i = 0; i < entity_count; i++) {
      if (ecs_has_component(i, COMP_RENDER)) {
        WidgetType t = render_components[i].type;
        if (t == WIDGET_STICKY || t == WIDGET_RECT || t == WIDGET_OVAL ||
            t == WIDGET_TRIANGLE) {
          shapes_count++;
        }
      }
    }
    js_update_stats(uniforms.pan_x, uniforms.pan_y, uniforms.zoom,
                    shapes_count);
    return;
  }

  // Clear dynamic draw buffers
  vertex_count = 0;
  index_count = 0;
  batch_count = 0;
  last_batch_index_start = 0;

  // 1. Calculate screen coordinates in world space
  float left = -uniforms.pan_x / uniforms.zoom;
  float right = (uniforms.screen_width - uniforms.pan_x) / uniforms.zoom;
  float top = -uniforms.pan_y / uniforms.zoom;
  float bottom = (uniforms.screen_height - uniforms.pan_y) / uniforms.zoom;

  // 3. Compile Connection Draft (if Shift is held down or arrow tool active)
  if (selected_node_idx != -1 && (shift_pressed || arrow_tool_active) &&
      editing_node_idx == -1) {
    TransformComponent *s = &transform_components[selected_node_idx];
    float x1 = s->x + s->w / 2.0f;
    float y1 = s->y + s->h / 2.0f;
    float x2 = mouse_world_x;
    float y2 = mouse_world_y;

    draw_line(x1, y1, x2, y2, 2.5f, 0.1f, 0.8f, 0.9f, 0.7f);

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

  // 5. Compile and draw entities/widgets
  render_system(left, right, top, bottom, editing_node_idx, texture_id);

  // 6. Draw marquee selection box
  if (is_selecting_marquee) {
    float w = mouse_world_x - marquee_start_x;
    float h = mouse_world_y - marquee_start_y;
    float draw_x = (w < 0.0f) ? marquee_start_x + w : marquee_start_x;
    float draw_y = (h < 0.0f) ? marquee_start_y + h : marquee_start_y;
    float draw_w = (w < 0.0f) ? -w : w;
    float draw_h = (h < 0.0f) ? -h : h;

    draw_rect(draw_x, draw_y, draw_w, draw_h, 94.0f / 255.0f, 106.0f / 255.0f,
              210.0f / 255.0f, 0.15f);
    draw_line(draw_x, draw_y, draw_x + draw_w, draw_y, 1.0f, 94.0f / 255.0f,
              106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x + draw_w, draw_y, draw_x + draw_w, draw_y + draw_h, 1.0f,
              94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x + draw_w, draw_y + draw_h, draw_x, draw_y + draw_h, 1.0f,
              94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
    draw_line(draw_x, draw_y + draw_h, draw_x, draw_y, 1.0f, 94.0f / 255.0f,
              106.0f / 255.0f, 210.0f / 255.0f, 0.7f);
  }

  flush_batch(0, texture_id);

  if (index_count > 0) {
    js_wgpu_write_buffer(vertex_buffer_id, 0, vertex_buffer,
                         vertex_count * sizeof(Vertex));
    js_wgpu_write_buffer(index_buffer_id, 0, index_buffer,
                         index_count * sizeof(uint32_t));
  }

  js_wgpu_begin_render_pass();
  upload_uniforms_if_changed();

  for (int b = 0; b < batch_count; b++) {
    DrawBatch *batch = &batches[b];
    js_wgpu_set_pipeline(batch->pipeline_id);
    js_wgpu_set_bind_group(0, uniform_buffer_id, batch->texture_id);
    js_wgpu_draw_indexed(batch->index_count, batch->index_start,
                         index_buffer_id, vertex_buffer_id);
  }

  js_wgpu_end_render_pass();

  // Count active shape entities for stats
  int shapes_count = 0;
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_RENDER)) {
      WidgetType t = render_components[i].type;
      if (t == WIDGET_STICKY || t == WIDGET_RECT || t == WIDGET_OVAL ||
          t == WIDGET_TRIANGLE) {
        shapes_count++;
      }
    }
  }

  js_update_stats(uniforms.pan_x, uniforms.pan_y, uniforms.zoom, shapes_count);
  needs_redraw = 0;
}

// flush_batch is defined in renderer.c

__attribute__((visibility("default"))) void create_100k_infographics() {
  generate_100k_infographics();
  mark_dirty();
}

static int type_counts[8];

__attribute__((visibility("default"))) int *get_widget_type_counts() {
  for (int i = 0; i < 8; i++) {
    type_counts[i] = 0;
  }
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_RENDER)) {
      WidgetType t = render_components[i].type;
      if (t >= 0 && t < 8) {
        type_counts[t]++;
      }
    }
  }
  return type_counts;
}

__attribute__((visibility("default"))) int get_debug_batch_count() {
  return batch_count;
}

__attribute__((visibility("default"))) int get_debug_vertex_count() {
  return vertex_count;
}

__attribute__((visibility("default"))) int get_debug_index_count() {
  return index_count;
}

__attribute__((visibility("default"))) int get_entity_mask(int idx) {
  if (idx < 0 || idx >= (int)entity_count) return 0;
  return (int)entity_masks[idx];
}

__attribute__((visibility("default"))) int get_connection_from(int idx) {
  if (idx < 0 || idx >= (int)entity_count) return -1;
  return connection_components[idx].from_entity;
}

__attribute__((visibility("default"))) int get_connection_to(int idx) {
  if (idx < 0 || idx >= (int)entity_count) return -1;
  return connection_components[idx].to_entity;
}

__attribute__((visibility("default"))) int get_path_start_idx(int idx) {
  if (idx < 0 || idx >= (int)entity_count) return -1;
  return path_components[idx].path_start_idx;
}

__attribute__((visibility("default"))) int get_path_point_len(int idx) {
  if (idx < 0 || idx >= (int)entity_count) return -1;
  return path_components[idx].path_point_len;
}

__attribute__((visibility("default"))) float get_path_point_x(int pt_idx) {
  if (pt_idx < 0 || pt_idx >= path_point_count) return 0.0f;
  return path_points[pt_idx].x;
}

__attribute__((visibility("default"))) float get_path_point_y(int pt_idx) {
  if (pt_idx < 0 || pt_idx >= path_point_count) return 0.0f;
  return path_points[pt_idx].y;
}

__attribute__((visibility("default"))) float get_camera_pan_x() {
  return uniforms.pan_x;
}

__attribute__((visibility("default"))) float get_camera_pan_y() {
  return uniforms.pan_y;
}

__attribute__((visibility("default"))) float get_camera_zoom() {
  return uniforms.zoom;
}

__attribute__((visibility("default"))) void set_camera_pan_zoom(float pan_x, float pan_y, float zoom) {
  uniforms.pan_x = pan_x;
  uniforms.pan_y = pan_y;
  uniforms.zoom = zoom;
  mark_dirty();
}

static char *canvas_strncpy(char *dest, const char *src, int n) {
  int i = 0;
  while (i < n - 1 && src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
  return dest;
}

static char temp_string_buffer[100005];

__attribute__((visibility("default"))) char* get_temp_string_buffer() {
  return temp_string_buffer;
}

__attribute__((visibility("default"))) int recreate_node(int type, float x, float y, float w, float h,
                                                         float bg_r, float bg_g, float bg_b, float bg_a,
                                                         float border_r, float border_g, float border_b, float border_a,
                                                         float r, float g, float b, float font_size, int texture_id,
                                                         const char *text) {
  Entity e = ecs_create_entity();
  if (e == (Entity)-1) return -1;

  ecs_add_component(e, COMP_TRANSFORM);
  transform_components[e].x = x;
  transform_components[e].y = y;
  transform_components[e].w = w;
  transform_components[e].h = h;

  ecs_add_component(e, COMP_RENDER);
  render_components[e].type = (WidgetType)type;
  render_components[e].bg_r = bg_r;
  render_components[e].bg_g = bg_g;
  render_components[e].bg_b = bg_b;
  render_components[e].bg_a = bg_a;
  render_components[e].border_r = border_r;
  render_components[e].border_g = border_g;
  render_components[e].border_b = border_b;
  render_components[e].border_a = border_a;
  render_components[e].r = r;
  render_components[e].g = g;
  render_components[e].b = b;
  render_components[e].font_size = font_size;
  render_components[e].texture_id = texture_id;

  ecs_add_component(e, COMP_TEXT);
  if (!text) text = "";
  unsigned int text_len = strlen(text);
  text_components[e].text = allocate_text(text, text_len);

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  mark_dirty();
  return (int)e;
}

__attribute__((visibility("default"))) int recreate_connection(int from_entity, int to_entity,
                                                               float r, float g, float b) {
  Entity e = ecs_create_entity();
  if (e == (Entity)-1) return -1;

  ecs_add_component(e, COMP_RENDER);
  render_components[e].type = WIDGET_ARROW;
  render_components[e].texture_id = -1;
  render_components[e].r = r;
  render_components[e].g = g;
  render_components[e].b = b;
  render_components[e].bg_r = r;
  render_components[e].bg_g = g;
  render_components[e].bg_b = b;
  render_components[e].bg_a = 1.0f;
  render_components[e].border_r = r;
  render_components[e].border_g = g;
  render_components[e].border_b = b;
  render_components[e].border_a = 1.0f;

  ecs_add_component(e, COMP_CONNECTION);
  connection_components[e].from_entity = from_entity;
  connection_components[e].to_entity = to_entity;

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  mark_dirty();
  return (int)e;
}

__attribute__((visibility("default"))) int recreate_path(float x, float y, float w, float h,
                                                         float r, float g, float b) {
  Entity e = ecs_create_entity();
  if (e == (Entity)-1) return -1;

  ecs_add_component(e, COMP_RENDER);
  render_components[e].type = WIDGET_PATH;
  render_components[e].r = r;
  render_components[e].g = g;
  render_components[e].b = b;
  render_components[e].bg_r = r;
  render_components[e].bg_g = g;
  render_components[e].bg_b = b;
  render_components[e].bg_a = 1.0f;
  render_components[e].texture_id = -1;

  ecs_add_component(e, COMP_PATH);
  path_components[e].path_start_idx = path_point_count;
  path_components[e].path_point_len = 0;

  ecs_add_component(e, COMP_TRANSFORM);
  transform_components[e].x = x;
  transform_components[e].y = y;
  transform_components[e].w = w;
  transform_components[e].h = h;

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  mark_dirty();
  return (int)e;
}

__attribute__((visibility("default"))) void recreate_path_point(int path_entity, float px, float py) {
  if (path_point_count < MAX_PATH_POINTS) {
    path_points[path_point_count] = (PathPoint){px, py};
    path_point_count++;
    path_components[path_entity].path_point_len++;
  }
  mark_dirty();
}

__attribute__((visibility("default"))) void set_node_position(int idx, float x, float y) {
  if (idx >= 0 && idx < (int)entity_count) {
    transform_components[idx].x = x;
    transform_components[idx].y = y;
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_node_bg_color_direct(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < (int)entity_count) {
    render_components[idx].bg_r = r;
    render_components[idx].bg_g = g;
    render_components[idx].bg_b = b;
    render_components[idx].bg_a = a;
    if (render_components[idx].type != WIDGET_IMAGE && render_components[idx].type != WIDGET_PATH) {
      js_init_node_texture(idx, text_components[idx].text, render_components[idx].type,
                           transform_components[idx].w, transform_components[idx].h);
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_node_border_color_direct(int idx, float r, float g, float b, float a) {
  if (idx >= 0 && idx < (int)entity_count) {
    render_components[idx].border_r = r;
    render_components[idx].border_g = g;
    render_components[idx].border_b = b;
    render_components[idx].border_a = a;
    if (render_components[idx].type != WIDGET_IMAGE && render_components[idx].type != WIDGET_PATH) {
      js_init_node_texture(idx, text_components[idx].text, render_components[idx].type,
                           transform_components[idx].w, transform_components[idx].h);
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_node_text_color_direct(int idx, float r, float g, float b) {
  if (idx >= 0 && idx < (int)entity_count) {
    render_components[idx].r = r;
    render_components[idx].g = g;
    render_components[idx].b = b;
    if (render_components[idx].type != WIDGET_IMAGE && render_components[idx].type != WIDGET_PATH) {
      js_init_node_texture(idx, text_components[idx].text, render_components[idx].type,
                           transform_components[idx].w, transform_components[idx].h);
    }
    mark_dirty();
  }
}

__attribute__((visibility("default"))) void set_node_size_direct(int idx, float w, float h) {
  if (idx >= 0 && idx < (int)entity_count) {
    transform_components[idx].w = w;
    transform_components[idx].h = h;
    if (render_components[idx].type != WIDGET_IMAGE && render_components[idx].type != WIDGET_PATH) {
      js_init_node_texture(idx, text_components[idx].text, render_components[idx].type,
                           transform_components[idx].w, transform_components[idx].h);
    }
    mark_dirty();
  }
}
