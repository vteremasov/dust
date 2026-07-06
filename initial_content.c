#include "initial_content.h"

typedef struct {
  float r, g, b;
} PastelColor;

static PastelColor colors[6] = {
    {238.0f / 255.0f, 242.0f / 255.0f, 255.0f / 255.0f}, // Light Indigo
    {240.0f / 255.0f, 253.0f / 255.0f, 244.0f / 255.0f}, // Light Green
    {254.0f / 255.0f, 243.0f / 255.0f, 199.0f / 255.0f}, // Light Yellow
    {255.0f / 255.0f, 241.0f / 255.0f, 242.0f / 255.0f}, // Light Rose
    {245.0f / 255.0f, 243.0f / 255.0f, 255.0f / 255.0f}, // Light Purple
    {255.0f / 255.0f, 247.0f / 255.0f, 237.0f / 255.0f}  // Light Orange
};

static unsigned int rng_state = 123456789;
static float local_random_float() {
  rng_state = rng_state * 1664525 + 1013904223;
  return (float)(rng_state & 0xFFFFFF) / 16777216.0f;
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
  if (color_idx < 0) color_idx = 0;
  if (color_idx > 5) color_idx = 5;
  
  r->bg_r = colors[color_idx].r;
  r->bg_g = colors[color_idx].g;
  r->bg_b = colors[color_idx].b;

  if (type == WIDGET_CODE) {
    r->bg_r = 30.0f / 255.0f;
    r->bg_g = 30.0f / 255.0f;
    r->bg_b = 34.0f / 255.0f;
    r->bg_a = 0.95f;
    r->r = 0.9f;
    r->g = 0.9f;
    r->b = 0.9f;
    r->border_r = 60.0f / 255.0f;
    r->border_g = 60.0f / 255.0f;
    r->border_b = 68.0f / 255.0f;
    r->border_a = 1.0f;
  } else if (type == WIDGET_TEXT) {
    r->bg_a = 0.0f;
    r->r = 15.0f / 255.0f; // High contrast Slate-900 for text
    r->g = 23.0f / 255.0f;
    r->b = 42.0f / 255.0f;
  } else {
    r->bg_a = 0.95f;
    r->r = 15.0f / 255.0f;
    r->g = 23.0f / 255.0f;
    r->b = 42.0f / 255.0f;
  }

  if (type != WIDGET_CODE) {
    r->border_r = r->bg_r * 0.85f;
    r->border_g = r->bg_g * 0.85f;
    r->border_b = r->bg_b * 0.85f;
    if (type == WIDGET_RECT || type == WIDGET_OVAL || type == WIDGET_TRIANGLE) {
      r->border_a = 1.0f;
    } else {
      r->border_a = 0.0f;
    }
  }

  r->font_size = (type == WIDGET_CODE) ? 14.0f : 18.0f;

  ecs_add_component(e, COMP_TEXT);
  if (!text) text = "";
  unsigned int text_len = 0;
  while (text[text_len] != '\0') text_len++;
  text_components[e].text = allocate_text(text, text_len);

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
  r->r = 79.0f / 255.0f;  // High contrast indigo arrow
  r->g = 70.0f / 255.0f;
  r->b = 229.0f / 255.0f;
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

static void set_text_color(Entity e, float r, float g, float b) {
  if (e == (Entity)-1) return;
  render_components[e].r = r;
  render_components[e].g = g;
  render_components[e].b = b;
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
  ecs_init();

  float slide_w = 800.0f;
  float slide_h = 500.0f;
  float spacing_x = 900.0f;

  Entity slides[15];

  static struct { float r, g, b; } slide_themes[5] = {
    {250.0f/255.0f, 245.0f/255.0f, 255.0f/255.0f}, // Pastel Purple
    {240.0f/255.0f, 248.0f/255.0f, 255.0f/255.0f}, // Pastel Blue
    {240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f}, // Pastel Green
    {254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f}, // Pastel Yellow
    {255.0f/255.0f, 247.0f/255.0f, 237.0f/255.0f}  // Pastel Orange
  };

  static const char *titles[15] = {
    "01. Dust Canvas Engine",
    "02. Technological Stack",
    "03. Freestanding C Target",
    "04. Wasm Dynamic Text Heap",
    "05. 100k Character Limit",
    "06. Decoupled Pipeline Layout",
    "07. Entity Component System",
    "08. Stream-Based Drawing",
    "09. Lexical Syntax Highlighter",
    "10. MSDF Font Compiler",
    "11. Vector Glyph Rendering",
    "12. Arrow Lines Connector",
    "13. WebGPU Render Pass",
    "14. 100k Infographics Scale",
    "15. Future Project Roadmap"
  };

  static const char *subtitles[15] = {
    "Infinite hardware-accelerated whiteboard in pure C",
    "Low-overhead native primitives & compilation targets",
    "Operating -nostdlib to minimize WASM binary size",
    "Structuring dynamic string memory inside static partitions",
    "Dynamic pool heap mapping multi-kilobyte text shapes",
    "Decoupling the browser event loop from drawing updates",
    "Fast data lookups for selections, transforms, & arrows",
    "Zero-allocation streaming line drawing parser",
    "Lexical keywords and comment tokenizer scanner",
    "Pre-compiling TrueType curves into a distance field atlas",
    "Pixel-perfect character scaling and zooming at 120 FPS",
    "Dynamic vector line routing between whiteboard nodes",
    "Packing vertices & indices for WebGPU shader execution",
    "Instancing benchmark metrics: HTML DOM vs custom batcher",
    "Whiteboard real-time collaborative sync & next-gen tools"
  };

  // High contrast slate/dark typography colors
  float tp_r = 15.0f / 255.0f, tp_g = 23.0f / 255.0f, tp_b = 42.0f / 255.0f; // Slate-900 (Dark text)
  float ta_r = 79.0f / 255.0f, ta_g = 70.0f / 255.0f, ta_b = 229.0f / 255.0f; // Indigo-600 (Accent)
  float tm_r = 71.0f / 255.0f, tm_g = 85.0f / 255.0f, tm_b = 105.0f / 255.0f; // Slate-600 (Muted)

  // 1. Generate 15 Slides arranged in a single horizontal line row
  for (int i = 0; i < 15; i++) {
    float x = 100.0f + i * spacing_x;
    float y = 100.0f;
    int theme_idx = i % 5;

    // Slide bottom shadow panel
    Entity shadow = add_entity_full(WIDGET_RECT, x + 8.0f, y + 8.0f, slide_w, slide_h, "", -1);
    set_shape_color(shadow, 15.0f/255.0f, 23.0f/255.0f, 42.0f/255.0f, 0.15f);
    render_components[shadow].border_a = 0.0f;

    // Main slide container card
    slides[i] = add_entity_full(WIDGET_RECT, x, y, slide_w, slide_h, "", -1);
    set_shape_color(slides[i], slide_themes[theme_idx].r, slide_themes[theme_idx].g, slide_themes[theme_idx].b, 1.0f);
    set_shape_border(slides[i], 203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f);

    // Accent top bar
    Entity stripe = add_entity_full(WIDGET_RECT, x, y, slide_w, 8.0f, "", -1);
    set_shape_color(stripe, ta_r, ta_g, ta_b, 1.0f);
    render_components[stripe].border_a = 0.0f;

    // Slide sidebar
    Entity sidebar = add_entity_full(WIDGET_RECT, x, y + 8.0f, 60.0f, slide_h - 8.0f, "", -1);
    set_shape_color(sidebar, tp_r, tp_g, tp_b, 1.0f);
    render_components[sidebar].border_a = 0.0f;

    // Slide number label
    char num_str[4];
    num_str[0] = '0' + (i + 1) / 10;
    num_str[1] = '0' + (i + 1) % 10;
    num_str[2] = '\0';
    Entity idx_txt = add_entity_full(WIDGET_TEXT, x + 12.0f, y + 30.0f, 36.0f, 32.0f, num_str, -1);
    set_text_color(idx_txt, 1.0f, 1.0f, 1.0f);
    render_components[idx_txt].font_size = 20.0f;

    Entity side_sym = add_entity_full(WIDGET_TEXT, x + 18.0f, y + 65.0f, 24.0f, 20.0f, "DUST", -1);
    set_text_color(side_sym, 1.0f, 0.8f, 0.3f);
    render_components[side_sym].font_size = 8.0f;

    // Header Title (High contrast dark slate text)
    Entity t_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, y + 20.0f, 530.0f, 32.0f, titles[i], -1);
    set_text_color(t_ent, tp_r, tp_g, tp_b);
    render_components[t_ent].font_size = 22.0f;

    // Horizontal Separator
    Entity line = add_entity_full(WIDGET_RECT, x + 80.0f, y + 55.0f, slide_w - 120.0f, 1.5f, "", -1);
    set_shape_color(line, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f);
    render_components[line].border_a = 0.0f;

    // Subtitle (Indigo)
    Entity sub_ent = add_entity_full(WIDGET_TEXT, x + 80.0f, y + 68.0f, slide_w - 120.0f, 25.0f, subtitles[i], -1);
    set_text_color(sub_ent, ta_r, ta_g, ta_b);
    render_components[sub_ent].font_size = 13.0f;
  }

  // 2. Define Slide Contents (One long horizontal chain)
  
  // Slide 1
  {
    float x = 100.0f + 0 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Standalone C logic compiling directly to freestanding WebAssembly.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Pure WebGPU drawing pipeline for infinite rendering scales and zooms.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Entity Component System (ECS) architecture manages active components.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Wasm Core Init\nvoid wasm_init() {\n  ecs_init();\n  reset_text_heap();\n  mark_dirty();\n}\n\nEntity create_node(int type) {\n  Entity e = ecs_create();\n  ecs_add(e, COMP_TRANS);\n  ecs_add(e, COMP_RENDER);\n  return e;\n}",
      -1
    );
  }

  // Slide 2
  {
    float x = 100.0f + 1 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Low-overhead target environment: zero libc standard libraries.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Shaders pre-compiled to target native WGSL binding configurations.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• MSDF vector glyph mapping ensures smooth high-zoom font rendering.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Compile Target Command\n// clang -target wasm32 \\\n//   -O3 -nostdlib \\\n//   -Wl,--no-entry \\\n//   -o canvas.wasm \\\n//   canvas.c renderer.c ecs.c",
      -1
    );
  }

  // Slide 3
  {
    float x = 100.0f + 2 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• The target compiler runs -nostdlib to strip runtime junk.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• String utilities are declared manually inside custom source files.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Direct mappings map Javascript calls straight to exported pointers.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Custom strlen function\nint local_strlen(const char *s) {\n  int len = 0;\n  while (s[len] != '\\0') {\n    len++;\n  }\n  return len;\n}",
      -1
    );
  }

  // Slide 4
  {
    float x = 100.0f + 3 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Text storage uses a contiguous static 24MB array buffer heap.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Avoids allocating individual arrays in all entity component slots.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Resets allocation counters on ecs_init to prevent runtime leaks.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Heap allocation routine\nchar *allocate_text(\n  const char *src, int len\n) {\n  char *dest = &heap[offset];\n  memcpy(dest, src, len);\n  offset += len + 1;\n  return dest;\n}",
      -1
    );
  }

  // Slide 5
  {
    float x = 100.0f + 4 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Whiteboard shapes accept dynamic text sizes up to 100k characters.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Direct edits write to a safe intermediate buffer inside the sandbox.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Commits allocate new dynamic segments inside the text heap array.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Edit Commit handler\nvoid on_text_commit() {\n  int len = strlen(edit_buf);\n  text_components[idx].text =\n    allocate_text(edit_buf, len);\n}",
      -1
    );
  }

  // Slide 6
  {
    float x = 100.0f + 5 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• The browser DOM is bypassed entirely for high-performance canvas.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• JS listens to mouse/key interactions and propagates them to WASM.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Drawing commands queue geometry data to dynamic render buffers.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// JS event capturing\ncanvas.onmousemove = (e) => {\n  const world = toWorld(\n    e.clientX, e.clientY\n  );\n  wasm.exports.on_mouse_move(\n    world.x, world.y\n  );\n};",
      -1
    );
  }

  // Slide 7
  {
    float x = 100.0f + 6 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Struct arrays store transforms, renders, paths, and interactions.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• System loops iterate over specific masks to transform coordinates.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Selection hits, dragging offsets, and arrows update dynamically.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Component Declarations\ntypedef unsigned int Entity;\n#define COMP_TRANSFORM (1 << 0)\n#define COMP_RENDER    (1 << 1)\n#define COMP_TEXT      (1 << 2)",
      -1
    );
  }

  // Slide 8
  {
    float x = 100.0f + 7 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Scans text string on-the-fly to eliminate pre-cached arrays.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Translates tabs to custom 4-space indent alignments on the fly.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Supports drawing infinite text lines inside custom Code blocks.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Streaming rendering loop\nwhile (text[j] != '\\0') {\n  while (text[j] != '\\n') {\n    unsigned int cp = decode(&text[j]);\n    draw_char(cp);\n  }\n}",
      -1
    );
  }

  // Slide 9
  {
    float x = 100.0f + 8 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Features a built-in lexical scanner for indexing keyword tokens.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Highlights keywords, strings, comments, and numbers on the fly.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Non-code shapes remain cleanly un-colorized to optimize speed.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Lexical C Keywords\nconst char *keywords[] = {\n  \"int\", \"void\", \"function\",\n  \"const\", \"char\", \"float\",\n  \"return\", \"struct\", \"if\"\n};",
      -1
    );
  }

  // Slide 10
  {
    float x = 100.0f + 9 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Vector glyph shapes are mapped to a dynamic 2048x2048 grid texture.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Scans monospace fonts and maps glyph curves to private sectors.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Fallback Arial compiler automatically loads system fonts.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Compile MSDF Font\nint compile_font() {\n  stbtt_InitFont(&info, font_data);\n  generate_msdf_atlas();\n  write_header();\n}",
      -1
    );
  }

  // Slide 11
  {
    float x = 100.0f + 10 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Fragment shader samples distance coordinates to shape vector outlines.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Prevents blurriness and pixelation under high whiteboard zooms.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Vector curves render smoothly at a stable 120 FPS frame rate.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// WGSL MSDF Shader\nfn get_alpha(uv: vec2f) -> f32 {\n  let sigDist = textureSample(t, s, uv);\n  let median = max(min(sigDist.r, sigDist.g), \\\n                   min(max(sigDist.r, sigDist.g), sigDist.b));\n  return screenClear(median);\n}",
      -1
    );
  }

  // Slide 12
  {
    float x = 100.0f + 11 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Lines automatically route routes between linked shapes on the screen.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Intersection algorithms compute line endpoints on shapes' borders.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Arrow headers and lines are compiled into static vertex arrays.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Draw Connection Line\nvoid draw_arrow(Entity from, Entity to) {\n  float dx = transform[to].x - transform[from].x;\n  float dy = transform[to].y - transform[from].y;\n  draw_rect_rotated(angle);\n}",
      -1
    );
  }

  // Slide 13
  {
    float x = 100.0f + 12 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Dynamic buffers batch vertex positions and texture mappings.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Reduces Wasm-to-JS pipeline cross calls to optimize rendering.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Maps uniforms and viewport sizes directly inside a single pass.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// WebGPU Render loop\nvoid draw_frame() {\n  js_wgpu_begin_render_pass();\n  js_wgpu_draw_indexed(\n    index_count, 0\n  );\n}",
      -1
    );
  }

  // Slide 14
  {
    float x = 100.0f + 13 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Standard HTML DOM whiteboard containers lag beyond 2,000 nodes.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Dust maintains solid 120 FPS layouts at 100,000 active shape entities.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Contiguous memory allocation minimizes CPU cache line misses.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// Scale Performance test\nvoid create_100k_test() {\n  for(int i = 0; i < 100000; i++) {\n    create_node(i);\n  }\n}",
      -1
    );
  }

  // Slide 15
  {
    float x = 100.0f + 14 * spacing_x;
    float y = 100.0f;
    Entity d1 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 150.0f, 400.0f, 30.0f, "• Implement real-time WebSockets state synchronization.", -1);
    set_text_color(d1, tm_r, tm_g, tm_b);
    Entity d2 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 200.0f, 400.0f, 30.0f, "• Add multi-user edit conflicts resolutions algorithms.", -1);
    set_text_color(d2, tm_r, tm_g, tm_b);
    Entity d3 = add_entity_full(WIDGET_TEXT, x + 90.0f, y + 250.0f, 400.0f, 30.0f, "• Advanced formatting: text-wrapping & direct canvas image load.", -1);
    set_text_color(d3, tm_r, tm_g, tm_b);
    add_entity_full(
      WIDGET_CODE, x + 510.0f, y + 140.0f, 260.0f, 320.0f,
      "// WS Sync callback\nvoid on_sync_receive(\n  const char *msg\n) {\n  deserialize_state(msg);\n  mark_dirty();\n}",
      -1
    );
  }

  // ==========================================
  // ARCHITECTURAL FLOW CHART DIAGRAM (Under slides at y = 680)
  // ==========================================
  float flow_y = 680.0f;

  // Flowchart Backdrop Panel Header
  Entity diagram_hdr = add_entity_full(WIDGET_RECT, 100.0f, flow_y, 2600.0f, 45.0f, "", -1);
  set_shape_color(diagram_hdr, 30.0f/255.0f, 41.0f/255.0f, 59.0f/255.0f, 1.0f); // Slate-800
  render_components[diagram_hdr].border_a = 0.0f;

  Entity hdr_txt = add_entity_full(WIDGET_TEXT, 120.0f, flow_y + 13.0f, 500.0f, 25.0f, "PROJECT ARCHITECTURE & DATA FLOW PIPELINE", -1);
  set_text_color(hdr_txt, 1.0f, 1.0f, 1.0f);
  render_components[hdr_txt].font_size = 14.0f;

  // Flowchart Backdrop body
  Entity diagram_body = add_entity_full(WIDGET_RECT, 100.0f, flow_y + 45.0f, 2600.0f, 580.0f, "", -1);
  set_shape_color(diagram_body, 248.0f/255.0f, 250.0f/255.0f, 252.0f/255.0f, 1.0f); // Slate-50 (Very light gray)
  set_shape_border(diagram_body, 226.0f/255.0f, 232.0f/255.0f, 240.0f/255.0f, 1.0f); // Slate-200

  // Flowchart Nodes (Steps)
  Entity node_js = add_entity_full(
    WIDGET_RECT, 200.0f, flow_y + 100.0f, 250.0f, 100.0f,
    "JS Input Event Loop\n(bridge.js)\n• Captured mouse moves\n• Text editing overlay\n• State save / load", -1
  );
  set_shape_color(node_js, 254.0f/255.0f, 243.0f/255.0f, 199.0f/255.0f, 1.0f); // Amber-100 (High contrast light backdrop)
  set_text_color(node_js, tp_r, tp_g, tp_b);
  render_components[node_js].font_size = 11.0f;

  Entity node_bridge = add_entity_full(
    WIDGET_RECT, 580.0f, flow_y + 100.0f, 250.0f, 100.0f,
    "Wasm API Interface\n(canvas.c)\n• Hit-testing logic\n• Exported handlers\n• Text buffers manager", -1
  );
  set_shape_color(node_bridge, 239.0f/255.0f, 246.0f/255.0f, 255.0f/255.0f, 1.0f); // Blue-50
  set_text_color(node_bridge, tp_r, tp_g, tp_b);
  render_components[node_bridge].font_size = 11.0f;

  Entity node_ecs = add_entity_full(
    WIDGET_RECT, 960.0f, flow_y + 70.0f, 280.0f, 90.0f,
    "ECS Memory Model\n(ecs.c / ecs.h)\n• Transform & render matrices\n• 24MB Dynamic Text heap", -1
  );
  set_shape_color(node_ecs, 240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f, 1.0f); // Green-50
  set_text_color(node_ecs, tp_r, tp_g, tp_b);
  render_components[node_ecs].font_size = 11.0f;

  Entity node_systems = add_entity_full(
    WIDGET_RECT, 960.0f, flow_y + 190.0f, 280.0f, 90.0f,
    "ECS Systems\n(systems.c)\n• Selection & dragging handlers\n• C Syntax Highlighter", -1
  );
  set_shape_color(node_systems, 240.0f/255.0f, 253.0f/255.0f, 244.0f/255.0f, 1.0f); // Green-50
  set_text_color(node_systems, tp_r, tp_g, tp_b);
  render_components[node_systems].font_size = 11.0f;

  Entity node_renderer = add_entity_full(
    WIDGET_RECT, 1370.0f, flow_y + 100.0f, 280.0f, 100.0f,
    "Geometry Batcher\n(renderer.c)\n• MSDF font atlas indices\n• Arrow lines generator\n• Vertex / Index buffer packing", -1
  );
  set_shape_color(node_renderer, 245.0f/255.0f, 243.0f/255.0f, 255.0f/255.0f, 1.0f); // Purple-50
  set_text_color(node_renderer, tp_r, tp_g, tp_b);
  render_components[node_renderer].font_size = 11.0f;

  Entity node_gpu = add_entity_full(
    WIDGET_RECT, 1780.0f, flow_y + 100.0f, 280.0f, 100.0f,
    "WebGPU Context\n(GPU Output)\n• Vertex Shader compilation\n• MSDF Fragment Shader mapping\n• Hardware Draw Calls", -1
  );
  set_shape_color(node_gpu, 255.0f/255.0f, 241.0f/255.0f, 242.0f/255.0f, 1.0f); // Rose-50
  set_text_color(node_gpu, tp_r, tp_g, tp_b);
  render_components[node_gpu].font_size = 11.0f;

  // Connect flowchart nodes using line arrows
  add_connection_entity(node_js, node_bridge);
  add_connection_entity(node_bridge, node_ecs);
  add_connection_entity(node_bridge, node_systems);
  add_connection_entity(node_ecs, node_renderer);
  add_connection_entity(node_systems, node_renderer);
  add_connection_entity(node_renderer, node_gpu);

  // Flowchart Code Block attachments (Decorations under nodes)
  Entity code_js_bridge = add_entity_full(
    WIDGET_CODE, 580.0f, flow_y + 310.0f, 250.0f, 190.0f,
    "// C Export Entrypoint\nvoid on_mouse_down(\n  float x, float y\n) {\n  int hit = hit_test(x, y);\n  if (hit != -1) {\n    select_node(hit);\n  }\n}",
    -1
  );
  add_connection_entity(node_bridge, code_js_bridge);

  Entity code_ecs_heap = add_entity_full(
    WIDGET_CODE, 960.0f, flow_y + 310.0f, 280.0f, 190.0f,
    "// Stream-Based Drawing\nvoid draw_text() {\n  const char *str = text;\n  while (*str) {\n    unsigned int cp = decode(&str);\n    draw_char(cp);\n  }\n}",
    -1
  );
  add_connection_entity(node_systems, code_ecs_heap);

  Entity code_gpu_draw = add_entity_full(
    WIDGET_CODE, 1370.0f, flow_y + 310.0f, 280.0f, 190.0f,
    "// Render pipeline pack\nvoid draw_char(char cp) {\n  Glyph g = lookup(cp);\n  pack_vertex(g.x, g.y);\n  pack_indices();\n}",
    -1
  );
  add_connection_entity(node_renderer, code_gpu_draw);
}

void generate_100k_infographics() {
  ecs_init();

  int cols = 50;
  int rows = 50;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      float x_offset = col * 1500.0f;
      float y_offset = row * 800.0f;

      // Left Panel Background (Dark Slate)
      Entity left_bg =
          add_entity_full(WIDGET_RECT, 100.0f + x_offset, 100.0f + y_offset,
                          300.0f, 550.0f, "", -1);
      render_components[left_bg].bg_r = 15.0f / 255.0f;
      render_components[left_bg].bg_g = 18.0f / 255.0f;
      render_components[left_bg].bg_b = 25.0f / 255.0f;
      render_components[left_bg].bg_a = 1.0f;
      render_components[left_bg].border_a = 0.0f;

      // Right Panel Background (Sleek Dark Slate)
      Entity right_bg =
          add_entity_full(WIDGET_RECT, 400.0f + x_offset, 100.0f + y_offset,
                          700.0f, 550.0f, "", -1);
      render_components[right_bg].bg_r = 20.0f / 255.0f;
      render_components[right_bg].bg_g = 24.0f / 255.0f;
      render_components[right_bg].bg_b = 33.0f / 255.0f;
      render_components[right_bg].bg_a = 1.0f;
      render_components[right_bg].border_a = 0.0f;

      // Title Text "DUST" in White (on dark slate panel)
      Entity title_idx =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 130.0f + y_offset,
                          240.0f, 60.0f, "DUST", -1);
      render_components[title_idx].bg_r = 226.0f / 255.0f;
      render_components[title_idx].bg_g = 232.0f / 255.0f;
      render_components[title_idx].bg_b = 240.0f / 255.0f;
      render_components[title_idx].r = 226.0f / 255.0f;
      render_components[title_idx].g = 232.0f / 255.0f;
      render_components[title_idx].b = 240.0f / 255.0f;
      render_components[title_idx].font_size = 48.0f;

      // Subtitle Text "WEBGPU WHITEBOARD" in Accent Indigo
      Entity sub_idx =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 200.0f + y_offset,
                          240.0f, 30.0f, "WEBGPU WHITEBOARD", -1);
      render_components[sub_idx].bg_r = 94.0f / 255.0f;
      render_components[sub_idx].bg_g = 106.0f / 255.0f;
      render_components[sub_idx].bg_b = 210.0f / 255.0f;
      render_components[sub_idx].r = 94.0f / 255.0f;
      render_components[sub_idx].g = 106.0f / 255.0f;
      render_components[sub_idx].b = 210.0f / 255.0f;
      render_components[sub_idx].font_size = 14.0f;

      // Info text block
      Entity desc_idx =
          add_entity_full(WIDGET_TEXT, 130.0f + x_offset, 250.0f + y_offset,
                          240.0f, 120.0f,
                          "Hardware accelerated whiteboard engine in freestanding WebAssembly.\nHandles 100k+ nodes smoothly at 120 FPS.", -1);
      render_components[desc_idx].bg_r = 148.0f / 255.0f;
      render_components[desc_idx].bg_g = 163.0f / 255.0f;
      render_components[desc_idx].bg_b = 184.0f / 255.0f;
      render_components[desc_idx].r = 148.0f / 255.0f;
      render_components[desc_idx].g = 163.0f / 255.0f;
      render_components[desc_idx].b = 184.0f / 255.0f;
      render_components[desc_idx].font_size = 11.0f;

      // Metric Badge (120 FPS)
      Entity fps_badge =
          add_entity_full(WIDGET_RECT, 130.0f + x_offset, 400.0f + y_offset,
                          100.0f, 50.0f, "120 FPS", -1);
      render_components[fps_badge].bg_r = 34.0f / 255.0f;
      render_components[fps_badge].bg_g = 197.0f / 255.0f;
      render_components[fps_badge].bg_b = 94.0f / 255.0f;
      render_components[fps_badge].bg_a = 0.15f;
      render_components[fps_badge].r = 74.0f / 255.0f;
      render_components[fps_badge].g = 222.0f / 255.0f;
      render_components[fps_badge].b = 128.0f / 255.0f;
      render_components[fps_badge].border_r = 74.0f / 255.0f;
      render_components[fps_badge].border_g = 222.0f / 255.0f;
      render_components[fps_badge].border_b = 128.0f / 255.0f;
      render_components[fps_badge].font_size = 14.0f;

      // Metric Badge (0 DEP)
      Entity dep_badge =
          add_entity_full(WIDGET_RECT, 245.0f + x_offset, 400.0f + y_offset,
                          100.0f, 50.0f, "0 DEP", -1);
      render_components[dep_badge].bg_r = 239.0f / 255.0f;
      render_components[dep_badge].bg_g = 68.0f / 255.0f;
      render_components[dep_badge].bg_b = 68.0f / 255.0f;
      render_components[dep_badge].bg_a = 0.15f;
      render_components[dep_badge].r = 248.0f / 255.0f;
      render_components[dep_badge].g = 113.0f / 255.0f;
      render_components[dep_badge].b = 113.0f / 255.0f;
      render_components[dep_badge].border_r = 248.0f / 255.0f;
      render_components[dep_badge].border_g = 113.0f / 255.0f;
      render_components[dep_badge].border_b = 113.0f / 255.0f;
      render_components[dep_badge].font_size = 14.0f;

      // Performance Graph / Chart Card
      Entity chart_card =
          add_entity_full(WIDGET_RECT, 440.0f + x_offset, 140.0f + y_offset,
                          620.0f, 320.0f, "", -1);
      render_components[chart_card].bg_r = 30.0f / 255.0f;
      render_components[chart_card].bg_g = 41.0f / 255.0f;
      render_components[chart_card].bg_b = 59.0f / 255.0f;
      render_components[chart_card].bg_a = 1.0f;
      render_components[chart_card].border_a = 0.0f;

      // Graph title
      Entity gr_title =
          add_entity_full(WIDGET_TEXT, 470.0f + x_offset, 170.0f + y_offset,
                          350.0f, 25.0f, "SCALING RENDERING THRU ECS & BATCHING", -1);
      render_components[gr_title].bg_r = 241.0f / 255.0f;
      render_components[gr_title].bg_g = 245.0f / 255.0f;
      render_components[gr_title].bg_b = 249.0f / 255.0f;
      render_components[gr_title].r = 241.0f / 255.0f;
      render_components[gr_title].g = 245.0f / 255.0f;
      render_components[gr_title].b = 249.0f / 255.0f;
      render_components[gr_title].font_size = 12.0f;

      // Graph y-axis label
      Entity gr_y =
          add_entity_full(WIDGET_TEXT, 470.0f + x_offset, 210.0f + y_offset,
                          150.0f, 25.0f, "Frame Time (ms)", -1);
      render_components[gr_y].bg_r = 148.0f / 255.0f;
      render_components[gr_y].bg_g = 163.0f / 255.0f;
      render_components[gr_y].bg_b = 184.0f / 255.0f;
      render_components[gr_y].r = 148.0f / 255.0f;
      render_components[gr_y].g = 163.0f / 255.0f;
      render_components[gr_y].b = 184.0f / 255.0f;
      render_components[gr_y].font_size = 10.0f;

      // Axis Line
      Entity axis =
          add_entity_full(WIDGET_RECT, 470.0f + x_offset, 400.0f + y_offset,
                          560.0f, 2.0f, "", -1);
      render_components[axis].bg_r = 71.0f / 255.0f;
      render_components[axis].bg_g = 85.0f / 255.0f;
      render_components[axis].bg_b = 105.0f / 255.0f;
      render_components[axis].bg_a = 1.0f;
      render_components[axis].border_a = 0.0f;

      // Chart Bar 1 (Standard DOM)
      Entity bar1 =
          add_entity_full(WIDGET_RECT, 520.0f + x_offset, 240.0f + y_offset,
                          60.0f, 160.0f, "SVG / DOM", -1);
      render_components[bar1].bg_r = 239.0f / 255.0f;
      render_components[bar1].bg_g = 68.0f / 255.0f;
      render_components[bar1].bg_b = 68.0f / 255.0f;
      render_components[bar1].bg_a = 0.8f;
      render_components[bar1].r = 1.0f;
      render_components[bar1].g = 1.0f;
      render_components[bar1].b = 1.0f;
      render_components[bar1].border_a = 0.0f;
      render_components[bar1].font_size = 9.0f;

      // Chart Bar 2 (Dust Engine)
      Entity bar2 =
          add_entity_full(WIDGET_RECT, 640.0f + x_offset, 380.0f + y_offset,
                          60.0f, 20.0f, "DUST", -1);
      render_components[bar2].bg_r = 34.0f / 255.0f;
      render_components[bar2].bg_g = 197.0f / 255.0f;
      render_components[bar2].bg_b = 94.0f / 255.0f;
      render_components[bar2].bg_a = 0.8f;
      render_components[bar2].r = 1.0f;
      render_components[bar2].g = 1.0f;
      render_components[bar2].b = 1.0f;
      render_components[bar2].border_a = 0.0f;
      render_components[bar2].font_size = 9.0f;

      // Note annotation
      Entity note =
          add_entity_full(WIDGET_TEXT, 750.0f + x_offset, 240.0f + y_offset,
                          220.0f, 120.0f,
                          "Notice:\nStandard SVG pipelines drop below 60fps at ~2,000 shapes.\nDust keeps solid 120fps even at 100,000 active nodes.", -1);
      render_components[note].bg_r = 148.0f / 255.0f;
      render_components[note].bg_g = 163.0f / 255.0f;
      render_components[note].bg_b = 184.0f / 255.0f;
      render_components[note].r = 148.0f / 255.0f;
      render_components[note].g = 163.0f / 255.0f;
      render_components[note].b = 184.0f / 255.0f;
      render_components[note].font_size = 10.0f;

      // Connect badges to their parent container
      add_connection_entity(left_bg, right_bg);
    }
  }
}
