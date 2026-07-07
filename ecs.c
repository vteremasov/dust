#include "ecs.h"

unsigned int entity_count = 0;
ComponentMask entity_masks[MAX_ENTITIES];
TransformComponent transform_components[MAX_ENTITIES];
RenderComponent render_components[MAX_ENTITIES];
TextComponent text_components[MAX_ENTITIES];
PathDrawingComponent path_components[MAX_ENTITIES];
ConnectionComponent connection_components[MAX_ENTITIES];
InteractionComponent interaction_components[MAX_ENTITIES];

PathPoint path_points[MAX_PATH_POINTS];
int path_point_count = 0;

#define TEXT_HEAP_SIZE (24 * 1024 * 1024)
static char text_heap[TEXT_HEAP_SIZE];
static unsigned int text_heap_offset = 0;

void reset_text_heap() {
  text_heap_offset = 0;
  text_heap[0] = '\0';
}

char *allocate_text(const char *src, unsigned int len) {
  if (text_heap_offset + len + 1 > TEXT_HEAP_SIZE) {
    return "";
  }
  char *dest = &text_heap[text_heap_offset];
  for (unsigned int i = 0; i < len; i++) {
    dest[i] = src[i];
  }
  dest[len] = '\0';
  text_heap_offset += len + 1;
  return dest;
}

void ecs_init() {
  entity_count = 0;
  path_point_count = 0;
  reset_text_heap();
}

Entity ecs_create_entity() {
  if (entity_count >= MAX_ENTITIES) {
    return (Entity)-1;
  }
  Entity e = entity_count++;
  entity_masks[e] = COMP_NONE;
  
  // Clean initialization of memory
  transform_components[e].x = 0.0f;
  transform_components[e].y = 0.0f;
  transform_components[e].w = 0.0f;
  transform_components[e].h = 0.0f;
  
  render_components[e].type = WIDGET_STICKY;
  render_components[e].bg_r = 0.0f;
  render_components[e].bg_g = 0.0f;
  render_components[e].bg_b = 0.0f;
  render_components[e].bg_a = 0.0f;
  render_components[e].border_r = 0.0f;
  render_components[e].border_g = 0.0f;
  render_components[e].border_b = 0.0f;
  render_components[e].border_a = 0.0f;
  render_components[e].r = 0.0f;
  render_components[e].g = 0.0f;
  render_components[e].b = 0.0f;
  render_components[e].font_size = 18.0f;
  render_components[e].texture_id = -1;
  
  text_components[e].text = "";
  
  path_components[e].path_start_idx = 0;
  path_components[e].path_point_len = 0;
  
  connection_components[e].from_entity = -1;
  connection_components[e].to_entity = -1;
  
  interaction_components[e].selected = 0;
  interaction_components[e].is_dragging = 0;
  
  return e;
}

#define WASM_IMPORT(name) __attribute__((import_module("env"), import_name(name)))
WASM_IMPORT("js_on_entity_deleted") void js_on_entity_deleted(int entity);

void ecs_delete_entity(Entity entity) {
  if (entity >= entity_count) return;

  // 1. Delete connection entities referencing this entity
  for (unsigned int i = 0; i < entity_count; ) {
    if (ecs_has_component(i, COMP_CONNECTION)) {
      ConnectionComponent *conn = &connection_components[i];
      if (conn->from_entity == (int)entity || conn->to_entity == (int)entity) {
        js_on_entity_deleted((int)i);
        // Shift components to delete the connection entity i
        for (unsigned int k = i; k < entity_count - 1; k++) {
          entity_masks[k] = entity_masks[k + 1];
          transform_components[k] = transform_components[k + 1];
          render_components[k] = render_components[k + 1];
          text_components[k] = text_components[k + 1];
          path_components[k] = path_components[k + 1];
          connection_components[k] = connection_components[k + 1];
          interaction_components[k] = interaction_components[k + 1];
        }
        entity_count--;
        if (i < entity) {
          entity--;
        }
        continue;
      }
    }
    i++;
  }

  // 2. Adjust remaining connection references
  for (unsigned int i = 0; i < entity_count; i++) {
    if (ecs_has_component(i, COMP_CONNECTION)) {
      ConnectionComponent *conn = &connection_components[i];
      if (conn->from_entity > (int)entity) {
        conn->from_entity--;
      }
      if (conn->to_entity > (int)entity) {
        conn->to_entity--;
      }
    }
  }

  js_on_entity_deleted((int)entity);
  // 3. Move/Shift entities starting from deleted one
  for (unsigned int k = entity; k < entity_count - 1; k++) {
    entity_masks[k] = entity_masks[k + 1];
    transform_components[k] = transform_components[k + 1];
    render_components[k] = render_components[k + 1];
    text_components[k] = text_components[k + 1];
    path_components[k] = path_components[k + 1];
    connection_components[k] = connection_components[k + 1];
    interaction_components[k] = interaction_components[k + 1];
  }
  entity_count--;
}

void ecs_add_component(Entity entity, ComponentMask component) {
  if (entity < entity_count) {
    entity_masks[entity] |= component;
  }
}

void ecs_remove_component(Entity entity, ComponentMask component) {
  if (entity < entity_count) {
    entity_masks[entity] &= ~component;
  }
}


