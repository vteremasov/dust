#ifndef INITIAL_CONTENT_H
#define INITIAL_CONTENT_H

#include "ecs.h"

// Entity creation helpers
Entity add_entity_full(WidgetType type, float world_x, float world_y, float w, float h, const char *text, int texture_id);
Entity add_connection_entity(int from, int to);

// Generators
void generate_initial_content();
void generate_100k_infographics();

#endif // INITIAL_CONTENT_H
