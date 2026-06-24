#include "initial_content.h"

typedef struct {
  float r, g, b;
} PastelColor;

static PastelColor colors[6] = {
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

static char *local_strcpy(char *dest, const char *src) {
  int i = 0;
  while (src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
  return dest;
}

Entity add_entity_full(WidgetType type, float world_x, float world_y, float w,
                       float h, const char *text, int texture_id) {
  Entity e = ecs_create_entity();
  if (e == (Entity)-1)
    return e;

  ecs_add_component(e, COMP_TRANSFORM);
  transform_components[e].x = world_x;
  transform_components[e].y = world_y;
  transform_components[e].w = w;
  transform_components[e].h = h;

  ecs_add_component(e, COMP_RENDER);
  RenderComponent *r = &render_components[e];
  r->type = type;
  r->texture_id = texture_id;

  int color_idx = (int)(local_random_float() * 6.0f);
  if (color_idx < 0)
    color_idx = 0;
  if (color_idx > 5)
    color_idx = 5;
  r->bg_r = colors[color_idx].r;
  r->bg_g = colors[color_idx].g;
  r->bg_b = colors[color_idx].b;

  if (type == WIDGET_TEXT) {
    r->bg_a = 0.0f;
    r->r = r->bg_r;
    r->g = r->bg_g;
    r->b = r->bg_b;
  } else {
    r->bg_a = 0.9f;
    if (r->bg_r < 0.5f) {
      r->r = 25.0f / 255.0f;
    } else {
      r->r = 1.0f;
    }
    if (r->bg_g < 0.5f) {
      r->g = 25.0f / 255.0f;
    } else {
      r->g = 1.0f;
    }
    if (r->bg_b < 0.5f) {
      r->b = 25.0f / 255.0f;
    } else {
      r->b = 1.0f;
    }
  }

  r->border_r = r->bg_r * 0.7f;
  r->border_g = r->bg_g * 0.7f;
  r->border_b = r->bg_b * 0.7f;
  if (type == WIDGET_RECT || type == WIDGET_OVAL || type == WIDGET_TRIANGLE) {
    r->border_a = 1.0f;
  } else {
    r->border_a = 0.0f;
  }

  r->font_size = 18.0f;

  ecs_add_component(e, COMP_TEXT);
  local_strcpy(text_components[e].text, text);

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  return e;
}

Entity add_connection_entity(int from, int to) {
  if (from == to)
    return (Entity)-1;
  if (from < 0 || from >= (int)entity_count || to < 0 ||
      to >= (int)entity_count)
    return (Entity)-1;

  // Check duplicate connection
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_CONNECTION)) {
      ConnectionComponent *conn = &connection_components[i];
      if ((conn->from_entity == from && conn->to_entity == to) ||
          (conn->from_entity == to && conn->to_entity == from)) {
        return (Entity)-1;
      }
    }
  }

  Entity e = ecs_create_entity();
  if (e == (Entity)-1)
    return e;

  ecs_add_component(e, COMP_RENDER);
  RenderComponent *r = &render_components[e];
  r->type = WIDGET_ARROW;
  r->texture_id = -1;
  r->r = 94.0f / 255.0f;
  r->g = 106.0f / 255.0f;
  r->b = 210.0f / 255.0f;
  r->bg_r = r->r;
  r->bg_g = r->g;
  r->bg_b = r->b;
  r->bg_a = 1.0f;
  r->border_r = r->r;
  r->border_g = r->g;
  r->border_b = r->b;
  r->border_a = 1.0f;

  ecs_add_component(e, COMP_CONNECTION);
  connection_components[e].from_entity = from;
  connection_components[e].to_entity = to;

  ecs_add_component(e, COMP_INTERACTION);
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;

  return e;
}

void generate_initial_content() {
  // 1. Left Panel Background (Dark Slate)
  Entity left_bg =
      add_entity_full(WIDGET_RECT, 100.0f, 100.0f, 300.0f, 550.0f, "", -1);
  render_components[left_bg].bg_r = 15.0f / 255.0f;
  render_components[left_bg].bg_g = 18.0f / 255.0f;
  render_components[left_bg].bg_b = 25.0f / 255.0f;
  render_components[left_bg].bg_a = 1.0f;
  render_components[left_bg].border_a = 0.0f;

  // 2. Right Panel Background (Sleek Dark Slate)
  Entity right_bg =
      add_entity_full(WIDGET_RECT, 400.0f, 100.0f, 700.0f, 550.0f, "", -1);
  render_components[right_bg].bg_r = 20.0f / 255.0f;
  render_components[right_bg].bg_g = 24.0f / 255.0f;
  render_components[right_bg].bg_b = 33.0f / 255.0f;
  render_components[right_bg].bg_a = 1.0f;
  render_components[right_bg].border_a = 0.0f;

  // 3. Title Text "DUST" in White
  Entity title_idx =
      add_entity_full(WIDGET_TEXT, 130.0f, 130.0f, 240.0f, 60.0f, "DUST", -1);
  render_components[title_idx].bg_r = 226.0f / 255.0f;
  render_components[title_idx].bg_g = 232.0f / 255.0f;
  render_components[title_idx].bg_b = 240.0f / 255.0f;
  render_components[title_idx].font_size = 48.0f;

  // 4. Subtitle Text "WEBGPU WHITEBOARD" in Accent Indigo
  Entity sub_idx = add_entity_full(WIDGET_TEXT, 130.0f, 190.0f, 240.0f, 30.0f,
                                   "WEBGPU WHITEBOARD", -1);
  render_components[sub_idx].bg_r = 94.0f / 255.0f;
  render_components[sub_idx].bg_g = 106.0f / 255.0f;
  render_components[sub_idx].bg_b = 210.0f / 255.0f;
  render_components[sub_idx].font_size = 14.0f;

  // 5. Left Panel Section header
  Entity rm_header = add_entity_full(WIDGET_TEXT, 130.0f, 270.0f, 240.0f, 30.0f,
                                     "WHITEBOARD LAYERS", -1);
  render_components[rm_header].bg_r = 148.0f / 255.0f;
  render_components[rm_header].bg_g = 163.0f / 255.0f;
  render_components[rm_header].bg_b = 184.0f / 255.0f;
  render_components[rm_header].font_size = 14.0f;

  // 6. Left Panel list items
  Entity item1 =
      add_entity_full(WIDGET_TEXT, 130.0f, 310.0f, 240.0f, 30.0f, "Shapes", -1);
  render_components[item1].bg_r = 226.0f / 255.0f;
  render_components[item1].bg_g = 232.0f / 255.0f;
  render_components[item1].bg_b = 240.0f / 255.0f;
  render_components[item1].font_size = 18.0f;

  Entity item2 =
      add_entity_full(WIDGET_TEXT, 130.0f, 350.0f, 240.0f, 30.0f, "Text", -1);
  render_components[item2].bg_r = 226.0f / 255.0f;
  render_components[item2].bg_g = 232.0f / 255.0f;
  render_components[item2].bg_b = 240.0f / 255.0f;
  render_components[item2].font_size = 18.0f;

  Entity item3 =
      add_entity_full(WIDGET_TEXT, 130.0f, 390.0f, 240.0f, 30.0f, "Arrows", -1);
  render_components[item3].bg_r = 226.0f / 255.0f;
  render_components[item3].bg_g = 232.0f / 255.0f;
  render_components[item3].bg_b = 240.0f / 255.0f;
  render_components[item3].font_size = 18.0f;

  // 7. Left Panel footer
  Entity ft1 = add_entity_full(WIDGET_TEXT, 130.0f, 510.0f, 240.0f, 30.0f,
                               "Wasm Core", -1);
  render_components[ft1].bg_r = 148.0f / 255.0f;
  render_components[ft1].bg_g = 163.0f / 255.0f;
  render_components[ft1].bg_b = 184.0f / 255.0f;
  render_components[ft1].font_size = 16.0f;

  Entity ft2 = add_entity_full(WIDGET_TEXT, 130.0f, 550.0f, 240.0f, 30.0f,
                               "Pure C · WebGPU · JS", -1);
  render_components[ft2].bg_r = 94.0f / 255.0f;
  render_components[ft2].bg_g = 106.0f / 255.0f;
  render_components[ft2].bg_b = 210.0f / 255.0f;
  render_components[ft2].font_size = 13.0f;

  // 8. Right Panel big statement
  Entity right_title =
      add_entity_full(WIDGET_TEXT, 440.0f, 130.0f, 600.0f, 80.0f,
                      "Pure C Engine.\nWebGPU Pipelines.", -1);
  render_components[right_title].bg_r = 226.0f / 255.0f;
  render_components[right_title].bg_g = 232.0f / 255.0f;
  render_components[right_title].bg_b = 240.0f / 255.0f;
  render_components[right_title].font_size = 32.0f;

  Entity right_sub = add_entity_full(
      WIDGET_TEXT, 440.0f, 220.0f, 600.0f, 40.0f,
      "Infinite collaborative canvas built natively on raw WebGPU primitives.",
      -1);
  render_components[right_sub].bg_r = 148.0f / 255.0f;
  render_components[right_sub].bg_g = 163.0f / 255.0f;
  render_components[right_sub].bg_b = 184.0f / 255.0f;
  render_components[right_sub].font_size = 15.0f;

  // 9. Card 1
  Entity card1 = add_entity_full(WIDGET_RECT, 440.0f, 280.0f, 290.0f, 100.0f,
                                 "01   WASM Core\nPure C whiteboard code "
                                 "compiles directly to WebAssembly.",
                                 -1);
  render_components[card1].bg_r = 30.0f / 255.0f;
  render_components[card1].bg_g = 35.0f / 255.0f;
  render_components[card1].bg_b = 48.0f / 255.0f;
  render_components[card1].bg_a = 1.0f;
  render_components[card1].border_r = 94.0f / 255.0f;
  render_components[card1].border_g = 106.0f / 255.0f;
  render_components[card1].border_b = 210.0f / 255.0f;
  render_components[card1].border_a = 1.0f;
  render_components[card1].font_size = 13.0f;

  // 10. Card 2
  Entity card2 = add_entity_full(WIDGET_RECT, 750.0f, 280.0f, 290.0f, 100.0f,
                                 "02   Smooth MSAA\nHardware MSAA 4x rendering "
                                 "for crisp text, ovals & lines.",
                                 -1);
  render_components[card2].bg_r = 30.0f / 255.0f;
  render_components[card2].bg_g = 35.0f / 255.0f;
  render_components[card2].bg_b = 48.0f / 255.0f;
  render_components[card2].bg_a = 1.0f;
  render_components[card2].border_r = 94.0f / 255.0f;
  render_components[card2].border_g = 106.0f / 255.0f;
  render_components[card2].border_b = 210.0f / 255.0f;
  render_components[card2].border_a = 1.0f;
  render_components[card2].font_size = 13.0f;

  // 11. Frame Composition Title
  Entity fc_title = add_entity_full(WIDGET_TEXT, 440.0f, 410.0f, 600.0f, 30.0f,
                                    "ENGINE COMPOSITION", -1);
  render_components[fc_title].bg_r = 148.0f / 255.0f;
  render_components[fc_title].bg_g = 163.0f / 255.0f;
  render_components[fc_title].bg_b = 184.0f / 255.0f;
  render_components[fc_title].font_size = 14.0f;

  // 12. Frame Composition Rows
  // Row 1: C Geometry
  Entity geom_lbl = add_entity_full(WIDGET_TEXT, 440.0f, 450.0f, 150.0f, 30.0f,
                                    "C Geometry", -1);
  render_components[geom_lbl].bg_r = 148.0f / 255.0f;
  render_components[geom_lbl].bg_g = 163.0f / 255.0f;
  render_components[geom_lbl].bg_b = 184.0f / 255.0f;
  render_components[geom_lbl].font_size = 14.0f;

  Entity geom_bar_bg =
      add_entity_full(WIDGET_RECT, 600.0f, 455.0f, 400.0f, 20.0f, "", -1);
  render_components[geom_bar_bg].bg_r = 30.0f / 255.0f;
  render_components[geom_bar_bg].bg_g = 35.0f / 255.0f;
  render_components[geom_bar_bg].bg_b = 48.0f / 255.0f;
  render_components[geom_bar_bg].bg_a = 1.0f;
  render_components[geom_bar_bg].border_a = 0.0f;

  Entity geom_bar_val =
      add_entity_full(WIDGET_RECT, 600.0f, 455.0f, 300.0f, 20.0f, "", -1);
  render_components[geom_bar_val].bg_r = 94.0f / 255.0f;
  render_components[geom_bar_val].bg_g = 106.0f / 255.0f;
  render_components[geom_bar_val].bg_b = 210.0f / 255.0f;
  render_components[geom_bar_val].bg_a = 1.0f;
  render_components[geom_bar_val].border_a = 0.0f;

  // Row 2: Wasm Textures
  Entity typo_lbl = add_entity_full(WIDGET_TEXT, 440.0f, 490.0f, 150.0f, 30.0f,
                                    "Wasm Textures", -1);
  render_components[typo_lbl].bg_r = 148.0f / 255.0f;
  render_components[typo_lbl].bg_g = 163.0f / 255.0f;
  render_components[typo_lbl].bg_b = 184.0f / 255.0f;
  render_components[typo_lbl].font_size = 14.0f;

  Entity typo_bar_bg =
      add_entity_full(WIDGET_RECT, 600.0f, 495.0f, 400.0f, 20.0f, "", -1);
  render_components[typo_bar_bg].bg_r = 30.0f / 255.0f;
  render_components[typo_bar_bg].bg_g = 35.0f / 255.0f;
  render_components[typo_bar_bg].bg_b = 48.0f / 255.0f;
  render_components[typo_bar_bg].bg_a = 1.0f;
  render_components[typo_bar_bg].border_a = 0.0f;

  Entity typo_bar_val =
      add_entity_full(WIDGET_RECT, 600.0f, 495.0f, 240.0f, 20.0f, "", -1);
  render_components[typo_bar_val].bg_r = 6.0f / 255.0f;
  render_components[typo_bar_val].bg_g = 182.0f / 255.0f;
  render_components[typo_bar_val].bg_b = 212.0f / 255.0f;
  render_components[typo_bar_val].bg_a = 1.0f;
  render_components[typo_bar_val].border_a = 0.0f;

  // Row 3: JS WebGPU
  Entity int_lbl = add_entity_full(WIDGET_TEXT, 440.0f, 530.0f, 150.0f, 30.0f,
                                   "JS WebGPU", -1);
  render_components[int_lbl].bg_r = 148.0f / 255.0f;
  render_components[int_lbl].bg_g = 163.0f / 255.0f;
  render_components[int_lbl].bg_b = 184.0f / 255.0f;
  render_components[int_lbl].font_size = 14.0f;

  Entity int_bar_bg =
      add_entity_full(WIDGET_RECT, 600.0f, 535.0f, 400.0f, 20.0f, "", -1);
  render_components[int_bar_bg].bg_r = 30.0f / 255.0f;
  render_components[int_bar_bg].bg_g = 35.0f / 255.0f;
  render_components[int_bar_bg].bg_b = 48.0f / 255.0f;
  render_components[int_bar_bg].bg_a = 1.0f;
  render_components[int_bar_bg].border_a = 0.0f;

  Entity int_bar_val =
      add_entity_full(WIDGET_RECT, 600.0f, 535.0f, 180.0f, 20.0f, "", -1);
  render_components[int_bar_val].bg_r = 139.0f / 255.0f;
  render_components[int_bar_val].bg_g = 92.0f / 255.0f;
  render_components[int_bar_val].bg_b = 246.0f / 255.0f;
  render_components[int_bar_val].bg_a = 1.0f;
  render_components[int_bar_val].border_a = 0.0f;

  // 13. Create connection arrows
  add_connection_entity(card1, geom_lbl);
  add_connection_entity(card2, typo_lbl);
}

void generate_100k_infographics() {
  ecs_init();

  int cols = 20;
  int rows = 20;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      float x_offset = col * 1500.0f;
      float y_offset = row * 800.0f;

      // 1. Left Panel Background (Dark Slate)
      Entity left_bg =
          add_entity_full(WIDGET_RECT, 100.0f + x_offset, 100.0f + y_offset,
                          300.0f, 550.0f, "", -1);
      render_components[left_bg].bg_r = 15.0f / 255.0f;
      render_components[left_bg].bg_g = 18.0f / 255.0f;
      render_components[left_bg].bg_b = 25.0f / 255.0f;
      render_components[left_bg].bg_a = 1.0f;
      render_components[left_bg].border_a = 0.0f;

      // 2. Right Panel Background (Sleek Dark Slate)
      Entity right_bg =
          add_entity_full(WIDGET_RECT, 400.0f + x_offset, 100.0f + y_offset,
                          700.0f, 550.0f, "", -1);
      render_components[right_bg].bg_r = 20.0f / 255.0f;
      render_components[right_bg].bg_g = 24.0f / 255.0f;
      render_components[right_bg].bg_b = 33.0f / 255.0f;
      render_components[right_bg].bg_a = 1.0f;
      render_components[right_bg].border_a = 0.0f;

      // 3. Title Text "DUST" in White
      Entity title_idx =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 130.0f + y_offset,
                          240.0f, 60.0f, "DUST", -1);
      render_components[title_idx].bg_r = 226.0f / 255.0f;
      render_components[title_idx].bg_g = 232.0f / 255.0f;
      render_components[title_idx].bg_b = 240.0f / 255.0f;
      render_components[title_idx].font_size = 48.0f;

      // 4. Subtitle Text "WEBGPU WHITEBOARD" in Accent Indigo
      Entity sub_idx =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 190.0f + y_offset,
                          240.0f, 30.0f, "WEBGPU WHITEBOARD", -1);
      render_components[sub_idx].bg_r = 94.0f / 255.0f;
      render_components[sub_idx].bg_g = 106.0f / 255.0f;
      render_components[sub_idx].bg_b = 210.0f / 255.0f;
      render_components[sub_idx].font_size = 14.0f;

      // 5. Left Panel Section header
      Entity rm_header =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 270.0f + y_offset,
                          240.0f, 30.0f, "WHITEBOARD LAYERS", -1);
      render_components[rm_header].bg_r = 148.0f / 255.0f;
      render_components[rm_header].bg_g = 163.0f / 255.0f;
      render_components[rm_header].bg_b = 184.0f / 255.0f;
      render_components[rm_header].font_size = 14.0f;

      // 6. Left Panel list items
      Entity item1 =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 310.0f + y_offset,
                          240.0f, 30.0f, "Shapes", -1);
      render_components[item1].bg_r = 226.0f / 255.0f;
      render_components[item1].bg_g = 232.0f / 255.0f;
      render_components[item1].bg_b = 240.0f / 255.0f;
      render_components[item1].font_size = 18.0f;

      Entity item2 =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 350.0f + y_offset,
                          240.0f, 30.0f, "Text", -1);
      render_components[item2].bg_r = 226.0f / 255.0f;
      render_components[item2].bg_g = 232.0f / 255.0f;
      render_components[item2].bg_b = 240.0f / 255.0f;
      render_components[item2].font_size = 18.0f;

      Entity item3 =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 390.0f + y_offset,
                          240.0f, 30.0f, "Arrows", -1);
      render_components[item3].bg_r = 226.0f / 255.0f;
      render_components[item3].bg_g = 232.0f / 255.0f;
      render_components[item3].bg_b = 240.0f / 255.0f;
      render_components[item3].font_size = 18.0f;

      // 7. Left Panel footer
      Entity ft1 =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 510.0f + y_offset,
                          240.0f, 30.0f, "Wasm Core", -1);
      render_components[ft1].bg_r = 148.0f / 255.0f;
      render_components[ft1].bg_g = 163.0f / 255.0f;
      render_components[ft1].bg_b = 184.0f / 255.0f;
      render_components[ft1].font_size = 16.0f;

      Entity ft2 =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 550.0f + y_offset,
                          240.0f, 30.0f, "Pure C · WebGPU · JS", -1);
      render_components[ft2].bg_r = 94.0f / 255.0f;
      render_components[ft2].bg_g = 106.0f / 255.0f;
      render_components[ft2].bg_b = 210.0f / 255.0f;
      render_components[ft2].font_size = 13.0f;

      // 8. Right Panel big statement
      Entity right_title = add_entity_full(
          WIDGET_TEXT, 440.0f + x_offset, 130.0f + y_offset, 600.0f, 80.0f,
          "Pure C Engine.\nWebGPU Pipelines.", -1);
      render_components[right_title].bg_r = 226.0f / 255.0f;
      render_components[right_title].bg_g = 232.0f / 255.0f;
      render_components[right_title].bg_b = 240.0f / 255.0f;
      render_components[right_title].font_size = 32.0f;

      Entity right_sub = add_entity_full(WIDGET_TEXT, 440.0f + x_offset,
                                         220.0f + y_offset, 600.0f, 40.0f,
                                         "Infinite collaborative canvas built "
                                         "natively on raw WebGPU primitives.",
                                         -1);
      render_components[right_sub].bg_r = 148.0f / 255.0f;
      render_components[right_sub].bg_g = 163.0f / 255.0f;
      render_components[right_sub].bg_b = 184.0f / 255.0f;
      render_components[right_sub].font_size = 15.0f;

      // 9. Card 1
      Entity card1 = add_entity_full(WIDGET_RECT, 440.0f + x_offset,
                                     280.0f + y_offset, 290.0f, 100.0f,
                                     "01   WASM Core\nPure C whiteboard code "
                                     "compiles directly to WebAssembly.",
                                     -1);
      render_components[card1].bg_r = 30.0f / 255.0f;
      render_components[card1].bg_g = 35.0f / 255.0f;
      render_components[card1].bg_b = 48.0f / 255.0f;
      render_components[card1].bg_a = 1.0f;
      render_components[card1].border_r = 94.0f / 255.0f;
      render_components[card1].border_g = 106.0f / 255.0f;
      render_components[card1].border_b = 210.0f / 255.0f;
      render_components[card1].border_a = 1.0f;
      render_components[card1].font_size = 13.0f;

      // 10. Card 2
      Entity card2 = add_entity_full(WIDGET_RECT, 750.0f + x_offset,
                                     280.0f + y_offset, 290.0f, 100.0f,
                                     "02   Smooth MSAA\nHardware MSAA 4x "
                                     "rendering for crisp text, ovals & lines.",
                                     -1);
      render_components[card2].bg_r = 30.0f / 255.0f;
      render_components[card2].bg_g = 35.0f / 255.0f;
      render_components[card2].bg_b = 48.0f / 255.0f;
      render_components[card2].bg_a = 1.0f;
      render_components[card2].border_r = 94.0f / 255.0f;
      render_components[card2].border_g = 106.0f / 255.0f;
      render_components[card2].border_b = 210.0f / 255.0f;
      render_components[card2].border_a = 1.0f;
      render_components[card2].font_size = 13.0f;

      // 11. Frame Composition Title
      Entity fc_title =
          add_entity_full(WIDGET_TEXT, 440.0f + x_offset, 410.0f + y_offset,
                          600.0f, 30.0f, "ENGINE COMPOSITION", -1);
      render_components[fc_title].bg_r = 148.0f / 255.0f;
      render_components[fc_title].bg_g = 163.0f / 255.0f;
      render_components[fc_title].bg_b = 184.0f / 255.0f;
      render_components[fc_title].font_size = 14.0f;

      // 12. Frame Composition Rows
      // Row 1: C Geometry
      Entity geom_lbl =
          add_entity_full(WIDGET_TEXT, 440.0f + x_offset, 450.0f + y_offset,
                          150.0f, 30.0f, "C Geometry", -1);
      render_components[geom_lbl].bg_r = 148.0f / 255.0f;
      render_components[geom_lbl].bg_g = 163.0f / 255.0f;
      render_components[geom_lbl].bg_b = 184.0f / 255.0f;
      render_components[geom_lbl].font_size = 14.0f;

      Entity geom_bar_bg =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 455.0f + y_offset,
                          400.0f, 20.0f, "", -1);
      render_components[geom_bar_bg].bg_r = 30.0f / 255.0f;
      render_components[geom_bar_bg].bg_g = 35.0f / 255.0f;
      render_components[geom_bar_bg].bg_b = 48.0f / 255.0f;
      render_components[geom_bar_bg].bg_a = 1.0f;
      render_components[geom_bar_bg].border_a = 0.0f;

      Entity geom_bar_val =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 455.0f + y_offset,
                          300.0f, 20.0f, "", -1);
      render_components[geom_bar_val].bg_r = 94.0f / 255.0f;
      render_components[geom_bar_val].bg_g = 106.0f / 255.0f;
      render_components[geom_bar_val].bg_b = 210.0f / 255.0f;
      render_components[geom_bar_val].bg_a = 1.0f;
      render_components[geom_bar_val].border_a = 0.0f;

      // Row 2: Wasm Textures
      Entity typo_lbl =
          add_entity_full(WIDGET_TEXT, 440.0f + x_offset, 490.0f + y_offset,
                          150.0f, 30.0f, "Wasm Textures", -1);
      render_components[typo_lbl].bg_r = 148.0f / 255.0f;
      render_components[typo_lbl].bg_g = 163.0f / 255.0f;
      render_components[typo_lbl].bg_b = 184.0f / 255.0f;
      render_components[typo_lbl].font_size = 14.0f;

      Entity typo_bar_bg =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 495.0f + y_offset,
                          400.0f, 20.0f, "", -1);
      render_components[typo_bar_bg].bg_r = 30.0f / 255.0f;
      render_components[typo_bar_bg].bg_g = 35.0f / 255.0f;
      render_components[typo_bar_bg].bg_b = 48.0f / 255.0f;
      render_components[typo_bar_bg].bg_a = 1.0f;
      render_components[typo_bar_bg].border_a = 0.0f;

      Entity typo_bar_val =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 495.0f + y_offset,
                          240.0f, 20.0f, "", -1);
      render_components[typo_bar_val].bg_r = 6.0f / 255.0f;
      render_components[typo_bar_val].bg_g = 182.0f / 255.0f;
      render_components[typo_bar_val].bg_b = 212.0f / 255.0f;
      render_components[typo_bar_val].bg_a = 1.0f;
      render_components[typo_bar_val].border_a = 0.0f;

      // Row 3: JS WebGPU
      Entity int_lbl =
          add_entity_full(WIDGET_TEXT, 440.0f + x_offset, 530.0f + y_offset,
                          150.0f, 30.0f, "JS WebGPU", -1);
      render_components[int_lbl].bg_r = 148.0f / 255.0f;
      render_components[int_lbl].bg_g = 163.0f / 255.0f;
      render_components[int_lbl].bg_b = 184.0f / 255.0f;
      render_components[int_lbl].font_size = 14.0f;

      Entity int_bar_bg =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 535.0f + y_offset,
                          400.0f, 20.0f, "", -1);
      render_components[int_bar_bg].bg_r = 30.0f / 255.0f;
      render_components[int_bar_bg].bg_g = 35.0f / 255.0f;
      render_components[int_bar_bg].bg_b = 48.0f / 255.0f;
      render_components[int_bar_bg].bg_a = 1.0f;
      render_components[int_bar_bg].border_a = 0.0f;

      Entity int_bar_val =
          add_entity_full(WIDGET_RECT, 600.0f + x_offset, 535.0f + y_offset,
                          180.0f, 20.0f, "", -1);
      render_components[int_bar_val].bg_r = 139.0f / 255.0f;
      render_components[int_bar_val].bg_g = 92.0f / 255.0f;
      render_components[int_bar_val].bg_b = 246.0f / 255.0f;
      render_components[int_bar_val].bg_a = 1.0f;
      render_components[int_bar_val].border_a = 0.0f;

      // 13. Create connection arrows
      add_connection_entity(card1, geom_lbl);
      add_connection_entity(card2, typo_lbl);
    }
  }
}
