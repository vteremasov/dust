#ifndef ECS_H
#define ECS_H

#define MAX_ENTITIES 2700000
#define MAX_PATH_POINTS 20000

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef enum {
  WIDGET_STICKY = 0,
  WIDGET_RECT = 1,
  WIDGET_OVAL = 2,
  WIDGET_TEXT = 3,
  WIDGET_IMAGE = 4,
  WIDGET_PATH = 5,
  WIDGET_ARROW = 6,
  WIDGET_TRIANGLE = 7
} WidgetType;

typedef struct {
  float x, y;
} PathPoint;

typedef unsigned int Entity;

typedef enum {
  COMP_NONE        = 0,
  COMP_TRANSFORM   = 1 << 0,
  COMP_RENDER      = 1 << 1,
  COMP_TEXT        = 1 << 2,
  COMP_PATH        = 1 << 3,
  COMP_CONNECTION  = 1 << 4,
  COMP_INTERACTION = 1 << 5
} ComponentMask;

typedef struct {
  float x, y;
  float w, h;
} TransformComponent;

typedef struct {
  WidgetType type;
  float bg_r, bg_g, bg_b, bg_a;
  float border_r, border_g, border_b, border_a;
  float r, g, b; // Text / foreground color
  float font_size;
  int texture_id;
} RenderComponent;

typedef struct {
  char text[128];
} TextComponent;

typedef struct {
  int path_start_idx;
  int path_point_len;
} PathDrawingComponent;

typedef struct {
  int from_entity;
  int to_entity;
} ConnectionComponent;

typedef struct {
  int selected;
  int is_dragging;
} InteractionComponent;

// ECS Globals
extern unsigned int entity_count;
extern ComponentMask entity_masks[MAX_ENTITIES];
extern TransformComponent transform_components[MAX_ENTITIES];
extern RenderComponent render_components[MAX_ENTITIES];
extern TextComponent text_components[MAX_ENTITIES];
extern PathDrawingComponent path_components[MAX_ENTITIES];
extern ConnectionComponent connection_components[MAX_ENTITIES];
extern InteractionComponent interaction_components[MAX_ENTITIES];

// Path points array (referenced by COMP_PATH)
extern PathPoint path_points[MAX_PATH_POINTS];
extern int path_point_count;

// ECS API
void ecs_init();
Entity ecs_create_entity();
void ecs_delete_entity(Entity entity);
void ecs_add_component(Entity entity, ComponentMask component);
void ecs_remove_component(Entity entity, ComponentMask component);
int ecs_has_component(Entity entity, ComponentMask component);

#endif // ECS_H
