#ifndef GDI_TEXT_H
#define GDI_TEXT_H
#include <stdint.h>
/* Rasterises UTF-16 text (already in visual order) with a system font into an 8-bit alpha mask.
   Returns 1 on success; *mask is malloc'd, free it with gdi_free_mask. Glyphs the font lacks become '?'. */
int  gdi_render_mask(const uint16_t* text, int n, const char* face, int px,
                     unsigned char** mask, int* w, int* h);
void gdi_free_mask(unsigned char* mask);
#endif
