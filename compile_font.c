#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

#define CELL_W 64
#define CELL_H 64
#define GRID_SIZE 32
#define ATLAS_SIZE 2048
#define FONT_SIZE 38.0f
#define PADDING 8
#define SPREAD 3.0f

unsigned char atlas_pixels[ATLAS_SIZE * ATLAS_SIZE * 3];

typedef struct {
    uint32_t codepoint;
    uint16_t atlas_index;
    float advance;
} GlyphInfo;

GlyphInfo font_glyphs[GRID_SIZE * GRID_SIZE];
int font_glyphs_count = 0;

typedef struct {
    float x, y;
} Point;

typedef struct {
    Point start;
    Point end;
    int colorMask; // 1=R, 2=G, 4=B
} Segment;

typedef struct {
    Point *points;
    int count;
    int capacity;
} Contour;

typedef struct {
    Segment *data;
    int count;
    int capacity;
} SegmentArray;

void add_point(Contour *c, float x, float y) {
    if (c->count > 0) {
        Point prev = c->points[c->count - 1];
        if (prev.x == x && prev.y == y) return;
    }
    if (c->count >= c->capacity) {
        c->capacity = c->capacity == 0 ? 32 : c->capacity * 2;
        c->points = realloc(c->points, c->capacity * sizeof(Point));
    }
    c->points[c->count++] = (Point){x, y};
}

void add_segment(SegmentArray *arr, Segment seg) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 64 : arr->capacity * 2;
        arr->data = realloc(arr->data, arr->capacity * sizeof(Segment));
    }
    arr->data[arr->count++] = seg;
}

int is_corner(Point prev, Point curr, Point next) {
    float incomingX = curr.x - prev.x;
    float incomingY = curr.y - prev.y;
    float outgoingX = next.x - curr.x;
    float outgoingY = next.y - curr.y;
    float incomingLength = sqrtf(incomingX * incomingX + incomingY * incomingY);
    float outgoingLength = sqrtf(outgoingX * outgoingX + outgoingY * outgoingY);
    if (incomingLength == 0.0f || outgoingLength == 0.0f) {
        return 0;
    }
    float cosine = (incomingX * outgoingX + incomingY * outgoingY) / (incomingLength * outgoingLength);
    return cosine < 0.9f;
}

void color_contour(Contour contour, SegmentArray *all_segments) {
    if (contour.count < 2) return;
    
    int *corner_indices = malloc(contour.count * sizeof(int));
    int num_corners = 0;
    for (int i = 0; i < contour.count; i++) {
        Point prev = contour.points[(i + contour.count - 1) % contour.count];
        Point curr = contour.points[i];
        Point next = contour.points[(i + 1) % contour.count];
        if (is_corner(prev, curr, next)) {
            corner_indices[num_corners++] = i;
        }
    }
    
    int color_masks[3] = {6, 5, 3}; // 0b110, 0b101, 0b011
    
    for (int i = 0; i < contour.count; i++) {
        int color_idx;
        if (num_corners >= 3) {
            int span = 0;
            for (int k = 0; k < num_corners; k++) {
                if (corner_indices[k] <= i) {
                    span = k;
                }
            }
            color_idx = span % 3;
        } else {
            color_idx = i * 3 / contour.count;
            if (color_idx > 2) color_idx = 2;
        }
        
        Segment seg = {
            contour.points[i],
            contour.points[(i + 1) % contour.count],
            color_masks[color_idx]
        };
        add_segment(all_segments, seg);
    }
    free(corner_indices);
}

float segment_distance_squared(Segment seg, float x, float y) {
    float dx = seg.end.x - seg.start.x;
    float dy = seg.end.y - seg.start.y;
    float len_sq = dx * dx + dy * dy;
    float proj = 0.0f;
    if (len_sq != 0.0f) {
        proj = ((x - seg.start.x) * dx + (y - seg.start.y) * dy) / len_sq;
        if (proj < 0.0f) proj = 0.0f;
        if (proj > 1.0f) proj = 1.0f;
    }
    float off_x = x - (seg.start.x + proj * dx);
    float off_y = y - (seg.start.y + proj * dy);
    return off_x * off_x + off_y * off_y;
}

int point_inside(Contour *contours, int num_contours, float x, float y) {
    // Even-odd ray casting (direction-agnostic, works with Y-negated coords)
    int crossings = 0;
    for (int c = 0; c < num_contours; c++) {
        Contour contour = contours[c];
        if (contour.count < 2) continue;
        for (int i = 0; i < contour.count; i++) {
            Point a = contour.points[i];
            Point b = contour.points[(i + 1) % contour.count];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                float t = (y - a.y) / (b.y - a.y);
                float ix = a.x + t * (b.x - a.x);
                if (x < ix) {
                    crossings++;
                }
            }
        }
    }
    return crossings & 1;
}

void generate_msdf_sample(SegmentArray segments, Contour *contours, int num_contours, float x, float y, float *out_rgb) {
    float r_min_sq = INFINITY;
    float g_min_sq = INFINITY;
    float b_min_sq = INFINITY;
    
    for (int i = 0; i < segments.count; i++) {
        Segment seg = segments.data[i];
        float dist_sq = segment_distance_squared(seg, x, y);
        if ((seg.colorMask & 1) != 0) {
            if (dist_sq < r_min_sq) r_min_sq = dist_sq;
        }
        if ((seg.colorMask & 2) != 0) {
            if (dist_sq < g_min_sq) g_min_sq = dist_sq;
        }
        if ((seg.colorMask & 4) != 0) {
            if (dist_sq < b_min_sq) b_min_sq = dist_sq;
        }
    }
    
    if (r_min_sq == INFINITY) r_min_sq = 1000000.0f;
    if (g_min_sq == INFINITY) g_min_sq = 1000000.0f;
    if (b_min_sq == INFINITY) b_min_sq = 1000000.0f;
    
    float sign = point_inside(contours, num_contours, x, y) ? 1.0f : -1.0f;
    out_rgb[0] = sign * sqrtf(r_min_sq);
    out_rgb[1] = sign * sqrtf(g_min_sq);
    out_rgb[2] = sign * sqrtf(b_min_sq);
}

void check_and_add_contour(Contour **contours, int *num_contours, Contour curr) {
    if (curr.count > 1) {
        Point first = curr.points[0];
        Point last = curr.points[curr.count - 1];
        if (first.x == last.x && first.y == last.y) {
            curr.count--;
        }
    }
    if (curr.count > 1) {
        *contours = realloc(*contours, (*num_contours + 1) * sizeof(Contour));
        (*contours)[*num_contours] = curr;
        (*num_contours)++;
    } else {
        if (curr.points) free(curr.points);
    }
}

void write_be32(unsigned char *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

void write_png_chunk(FILE *f, const char *type, const unsigned char *data, uint32_t len) {
    unsigned char len_buf[4];
    write_be32(len_buf, len);
    fwrite(len_buf, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (len > 0) {
        fwrite(data, 1, len, f);
    }
    
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (len > 0) {
        crc = crc32(crc, (const Bytef *)data, len);
    }
    unsigned char crc_buf[4];
    write_be32(crc_buf, crc);
    fwrite(crc_buf, 1, 4, f);
}

int save_png(const char *filename, const unsigned char *rgb_pixels, int width, int height) {
    uint32_t raw_size = height * (1 + width * 3);
    unsigned char *raw_data = malloc(raw_size);
    if (!raw_data) return 0;
    
    for (int y = 0; y < height; y++) {
        uint32_t row_dest = y * (1 + width * 3);
        uint32_t row_src = y * width * 3;
        raw_data[row_dest] = 0; // Filter type: None
        memcpy(raw_data + row_dest + 1, rgb_pixels + row_src, width * 3);
    }
    
    uLongf compressed_size = compressBound(raw_size);
    unsigned char *compressed_data = malloc(compressed_size);
    if (!compressed_data) {
        free(raw_data);
        return 0;
    }
    
    if (compress(compressed_data, &compressed_size, raw_data, raw_size) != Z_OK) {
        free(raw_data);
        free(compressed_data);
        return 0;
    }
    
    FILE *png_file = fopen(filename, "wb");
    if (!png_file) {
        free(raw_data);
        free(compressed_data);
        return 0;
    }
    
    unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(sig, 1, 8, png_file);
    
    unsigned char ihdr[13];
    write_be32(ihdr, width);
    write_be32(ihdr + 4, height);
    ihdr[8] = 8; // bit depth
    ihdr[9] = 2; // color type RGB
    ihdr[10] = 0; // compression method
    ihdr[11] = 0; // filter method
    ihdr[12] = 0; // interlace method
    write_png_chunk(png_file, "IHDR", ihdr, 13);
    
    write_png_chunk(png_file, "IDAT", compressed_data, compressed_size);
    write_png_chunk(png_file, "IEND", NULL, 0);
    
    fclose(png_file);
    free(raw_data);
    free(compressed_data);
    return 1;
}

int is_unicode_supported(uint32_t cp) {
    if (cp >= 32 && cp <= 126) return 1;
    if (cp >= 160 && cp <= 255) return 1;
    if (cp >= 256 && cp <= 383) return 1;
    if (cp >= 384 && cp <= 591) return 1;
    if (cp >= 1024 && cp <= 1279) return 1;
    if (cp >= 8192 && cp <= 8303) return 1;
    if (cp >= 8352 && cp <= 8399) return 1;
    if (cp >= 8592 && cp <= 9215) return 1;
    if (cp >= 0xE000 && cp <= 0xE07E) return 1;
    return 0;
}

int main() {
    FILE *f = fopen("Outfit-Medium.ttf", "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open Outfit-Medium.ttf\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *font_buffer = malloc(size);
    if (!font_buffer) {
        fprintf(stderr, "Error: Out of memory reading font file\n");
        fclose(f);
        return 1;
    }
    size_t read_bytes = fread(font_buffer, 1, size, f);
    fclose(f);
    if (read_bytes != size) {
        fprintf(stderr, "Error: Failed to read complete font file\n");
        free(font_buffer);
        return 1;
    }

    stbtt_fontinfo info;
    int offset = stbtt_GetFontOffsetForIndex(font_buffer, 0);
    if (offset < 0) {
        fprintf(stderr, "Error: Invalid font file format (offset < 0)\n");
        free(font_buffer);
        return 1;
    }
    if (!stbtt_InitFont(&info, font_buffer, offset)) {
        fprintf(stderr, "Error: Could not initialize stb_truetype font info\n");
        free(font_buffer);
        return 1;
    }

    const char *fallback_paths[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/TTF/arial.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
    FILE *f_fallback = NULL;
    for (int i = 0; i < sizeof(fallback_paths)/sizeof(fallback_paths[0]); i++) {
        f_fallback = fopen(fallback_paths[i], "rb");
        if (f_fallback) {
            fprintf(stderr, "Using fallback font: %s\n", fallback_paths[i]);
            break;
        }
    }
    unsigned char *fallback_font_buffer = NULL;
    stbtt_fontinfo fallback_info;
    int has_fallback = 0;
    float fallback_scale = 0.0f;

    if (f_fallback) {
        fseek(f_fallback, 0, SEEK_END);
        long fb_size = ftell(f_fallback);
        fseek(f_fallback, 0, SEEK_SET);
        fallback_font_buffer = malloc(fb_size);
        if (fallback_font_buffer) {
            fread(fallback_font_buffer, 1, fb_size, f_fallback);
            int fb_offset = stbtt_GetFontOffsetForIndex(fallback_font_buffer, 0);
            if (fb_offset >= 0 && stbtt_InitFont(&fallback_info, fallback_font_buffer, fb_offset)) {
                has_fallback = 1;
                fallback_scale = stbtt_ScaleForPixelHeight(&fallback_info, FONT_SIZE);
            }
        }
        fclose(f_fallback);
    }

    const char *mono_paths[] = {
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/System/Library/Fonts/Courier.ttc",
        "/System/Library/Fonts/Supplemental/PTMono.ttc",
        "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
        "/usr/share/fonts/TTF/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\cour.ttf"
    };
    FILE *f_mono = NULL;
    for (int i = 0; i < sizeof(mono_paths)/sizeof(mono_paths[0]); i++) {
        f_mono = fopen(mono_paths[i], "rb");
        if (f_mono) {
            fprintf(stderr, "Using monospace font: %s\n", mono_paths[i]);
            break;
        }
    }
    unsigned char *mono_font_buffer = NULL;
    stbtt_fontinfo mono_info;
    int has_mono = 0;
    float mono_scale = 0.0f;

    if (f_mono) {
        fseek(f_mono, 0, SEEK_END);
        long mono_size = ftell(f_mono);
        fseek(f_mono, 0, SEEK_SET);
        mono_font_buffer = malloc(mono_size);
        if (mono_font_buffer) {
            fread(mono_font_buffer, 1, mono_size, f_mono);
            int mono_offset = stbtt_GetFontOffsetForIndex(mono_font_buffer, 0);
            if (mono_offset >= 0 && stbtt_InitFont(&mono_info, mono_font_buffer, mono_offset)) {
                has_mono = 1;
                mono_scale = stbtt_ScaleForPixelHeight(&mono_info, FONT_SIZE);
            }
        }
        fclose(f_mono);
    }

    memset(atlas_pixels, 0, sizeof(atlas_pixels));
    float scale = stbtt_ScaleForPixelHeight(&info, FONT_SIZE);

    // Solid block character (index 1)
    int col1 = 1 % GRID_SIZE;
    int row1 = 1 / GRID_SIZE;
    for (int y = 0; y < CELL_H; y++) {
        for (int x = 0; x < CELL_W; x++) {
            int dest_idx = ((row1 * CELL_H + y) * ATLAS_SIZE + (col1 * CELL_W + x)) * 3;
            atlas_pixels[dest_idx] = 255;
            atlas_pixels[dest_idx + 1] = 255;
            atlas_pixels[dest_idx + 2] = 255;
        }
    }

    int next_atlas_index = 2; // Cell 0 is empty, Cell 1 is solid block

    for (uint32_t cp = 1; cp < 65536; cp++) {
        if (!is_unicode_supported(cp)) {
            continue;
        }

        stbtt_fontinfo *active_info = &info;
        float active_scale = scale;
        uint32_t font_cp = cp;
        int is_mono = 0;

        if (cp >= 0xE000 && cp <= 0xE07E) {
            if (!has_mono) {
                continue;
            }
            active_info = &mono_info;
            active_scale = mono_scale;
            font_cp = cp - 0xE000;
            is_mono = 1;
        }

        int glyph_index = stbtt_FindGlyphIndex(active_info, font_cp);

        if (glyph_index == 0 && !is_mono && has_fallback) {
            glyph_index = stbtt_FindGlyphIndex(&fallback_info, font_cp);
            if (glyph_index != 0) {
                active_info = &fallback_info;
                active_scale = fallback_scale;
            }
        }

        if (glyph_index == 0 && font_cp != 32 && font_cp != 160) {
            continue; // Not found and not space
        }

        if (next_atlas_index >= GRID_SIZE * GRID_SIZE) {
            fprintf(stderr, "Warning: Atlas full at codepoint %u\n", cp);
            break;
        }

        int atlas_index = next_atlas_index++;
        
        int col = atlas_index % GRID_SIZE;
        int row = atlas_index / GRID_SIZE;
        int cellX = col * CELL_W;
        int cellY = row * CELL_H;

        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(active_info, font_cp, &advanceWidth, &leftSideBearing);
        float advance = (float)advanceWidth * active_scale / FONT_SIZE;

        // Save glyph info
        font_glyphs[font_glyphs_count].codepoint = cp;
        font_glyphs[font_glyphs_count].atlas_index = atlas_index;
        font_glyphs[font_glyphs_count].advance = advance;
        font_glyphs_count++;

        stbtt_vertex *vertices = NULL;
        int num_vertices = stbtt_GetCodepointShape(active_info, font_cp, &vertices);

        if (num_vertices > 0) {
            int num_contours = 0;
            Contour *contours = NULL;
            Contour curr = {0};

            for (int i = 0; i < num_vertices; i++) {
                stbtt_vertex v = vertices[i];
                float vx = v.x * active_scale;
                float vy = -v.y * active_scale; // Invert Y
                float vcx = v.cx * active_scale;
                float vcy = -v.cy * active_scale;
                float vcx1 = v.cx1 * active_scale;
                float vcy1 = -v.cy1 * active_scale;

                if (v.type == 1) { // Move to
                    if (curr.count > 0) {
                        check_and_add_contour(&contours, &num_contours, curr);
                        curr = (Contour){0};
                    }
                    add_point(&curr, vx, vy);
                } else if (v.type == 2) { // Line to
                    add_point(&curr, vx, vy);
                } else if (v.type == 3) { // Quad curve to
                    Point start = curr.count > 0 ? curr.points[curr.count - 1] : (Point){0, 0};
                    for (int step = 1; step <= 12; step++) {
                        float t = step / 12.0f;
                        float inv = 1.0f - t;
                        float px = inv * inv * start.x + 2.0f * inv * t * vcx + t * t * vx;
                        float py = inv * inv * start.y + 2.0f * inv * t * vcy + t * t * vy;
                        add_point(&curr, px, py);
                    }
                } else if (v.type == 4) { // Cubic curve to
                    Point start = curr.count > 0 ? curr.points[curr.count - 1] : (Point){0, 0};
                    for (int step = 1; step <= 16; step++) {
                        float t = step / 16.0f;
                        float inv = 1.0f - t;
                        float px = inv * inv * inv * start.x +
                                   3.0f * inv * inv * t * vcx +
                                   3.0f * inv * t * t * vcx1 +
                                   t * t * t * vx;
                        float py = inv * inv * inv * start.y +
                                   3.0f * inv * inv * t * vcy +
                                   3.0f * inv * t * t * vcy1 +
                                   t * t * t * vy;
                        add_point(&curr, px, py);
                    }
                }
            }
            if (curr.count > 0) {
                check_and_add_contour(&contours, &num_contours, curr);
            }

            SegmentArray segments = {0};
            for (int i = 0; i < num_contours; i++) {
                color_contour(contours[i], &segments);
            }

            int originX = 24;  // Shifted left from center (32) to fit wide glyphs like W
            int originY = 46;

            // Debug for 'O'
            if (cp == 'O') {
                fprintf(stderr, "DIAG 'O': %d contours, %d segments\n", num_contours, segments.count);
                for (int ci = 0; ci < num_contours; ci++) {
                    float minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
                    for (int pi = 0; pi < contours[ci].count; pi++) {
                        if (contours[ci].points[pi].x < minx) minx = contours[ci].points[pi].x;
                        if (contours[ci].points[pi].x > maxx) maxx = contours[ci].points[pi].x;
                        if (contours[ci].points[pi].y < miny) miny = contours[ci].points[pi].y;
                        if (contours[ci].points[pi].y > maxy) maxy = contours[ci].points[pi].y;
                    }
                    fprintf(stderr, "  contour %d: %d pts, bbox=(%.1f,%.1f)-(%.1f,%.1f)\n",
                            ci, contours[ci].count, minx, miny, maxx, maxy);
                }
                // Test point_inside at expected center of 'O': 
                float testX = 11.0f; // rough horizontal center
                float testY = -13.0f; // rough vertical center
                fprintf(stderr, "  point_inside(%.1f,%.1f) = %d\n", testX, testY, 
                        point_inside(contours, num_contours, testX, testY));
                // Try at 0,0
                fprintf(stderr, "  point_inside(0,0) = %d\n", 
                        point_inside(contours, num_contours, 0.0f, 0.0f));
                // Horizontal scan at y=-10 (mid-glyph) to see ring pattern
                for (float tx = 0; tx <= 24; tx += 1.5f) {
                    fprintf(stderr, "  point_inside(%.1f,-10) = %d\n", tx, 
                            point_inside(contours, num_contours, tx, -10.0f));
                }
            }

            for (int y = 0; y < CELL_H; y++) {
                for (int x = 0; x < CELL_W; x++) {
                    float sampleX = (float)x - (float)originX + 0.5f;
                    float sampleY = (float)y - (float)originY + 0.5f;

                    float rgb_dist[3] = {0, 0, 0};
                    generate_msdf_sample(segments, contours, num_contours, sampleX, sampleY, rgb_dist);

                    int r_val = (int)roundf(fmaxf(0.0f, fminf(1.0f, 0.5f + rgb_dist[0] / SPREAD)) * 255.0f);
                    int g_val = (int)roundf(fmaxf(0.0f, fminf(1.0f, 0.5f + rgb_dist[1] / SPREAD)) * 255.0f);
                    int b_val = (int)roundf(fmaxf(0.0f, fminf(1.0f, 0.5f + rgb_dist[2] / SPREAD)) * 255.0f);

                    int dest_idx = ((cellY + y) * ATLAS_SIZE + (cellX + x)) * 3;
                    atlas_pixels[dest_idx] = r_val;
                    atlas_pixels[dest_idx + 1] = g_val;
                    atlas_pixels[dest_idx + 2] = b_val;
                }
            }

            if (segments.data) free(segments.data);
            for (int i = 0; i < num_contours; i++) {
                if (contours[i].points) free(contours[i].points);
            }
            if (contours) free(contours);
            stbtt_FreeShape(active_info, vertices);
        }
    }

    FILE *out = fopen("font.generated.h", "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open font.generated.h for writing\n");
        free(font_buffer);
        if (fallback_font_buffer) free(fallback_font_buffer);
        return 1;
    }

    fprintf(out, "/* Generated by compile_font.c */\n");
    fprintf(out, "#ifndef FONT_GENERATED_H\n");
    fprintf(out, "#define FONT_GENERATED_H\n\n");
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "typedef struct {\n");
    fprintf(out, "    uint32_t codepoint;\n");
    fprintf(out, "    uint16_t atlas_index;\n");
    fprintf(out, "    float advance;\n");
    fprintf(out, "} GlyphInfo;\n\n");
    fprintf(out, "#define FONT_GLYPHS_COUNT %d\n\n", font_glyphs_count);
    fprintf(out, "static const GlyphInfo font_glyphs[FONT_GLYPHS_COUNT] = {\n");
    for (int i = 0; i < font_glyphs_count; i++) {
        fprintf(out, "  { %u, %u, %ff },\n", 
                font_glyphs[i].codepoint, 
                font_glyphs[i].atlas_index, 
                font_glyphs[i].advance);
    }
    fprintf(out, "};\n\n");
    fprintf(out, "#endif\n");
    fclose(out);

    // Save RGB pixels directly as a compressed PNG atlas
    if (!save_png("font_atlas.png", atlas_pixels, ATLAS_SIZE, ATLAS_SIZE)) {
        fprintf(stderr, "Error: Failed to write complete font_atlas.png\n");
        free(font_buffer);
        if (fallback_font_buffer) free(fallback_font_buffer);
        if (mono_font_buffer) free(mono_font_buffer);
        return 1;
    }

    free(font_buffer);
    if (fallback_font_buffer) free(fallback_font_buffer);
    if (mono_font_buffer) free(mono_font_buffer);

    // Diagnostic: horizontal scan of 'O' atlas at mid-glyph
    {
        int diag_cp = 'O';
        int diag_atlas_idx = -1;
        for (int i = 0; i < font_glyphs_count; i++) {
            if (font_glyphs[i].codepoint == diag_cp) {
                diag_atlas_idx = font_glyphs[i].atlas_index;
                break;
            }
        }
        if (diag_atlas_idx != -1) {
            int diag_row = diag_atlas_idx / GRID_SIZE;
            int diag_col = diag_atlas_idx % GRID_SIZE;
            // Glyph ring at y=-10 (for FONT_SIZE=38.0f) maps to cell y = originY + (-10) = 46 - 10 = 36
            int cy = diag_row * CELL_H + 36;
            fprintf(stderr, "DIAG 'O' atlas horizontal scan at cell-relative y=36 (sampleY=-9.5):\n");
            for (int dx = 0; dx < CELL_W; dx += 2) {
                int sx = diag_col * CELL_W + dx;
                int si = (cy * ATLAS_SIZE + sx) * 3;
                fprintf(stderr, "  cell_x=%3d: R=%3d G=%3d B=%3d\n", dx, atlas_pixels[si], atlas_pixels[si+1], atlas_pixels[si+2]);
            }
        }
    }

    printf("Successfully wrote font.generated.h with %dx%d 3-channel MSDF atlas (found %d glyphs).\n", ATLAS_SIZE, ATLAS_SIZE, font_glyphs_count);
    return 0;
}
