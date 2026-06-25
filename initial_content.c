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
  render_components[e].bg_r = r;
  render_components[e].bg_g = g;
  render_components[e].bg_b = b;
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

  Entity slides[10];

  static struct { float r, g, b; } slide_themes[10] = {
    {255.0f/255.0f, 240.0f/255.0f, 245.0f/255.0f}, // Lavender-Rose (Slide 1)
    {240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f}, // Sky Blue (Slide 2)
    {245.0f/255.0f, 255.0f/255.0f, 250.0f/255.0f}, // Mint Green (Slide 3)
    {255.0f/255.0f, 253.0f/255.0f, 245.0f/255.0f}, // Ivory-Cream (Slide 4)
    {250.0f/255.0f, 245.0f/255.0f, 255.0f/255.0f}, // Lavender-Grey (Slide 5)
    {255.0f/255.0f, 248.0f/255.0f, 240.0f/255.0f}, // Peach (Slide 6)
    {245.0f/255.0f, 250.0f/255.0f, 255.0f/255.0f}, // Light slate (Slide 7)
    {255.0f/255.0f, 252.0f/255.0f, 245.0f/255.0f}, // Warm Cream (Slide 8)
    {248.0f/255.0f, 249.0f/255.0f, 250.0f/255.0f}, // Silver-Grey (Slide 9)
    {245.0f/255.0f, 253.0f/255.0f, 253.0f/255.0f}  // soft teal (Slide 10)
  };

  static const char *titles[10] = {
    "Adopt, Don't Shop!",
    "The Shelter Reality",
    "Why Adoption is Best",
    "Dogs or Cats?",
    "Financial Comparison",
    "The Adoption Journey",
    "The Health Advantage",
    "Preparing Your Home",
    "From Shelter to Sofa",
    "Ready to Adopt?"
  };

  static const char *subtitles[10] = {
    "Find Your Furry Best Friend",
    "Admissions & Euthanasia Stats",
    "Key benefits of bringing home a shelter pet",
    "Choose the companion that matches your routine",
    "Initial costs: Adopting vs. Buying",
    "Three simple steps to welcoming your new pet",
    "Mixed breed pets have the genetic edge",
    "Gather these essentials before arrival",
    "The profound transformation of a rescued pet",
    "Your adoption journey starts right now"
  };

  // Typographic Colors
  float tp_r = 30.0f / 255.0f, tp_g = 41.0f / 255.0f, tp_b = 59.0f / 255.0f; // Slate Navy
  float ta_r = 79.0f / 255.0f, ta_g = 70.0f / 255.0f, ta_b = 229.0f / 255.0f; // Indigo
  float tm_r = 71.0f / 255.0f, tm_g = 85.0f / 255.0f, tm_b = 105.0f / 255.0f; // Slate Muted
  float tr_r = 239.0f / 255.0f, tr_g = 68.0f / 255.0f, tr_b = 68.0f / 255.0f; // Red/Alert
  float tg_r = 245.0f / 255.0f, tg_g = 158.0f / 255.0f, tg_b = 11.0f / 255.0f;  // Honey Gold

  // 1. Initialize slide backgrounds & brand layouts
  for (int i = 0; i < 10; i++) {
    float x = 100.0f + i * spacing;

    // Tactile Floating Shadow (with offset y+8, x+8)
    Entity shadow = add_entity_full(WIDGET_RECT, x + 8.0f, slide_y + 8.0f, slide_w, slide_h, "", -1);
    set_shape_color(shadow, 12.0f/255.0f, 15.0f/255.0f, 20.0f/255.0f, 0.22f);
    render_components[shadow].border_a = 0.0f;

    // Slide Body background card
    slides[i] = add_entity_full(WIDGET_RECT, x, slide_y, slide_w, slide_h, "", -1);
    set_shape_color(slides[i], slide_themes[i].r, slide_themes[i].g, slide_themes[i].b, 1.0f);
    set_shape_border(slides[i], 203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f); // Slate-200

    // Top Stripe accent (Coral Pink or Honey Gold)
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

    // Slide Index Label (e.g. "01", "02" ...) inside Sidebar
    char idx_str[4];
    idx_str[0] = '0' + ((i + 1) / 10);
    idx_str[1] = '0' + ((i + 1) % 10);
    idx_str[2] = '\0';
    Entity idx_txt = add_entity_full(WIDGET_TEXT, x + 12.0f, slide_y + 30.0f, 36.0f, 32.0f, idx_str, -1);
    set_text_color(idx_txt, 1.0f, 1.0f, 1.0f);
    render_components[idx_txt].font_size = 20.0f;

    // Small decorative paw under index inside Sidebar
    Entity side_paw = add_entity_full(WIDGET_TEXT, x + 18.0f, slide_y + 65.0f, 24.0f, 20.0f, "🐾", -1);
    set_text_color(side_paw, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f);
    render_components[side_paw].font_size = 14.0f;

    // Cozy Paws logo badge on the right side of header
    Entity logo_bg = add_entity_full(WIDGET_RECT, x + slide_w - 170.0f, slide_y + 20.0f, 130.0f, 26.0f, "", -1);
    set_shape_color(logo_bg, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.9f);
    set_shape_border(logo_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    Entity logo_txt = add_entity_full(WIDGET_TEXT, x + slide_w - 165.0f, slide_y + 24.0f, 120.0f, 22.0f, "🐾 COZY PAWS", -1);
    set_text_color(logo_txt, ta_r, ta_g, ta_b);
    render_components[logo_txt].font_size = 11.0f;

    // Header Title (Shifted right by 80px to clear the sidebar)
    Entity t_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, slide_y + 20.0f, 530.0f, 32.0f, titles[i], -1);
    set_text_color(t_ent, tp_r, tp_g, tp_b);
    render_components[t_ent].font_size = 22.0f;

    // Horizontal Divider (Shifted right to clear sidebar)
    Entity line = add_entity_full(WIDGET_RECT, x + 80.0f, slide_y + 55.0f, slide_w - 120.0f, 1.5f, "", -1);
    set_shape_color(line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[line].border_a = 0.0f;

    // Header Subtitle (Shifted right by 80px to clear the sidebar)
    Entity sub_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, slide_y + 68.0f, slide_w - 120.0f, 25.0f, subtitles[i], -1);
    set_text_color(sub_ent, i % 2 == 0 ? ta_r : tg_r, i % 2 == 0 ? ta_g : tg_g, i % 2 == 0 ? ta_b : tg_b);
    render_components[sub_ent].font_size = 13.0f;
  }

  // Connect slides sequentially with arrows
  for (int i = 0; i < 9; i++) {
    Entity arr = add_connection_entity(slides[i], slides[i + 1]);
    set_shape_color(arr, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 1.0f);
  }

  // ==========================================
  // SLIDE 1: Title Slide (Welcome)
  // ==========================================
  {
    float x = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 150.0f, 380.0f, 30.0f, "Millions of shelter animals are waiting for a home.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    render_components[d1].font_size = 14.0f;

    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 190.0f, 380.0f, 30.0f, "Adopting saves a life and brings unconditional love.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    render_components[d2].font_size = 14.0f;

    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, slide_y + 230.0f, 380.0f, 30.0f, "Are you ready to meet your new family member?", -1);
    set_text_color(d3, ta_r, ta_g, ta_b);
    render_components[d3].font_size = 14.0f;

    // Pet House roof (triangle)
    Entity house_roof = add_entity_full(WIDGET_TRIANGLE, x + 530.0f, slide_y + 150.0f, 180.0f, 70.0f, "", -1);
    set_shape_color(house_roof, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f, 1.0f); // Coral
    render_components[house_roof].border_a = 0.0f;

    // Chimney
    Entity chimney = add_entity_full(WIDGET_RECT, x + 645.0f, slide_y + 130.0f, 25.0f, 60.0f, "", -1);
    set_shape_color(chimney, tm_r, tm_g, tm_b, 1.0f);
    render_components[chimney].border_a = 0.0f;

    // Chimney smoke circles
    Entity smoke1 = add_entity_full(WIDGET_OVAL, x + 648.0f, slide_y + 105.0f, 15.0f, 15.0f, "", -1);
    set_shape_color(smoke1, tm_r, tm_g, tm_b, 0.4f);
    render_components[smoke1].border_a = 0.0f;
    Entity smoke2 = add_entity_full(WIDGET_OVAL, x + 655.0f, slide_y + 85.0f, 22.0f, 22.0f, "", -1);
    set_shape_color(smoke2, tm_r, tm_g, tm_b, 0.2f);
    render_components[smoke2].border_a = 0.0f;

    // Pet House body (rect)
    Entity house_body = add_entity_full(WIDGET_RECT, x + 545.0f, slide_y + 220.0f, 150.0f, 130.0f, "", -1);
    set_shape_color(house_body, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(house_body, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    // Door (oval arch-like or rect)
    Entity house_door = add_entity_full(WIDGET_OVAL, x + 595.0f, slide_y + 280.0f, 50.0f, 70.0f, "", -1);
    set_shape_color(house_door, tp_r, tp_g, tp_b, 1.0f);
    render_components[house_door].border_a = 0.0f;

    // Small yellow window inside roof
    Entity house_win = add_entity_full(WIDGET_OVAL, x + 605.0f, slide_y + 180.0f, 30.0f, 30.0f, "", -1);
    set_shape_color(house_win, tg_r, tg_g, tg_b, 1.0f);
    render_components[house_win].border_a = 0.0f;

    // Floating hearts
    Entity h1 = add_entity_full(WIDGET_OVAL, x + 490.0f, slide_y + 130.0f, 15.0f, 15.0f, "", -1);
    set_shape_color(h1, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f, 0.9f);
    render_components[h1].border_a = 0.0f;
    Entity h2 = add_entity_full(WIDGET_OVAL, x + 500.0f, slide_y + 120.0f, 10.0f, 10.0f, "", -1);
    set_shape_color(h2, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f, 0.7f);
    render_components[h2].border_a = 0.0f;

    // Welcome Cozy Paws sign
    Entity sign = add_entity_full(WIDGET_RECT, x + 90.0f, slide_y + 300.0f, 320.0f, 60.0f, "", -1);
    set_shape_color(sign, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 0.9f);
    set_shape_border(sign, ta_r, ta_g, ta_b, 1.0f);

    Entity sign_t = add_entity_full(WIDGET_TEXT, x + 110.0f, slide_y + 318.0f, 280.0f, 25.0f, "🐾 Join over 10,000+ happy homes!", -1);
    set_text_color(sign_t, ta_r, ta_g, ta_b);
    render_components[sign_t].font_size = 14.0f;
  }

  // ==========================================
  // SLIDE 2: Shelter Statistics (The Need)
  // ==========================================
  {
    float x = 100.0f + spacing;
    // Left Half: Stats Bars
    Entity dog_lbl = add_entity_full(WIDGET_TEXT, x + 85.0f, slide_y + 140.0f, 140.0f, 25.0f, "Dogs: 3.1M", -1);
    set_text_color(dog_lbl, tp_r, tp_g, tp_b);
    render_components[dog_lbl].font_size = 13.0f;

    Entity dog_bar_bg = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 142.0f, 200.0f, 16.0f, "", -1);
    set_shape_color(dog_bar_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[dog_bar_bg].border_a = 0.0f;

    Entity dog_bar_val = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 142.0f, 155.0f, 16.0f, "", -1);
    set_shape_color(dog_bar_val, 135.0f/255.0f, 206.0f/255.0f, 250.0f/255.0f, 1.0f); // Sky Blue
    render_components[dog_bar_val].border_a = 0.0f;

    Entity cat_lbl = add_entity_full(WIDGET_TEXT, x + 85.0f, slide_y + 190.0f, 140.0f, 25.0f, "Cats: 3.2M", -1);
    set_text_color(cat_lbl, tp_r, tp_g, tp_b);
    render_components[cat_lbl].font_size = 13.0f;

    Entity cat_bar_bg = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 192.0f, 200.0f, 16.0f, "", -1);
    set_shape_color(cat_bar_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[cat_bar_bg].border_a = 0.0f;

    Entity cat_bar_val = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 192.0f, 160.0f, 16.0f, "", -1);
    set_shape_color(cat_bar_val, 255.0f/255.0f, 182.0f/255.0f, 193.0f/255.0f, 1.0f); // Light Pink
    render_components[cat_bar_val].border_a = 0.0f;

    Entity adopt_lbl = add_entity_full(WIDGET_TEXT, x + 85.0f, slide_y + 240.0f, 140.0f, 25.0f, "Adopted: 4.1M", -1);
    set_text_color(adopt_lbl, tp_r, tp_g, tp_b);
    render_components[adopt_lbl].font_size = 13.0f;

    Entity adopt_bar_bg = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 242.0f, 200.0f, 16.0f, "", -1);
    set_shape_color(adopt_bar_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[adopt_bar_bg].border_a = 0.0f;

    Entity adopt_bar_val = add_entity_full(WIDGET_RECT, x + 230.0f, slide_y + 242.0f, 200.0f, 16.0f, "", -1);
    set_shape_color(adopt_bar_val, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); // Mint Green
    render_components[adopt_bar_val].border_a = 0.0f;

    // Right Half: The Path to Rescue Split Screen
    Entity path_lbl = add_entity_full(WIDGET_TEXT, x + 460.0f, slide_y + 120.0f, 280.0f, 25.0f, "THE RESCUE PIPELINE", -1);
    set_text_color(path_lbl, ta_r, ta_g, ta_b);
    render_components[path_lbl].font_size = 13.0f;

    // Shelter box
    Entity sh_box = add_entity_full(WIDGET_RECT, x + 460.0f, slide_y + 155.0f, 110.0f, 110.0f, "", -1);
    set_shape_color(sh_box, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    set_shape_border(sh_box, 148.0f/255.0f, 163.0f/255.0f, 184.0f/255.0f, 1.0f);

    // Bars representing cage
    for (int b = 0; b < 3; b++) {
      Entity bar = add_entity_full(WIDGET_RECT, x + 482.0f + b * 22.0f, slide_y + 155.0f, 3.0f, 110.0f, "", -1);
      set_shape_color(bar, 148.0f/255.0f, 163.0f/255.0f, 184.0f/255.0f, 1.0f);
      render_components[bar].border_a = 0.0f;
    }

    // Gloomy cloud above shelter
    Entity cl1 = add_entity_full(WIDGET_OVAL, x + 480.0f, slide_y + 140.0f, 35.0f, 20.0f, "", -1);
    set_shape_color(cl1, tm_r, tm_g, tm_b, 0.4f);
    render_components[cl1].border_a = 0.0f;
    Entity cl2 = add_entity_full(WIDGET_OVAL, x + 500.0f, slide_y + 135.0f, 40.0f, 25.0f, "", -1);
    set_shape_color(cl2, tm_r, tm_g, tm_b, 0.4f);
    render_components[cl2].border_a = 0.0f;

    // Home box
    Entity hm_box = add_entity_full(WIDGET_RECT, x + 630.0f, slide_y + 155.0f, 110.0f, 110.0f, "", -1);
    set_shape_color(hm_box, 255.0f/255.0f, 253.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(hm_box, tg_r, tg_g, tg_b, 1.0f);

    // Window in home
    Entity hm_win = add_entity_full(WIDGET_RECT, x + 670.0f, slide_y + 180.0f, 30.0f, 40.0f, "", -1);
    set_shape_color(hm_win, 1.0f, 1.0f, 1.0f, 1.0f);
    set_shape_border(hm_win, tg_r, tg_g, tg_b, 1.0f);

    // Sun above home
    Entity sun_disc = add_entity_full(WIDGET_OVAL, x + 710.0f, slide_y + 125.0f, 35.0f, 35.0f, "", -1);
    set_shape_color(sun_disc, 245.0f/255.0f, 158.0f/255.0f, 11.0f/255.0f, 1.0f);
    render_components[sun_disc].border_a = 0.0f;

    // Connection arrow
    Entity arr = add_connection_entity(sh_box, hm_box);
    set_shape_color(arr, ta_r, ta_g, ta_b, 1.0f);

    // Arrow badge label
    Entity arr_lbl = add_entity_full(WIDGET_TEXT, x + 580.0f, slide_y + 200.0f, 45.0f, 20.0f, "RESCUE", -1);
    set_text_color(arr_lbl, tr_r, tr_g, tr_b);
    render_components[arr_lbl].font_size = 10.0f;

    // Grid details
    for (int g = 1; g <= 4; g++) {
      float gx = x + 230.0f + g * 50.0f;
      Entity grid = add_entity_full(WIDGET_RECT, gx, slide_y + 140.0f, 1.0f, 130.0f, "", -1);
      set_shape_color(grid, 203.0f/255.0f, 213.0f/255.0f, 225.0f/255.0f, 0.4f);
      render_components[grid].border_a = 0.0f;
    }

    // Summary text
    Entity sum1 = add_entity_full(WIDGET_TEXT, x + 85.0f, slide_y + 325.0f, 650.0f, 30.0f, "Roughly 920,000 shelter animals are euthanized each year.", -1);
    set_text_color(sum1, tr_r, tr_g, tr_b);
    render_components[sum1].font_size = 14.0f;

    Entity sum2 = add_entity_full(WIDGET_TEXT, x + 85.0f, slide_y + 365.0f, 650.0f, 30.0f, "Adoption rescues these pets and opens up shelter spaces.", -1);
    set_text_color(sum2, tm_r, tm_g, tm_b);
    render_components[sum2].font_size = 14.0f;
  }

  // ==========================================
  // SLIDE 3: Benefits of Adoption (2x2 Grid)
  // ==========================================
  {
    float x = 100.0f + spacing * 2.0f;
    float gx[2] = {x + 85.0f, x + 425.0f};
    float gy[2] = {slide_y + 140.0f, slide_y + 295.0f};
    const char *card_titles[4] = {"1. Save a Life", "2. Fully Vetted", "3. House-Trained", "4. Cost Effective"};
    const char *desc[4] = {
      "Rescue a pet from euthanasia and provide a second chance.",
      "Spaying, neutering, and microchipping are done.",
      "Many shelter pets are adults who are housebroken.",
      "Fees are far lower than purchasing from breeders."
    };

    for (int k = 0; k < 4; k++) {
      int ix = k % 2;
      int iy = k / 2;
      Entity card = add_entity_full(WIDGET_RECT, gx[ix], gy[iy], 310.0f, 125.0f, "", -1);
      set_shape_color(card, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
      set_shape_border(card, 187.0f/255.0f, 247.0f/255.0f, 208.0f/255.0f, 1.0f);

      // Icons based on card
      if (k == 0) { // Life ring
        Entity icon_o = add_entity_full(WIDGET_OVAL, gx[ix] + 15.0f, gy[iy] + 25.0f, 45.0f, 45.0f, "", -1);
        set_shape_color(icon_o, tr_r, tr_g, tr_b, 1.0f);
        render_components[icon_o].border_a = 0.0f;

        Entity icon_i = add_entity_full(WIDGET_OVAL, gx[ix] + 25.0f, gy[iy] + 35.0f, 25.0f, 25.0f, "", -1);
        set_shape_color(icon_i, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
        render_components[icon_i].border_a = 0.0f;
      } else if (k == 1) { // Med Cross
        Entity icon_o = add_entity_full(WIDGET_OVAL, gx[ix] + 15.0f, gy[iy] + 25.0f, 45.0f, 45.0f, "", -1);
        set_shape_color(icon_o, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); // Green
        render_components[icon_o].border_a = 0.0f;

        Entity icon_v = add_entity_full(WIDGET_RECT, gx[ix] + 34.0f, gy[iy] + 33.0f, 7.0f, 29.0f, "", -1);
        set_shape_color(icon_v, 1.0f, 1.0f, 1.0f, 1.0f);
        render_components[icon_v].border_a = 0.0f;

        Entity icon_h = add_entity_full(WIDGET_RECT, gx[ix] + 23.0f, gy[iy] + 44.0f, 29.0f, 7.0f, "", -1);
        set_shape_color(icon_h, 1.0f, 1.0f, 1.0f, 1.0f);
        render_components[icon_h].border_a = 0.0f;
      } else if (k == 2) { // Little dog house
        Entity icon_o = add_entity_full(WIDGET_OVAL, gx[ix] + 15.0f, gy[iy] + 25.0f, 45.0f, 45.0f, "", -1);
        set_shape_color(icon_o, ta_r, ta_g, ta_b, 1.0f); // Indigo
        render_components[icon_o].border_a = 0.0f;

        Entity roof = add_entity_full(WIDGET_TRIANGLE, gx[ix] + 23.0f, gy[iy] + 32.0f, 29.0f, 15.0f, "", -1);
        set_shape_color(roof, 1.0f, 1.0f, 1.0f, 1.0f);
        render_components[roof].border_a = 0.0f;

        Entity body = add_entity_full(WIDGET_RECT, gx[ix] + 27.0f, gy[iy] + 47.0f, 21.0f, 15.0f, "", -1);
        set_shape_color(body, 1.0f, 1.0f, 1.0f, 1.0f);
        render_components[body].border_a = 0.0f;
      } else { // Gold coin
        Entity icon_o = add_entity_full(WIDGET_OVAL, gx[ix] + 15.0f, gy[iy] + 25.0f, 45.0f, 45.0f, "", -1);
        set_shape_color(icon_o, tg_r, tg_g, tg_b, 1.0f); // Gold
        render_components[icon_o].border_a = 0.0f;

        Entity coin_t = add_entity_full(WIDGET_TEXT, gx[ix] + 32.0f, gy[iy] + 36.0f, 20.0f, 20.0f, "$", -1);
        set_text_color(coin_t, 1.0f, 1.0f, 1.0f);
        render_components[coin_t].font_size = 14.0f;
      }

      Entity c_title = add_entity_full(WIDGET_TEXT, gx[ix] + 75.0f, gy[iy] + 15.0f, 220.0f, 25.0f, card_titles[k], -1);
      set_text_color(c_title, ta_r, ta_g, ta_b);
      render_components[c_title].font_size = 14.0f;

      Entity c_desc = add_entity_full(WIDGET_TEXT, gx[ix] + 75.0f, gy[iy] + 45.0f, 220.0f, 70.0f, desc[k], -1);
      set_text_color(c_desc, tm_r, tm_g, tm_b);
      render_components[c_desc].font_size = 12.0f;
    }
  }

  // ==========================================
  // SLIDE 4: Dogs or Cats? (Split screen / Face Portholes)
  // ==========================================
  {
    float x = 100.0f + spacing * 3.0f;
    // Dog Card
    Entity dog_card = add_entity_full(WIDGET_RECT, x + 85.0f, slide_y + 140.0f, 310.0f, 310.0f, "", -1);
    set_shape_color(dog_card, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(dog_card, 135.0f/255.0f, 206.0f/255.0f, 250.0f/255.0f, 1.0f);

    Entity d_title = add_entity_full(WIDGET_TEXT, x + 105.0f, slide_y + 155.0f, 270.0f, 28.0f, "Dogs (Active & Loyal)", -1);
    set_text_color(d_title, ta_r, ta_g, ta_b);
    render_components[d_title].font_size = 15.0f;

    Entity d_det1 = add_entity_full(WIDGET_TEXT, x + 105.0f, slide_y + 190.0f, 270.0f, 25.0f, "• Thrive on social interaction", -1);
    set_text_color(d_det1, tm_r, tm_g, tm_b);
    render_components[d_det1].font_size = 13.0f;

    Entity d_det2 = add_entity_full(WIDGET_TEXT, x + 105.0f, slide_y + 215.0f, 270.0f, 25.0f, "• Require daily walks & play", -1);
    set_text_color(d_det2, tm_r, tm_g, tm_b);
    render_components[d_det2].font_size = 13.0f;

    // Dog Porthole
    float bx = x + 130.0f;
    float by = slide_y + 310.0f;
    Entity d_port = add_entity_full(WIDGET_OVAL, bx + 10.0f, by - 40.0f, 150.0f, 130.0f, "", -1);
    set_shape_color(d_port, 240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f, 1.0f); // Alice Blue
    set_shape_border(d_port, 135.0f/255.0f, 206.0f/255.0f, 250.0f/255.0f, 1.0f);

    // Left ear (floppy)
    Entity d_ear1 = add_entity_full(WIDGET_OVAL, bx - 5.0f, by - 15.0f, 35.0f, 75.0f, "", -1);
    set_shape_color(d_ear1, 100.0f/255.0f, 149.0f/255.0f, 237.0f/255.0f, 1.0f);
    render_components[d_ear1].border_a = 0.0f;

    // Right ear (floppy)
    Entity d_ear2 = add_entity_full(WIDGET_OVAL, bx + 135.0f, by - 15.0f, 35.0f, 75.0f, "", -1);
    set_shape_color(d_ear2, 100.0f/255.0f, 149.0f/255.0f, 237.0f/255.0f, 1.0f);
    render_components[d_ear2].border_a = 0.0f;

    // Head
    Entity d_head = add_entity_full(WIDGET_OVAL, bx + 15.0f, by - 10.0f, 140.0f, 90.0f, "", -1);
    set_shape_color(d_head, 135.0f/255.0f, 206.0f/255.0f, 250.0f/255.0f, 1.0f);
    render_components[d_head].border_a = 0.0f;

    // Eyes
    Entity d_eye1 = add_entity_full(WIDGET_OVAL, bx + 45.0f, by + 15.0f, 12.0f, 16.0f, "", -1);
    set_shape_color(d_eye1, tp_r, tp_g, tp_b, 1.0f);
    render_components[d_eye1].border_a = 0.0f;

    Entity d_eye2 = add_entity_full(WIDGET_OVAL, bx + 105.0f, by + 15.0f, 12.0f, 16.0f, "", -1);
    set_shape_color(d_eye2, tp_r, tp_g, tp_b, 1.0f);
    render_components[d_eye2].border_a = 0.0f;

    // Snout
    Entity d_snout = add_entity_full(WIDGET_OVAL, bx + 55.0f, by + 30.0f, 60.0f, 40.0f, "", -1);
    set_shape_color(d_snout, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    render_components[d_snout].border_a = 0.0f;

    // Nose
    Entity d_nose = add_entity_full(WIDGET_OVAL, bx + 73.0f, by + 33.0f, 24.0f, 16.0f, "", -1);
    set_shape_color(d_nose, 30.0f/255.0f, 41.0f/255.0f, 59.0f/255.0f, 1.0f);
    render_components[d_nose].border_a = 0.0f;

    // Cat Card
    Entity cat_card = add_entity_full(WIDGET_RECT, x + 425.0f, slide_y + 140.0f, 310.0f, 310.0f, "", -1);
    set_shape_color(cat_card, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(cat_card, 255.0f/255.0f, 182.0f/255.0f, 193.0f/255.0f, 1.0f);

    Entity c_title = add_entity_full(WIDGET_TEXT, x + 445.0f, slide_y + 155.0f, 270.0f, 28.0f, "Cats (Independent & Quiet)", -1);
    set_text_color(c_title, ta_r, ta_g, ta_b);
    render_components[c_title].font_size = 15.0f;

    Entity c_det1 = add_entity_full(WIDGET_TEXT, x + 445.0f, slide_y + 190.0f, 270.0f, 25.0f, "• Highly self-sufficient", -1);
    set_text_color(c_det1, tm_r, tm_g, tm_b);
    render_components[c_det1].font_size = 13.0f;

    Entity c_det2 = add_entity_full(WIDGET_TEXT, x + 445.0f, slide_y + 215.0f, 270.0f, 25.0f, "• Perfect for cozy apartments", -1);
    set_text_color(c_det2, tm_r, tm_g, tm_b);
    render_components[c_det2].font_size = 13.0f;

    // Cat Porthole
    float cx = x + 470.0f;
    float cy = slide_y + 310.0f;
    Entity c_port = add_entity_full(WIDGET_OVAL, cx + 10.0f, cy - 40.0f, 150.0f, 130.0f, "", -1);
    set_shape_color(c_port, 255.0f/255.0f, 240.0f/255.0f, 245.0f/255.0f, 1.0f); // Lavender Blush
    set_shape_border(c_port, 255.0f/255.0f, 182.0f/255.0f, 193.0f/255.0f, 1.0f);

    // Outer ears (triangles)
    Entity c_ear1 = add_entity_full(WIDGET_TRIANGLE, cx + 15.0f, cy - 25.0f, 40.0f, 40.0f, "", -1);
    set_shape_color(c_ear1, 244.0f/255.0f, 143.0f/255.0f, 177.0f/255.0f, 1.0f);
    render_components[c_ear1].border_a = 0.0f;

    Entity c_ear2 = add_entity_full(WIDGET_TRIANGLE, cx + 115.0f, cy - 25.0f, 40.0f, 40.0f, "", -1);
    set_shape_color(c_ear2, 244.0f/255.0f, 143.0f/255.0f, 177.0f/255.0f, 1.0f);
    render_components[c_ear2].border_a = 0.0f;

    // Inner ears
    Entity c_iear1 = add_entity_full(WIDGET_TRIANGLE, cx + 22.0f, cy - 15.0f, 25.0f, 25.0f, "", -1);
    set_shape_color(c_iear1, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    render_components[c_iear1].border_a = 0.0f;

    Entity c_iear2 = add_entity_full(WIDGET_TRIANGLE, cx + 122.0f, cy - 15.0f, 25.0f, 25.0f, "", -1);
    set_shape_color(c_iear2, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    render_components[c_iear2].border_a = 0.0f;

    // Head
    Entity c_head = add_entity_full(WIDGET_OVAL, cx + 5.0f, cy - 10.0f, 160.0f, 95.0f, "", -1);
    set_shape_color(c_head, 255.0f/255.0f, 182.0f/255.0f, 193.0f/255.0f, 1.0f);
    render_components[c_head].border_a = 0.0f;

    // Eyes
    Entity c_eye1 = add_entity_full(WIDGET_OVAL, cx + 38.0f, cy + 18.0f, 11.0f, 15.0f, "", -1);
    set_shape_color(c_eye1, tp_r, tp_g, tp_b, 1.0f);
    render_components[c_eye1].border_a = 0.0f;

    Entity c_eye2 = add_entity_full(WIDGET_OVAL, cx + 118.0f, cy + 18.0f, 11.0f, 15.0f, "", -1);
    set_shape_color(c_eye2, tp_r, tp_g, tp_b, 1.0f);
    render_components[c_eye2].border_a = 0.0f;

    // Nose
    Entity c_nose = add_entity_full(WIDGET_OVAL, cx + 78.0f, cy + 34.0f, 14.0f, 10.0f, "", -1);
    set_shape_color(c_nose, 244.0f/255.0f, 143.0f/255.0f, 177.0f/255.0f, 1.0f);
    render_components[c_nose].border_a = 0.0f;

    // Whiskers
    Entity w1 = add_entity_full(WIDGET_RECT, cx - 10.0f, cy + 35.0f, 25.0f, 1.5f, "", -1);
    set_shape_color(w1, tm_r, tm_g, tm_b, 0.6f); render_components[w1].border_a = 0.0f;
    Entity w2 = add_entity_full(WIDGET_RECT, cx - 10.0f, cy + 42.0f, 25.0f, 1.5f, "", -1);
    set_shape_color(w2, tm_r, tm_g, tm_b, 0.6f); render_components[w2].border_a = 0.0f;
    Entity w3 = add_entity_full(WIDGET_RECT, cx + 155.0f, cy + 35.0f, 25.0f, 1.5f, "", -1);
    set_shape_color(w3, tm_r, tm_g, tm_b, 0.6f); render_components[w3].border_a = 0.0f;
    Entity w4 = add_entity_full(WIDGET_RECT, cx + 155.0f, cy + 42.0f, 25.0f, 1.5f, "", -1);
    set_shape_color(w4, tm_r, tm_g, tm_b, 0.6f); render_components[w4].border_a = 0.0f;

    // VS Badge
    Entity vs_bg = add_entity_full(WIDGET_OVAL, x + 385.0f, slide_y + 240.0f, 50.0f, 50.0f, "", -1);
    set_shape_color(vs_bg, tp_r, tp_g, tp_b, 1.0f);
    set_shape_border(vs_bg, 1.0f, 1.0f, 1.0f, 1.0f);

    Entity vs_txt = add_entity_full(WIDGET_TEXT, x + 397.0f, slide_y + 253.0f, 30.0f, 25.0f, "VS", -1);
    set_text_color(vs_txt, 1.0f, 1.0f, 1.0f);
    render_components[vs_txt].font_size = 14.0f;
  }

  // ==========================================
  // SLIDE 5: Cost Comparison
  // ==========================================
  {
    float x = 100.0f + spacing * 4.0f;
    // Staggered heights to represent cost
    // Adopting a Pet card (lower height represents lower cost, positioned lower down)
    Entity c1 = add_entity_full(WIDGET_RECT, x + 95.0f, slide_y + 180.0f, 270.0f, 270.0f, "", -1);
    set_shape_color(c1, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(c1, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); // Green

    Entity t1_bg = add_entity_full(WIDGET_RECT, x + 95.0f, slide_y + 180.0f, 270.0f, 40.0f, "", -1);
    set_shape_color(t1_bg, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f);
    render_components[t1_bg].border_a = 0.0f;

    Entity t1 = add_entity_full(WIDGET_TEXT, x + 115.0f, slide_y + 188.0f, 230.0f, 24.0f, "Adopting a Pet", -1);
    set_text_color(t1, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f);
    render_components[t1].font_size = 14.0f;

    Entity p1 = add_entity_full(WIDGET_TEXT, x + 115.0f, slide_y + 235.0f, 230.0f, 32.0f, "$150 - $350 Total", -1);
    set_text_color(p1, tp_r, tp_g, tp_b);
    render_components[p1].font_size = 20.0f;

    Entity div1 = add_entity_full(WIDGET_RECT, x + 115.0f, slide_y + 275.0f, 230.0f, 1.5f, "", -1);
    set_shape_color(div1, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[div1].border_a = 0.0f;

    Entity i1_1 = add_entity_full(WIDGET_TEXT, x + 115.0f, slide_y + 295.0f, 230.0f, 22.0f, "✓ Spay/Neuter (Included)", -1);
    set_text_color(i1_1, tm_r, tm_g, tm_b); render_components[i1_1].font_size = 12.0f;
    Entity i1_2 = add_entity_full(WIDGET_TEXT, x + 115.0f, slide_y + 325.0f, 230.0f, 22.0f, "✓ Core Vaccines (Included)", -1);
    set_text_color(i1_2, tm_r, tm_g, tm_b); render_components[i1_2].font_size = 12.0f;
    Entity i1_3 = add_entity_full(WIDGET_TEXT, x + 115.0f, slide_y + 355.0f, 230.0f, 22.0f, "✓ Microchip ID (Included)", -1);
    set_text_color(i1_3, tm_r, tm_g, tm_b); render_components[i1_3].font_size = 12.0f;

    // Breeder card (taller height represents towering cost, positioned higher up)
    Entity c2 = add_entity_full(WIDGET_RECT, x + 445.0f, slide_y + 120.0f, 270.0f, 330.0f, "", -1);
    set_shape_color(c2, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(c2, 248.0f/255.0f, 113.0f/255.0f, 113.0f/255.0f, 1.0f); // Red

    Entity t2_bg = add_entity_full(WIDGET_RECT, x + 445.0f, slide_y + 120.0f, 270.0f, 40.0f, "", -1);
    set_shape_color(t2_bg, 248.0f/255.0f, 113.0f/255.0f, 113.0f/255.0f, 1.0f);
    render_components[t2_bg].border_a = 0.0f;

    Entity t2 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 128.0f, 230.0f, 24.0f, "Buying from Breeder", -1);
    set_text_color(t2, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f);
    render_components[t2].font_size = 14.0f;

    Entity p2 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 175.0f, 230.0f, 32.0f, "$1,500 - $4,000+", -1);
    set_text_color(p2, tr_r, tr_g, tr_b);
    render_components[p2].font_size = 20.0f;

    Entity div2 = add_entity_full(WIDGET_RECT, x + 465.0f, slide_y + 215.0f, 230.0f, 1.5f, "", -1);
    set_shape_color(div2, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[div2].border_a = 0.0f;

    Entity i2_1 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 235.0f, 230.0f, 22.0f, "✗ Purchase Price: $1500+", -1);
    set_text_color(i2_1, tm_r, tm_g, tm_b); render_components[i2_1].font_size = 12.0f;
    Entity i2_2 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 265.0f, 230.0f, 22.0f, "✗ Spay/Neuter: +$200-$400", -1);
    set_text_color(i2_2, tm_r, tm_g, tm_b); render_components[i2_2].font_size = 12.0f;
    Entity i2_3 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 295.0f, 230.0f, 22.0f, "✗ First Vaccines: +$100", -1);
    set_text_color(i2_3, tm_r, tm_g, tm_b); render_components[i2_3].font_size = 12.0f;
    Entity i2_4 = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 325.0f, 230.0f, 22.0f, "✗ Microchip: +$50", -1);
    set_text_color(i2_4, tm_r, tm_g, tm_b); render_components[i2_4].font_size = 12.0f;
  }

  // ==========================================
  // SLIDE 6: Adoption steps / flow
  // ==========================================
  {
    float x = 100.0f + spacing * 5.0f;
    float circle_x[3] = {x + 120.0f, x + 370.0f, x + 620.0f};
    const char *labels[3] = {"01", "02", "03"};
    const char *step_names[3] = {"1. Choose", "2. Meet", "3. Home!"};
    const char *details[3] = {
      "Find local pets online or\nvisit a nearby shelter.",
      "Interact to ensure matching\ntemperament & energy.",
      "Complete adoption forms\n& prepare their space."
    };

    // Connecting Track (background pipeline path)
    Entity track_bg = add_entity_full(WIDGET_RECT, x + 120.0f, slide_y + 215.0f, 500.0f, 8.0f, "", -1);
    set_shape_color(track_bg, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[track_bg].border_a = 0.0f;

    // Progress bar overlay (completed up to step 2)
    Entity track_fg = add_entity_full(WIDGET_RECT, x + 120.0f, slide_y + 215.0f, 250.0f, 8.0f, "", -1);
    set_shape_color(track_fg, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 1.0f); // Indigo
    render_components[track_fg].border_a = 0.0f;

    // Paw prints along path
    Entity step_paw1 = add_entity_full(WIDGET_TEXT, x + 230.0f, slide_y + 208.0f, 20.0f, 20.0f, "🐾", -1);
    set_text_color(step_paw1, ta_r, ta_g, ta_b); render_components[step_paw1].font_size = 11.0f;
    Entity step_paw2 = add_entity_full(WIDGET_TEXT, x + 480.0f, slide_y + 208.0f, 20.0f, 20.0f, "🐾", -1);
    set_text_color(step_paw2, tm_r, tm_g, tm_b); render_components[step_paw2].font_size = 11.0f;

    for (int k = 0; k < 3; k++) {
      // Outer Circle
      Entity outer = add_entity_full(WIDGET_OVAL, circle_x[k], slide_y + 175.0f, 88.0f, 88.0f, "", -1);
      set_shape_color(outer, 94.0f / 255.0f, 106.0f / 255.0f, 210.0f / 255.0f, 1.0f);
      render_components[outer].border_a = 0.0f;

      // Inner Circle (to make it a ring)
      Entity inner = add_entity_full(WIDGET_OVAL, circle_x[k] + 6.0f, slide_y + 181.0f, 76.0f, 76.0f, "", -1);
      set_shape_color(inner, slide_themes[5].r, slide_themes[5].g, slide_themes[5].b, 1.0f);
      render_components[inner].border_a = 0.0f;

      // Text content inside ring
      Entity lbl = add_entity_full(WIDGET_TEXT, circle_x[k] + 25.0f, slide_y + 203.0f, 40.0f, 28.0f, labels[k], -1);
      set_text_color(lbl, ta_r, ta_g, ta_b);
      render_components[lbl].font_size = 18.0f;

      // Step title
      Entity st_lbl = add_entity_full(WIDGET_TEXT, circle_x[k] - 20.0f, slide_y + 140.0f, 130.0f, 25.0f, step_names[k], -1);
      set_text_color(st_lbl, tp_r, tp_g, tp_b);
      render_components[st_lbl].font_size = 14.0f;

      // Description text under ring
      Entity det = add_entity_full(WIDGET_TEXT, circle_x[k] - 50.0f, slide_y + 280.0f, 188.0f, 60.0f, details[k], -1);
      set_text_color(det, tm_r, tm_g, tm_b);
      render_components[det].font_size = 12.0f;
    }
  }

  // ==========================================
  // SLIDE 7: Genetic Health (DNA helix)
  // ==========================================
  {
    float x = 100.0f + spacing * 6.0f;
    // DNA double-helix programmatic drawing
    float helix_x_start = x + 95.0f;
    float helix_y_center = slide_y + 240.0f;
    for (int j = 0; j < 8; j++) {
      float px = helix_x_start + j * 42.0f;
      float rad = j * 0.785f;
      float py1 = helix_y_center + float_sin(rad) * 55.0f;
      float py2 = helix_y_center + float_sin(rad + 3.14159f) * 55.0f;

      // Draw rung
      float r_y = (py1 < py2) ? py1 : py2;
      float r_h = (py1 < py2) ? (py2 - py1) : (py1 - py2);
      if (r_h > 4.0f) {
        Entity rung = add_entity_full(WIDGET_RECT, px + 8.0f, r_y + 10.0f, 4.0f, r_h - 20.0f, "", -1);
        set_shape_color(rung, 148.0f/255.0f, 163.0f/255.0f, 184.0f/255.0f, 0.4f);
        render_components[rung].border_a = 0.0f;
      }

      // Draw Wave 1 node (Green)
      Entity n1 = add_entity_full(WIDGET_OVAL, px, py1 - 8.0f, 18.0f, 18.0f, "", -1);
      set_shape_color(n1, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f);
      render_components[n1].border_a = 0.0f;

      // Draw Wave 2 node (Indigo)
      Entity n2 = add_entity_full(WIDGET_OVAL, px, py2 - 8.0f, 18.0f, 18.0f, "", -1);
      set_shape_color(n2, 94.0f/255.0f, 106.0f/255.0f, 210.0f/255.0f, 1.0f);
      render_components[n2].border_a = 0.0f;
    }

    // Comparison cards on the right
    Entity card1 = add_entity_full(WIDGET_RECT, x + 445.0f, slide_y + 130.0f, 280.0f, 140.0f, "", -1);
    set_shape_color(card1, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(card1, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); // Green

    Entity c1_t = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 145.0f, 240.0f, 25.0f, "Mixed Breeds (Genetic Strength)", -1);
    set_text_color(c1_t, 34.0f/255.0f, 197.0f/255.0f, 94.0f/255.0f); render_components[c1_t].font_size = 14.0f;
    Entity c1_d = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 175.0f, 240.0f, 80.0f, "Lower risk of hip dysplasia, cancers, and genetic issues due to hybrid vigor.", -1);
    set_text_color(c1_d, tm_r, tm_g, tm_b); render_components[c1_d].font_size = 12.0f;

    // Protective Shield Badge
    Entity shield = add_entity_full(WIDGET_OVAL, x + 685.0f, slide_y + 140.0f, 30.0f, 30.0f, "", -1);
    set_shape_color(shield, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); render_components[shield].border_a = 0.0f;
    Entity sh_txt = add_entity_full(WIDGET_TEXT, x + 694.0f, slide_y + 146.0f, 15.0f, 15.0f, "✓", -1);
    set_text_color(sh_txt, 1.0f, 1.0f, 1.0f); render_components[sh_txt].font_size = 12.0f;

    Entity card2 = add_entity_full(WIDGET_RECT, x + 445.0f, slide_y + 290.0f, 280.0f, 140.0f, "", -1);
    set_shape_color(card2, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
    set_shape_border(card2, 248.0f/255.0f, 113.0f/255.0f, 113.0f/255.0f, 1.0f); // Red

    Entity c2_t = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 305.0f, 240.0f, 25.0f, "Purebreds (Inherited Risks)", -1);
    set_text_color(c2_t, tr_r, tr_g, tr_b); render_components[c2_t].font_size = 14.0f;
    Entity c2_d = add_entity_full(WIDGET_TEXT, x + 465.0f, slide_y + 335.0f, 240.0f, 80.0f, "Bottleneck breeding increases occurrences of joint, heart, and skin disorders.", -1);
    set_text_color(c2_d, tm_r, tm_g, tm_b); render_components[c2_d].font_size = 12.0f;

    // Warning Badge
    Entity warn_b = add_entity_full(WIDGET_OVAL, x + 685.0f, slide_y + 300.0f, 30.0f, 30.0f, "", -1);
    set_shape_color(warn_b, tr_r, tr_g, tr_b, 1.0f); render_components[warn_b].border_a = 0.0f;
    Entity wr_txt = add_entity_full(WIDGET_TEXT, x + 695.0f, slide_y + 306.0f, 15.0f, 15.0f, "!", -1);
    set_text_color(wr_txt, 1.0f, 1.0f, 1.0f); render_components[wr_txt].font_size = 12.0f;
  }

  // ==========================================
  // SLIDE 8: Prepare Your Home
  // ==========================================
  {
    float x = 100.0f + spacing * 7.0f;
    // Clipboard sheet
    Entity clipboard = add_entity_full(WIDGET_RECT, x + 95.0f, slide_y + 130.0f, 620.0f, 330.0f, "", -1);
    set_shape_color(clipboard, 255.0f/255.0f, 253.0f/255.0f, 245.0f/255.0f, 1.0f);
    set_shape_border(clipboard, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);

    // Binder Clip
    Entity clip = add_entity_full(WIDGET_RECT, x + 365.0f, slide_y + 115.0f, 80.0f, 20.0f, "", -1);
    set_shape_color(clip, tp_r, tp_g, tp_b, 1.0f);
    render_components[clip].border_a = 0.0f;

    Entity clip_t = add_entity_full(WIDGET_TEXT, x + 380.0f, slide_y + 120.0f, 50.0f, 16.0f, "COZY PAWS", -1);
    set_text_color(clip_t, 1.0f, 1.0f, 1.0f); render_components[clip_t].font_size = 8.0f;

    // Checklist checkboxes
    float item_y[4] = {slide_y + 170.0f, slide_y + 230.0f, slide_y + 290.0f, slide_y + 350.0f};
    const char *checklist_items[4] = {
      "Food & Water Bowls",
      "Cozy Pet Bed",
      "Collar & Leash",
      "Toys & Treats"
    };

    for (int k = 0; k < 4; k++) {
      // Checkbox square
      Entity chk = add_entity_full(WIDGET_RECT, x + 135.0f, item_y[k], 20.0f, 20.0f, "", -1);
      set_shape_color(chk, 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f);
      set_shape_border(chk, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f);

      // Checkmark tick inside
      Entity tick = add_entity_full(WIDGET_TEXT, x + 139.0f, item_y[k] + 2.0f, 15.0f, 15.0f, "✓", -1);
      set_text_color(tick, 34.0f/255.0f, 197.0f/255.0f, 94.0f/255.0f);
      render_components[tick].font_size = 12.0f;

      // Item text
      Entity item_txt = add_entity_full(WIDGET_TEXT, x + 175.0f, item_y[k] - 2.0f, 250.0f, 25.0f, checklist_items[k], -1);
      set_text_color(item_txt, tp_r, tp_g, tp_b);
      render_components[item_txt].font_size = 14.0f;
    }

    // Illustrations on the right side of clipboard
    // 1. Food bowls
    Entity bowl1 = add_entity_full(WIDGET_OVAL, x + 475.0f, slide_y + 170.0f, 40.0f, 25.0f, "", -1);
    set_shape_color(bowl1, 148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 0.8f); render_components[bowl1].border_a = 0.0f;
    Entity bowl2 = add_entity_full(WIDGET_OVAL, x + 505.0f, slide_y + 170.0f, 40.0f, 25.0f, "", -1);
    set_shape_color(bowl2, 148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 0.8f); render_components[bowl2].border_a = 0.0f;

    // 2. Cozy Pet Bed with sleeping cat silhouette
    Entity bed = add_entity_full(WIDGET_OVAL, x + 480.0f, slide_y + 225.0f, 90.0f, 50.0f, "", -1);
    set_shape_color(bed, 255.0f / 255.0f, 180.0f / 255.0f, 150.0f / 255.0f, 0.8f);
    render_components[bed].border_a = 0.0f;

    Entity cat_body = add_entity_full(WIDGET_OVAL, x + 510.0f, slide_y + 235.0f, 40.0f, 25.0f, "", -1);
    set_shape_color(cat_body, tm_r, tm_g, tm_b, 0.9f); render_components[cat_body].border_a = 0.0f;

    Entity cat_head = add_entity_full(WIDGET_OVAL, x + 538.0f, slide_y + 230.0f, 18.0f, 18.0f, "", -1);
    set_shape_color(cat_head, tm_r, tm_g, tm_b, 0.9f); render_components[cat_head].border_a = 0.0f;

    // 3. Leash/Collar (represented by thin red oval collar and path)
    Entity collar = add_entity_full(WIDGET_OVAL, x + 485.0f, slide_y + 295.0f, 40.0f, 15.0f, "", -1);
    set_shape_color(collar, tr_r, tr_g, tr_b, 1.0f);
    render_components[collar].border_a = 0.0f;

    Entity leash = add_entity_full(WIDGET_RECT, x + 520.0f, slide_y + 301.0f, 40.0f, 3.0f, "", -1);
    set_shape_color(leash, tr_r, tr_g, tr_b, 1.0f);
    render_components[leash].border_a = 0.0f;

    // 4. Toy bone
    Entity bone_mid = add_entity_full(WIDGET_RECT, x + 495.0f, slide_y + 360.0f, 40.0f, 10.0f, "", -1);
    set_shape_color(bone_mid, 241.0f/255.0f, 245.0f/255.0f, 249.0f/255.0f, 1.0f);
    set_shape_border(bone_mid, 203.0f/255.0f, 213.0f/255.0f, 225.0f/255.0f, 1.0f);

    Entity bone_l1 = add_entity_full(WIDGET_OVAL, x + 487.0f, slide_y + 353.0f, 14.0f, 14.0f, "", -1);
    set_shape_color(bone_l1, 241.0f/255.0f, 245.0f/255.0f, 249.0f/255.0f, 1.0f);
    render_components[bone_l1].border_a = 0.0f;

    Entity bone_l2 = add_entity_full(WIDGET_OVAL, x + 487.0f, slide_y + 363.0f, 14.0f, 14.0f, "", -1);
    set_shape_color(bone_l2, 241.0f/255.0f, 245.0f/255.0f, 249.0f/255.0f, 1.0f);
    render_components[bone_l2].border_a = 0.0f;

    Entity bone_r1 = add_entity_full(WIDGET_OVAL, x + 529.0f, slide_y + 353.0f, 14.0f, 14.0f, "", -1);
    set_shape_color(bone_r1, 241.0f/255.0f, 245.0f/255.0f, 249.0f/255.0f, 1.0f);
    render_components[bone_r1].border_a = 0.0f;

    Entity bone_r2 = add_entity_full(WIDGET_OVAL, x + 529.0f, slide_y + 363.0f, 14.0f, 14.0f, "", -1);
    set_shape_color(bone_r2, 241.0f/255.0f, 245.0f/255.0f, 249.0f/255.0f, 1.0f);
    render_components[bone_r2].border_a = 0.0f;
  }

  // ==========================================
  // SLIDE 9: Before & After (From Shelter to Sofa)
  // ==========================================
  {
    float x = 100.0f + spacing * 8.0f;
    // Shelter Cage Panel
    Entity before_card = add_entity_full(WIDGET_RECT, x + 95.0f, slide_y + 140.0f, 270.0f, 280.0f, "", -1);
    set_shape_color(before_card, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    set_shape_border(before_card, 148.0f/255.0f, 163.0f/255.0f, 184.0f/255.0f, 1.0f);

    // Bars
    for (int b = 0; b < 6; b++) {
      Entity bar = add_entity_full(WIDGET_RECT, x + 115.0f + b * 40.0f, slide_y + 140.0f, 4.0f, 280.0f, "", -1);
      set_shape_color(bar, 148.0f/255.0f, 163.0f/255.0f, 184.0f/255.0f, 1.0f);
      render_components[bar].border_a = 0.0f;
    }

    // Gloomy raincloud
    Entity cloud1 = add_entity_full(WIDGET_OVAL, x + 140.0f, slide_y + 160.0f, 50.0f, 25.0f, "", -1);
    set_shape_color(cloud1, tm_r, tm_g, tm_b, 0.4f); render_components[cloud1].border_a = 0.0f;
    Entity cloud2 = add_entity_full(WIDGET_OVAL, x + 165.0f, slide_y + 155.0f, 60.0f, 30.0f, "", -1);
    set_shape_color(cloud2, tm_r, tm_g, tm_b, 0.4f); render_components[cloud2].border_a = 0.0f;

    Entity b_txt = add_entity_full(WIDGET_TEXT, x + 105.0f, slide_y + 380.0f, 250.0f, 30.0f, "SHELTER: Lonely & Anxious", -1);
    set_text_color(b_txt, tr_r, tr_g, tr_b);
    render_components[b_txt].font_size = 13.0f;

    // Home Sweet Home Panel
    Entity after_card = add_entity_full(WIDGET_RECT, x + 445.0f, slide_y + 140.0f, 270.0f, 280.0f, "", -1);
    set_shape_color(after_card, 254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f, 1.0f); // Warm yellow
    set_shape_border(after_card, 251.0f/255.0f, 191.0f/255.0f, 36.0f/255.0f, 1.0f);

    // Glowing sun
    Entity sun = add_entity_full(WIDGET_OVAL, x + 630.0f, slide_y + 155.0f, 50.0f, 50.0f, "", -1);
    set_shape_color(sun, 245.0f/255.0f, 158.0f/255.0f, 11.0f/255.0f, 1.0f);
    render_components[sun].border_a = 0.0f;

    // Sofa
    Entity sofa = add_entity_full(WIDGET_RECT, x + 480.0f, slide_y + 260.0f, 200.0f, 60.0f, "", -1);
    set_shape_color(sofa, 255.0f/255.0f, 182.0f/255.0f, 193.0f/255.0f, 1.0f);
    set_shape_border(sofa, 244.0f/255.0f, 143.0f/255.0f, 177.0f/255.0f, 1.0f);

    // Cozy sleeping dog/cat on sofa
    Entity pet_body = add_entity_full(WIDGET_OVAL, x + 540.0f, slide_y + 245.0f, 50.0f, 30.0f, "", -1);
    set_shape_color(pet_body, tp_r, tp_g, tp_b, 1.0f); render_components[pet_body].border_a = 0.0f;

    Entity pet_head = add_entity_full(WIDGET_OVAL, x + 575.0f, slide_y + 240.0f, 20.0f, 20.0f, "", -1);
    set_shape_color(pet_head, tp_r, tp_g, tp_b, 1.0f); render_components[pet_head].border_a = 0.0f;

    Entity a_txt = add_entity_full(WIDGET_TEXT, x + 455.0f, slide_y + 380.0f, 250.0f, 30.0f, "HOME: Loved & Warm!", -1);
    set_text_color(a_txt, ta_r, ta_g, ta_b);
    render_components[a_txt].font_size = 14.0f;

    // Connect Shelter to Sofa
    Entity arrow = add_connection_entity(before_card, after_card);
    set_shape_color(arrow, 34.0f / 255.0f, 197.0f / 255.0f, 94.0f / 255.0f, 1.0f); // Green

    Entity arrow_lbl = add_entity_full(WIDGET_TEXT, x + 380.0f, slide_y + 245.0f, 50.0f, 25.0f, "ADOPT!", -1);
    set_text_color(arrow_lbl, 34.0f / 255.0f, 197.0f / 255.0f, 94.0f / 255.0f);
    render_components[arrow_lbl].font_size = 11.0f;
  }

  // ==========================================
  // SLIDE 10: Call to Action (CTA)
  // ==========================================
  {
    float x = 100.0f + spacing * 9.0f;
    // Big green button
    Entity btn = add_entity_full(WIDGET_RECT, x + 185.0f, slide_y + 170.0f, 450.0f, 110.0f, "FIND YOUR PET TODAY!", -1);
    set_shape_color(btn, 34.0f / 255.0f, 197.0f / 255.0f, 94.0f / 255.0f, 1.0f);
    set_shape_border(btn, 22.0f / 255.0f, 163.0f / 255.0f, 74.0f / 255.0f, 1.0f);
    render_components[btn].font_size = 24.0f;
    render_components[btn].r = 1.0f;
    render_components[btn].g = 1.0f;
    render_components[btn].b = 1.0f;

    // Confetti and celebration sparkles
    // Sparkle 1 (yellow triangle pair)
    Entity sp1_a = add_entity_full(WIDGET_TRIANGLE, x + 115.0f, slide_y + 125.0f, 25.0f, 25.0f, "", -1);
    set_shape_color(sp1_a, tg_r, tg_g, tg_b, 1.0f); render_components[sp1_a].border_a = 0.0f;

    Entity sp1_b = add_entity_full(WIDGET_TRIANGLE, x + 655.0f, slide_y + 125.0f, 25.0f, 25.0f, "", -1);
    set_shape_color(sp1_b, tg_r, tg_g, tg_b, 1.0f); render_components[sp1_b].border_a = 0.0f;

    // Confetti particles
    Entity cft1 = add_entity_full(WIDGET_RECT, x + 150.0f, slide_y + 320.0f, 15.0f, 8.0f, "", -1);
    set_shape_color(cft1, 251.0f/255.0f, 113.0f/255.0f, 133.0f/255.0f, 1.0f); render_components[cft1].border_a = 0.0f;

    Entity cft2 = add_entity_full(WIDGET_RECT, x + 630.0f, slide_y + 300.0f, 8.0f, 15.0f, "", -1);
    set_shape_color(cft2, 135.0f/255.0f, 206.0f/255.0f, 250.0f/255.0f, 1.0f); render_components[cft2].border_a = 0.0f;

    Entity cft3 = add_entity_full(WIDGET_OVAL, x + 120.0f, slide_y + 240.0f, 12.0f, 12.0f, "", -1);
    set_shape_color(cft3, tg_r, tg_g, tg_b, 1.0f); render_components[cft3].border_a = 0.0f;

    Entity cft4 = add_entity_full(WIDGET_OVAL, x + 680.0f, slide_y + 200.0f, 10.0f, 10.0f, "", -1);
    set_shape_color(cft4, 74.0f/255.0f, 222.0f/255.0f, 128.0f/255.0f, 1.0f); render_components[cft4].border_a = 0.0f;

    Entity sub_b = add_entity_full(WIDGET_TEXT, x + 185.0f, slide_y + 330.0f, 450.0f, 30.0f, "🐾 Over 10,000 lives saved. Join the movement.", -1);
    set_text_color(sub_b, ta_r, ta_g, ta_b);
    render_components[sub_b].font_size = 14.0f;
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
