/* Windows backend. Kept in its own file because <windows.h> clashes with ClassiCube's headers. */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "gdi_text.h"

#ifndef GGI_MARK_NONEXISTING_GLYPHS
#define GGI_MARK_NONEXISTING_GLYPHS 0x0001
#endif

void gdi_free_mask(unsigned char* mask) { free(mask); }

int gdi_render_mask(const uint16_t* text, int n, const char* face, int px,
                    unsigned char** mask, int* outW, int* outH) {
	wchar_t wface[64];
	HDC dc; HFONT font, oldFont; HBITMAP bmp, oldBmp;
	WORD glyphs[512], q;
	SIZE sz; BITMAPINFO bi; void* bits = NULL;
	int i, w, h, ok = 0;
	unsigned char* m;

	if (n <= 0 || n > 512) return 0;
	MultiByteToWideChar(CP_UTF8, 0, face, -1, wface, 64);

	dc = CreateCompatibleDC(NULL);
	if (!dc) return 0;
	font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
	                   CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wface);
	oldFont = (HFONT)SelectObject(dc, font);

	/* Map to glyph indices ourselves: this bypasses GDI's own shaping/reordering, so the
	   already-shaped, already-reordered text is drawn exactly left-to-right as given. */
	if (GetGlyphIndicesW(dc, (LPCWSTR)text, n, glyphs, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR) goto done;
	{
		WCHAR qm = L'?';
		if (GetGlyphIndicesW(dc, &qm, 1, &q, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR) q = 0;
	}
	for (i = 0; i < n; i++) if (glyphs[i] == 0xFFFF) glyphs[i] = q;

	if (!GetTextExtentPointI(dc, glyphs, n, &sz)) goto done;
	w = sz.cx + 4; h = sz.cy + 2;

	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; /* top-down */
	bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
	bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!bmp || !bits) goto done;
	oldBmp = (HBITMAP)SelectObject(dc, bmp);

	memset(bits, 0, (size_t)w * h * 4);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, RGB(255, 255, 255));
	ExtTextOutW(dc, 2, 1, ETO_GLYPH_INDEX, NULL, (LPCWSTR)glyphs, n, NULL);
	GdiFlush();

	m = (unsigned char*)malloc((size_t)w * h);
	if (m) {
		const unsigned char* src = (const unsigned char*)bits;
		for (i = 0; i < w * h; i++) m[i] = src[i * 4 + 1]; /* grayscale AA: any channel works */
		*mask = m; *outW = w; *outH = h; ok = 1;
	}
	SelectObject(dc, oldBmp);
	DeleteObject(bmp);
done:
	SelectObject(dc, oldFont);
	DeleteObject(font);
	DeleteDC(dc);
	return ok;
}
