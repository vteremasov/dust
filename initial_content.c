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

static char *local_strncpy(char *dest, const char *src, int n) {
  int i = 0;
  while (i < n - 1 && src[i] != '\0') {
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
  local_strncpy(text_components[e].text, text, 128);

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

static float float_sin(float val) {
  return __builtin_sinf(val);
}

static void set_text_color(Entity e, float r, float g, float b) {
  if (e == (Entity)-1) return;
  render_components[e].r = r;
  render_components[e].g = g;
  render_components[e].b = b;
  if (render_components[e].type == WIDGET_TEXT) {
    render_components[e].bg_r = r;
    render_components[e].bg_g = g;
    render_components[e].bg_b = b;
  }
}

static void set_shape_color(Entity e, float r, float g, float b, float a) {
  if (e == (Entity)-1) return;
  render_components[e].bg_r = r;
  render_components[e].bg_g = g;
  render_components[e].bg_b = b;
  render_components[e].bg_a = a;
}

static void set_shape_border(Entity e, float r, float g, float b, float a) {
  if (e == (Entity)-1) return;
  render_components[e].border_r = r;
  render_components[e].border_g = g;
  render_components[e].border_b = b;
  render_components[e].border_a = a;
}

void generate_initial_content() {
  float slide_y = 100.0f;
  float slide_w = 800.0f;
  float slide_h = 500.0f;
  float spacing = 900.0f;

  Entity slides[4];

  static struct { float r, g, b; } slide_themes[4] = {
    {250.0f/255.0f, 245.0f/255.0f, 255.0f/255.0f}, // Lavender-grey/Purple (Slide 1)
    {240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f}, // Sky Blue (Slide 2)
    {245.0f/255.0f, 255.0f/255.0f, 250.0f/255.0f}, // Mint Green (Slide 3)
    {255.0f/255.0f, 252.0f/255.0f, 245.0f/255.0f}  // Warm Cream (Slide 4)
  };

  static const char *titles[4] = {
    "Dust Canvas Engine",
    "Technologie Stack",
    "Architectural Approaches",
    "Design Principals"
  };

  static const char *subtitles[4] = {
    "Infinite hardware-accelerated whiteboard in pure C",
    "Low-overhead native primitives & compilation targets",
    "ECS abstractions, batch drawing, & module decoupling",
    "Performance-driven and dependency-free engineering"
  };

  // Typographic Colors
  float tp_r = 30.0f / 255.0f, tp_g = 41.0f / 255.0f, tp_b = 59.0f / 255.0f; // Slate Navy
  float ta_r = 79.0f / 255.0f, ta_g = 70.0f / 255.0f, ta_b = 229.0f / 255.0f; // Indigo
  float tm_r = 71.0f / 255.0f, tm_g = 85.0f / 255.0f, tm_b = 105.0f / 255.0f; // Slate Muted
  float tr_r = 239.0f / 255.0f, tr_g = 68.0f / 255.0f, tr_b = 68.0f / 255.0f; // Red/Alert
  float tg_r = 245.0f / 255.0f, tg_g = 158.0f / 255.0f, tg_b = 11.0f / 255.0f;  // Honey Gold

  // 1. Initialize slide backgrounds & brand layouts
  for (int i = 0; i < 4; i++) {
    float x = 100.0f + i * spacing;

    // Tactile Floating Shadow (with offset y+8, x+8)
    Entity shadow = add_entity_full(WIDGET_RECT, x + 8.0f, slide_y + 8.0f, slide_w, slide_h, "", -1);
    set_shape_color(shadow, 12.0f/255.0f, 15.0f/255.0f, 20.0f/255.0f, 0.22f);
    render_components[shadow].border_a = 0.0f;

    // Slide Body background card
    slides[i] = add_entity_full(WIDGET_RECT, x, slide_y, slide_w, slide_h, "", -1);
    set_shape_color(slides[i], slide_themes[i].r, slide_themes[i].g, slide_themes[i].b, 1.0f);
    set_shape_border(slides[i], 203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f); // Slate-200

    // Top Stripe accent
    Entity stripe = add_entity_full(WIDGET_RECT, x, slide_y, slide_w, 8.0f, "", -1);
    if (i % 2 == 0) {
      set_shape_color(stripe, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f, 1.0f); // Coral accent
    } else {
      set_shape_color(stripe, 245.0f/255.0f, 158.0f/255.0f, 11.0f/255.0f, 1.0f); // Honey Gold accent
    }
    render_components[stripe].border_a = 0.0f;

    // BRAND SIDEBAR (Left column, 60px wide, Slate Navy)
    Entity sidebar = add_entity_full(WIDGET_RECT, x, slide_y + 8.0f, 60.0f, slide_h - 8.0f, "", -1);
    set_shape_color(sidebar, tp_r, tp_g, tp_b, 1.0f);
    render_components[sidebar].border_a = 0.0f;

    // Slide Index Label inside Sidebar
    char idx_str[4];
    idx_str[0] = '0';
    idx_str[1] = '1' + i;
    idx_str[2] = '\0';
    Entity idx_txt = add_entity_full(WIDGET_TEXT, x + 12.0f, slide_y + 30.0f, 36.0f, 32.0f, idx_str, -1);
    set_text_color(idx_txt, 1.0f, 1.0f, 1.0f);
    render_components[idx_txt].font_size = 20.0f;

    // Small decorative symbol inside Sidebar
    Entity side_sym = add_entity_full(WIDGET_TEXT, x + 18.0f, slide_y + 65.0f, 24.0f, 20.0f, "⚡", -1);
    set_text_color(side_sym, 245.0f/255.0f, 158.0f/255.0f, 11.0f/255.0f); // Gold
    render_components[side_sym].font_size = 14.0f;

    // Dust Canvas logo badge on the right side of header
    Entity logo_bg = add_entity_full(WIDGET_RECT, x + slide_w - 170.0f, slide_y + 20.0f, 130.0f, 26.0f, "", -1);
    set_shape_color(logo_bg, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.9f);
    set_shape_border(logo_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity logo_txt = add_entity_full(WIDGET_TEXT, x + slide_w - 165.0f, slide_y + 24.0f, 120.0f, 22.0f, "⚡ DUST CANVAS", -1);
    set_text_color(logo_txt, ta_r, ta_g, ta_b);
    render_components[logo_txt].font_size = 11.0f;

    // Header Title
    Entity t_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, slide_y + 20.0f, 530.0f, 32.0f, titles[i], -1);
    set_text_color(t_ent, tp_r, tp_g, tp_b);
    render_components[t_ent].font_size = 22.0f;

    // Horizontal Divider
    Entity line = add_entity_full(WIDGET_RECT, x + 80.0f, slide_y + 55.0f, slide_w - 120.0f, 1.5f, "", -1);
    set_shape_color(line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[line].border_a = 0.0f;

    // Header Subtitle
    Entity sub_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, slide_y + 68.0f, slide_w - 120.0f, 25.0f, subtitles[i], -1);
    if (i % 2 == 0) {
      set_text_color(sub_ent, ta_r, ta_g, ta_b);
    } else {
      set_text_color(sub_ent, 180.0f / 255.0f, 83.0f / 255.0f, 9.0f / 255.0f); // Amber-700 (High contrast bronze)
    }
    render_components[sub_ent].font_size = 13.0f;
  }

  // Connect slides sequentially with arrows
  for (int i = 0; i < 3; i++) {
    Entity arr = add_connection_entity(slides[i], slides[i + 1]);
    set_shape_color(arr, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 1.0f);
  }

  // ==========================================
  // SLIDE 1: Dust Canvas Engine (Overview)
  // ==========================================
  {
    float x = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 150.0f, 400.0f, 30.0f, "• Pure C graphics engine compiling to standalone WebAssembly.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    render_components[d1].font_size = 13.0f;

    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 190.0f, 400.0f, 30.0f, "• Direct WebGPU rendering pipeline for infinite scaling & zoom.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    render_components[d2].font_size = 13.0f;

    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 230.0f, 400.0f, 30.0f, "• Custom Entity-Component-System (ECS) handles high node counts.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    render_components[d3].font_size = 13.0f;

    Entity d4 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 280.0f, 400.0f, 30.0f, "Explore the architecture & flow diagrams illustrated below!", -1);
    set_text_color(d4, ta_r, ta_g, ta_b);
    render_components[d4].font_size = 13.0f;

    // Visual composition: A layered stack graphic
    Entity stack_base = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 280.0f, 180.0f, 40.0f, "Hardware / GPU", -1);
    set_shape_color(stack_base, tp_r, tp_g, tp_b, 1.0f);
    set_text_color(stack_base, 1.0f, 1.0f, 1.0f);
    render_components[stack_base].font_size = 12.0f;

    Entity stack_mid = add_entity_full(WIDGET_RECT, x + 550.0f, slide_y + 210.0f, 180.0f, 40.0f, "Wasm Drawing Engine", -1);
    set_shape_color(stack_mid, ta_r, ta_g, ta_b, 1.0f);
    set_text_color(stack_mid, 1.0f, 1.0f, 1.0f);
    render_components[stack_mid].font_size = 12.0f;

    Entity stack_top = add_entity_full(WIDGET_RECT, x + 570.0f, slide_y + 140.0f, 180.0f, 40.0f, "JS Events & Input", -1);
    set_shape_color(stack_top, tg_r, tg_g, tg_b, 1.0f);
    set_text_color(stack_top, tp_r, tp_g, tp_b);
    render_components[stack_top].font_size = 12.0f;

    add_connection_entity(stack_top, stack_mid);
    add_connection_entity(stack_mid, stack_base);
  }

  // ==========================================
  // SLIDE 2: Technologie Stack
  // ==========================================
  {
    float x = 100.0f + spacing;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 150.0f, 420.0f, 30.0f, "• WebAssembly Target: freestanding compiling via LLVM/Clang.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    render_components[d1].font_size = 13.0f;

    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 190.0f, 420.0f, 30.0f, "• Freestanding C Environment: build has no stdlib/libc dependencies.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    render_components[d2].font_size = 13.0f;

    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 230.0f, 420.0f, 30.0f, "• WebGPU Graphics Bindings: handles vertices & index uploads directly.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    render_components[d3].font_size = 13.0f;

    Entity d4 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 270.0f, 420.0f, 30.0f, "• MSDF Scalable Typography: 2D vector font atlas from stb_truetype.", -1);
    set_text_color(d4, tm_r, tm_g, tm_b);
    render_components[d4].font_size = 13.0f;

    // Visual technology stack on the right side of the slide
    Entity tech_card3 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 280.0f, 200.0f, 40.0f, "Wasm Target (-nostdlib)", -1);
    set_shape_color(tech_card3, 34.0f/255.0f, 197.0f/255.0f, 94.0f/255.0f, 1.0f); // Green
    set_text_color(tech_card3, 1.0f, 1.0f, 1.0f);
    render_components[tech_card3].font_size = 11.0f;

    Entity tech_card2 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 210.0f, 200.0f, 40.0f, "WebGPU Pipeline Layer", -1);
    set_shape_color(tech_card2, 14.0f/255.0f, 165.0f/255.0f, 233.0f/255.0f, 1.0f); // Sky Blue
    set_text_color(tech_card2, 1.0f, 1.0f, 1.0f);
    render_components[tech_card2].font_size = 11.0f;

    Entity tech_card1 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 140.0f, 200.0f, 40.0f, "MSDF Atlas Renderer", -1);
    set_shape_color(tech_card1, tg_r, tg_g, tg_b, 1.0f); // Honey Gold
    set_text_color(tech_card1, tp_r, tp_g, tp_b);
    render_components[tech_card1].font_size = 11.0f;

    add_connection_entity(tech_card1, tech_card2);
    add_connection_entity(tech_card2, tech_card3);
  }

  // ==========================================
  // SLIDE 3: Architectural Approaches
  // ==========================================
  {
    float x = 100.0f + spacing * 2.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 150.0f, 420.0f, 30.0f, "• Decoupled JS/C Interface: JS holds events; C runs core logic.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    render_components[d1].font_size = 13.0f;

    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 190.0f, 420.0f, 30.0f, "• Modular Engine Design: renderer.c operates as isolated shader glue.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    render_components[d2].font_size = 13.0f;

    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 230.0f, 420.0f, 30.0f, "• Pure C ECS Component Layer: lightweight arrays index components.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    render_components[d3].font_size = 13.0f;

    Entity d4 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 270.0f, 420.0f, 30.0f, "• Dynamic Connection Lines: live arrow lines update between entities.", -1);
    set_text_color(d4, tm_r, tm_g, tm_b);
    render_components[d4].font_size = 13.0f;

    // A beautiful 2x2 grid representing components
    float gx[2] = {x + 530.0f, x + 640.0f};
    float gy[2] = {slide_y + 140.0f, slide_y + 240.0f};
    const char *comp_titles[4] = {"JS Input", "ECS Model", "Renderer", "WebGPU"};
    float c_colors[4][3] = {
      {238.0f/255.0f, 242.0f/255.0f, 255.0f/255.0f}, // Light Indigo
      {240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f}, // Light Green
      {254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f}, // Light Yellow
      {255.0f/255.0f, 241.0f/255.0f, 242.0f/255.0f}  // Light Rose
    };

    for (int k = 0; k < 4; k++) {
      int ix = k % 2;
      int iy = k / 2;
      Entity card = add_entity_full(WIDGET_RECT, gx[ix], gy[iy], 100.0f, 80.0f, comp_titles[k], -1);
      set_shape_color(card, c_colors[k][0], c_colors[k][1], c_colors[k][2], 1.0f);
      set_shape_border(card, c_colors[k][0] * 0.8f, c_colors[k][1] * 0.8f, c_colors[k][2] * 0.8f, 1.0f);
      set_text_color(card, tp_r, tp_g, tp_b);
      render_components[card].font_size = 11.0f;
    }
  }

  // ==========================================
  // SLIDE 4: Design Principals
  // ==========================================
  {
    float x = 100.0f + spacing * 3.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 150.0f, 420.0f, 30.0f, "• Maximized Performance: direct GPU draw pipeline & buffer mapping.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    render_components[d1].font_size = 13.0f;

    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 190.0f, 420.0f, 30.0f, "• Zero Dependencies: custom math, string, and allocation functions.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    render_components[d2].font_size = 13.0f;

    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 230.0f, 420.0f, 30.0f, "• Strict State Flow: data mutations map directly to components.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    render_components[d3].font_size = 13.0f;

    Entity d4 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 270.0f, 420.0f, 30.0f, "• Explicit Repaints: dirty flags bypass CPU cycles when idle.", -1);
    set_text_color(d4, tm_r, tm_g, tm_b);
    render_components[d4].font_size = 13.0f;

    // Visual badges representing goals
    Entity p1 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 140.0f, 210.0f, 50.0f, "Speed: 60 FPS Under Load", -1);
    set_shape_color(p1, 239.0f/255.0f, 68.0f/255.0f, 68.0f/255.0f, 0.1f);
    set_shape_border(p1, 239.0f/255.0f, 68.0f/255.0f, 68.0f/255.0f, 1.0f);
    set_text_color(p1, 185.0f/255.0f, 28.0f/255.0f, 28.0f/255.0f); // Red-700 for readability
    render_components[p1].font_size = 11.0f;

    Entity p2 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 210.0f, 210.0f, 50.0f, "Size: < 100KB Wasm Size", -1);
    set_shape_color(p2, 34.0f/255.0f, 197.0f/255.0f, 94.0f/255.0f, 0.1f);
    set_shape_border(p2, 34.0f/255.0f, 197.0f/255.0f, 94.0f/255.0f, 1.0f);
    set_text_color(p2, 21.0f/255.0f, 128.0f/255.0f, 61.0f/255.0f); // Green-700 for readability
    render_components[p2].font_size = 11.0f;

    Entity p3 = add_entity_full(WIDGET_RECT, x + 530.0f, slide_y + 280.0f, 210.0f, 50.0f, "Dep: Zero External Libs", -1);
    set_shape_color(p3, tg_r, tg_g, tg_b, 0.1f);
    set_shape_border(p3, tg_r, tg_g, tg_b, 1.0f);
    set_text_color(p3, 180.0f/255.0f, 83.0f/255.0f, 9.0f/255.0f); // Amber-700 for readability
    render_components[p3].font_size = 11.0f;
  }

  // ==========================================
  // DIAGRAMS SECTION (Flowchart & Dependencies)
  // ==========================================
  {
    // Section Title
    Entity sect_title = add_entity_full(WIDGET_TEXT, 100.0f, 650.0f, 700.0f, 40.0f, "SYSTEM FLOW & FILE DEPENDENCY ARCHITECTURE", -1);
    set_text_color(sect_title, tp_r, tp_g, tp_b);
    render_components[sect_title].font_size = 20.0f;

    Entity sect_line = add_entity_full(WIDGET_RECT, 100.0f, 695.0f, 780.0f, 2.0f, "", -1);
    set_shape_color(sect_line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[sect_line].border_a = 0.0f;

    // Background Panel for Logic Flowchart
    Entity flow_bg = add_entity_full(WIDGET_RECT, 100.0f, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(flow_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(flow_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity flow_label = add_entity_full(WIDGET_TEXT, 120.0f, 735.0f, 340.0f, 25.0f, "1. Render Logic Flowchart (Vertical Flow)", -1);
    set_text_color(flow_label, ta_r, ta_g, ta_b);
    render_components[flow_label].font_size = 13.0f;

    // Logic flowchart steps
    // Step A: Input Events
    Entity step_a = add_entity_full(WIDGET_RECT, 180.0f, 770.0f, 220.0f, 50.0f, "Browser Mouse / Key Event\n(forwarded from JS)", -1);
    set_shape_color(step_a, 240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(step_a, 186.0f/255.0f, 230.0f/255.0f, 253.0f/255.0f, 1.0f);
    set_text_color(step_a, tp_r, tp_g, tp_b);
    render_components[step_a].font_size = 11.0f;

    // Step B: canvas.c entrypoint
    Entity step_b = add_entity_full(WIDGET_RECT, 180.0f, 870.0f, 220.0f, 50.0f, "canvas.c (JS Exports)\n(receives events, maps positions)", -1);
    set_shape_color(step_b, 245.0f/255.0f, 245.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(step_b, 212.0f/255.0f, 212.0f/255.0f, 216.0f/255.0f, 1.0f);
    set_text_color(step_b, tp_r, tp_g, tp_b);
    render_components[step_b].font_size = 11.0f;

    // Step C: systems.c ECS step
    Entity step_c = add_entity_full(WIDGET_RECT, 180.0f, 970.0f, 220.0f, 50.0f, "systems.c (ECS Updates)\n(drags, highlights, & selects)", -1);
    set_shape_color(step_c, 240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f, 1.0f);
    set_shape_border(step_c, 187.0f/255.0f, 247.0f/255.0f, 208.0f/255.0f, 1.0f);
    set_text_color(step_c, tp_r, tp_g, tp_b);
    render_components[step_c].font_size = 11.0f;

    // Step D: renderer.c Batch Draw
    Entity step_d = add_entity_full(WIDGET_RECT, 180.0f, 1070.0f, 220.0f, 50.0f, "renderer.c (Drawing Engine)\n(populates index/vertex buffers)", -1);
    set_shape_color(step_d, 254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f, 1.0f);
    set_shape_border(step_d, 253.0f/255.0f, 230.0f/255.0f, 138.0f/255.0f, 1.0f);
    set_text_color(step_d, tp_r, tp_g, tp_b);
    render_components[step_d].font_size = 11.0f;

    // Step E: WebGPU Render
    Entity step_e = add_entity_full(WIDGET_RECT, 180.0f, 1170.0f, 220.0f, 50.0f, "WebGPU Graphics Context\n(flushes batch into textures)", -1);
    set_shape_color(step_e, 253.0f/255.0f, 244.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(step_e, 245.0f/255.0f, 208.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(step_e, tp_r, tp_g, tp_b);
    render_components[step_e].font_size = 11.0f;

    // Connections for Logic Flowchart
    add_connection_entity(step_a, step_b);
    add_connection_entity(step_b, step_c);
    add_connection_entity(step_c, step_d);
    add_connection_entity(step_d, step_e);

    // Background Panel for Dependencies
    Entity dep_bg = add_entity_full(WIDGET_RECT, 500.0f, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(dep_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(dep_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity dep_label = add_entity_full(WIDGET_TEXT, 520.0f, 735.0f, 340.0f, 25.0f, "2. File Dependencies & Method Calls", -1);
    set_text_color(dep_label, ta_r, ta_g, ta_b);
    render_components[dep_label].font_size = 13.0f;

    // File node cards
    // 1. bridge.js
    Entity node_js = add_entity_full(WIDGET_RECT, 580.0f, 770.0f, 220.0f, 50.0f, "bridge.js\n(JS browser controller)", -1);
    set_shape_color(node_js, 254.0f/255.0f, 249.0f/255.0f, 195.0f/255.0f, 1.0f);
    set_shape_border(node_js, 253.0f/255.0f, 224.0f/255.0f, 71.0f/255.0f, 1.0f);
    set_text_color(node_js, tp_r, tp_g, tp_b);
    render_components[node_js].font_size = 11.0f;

    // 2. canvas.c
    Entity node_canvas = add_entity_full(WIDGET_RECT, 580.0f, 870.0f, 220.0f, 50.0f, "canvas.c\n(isolated Wasm API exports)", -1);
    set_shape_color(node_canvas, 238.0f/255.0f, 242.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(node_canvas, 199.0f/255.0f, 210.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(node_canvas, tp_r, tp_g, tp_b);
    render_components[node_canvas].font_size = 11.0f;

    // 3. systems.c
    Entity node_systems = add_entity_full(WIDGET_RECT, 580.0f, 970.0f, 220.0f, 50.0f, "systems.c\n(ECS simulation systems)", -1);
    set_shape_color(node_systems, 236.0f/255.0f, 253.0f/255.0f, 250.0f/255.0f, 1.0f);
    set_shape_border(node_systems, 153.0f/255.0f, 246.0f/255.0f, 228.0f/255.0f, 1.0f);
    set_text_color(node_systems, tp_r, tp_g, tp_b);
    render_components[node_systems].font_size = 11.0f;

    // 4. renderer.c
    Entity node_renderer = add_entity_full(WIDGET_RECT, 580.0f, 1070.0f, 220.0f, 50.0f, "renderer.c\n(MSDF drawing primitives)", -1);
    set_shape_color(node_renderer, 254.0f/255.0f, 242.0f/255.0f, 242.0f/255.0f, 1.0f);
    set_shape_border(node_renderer, 254.0f/255.0f, 202.0f/255.0f, 202.0f/255.0f, 1.0f);
    set_text_color(node_renderer, tp_r, tp_g, tp_b);
    render_components[node_renderer].font_size = 11.0f;

    // 5. ecs.c
    Entity node_ecs = add_entity_full(WIDGET_RECT, 580.0f, 1170.0f, 220.0f, 50.0f, "ecs.c\n(data arrays store)", -1);
    set_shape_color(node_ecs, 245.0f/255.0f, 243.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(node_ecs, 221.0f/255.0f, 214.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(node_ecs, tp_r, tp_g, tp_b);
    render_components[node_ecs].font_size = 11.0f;

    // Connections representing File Dependencies & Method Calls
    add_connection_entity(node_js, node_canvas);       // bridge.js calls canvas.c exports (e.g. step, on_mouse_down)
    add_connection_entity(node_canvas, node_systems);   // canvas.c calls step_systems()
    add_connection_entity(node_systems, node_renderer); // systems.c calls draw_rect(), draw_line()
    add_connection_entity(node_systems, node_ecs);      // systems.c checks ecs_has_component()

    // Add labels next to method calls/dependencies to specify method names
    Entity lbl_calls1 = add_entity_full(WIDGET_TEXT, 620.0f, 830.0f, 200.0f, 20.0f, "Calls step(), mouse_down()", -1);
    set_text_color(lbl_calls1, tm_r, tm_g, tm_b); render_components[lbl_calls1].font_size = 8.0f;

    Entity lbl_calls2 = add_entity_full(WIDGET_TEXT, 620.0f, 930.0f, 200.0f, 20.0f, "Calls step_systems()", -1);
    set_text_color(lbl_calls2, tm_r, tm_g, tm_b); render_components[lbl_calls2].font_size = 8.0f;

    Entity lbl_calls3 = add_entity_full(WIDGET_TEXT, 620.0f, 1030.0f, 200.0f, 20.0f, "Calls draw_rect(), draw_line()", -1);
    set_text_color(lbl_calls3, tm_r, tm_g, tm_b); render_components[lbl_calls3].font_size = 8.0f;

    Entity lbl_calls4 = add_entity_full(WIDGET_TEXT, 620.0f, 1130.0f, 200.0f, 20.0f, "Checks components/entity masks", -1);
    set_text_color(lbl_calls4, tm_r, tm_g, tm_b); render_components[lbl_calls4].font_size = 8.0f;
  }

  // ==========================================
  // PROJECT ARCHITECTURE & MODULAR DESIGN DIAGRAMS (Slide 2, x = 1000.0f)
  // ==========================================
  {
    float x = 100.0f + spacing; // 1000.0f

    // Section Title
    Entity sect_title = add_entity_full(WIDGET_TEXT, x, 650.0f, 700.0f, 40.0f, "PROJECT ARCHITECTURE & MODULAR DESIGN", -1);
    set_text_color(sect_title, tp_r, tp_g, tp_b);
    render_components[sect_title].font_size = 20.0f;

    Entity sect_line = add_entity_full(WIDGET_RECT, x, 695.0f, 780.0f, 2.0f, "", -1);
    set_shape_color(sect_line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[sect_line].border_a = 0.0f;

    // Background Panel for Component Modules (Left)
    Entity mod_bg = add_entity_full(WIDGET_RECT, x, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(mod_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(mod_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity mod_label = add_entity_full(WIDGET_TEXT, x + 20.0f, 735.0f, 340.0f, 25.0f, "1. Engine Component Modules", -1);
    set_text_color(mod_label, ta_r, ta_g, ta_b);
    render_components[mod_label].font_size = 13.0f;

    // Module boxes
    Entity mod_a = add_entity_full(WIDGET_RECT, x + 80.0f, 770.0f, 220.0f, 50.0f, "User Interface\n(HTML, CSS, WebGL Canvas)", -1);
    set_shape_color(mod_a, 240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(mod_a, 186.0f/255.0f, 230.0f/255.0f, 253.0f/255.0f, 1.0f);
    set_text_color(mod_a, tp_r, tp_g, tp_b);
    render_components[mod_a].font_size = 11.0f;

    Entity mod_b = add_entity_full(WIDGET_RECT, x + 80.0f, 870.0f, 220.0f, 50.0f, "JS Orchestrator (bridge.js)\n(receives events, calls Wasm)", -1);
    set_shape_color(mod_b, 245.0f/255.0f, 245.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(mod_b, 212.0f/255.0f, 212.0f/255.0f, 216.0f/255.0f, 1.0f);
    set_text_color(mod_b, tp_r, tp_g, tp_b);
    render_components[mod_b].font_size = 11.0f;

    Entity mod_c = add_entity_full(WIDGET_RECT, x + 80.0f, 970.0f, 220.0f, 50.0f, "Wasm Boundary (canvas.c)\n(manages ECS state & ticks)", -1);
    set_shape_color(mod_c, 240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f, 1.0f);
    set_shape_border(mod_c, 187.0f/255.0f, 247.0f/255.0f, 208.0f/255.0f, 1.0f);
    set_text_color(mod_c, tp_r, tp_g, tp_b);
    render_components[mod_c].font_size = 11.0f;

    Entity mod_d = add_entity_full(WIDGET_RECT, x + 80.0f, 1070.0f, 220.0f, 50.0f, "WebGPU Batch Renderer (renderer.c)\n(shapes/MSDF text batcher)", -1);
    set_shape_color(mod_d, 254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f, 1.0f);
    set_shape_border(mod_d, 253.0f/255.0f, 230.0f/255.0f, 138.0f/255.0f, 1.0f);
    set_text_color(mod_d, tp_r, tp_g, tp_b);
    render_components[mod_d].font_size = 11.0f;

    Entity mod_e = add_entity_full(WIDGET_RECT, x + 80.0f, 1170.0f, 220.0f, 50.0f, "GPU Pipeline Shader (shader.wgsl)\n(MSAA rasterization)", -1);
    set_shape_color(mod_e, 253.0f/255.0f, 244.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(mod_e, 245.0f/255.0f, 208.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(mod_e, tp_r, tp_g, tp_b);
    render_components[mod_e].font_size = 11.0f;

    add_connection_entity(mod_a, mod_b);
    add_connection_entity(mod_b, mod_c);
    add_connection_entity(mod_c, mod_d);
    add_connection_entity(mod_d, mod_e);

    // Background Panel for Architecture State Flow (Right)
    Entity flow_panel_bg = add_entity_full(WIDGET_RECT, x + 400.0f, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(flow_panel_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(flow_panel_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity flow_panel_label = add_entity_full(WIDGET_TEXT, x + 420.0f, 735.0f, 340.0f, 25.0f, "2. Architecture State Flow", -1);
    set_text_color(flow_panel_label, ta_r, ta_g, ta_b);
    render_components[flow_panel_label].font_size = 13.0f;

    Entity box_1 = add_entity_full(WIDGET_RECT, x + 480.0f, 770.0f, 220.0f, 50.0f, "Input Listener\n(captures mouse click/drags)", -1);
    set_shape_color(box_1, 254.0f/255.0f, 243.0f/255.0f, 243.0f/255.0f, 1.0f);
    set_shape_border(box_1, 252.0f/255.0f, 165.0f/255.0f, 165.0f/255.0f, 1.0f);
    set_text_color(box_1, tp_r, tp_g, tp_b);
    render_components[box_1].font_size = 11.0f;

    Entity box_2 = add_entity_full(WIDGET_RECT, x + 480.0f, 870.0f, 220.0f, 50.0f, "System Controller\n(runs transform & drag ticks)", -1);
    set_shape_color(box_2, 239.0f/255.0f, 246.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(box_2, 191.0f/255.0f, 219.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(box_2, tp_r, tp_g, tp_b);
    render_components[box_2].font_size = 11.0f;

    Entity box_3 = add_entity_full(WIDGET_RECT, x + 480.0f, 970.0f, 220.0f, 50.0f, "Component Database\n(ecs.c arrays structure data)", -1);
    set_shape_color(box_3, 245.0f/255.0f, 243.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(box_3, 221.0f/255.0f, 214.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(box_3, tp_r, tp_g, tp_b);
    render_components[box_3].font_size = 11.0f;

    Entity box_4 = add_entity_full(WIDGET_RECT, x + 480.0f, 1070.0f, 220.0f, 50.0f, "Dirty Flag Tick\n(marks whiteboard dirty)", -1);
    set_shape_color(box_4, 255.0f/255.0f, 251.0f/255.0f, 235.0f/255.0f, 1.0f);
    set_shape_border(box_4, 253.0f/255.0f, 230.0f/255.0f, 138.0f/255.0f, 1.0f);
    set_text_color(box_4, tp_r, tp_g, tp_b);
    render_components[box_4].font_size = 11.0f;

    Entity box_5 = add_entity_full(WIDGET_RECT, x + 480.0f, 1170.0f, 220.0f, 50.0f, "GPU Command Queue\n(screen repaints visually)", -1);
    set_shape_color(box_5, 240.0f/255.0f, 253.0f/255.0f, 250.0f/255.0f, 1.0f);
    set_shape_border(box_5, 153.0f/255.0f, 246.0f/255.0f, 228.0f/255.0f, 1.0f);
    set_text_color(box_5, tp_r, tp_g, tp_b);
    render_components[box_5].font_size = 11.0f;

    add_connection_entity(box_1, box_2);
    add_connection_entity(box_2, box_3);
    add_connection_entity(box_3, box_4);
    add_connection_entity(box_4, box_5);
  }

  // ==========================================
  // ENTITY LIFECYCLE & OBJECT ARCHITECTURE DIAGRAMS (Slide 3, x = 1900.0f)
  // ==========================================
  {
    float x = 100.0f + spacing * 2.0f; // 1900.0f

    // Section Title
    Entity sect_title = add_entity_full(WIDGET_TEXT, x, 650.0f, 700.0f, 40.0f, "ENTITY LIFECYCLE & OBJECT ARCHITECTURE", -1);
    set_text_color(sect_title, tp_r, tp_g, tp_b);
    render_components[sect_title].font_size = 20.0f;

    Entity sect_line = add_entity_full(WIDGET_RECT, x, 695.0f, 780.0f, 2.0f, "", -1);
    set_shape_color(sect_line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[sect_line].border_a = 0.0f;

    // Background Panel for Lifecycle Pipeline (Left)
    Entity life_bg = add_entity_full(WIDGET_RECT, x, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(life_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(life_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity life_label = add_entity_full(WIDGET_TEXT, x + 20.0f, 735.0f, 340.0f, 25.0f, "1. Entity Lifecycle Pipeline", -1);
    set_text_color(life_label, ta_r, ta_g, ta_b);
    render_components[life_label].font_size = 13.0f;

    // Lifecycle phases
    Entity phase_a = add_entity_full(WIDGET_RECT, x + 80.0f, 770.0f, 220.0f, 50.0f, "1. Allocation & ECS Bind\n(ecs_create_entity() grabs ID)", -1);
    set_shape_color(phase_a, 240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f, 1.0f);
    set_shape_border(phase_a, 187.0f/255.0f, 247.0f/255.0f, 208.0f/255.0f, 1.0f);
    set_text_color(phase_a, tp_r, tp_g, tp_b);
    render_components[phase_a].font_size = 11.0f;

    Entity phase_b = add_entity_full(WIDGET_RECT, x + 80.0f, 870.0f, 220.0f, 50.0f, "2. Component Initialization\n(adds TRANSFORM & RENDER)", -1);
    set_shape_color(phase_b, 245.0f/255.0f, 245.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(phase_b, 212.0f/255.0f, 212.0f/255.0f, 216.0f/255.0f, 1.0f);
    set_text_color(phase_b, tp_r, tp_g, tp_b);
    render_components[phase_b].font_size = 11.0f;

    Entity phase_c = add_entity_full(WIDGET_RECT, x + 80.0f, 970.0f, 220.0f, 50.0f, "3. Mutation & Styling\n(updates color/size settings)", -1);
    set_shape_color(phase_c, 255.0f/255.0f, 241.0f/255.0f, 242.0f/255.0f, 1.0f);
    set_shape_border(phase_c, 254.0f/255.0f, 205.0f/255.0f, 211.0f/255.0f, 1.0f);
    set_text_color(phase_c, tp_r, tp_g, tp_b);
    render_components[phase_c].font_size = 11.0f;

    Entity phase_d = add_entity_full(WIDGET_RECT, x + 80.0f, 1070.0f, 220.0f, 50.0f, "4. Active Interaction Ticks\n(dragging system triggers)", -1);
    set_shape_color(phase_d, 239.0f/255.0f, 246.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(phase_d, 191.0f/255.0f, 219.0f/255.0f, 254.0f/255.0f, 1.0f);
    set_text_color(phase_d, tp_r, tp_g, tp_b);
    render_components[phase_d].font_size = 11.0f;

    Entity phase_e = add_entity_full(WIDGET_RECT, x + 80.0f, 1170.0f, 220.0f, 50.0f, "5. Deallocation & Reuse\n(deletes entity & recycles ID)", -1);
    set_shape_color(phase_e, 254.0f/255.0f, 242.0f/255.0f, 242.0f/255.0f, 1.0f);
    set_shape_border(phase_e, 254.0f/255.0f, 202.0f/255.0f, 202.0f/255.0f, 1.0f);
    set_text_color(phase_e, tp_r, tp_g, tp_b);
    render_components[phase_e].font_size = 11.0f;

    add_connection_entity(phase_a, phase_b);
    add_connection_entity(phase_b, phase_c);
    add_connection_entity(phase_c, phase_d);
    add_connection_entity(phase_d, phase_e);

    // Background Panel for Component Relationship Map (Right)
    Entity map_bg = add_entity_full(WIDGET_RECT, x + 400.0f, 720.0f, 380.0f, 570.0f, "", -1);
    set_shape_color(map_bg, 250.0f/255.0f, 250.0f/255.0f, 250.0f/255.0f, 0.9f);
    set_shape_border(map_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity map_label = add_entity_full(WIDGET_TEXT, x + 420.0f, 735.0f, 340.0f, 25.0f, "2. Component Relationship Map", -1);
    set_text_color(map_label, ta_r, ta_g, ta_b);
    render_components[map_label].font_size = 13.0f;

    Entity entity_node = add_entity_full(WIDGET_RECT, x + 480.0f, 770.0f, 220.0f, 50.0f, "Selected Entity ID\n(numeric reference key)", -1);
    set_shape_color(entity_node, 245.0f/255.0f, 245.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(entity_node, 203.0f/255.0f, 213.0f/255.0f, 225.0f/255.0f, 1.0f);
    set_text_color(entity_node, tp_r, tp_g, tp_b);
    render_components[entity_node].font_size = 11.0f;

    Entity comp_trans = add_entity_full(WIDGET_RECT, x + 480.0f, 880.0f, 220.0f, 50.0f, "TransformComponent\n(stores: x, y, width, height)", -1);
    set_shape_color(comp_trans, 254.0f/255.0f, 249.0f/255.0f, 195.0f/255.0f, 1.0f);
    set_shape_border(comp_trans, 253.0f/255.0f, 224.0f/255.0f, 71.0f/255.0f, 1.0f);
    set_text_color(comp_trans, tp_r, tp_g, tp_b);
    render_components[comp_trans].font_size = 11.0f;

    Entity comp_rend = add_entity_full(WIDGET_RECT, x + 480.0f, 990.0f, 220.0f, 50.0f, "RenderComponent\n(stores: shape type & colors)", -1);
    set_shape_color(comp_rend, 236.0f/255.0f, 253.0f/255.0f, 250.0f/255.0f, 1.0f);
    set_shape_border(comp_rend, 153.0f/255.0f, 246.0f/255.0f, 228.0f/255.0f, 1.0f);
    set_text_color(comp_rend, tp_r, tp_g, tp_b);
    render_components[comp_rend].font_size = 11.0f;

    Entity comp_inter = add_entity_full(WIDGET_RECT, x + 480.0f, 1100.0f, 220.0f, 50.0f, "InteractionComponent\n(stores: selected & drag states)", -1);
    set_shape_color(comp_inter, 245.0f/255.0f, 243.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(comp_inter, 196.0f/255.0f, 181.0f/255.0f, 253.0f/255.0f, 1.0f);
    set_text_color(comp_inter, tp_r, tp_g, tp_b);
    render_components[comp_inter].font_size = 11.0f;

    add_connection_entity(entity_node, comp_trans);
    add_connection_entity(entity_node, comp_rend);
    add_connection_entity(entity_node, comp_inter);
  }
}

void generate_100k_infographics() {
  ecs_init();

  int cols = 50;
  int rows = 50;

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
