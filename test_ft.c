#include "sys_text.h"
#include "arabic_core.h"
#include <stdio.h>
#include <string.h>
int main(void) {
	const char* s = "Ali: \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 \xDA\xAF\xDB\x8C 123"; /* mixed Latin/Arabic/Persian/digits */
	uint32_t cp[64], vis[64]; uint16_t u[64]; unsigned char* m; int w, h, n, i, x, y;
	n = ac_utf8_decode((const unsigned char*)s, (int)strlen(s), cp, 64);
	n = ac_to_visual(cp, n, vis, 64);
	for (i = 0; i < n; i++) u[i] = (uint16_t)vis[i];
	if (!sys_render_mask(u, n, "DejaVu Sans", 24, &m, &w, &h)) { puts("render FAILED"); return 1; }
	printf("mask %dx%d\n", w, h);
	for (y = 0; y < h; y += 2) { for (x = 0; x < w; x++) putchar(m[y*w+x] > 128 ? '#' : (m[y*w+x] > 40 ? '+' : ' ')); putchar('\n'); }
	sys_free_mask(m); return 0;
}
