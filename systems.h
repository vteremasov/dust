#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "ecs.h"

// Math and border utility prototypes
float get_shape_border_offset(Entity e, float ux, float uy);
float distance_to_segment(float px, float py, float x1, float y1, float x2, float y2);

// ECS Systems
void render_system(float left, float right, float top, float bottom, int editing_node_idx, int texture_id);
void drag_system(float dx, float dy);
void marquee_system(float draw_x, float draw_y, float draw_w, float draw_h, int *selected_node_idx);

#endif // SYSTEMS_H
