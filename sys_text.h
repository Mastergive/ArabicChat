#ifndef SYS_TEXT_H
#define SYS_TEXT_H
#include <stdint.h>
/* Rasterises UTF-16 text (implemented by gdi_text.c on Windows, ft_text.c on Linux) (already in visual order) with a system font into an 8-bit alpha mask.
   Returns 1 on success; *mask is malloc'd, free it with sys_free_mask. Glyphs the font lacks become '?'. */
int  sys_render_mask(const uint16_t* text, int n, const char* face, int px,
                     unsigned char** mask, int* w, int* h);
void sys_free_mask(unsigned char* mask);
#endif
