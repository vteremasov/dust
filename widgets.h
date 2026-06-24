#ifndef WIDGETS_H
#define WIDGETS_H

typedef enum {
  WIDGET_STICKY = 0,
  WIDGET_RECT = 1,
  WIDGET_OVAL = 2,
  WIDGET_TEXT = 3,
  WIDGET_IMAGE = 4,
  WIDGET_PATH = 5,
  WIDGET_ARROW = 6
} WidgetType;

typedef struct {
  float x, y;
} PathPoint;

typedef struct {
  WidgetType type;
  float x, y;
  float w, h;
  char text[128];
  float r, g, b;
  int selected;
  int is_dragging;
  int texture_id; // For WIDGET_IMAGE (-1 otherwise)
  int path_start_idx; // For WIDGET_PATH
  int path_point_len; // For WIDGET_PATH
  float bg_r, bg_g, bg_b, bg_a;
  float border_r, border_g, border_b, border_a;
  float font_size;
} Node;

// Draw functions
void draw_node_widget(Node *n, int is_editing, int default_texture_id);

#endif // WIDGETS_H
