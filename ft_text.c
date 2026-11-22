/* Linux backend: FreeType + fontconfig. Implements sys_text.h. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>
#include <stdlib.h>
#include <string.h>
#include "sys_text.h"

static FT_Library lib;
static FT_Face faceA, faceB;      /* A = requested family, B = fallback that has Arabic glyphs */
static char    cachedName[64];
static int     cachedPx;

static FT_Face open_match(const char* family, int needArabic) {
	FcPattern *pat, *match; FcResult res; FcChar8* file = NULL; FT_Face f = NULL;
	FcCharSet* cs = NULL;
	pat = FcPatternCreate();
	if (family && family[0]) FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family);
	if (needArabic) {
		cs = FcCharSetCreate();
		FcCharSetAddChar(cs, 0xFE8E); FcCharSetAddChar(cs, 0xFEE3); FcCharSetAddChar(cs, 0x0627);
		FcPatternAddCharSet(pat, FC_CHARSET, cs);
	}
	FcConfigSubstitute(NULL, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	match = FcFontMatch(NULL, pat, &res);
	if (match && FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
		int idx = 0; FcPatternGetInteger(match, FC_INDEX, 0, &idx);
		if (FT_New_Face(lib, (const char*)file, idx, &f)) f = NULL;
	}
	if (match) FcPatternDestroy(match);
	if (cs) FcCharSetDestroy(cs);
	FcPatternDestroy(pat);
	return f;
}

static int ensure_faces(const char* name, int px) {
	if (!lib && FT_Init_FreeType(&lib)) return 0;
	if (!faceA || strcmp(name, cachedName) != 0) {
		if (faceA) FT_Done_Face(faceA);
		if (faceB) FT_Done_Face(faceB);
		faceA = open_match(name, 0);
		faceB = open_match(name, 1);
		strncpy(cachedName, name, sizeof(cachedName) - 1); cachedName[sizeof(cachedName) - 1] = 0;
		cachedPx = 0;
	}
	if (!faceA) return 0;
	if (cachedPx != px) {
		FT_Set_Pixel_Sizes(faceA, 0, px);
		if (faceB) FT_Set_Pixel_Sizes(faceB, 0, px);
		cachedPx = px;
	}
	return 1;
}

/* pick the face + glyph index for a codepoint; falls back to '?' */
static FT_Face pick(unsigned cp, FT_UInt* gi) {
	FT_UInt g = FT_Get_Char_Index(faceA, cp);
	if (g) { *gi = g; return faceA; }
	if (faceB && (g = FT_Get_Char_Index(faceB, cp)) != 0) { *gi = g; return faceB; }
	*gi = FT_Get_Char_Index(faceA, '?');
	return faceA;
}

void sys_free_mask(unsigned char* mask) { free(mask); }

int sys_render_mask(const uint16_t* text, int n, const char* face, int px,
                    unsigned char** mask, int* outW, int* outH) {
	int i, pass, pen, asc, desc, w = 0, h, baseline;
	unsigned char* m = NULL;
	if (n <= 0 || n > 512 || !ensure_faces(face, px)) return 0;

	asc  = (int)(faceA->size->metrics.ascender  + 63) >> 6;
	desc = (int)(-faceA->size->metrics.descender + 63) >> 6;
	h = asc + desc + 2; baseline = asc + 1;

	for (pass = 0; pass < 2; pass++) {
		pen = 2;
		for (i = 0; i < n; i++) {
			FT_UInt gi; FT_Face f = pick(text[i], &gi);
			if (FT_Load_Glyph(f, gi, pass ? FT_LOAD_RENDER : FT_LOAD_DEFAULT)) continue;
			if (pass) {
				FT_Bitmap* b = &f->glyph->bitmap;
				int gx = pen + f->glyph->bitmap_left, gy = baseline - f->glyph->bitmap_top, r, c;
				for (r = 0; r < (int)b->rows; r++) for (c = 0; c < (int)b->width; c++) {
					int x = gx + c, y = gy + r, v;
					if (x < 0 || x >= w || y < 0 || y >= h) continue;
					v = b->buffer[r * b->pitch + c];
					if (v > m[y * w + x]) m[y * w + x] = (unsigned char)v;
				}
			}
			pen += (int)(f->glyph->advance.x >> 6);
		}
		if (!pass) {
			w = pen + 4;
			m = (unsigned char*)calloc((size_t)w * h, 1);
			if (!m) return 0;
		}
	}
	*mask = m; *outW = w; *outH = h;
	return 1;
}
