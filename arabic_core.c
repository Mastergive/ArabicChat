#include "arabic_core.h"
#include <string.h>

/* ---------------- UTF-8 ---------------- */
int ac_utf8_seqlen(unsigned char b) {
	if (b < 0x80) return 1;
	if (b >= 0xC2 && b <= 0xDF) return 2;
	if (b >= 0xE0 && b <= 0xEF) return 3;
	if (b >= 0xF0 && b <= 0xF4) return 4;
	return 0;
}

int ac_utf8_decode(const unsigned char* s, int n, uint32_t* out, int max) {
	int i = 0, c = 0;
	while (i < n) {
		int len = ac_utf8_seqlen(s[i]), k;
		uint32_t cp;
		if (!len || i + len > n || c >= max) return -1;
		if (len == 1) cp = s[i];
		else {
			cp = s[i] & (0xFF >> (len + 1));
			for (k = 1; k < len; k++) {
				if ((s[i + k] & 0xC0) != 0x80) return -1;
				cp = (cp << 6) | (s[i + k] & 0x3F);
			}
			if ((len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) return -1;
		}
		out[c++] = cp; i += len;
	}
	return c;
}

int ac_utf8_decode_lenient(const unsigned char* s, int n, uint32_t* out, int max) {
	int i = 0, c = 0;
	while (i < n && c < max) {
		int len = ac_utf8_seqlen(s[i]);
		uint32_t cp;
		if (len > 1 && i + len <= n && ac_utf8_decode(s + i, len, &cp, 1) == 1) { out[c++] = cp; i += len; }
		else if (len == 1) { out[c++] = s[i]; i++; }
		else { out[c++] = 0xFFFD; i++; }
	}
	return c;
}

int ac_utf8_encode(const uint32_t* cp, int n, unsigned char* out, int max) {
	int i, o = 0;
	for (i = 0; i < n; i++) {
		uint32_t c = cp[i];
		if (c < 0x80) { if (o + 1 > max) return 0; out[o++] = (unsigned char)c; }
		else if (c < 0x800) {
			if (o + 2 > max) return 0;
			out[o++] = 0xC0 | (c >> 6); out[o++] = 0x80 | (c & 0x3F);
		} else if (c < 0x10000) {
			if (o + 3 > max) return 0;
			out[o++] = 0xE0 | (c >> 12); out[o++] = 0x80 | ((c >> 6) & 0x3F); out[o++] = 0x80 | (c & 0x3F);
		} else {
			if (o + 4 > max) return 0;
			out[o++] = 0xF0 | (c >> 18); out[o++] = 0x80 | ((c >> 12) & 0x3F);
			out[o++] = 0x80 | ((c >> 6) & 0x3F); out[o++] = 0x80 | (c & 0x3F);
		}
	}
	return o;
}

int ac_is_tunnelled(const unsigned char* s, int n) {
	int i = 0;
	while (i < n) {
		int len = ac_utf8_seqlen(s[i]), k;
		if (len > 1 && i + len <= n) {
			uint32_t cp = s[i] & (0xFF >> (len + 1));
			int ok = 1;
			for (k = 1; k < len; k++) {
				if ((s[i + k] & 0xC0) != 0x80) { ok = 0; break; }
				cp = (cp << 6) | (s[i + k] & 0x3F);
			}
			if (ok && cp >= 0x100) return 1;
		}
		i += len ? len : 1;
	}
	return 0;
}

int ac_utf8_prefix(const unsigned char* s, int n, int maxBytes) {
	int i = 0;
	while (i < n) {
		int len = ac_utf8_seqlen(s[i]);
		if (!len) len = 1;
		if (i + len > maxBytes || i + len > n) break;
		i += len;
	}
	return i;
}

/* ---------------- Joining tables ---------------- */
typedef struct { uint16_t base, iso, fin, ini, med; } JoinRow;
#define D4(b, i) { b, i, (i) + 1, (i) + 2, (i) + 3 } /* dual-joining */
#define R2(b, i) { b, i, (i) + 1, 0, 0 }             /* right-joining */

static const JoinRow rows[] = {
	{ 0x0621, 0xFE80, 0, 0, 0 },
	R2(0x0622, 0xFE81), R2(0x0623, 0xFE83), R2(0x0624, 0xFE85), R2(0x0625, 0xFE87),
	D4(0x0626, 0xFE89), R2(0x0627, 0xFE8D), D4(0x0628, 0xFE8F), R2(0x0629, 0xFE93),
	D4(0x062A, 0xFE95), D4(0x062B, 0xFE99), D4(0x062C, 0xFE9D), D4(0x062D, 0xFEA1), D4(0x062E, 0xFEA5),
	R2(0x062F, 0xFEA9), R2(0x0630, 0xFEAB), R2(0x0631, 0xFEAD), R2(0x0632, 0xFEAF),
	D4(0x0633, 0xFEB1), D4(0x0634, 0xFEB5), D4(0x0635, 0xFEB9), D4(0x0636, 0xFEBD),
	D4(0x0637, 0xFEC1), D4(0x0638, 0xFEC5), D4(0x0639, 0xFEC9), D4(0x063A, 0xFECD),
	{ 0x0640, 0x0640, 0x0640, 0x0640, 0x0640 }, /* tatweel */
	D4(0x0641, 0xFED1), D4(0x0642, 0xFED5), D4(0x0643, 0xFED9), D4(0x0644, 0xFEDD),
	D4(0x0645, 0xFEE1), D4(0x0646, 0xFEE5), D4(0x0647, 0xFEE9),
	R2(0x0648, 0xFEED), R2(0x0649, 0xFEEF), D4(0x064A, 0xFEF1),
	/* Persian / Urdu extras (Presentation Forms-A) */
	D4(0x067E, 0xFB56), D4(0x0686, 0xFB7A), R2(0x0698, 0xFB8A),
	D4(0x06A9, 0xFB8E), D4(0x06AF, 0xFB92), D4(0x06CC, 0xFBFC)
};
#define NROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static const JoinRow* find_row(uint32_t c) {
	int i;
	if (c < 0x0621 || c > 0x06CC) return 0;
	for (i = 0; i < NROWS; i++) if (rows[i].base == c) return &rows[i];
	return 0;
}

static int is_mark(uint32_t c) {
	return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x06D6 && c <= 0x06ED);
}
static int joins_next(uint32_t c) { const JoinRow* r = find_row(c); return r && r->ini != 0; }
static int joins_prev(uint32_t c) { const JoinRow* r = find_row(c); return r && r->fin != 0; }

/* lam + alef ligatures: {alef, isolated, final} */
static int lam_alef(uint32_t alef, uint32_t* iso, uint32_t* fin) {
	switch (alef) {
	case 0x0622: *iso = 0xFEF5; *fin = 0xFEF6; return 1;
	case 0x0623: *iso = 0xFEF7; *fin = 0xFEF8; return 1;
	case 0x0625: *iso = 0xFEF9; *fin = 0xFEFA; return 1;
	case 0x0627: *iso = 0xFEFB; *fin = 0xFEFC; return 1;
	}
	return 0;
}

/* Shaping: logical -> logical (presentation forms). */
static int shape(const uint32_t* in, int n, uint32_t* out, int max) {
	int i, o = 0;
	for (i = 0; i < n; i++) {
		uint32_t c = in[i];
		const JoinRow* r = find_row(c);
		int j, prevJ = 0, nextJ = 0;
		if (o >= max) break;
		if (!r) { out[o++] = c; continue; }

		for (j = i - 1; j >= 0; j--) { if (is_mark(in[j])) continue; prevJ = joins_next(in[j]); break; }
		for (j = i + 1; j < n; j++)  { if (is_mark(in[j])) continue; nextJ = joins_prev(in[j]); break; }

		if (c == 0x0644 && i + 1 < n) { /* lam followed by alef -> ligature */
			uint32_t iso, fin;
			if (lam_alef(in[i + 1], &iso, &fin)) {
				out[o++] = prevJ ? fin : iso;
				i++; continue;
			}
		}
		if (r->ini == 0) out[o++] = (prevJ && r->fin) ? r->fin : r->iso;               /* right-joining only */
		else if (prevJ && nextJ) out[o++] = r->med;
		else if (prevJ)          out[o++] = r->fin;
		else if (nextJ)          out[o++] = r->ini;
		else                     out[o++] = r->iso;
	}
	return o;
}

/* ---------------- Bidi (simplified UBA) ---------------- */
enum { T_L, T_R, T_EN, T_N, T_M };

static int bidi_type(uint32_t c) {
	if (is_mark(c)) return T_M;
	if ((c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9)) return T_EN;
	if (c == 0x060C || c == 0x061B || c == 0x061F || c == 0x066A || c == 0x066B || c == 0x066C) return T_N;
	if (c < 0x80) return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) ? T_L : T_N;
	if ((c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF)) return T_R;
	if (c == 0x00A0 || (c >= 0x2000 && c <= 0x206F)) return T_N;
	return T_L;
}

#define MAXN 512

/* Reorders in place into visual order. */
static void reorder(uint32_t* s, int n) {
	int type[MAXN], eff[MAXN], level[MAXN];
	int i, j, para = 0, base, maxL = 0, minOdd = 99, lv;

	for (i = 0; i < n; i++) type[i] = bidi_type(s[i]);
	for (i = 0; i < n; i++) if (type[i] == T_L || type[i] == T_R) { para = (type[i] == T_R); break; }
	base = para; /* 0 = LTR paragraph, 1 = RTL paragraph */

	/* marks inherit the type of the preceding character */
	for (i = 0; i < n; i++) if (type[i] == T_M) type[i] = (i > 0) ? type[i - 1] : (para ? T_R : T_L);

	/* effective direction for neutrals (EN influences like R, UBA rule N1) */
	for (i = 0; i < n; i++) {
		if (type[i] != T_N) { eff[i] = (type[i] == T_L) ? 0 : 1; continue; }
		{
			int pd = para, nd = para;
			for (j = i - 1; j >= 0; j--) if (type[j] != T_N) { pd = (type[j] == T_L) ? 0 : 1; break; }
			for (j = i + 1; j < n; j++) if (type[j] != T_N) { nd = (type[j] == T_L) ? 0 : 1; break; }
			eff[i] = (pd == nd) ? pd : para;
		}
	}
	for (i = 0; i < n; i++) {
		if (type[i] == T_EN) {
			int ctx = para; /* direction of the previous strong char */
			for (j = i - 1; j >= 0; j--) if (type[j] == T_L || type[j] == T_R) { ctx = (type[j] == T_R); break; }
			level[i] = ctx ? 2 : (base ? 2 : 0);
		} else if (eff[i] == 1) level[i] = 1;
		else level[i] = base ? 2 : 0;
		if (level[i] > maxL) maxL = level[i];
		if ((level[i] & 1) && level[i] < minOdd) minOdd = level[i];
	}
	/* rule L2: reverse runs from highest level down to lowest odd level */
	for (lv = maxL; lv >= minOdd && lv >= 1; lv--) {
		for (i = 0; i < n; ) {
			if (level[i] >= lv) {
				uint32_t tmp; int a = i, b;
				for (b = i; b + 1 < n && level[b + 1] >= lv; b++) {}
				for (j = a; j < a + (b - a + 1) / 2; j++) {
					int k = b - (j - a);
					int tl; tmp = s[j]; s[j] = s[k]; s[k] = tmp;
					tl = level[j]; level[j] = level[k]; level[k] = tl;
				}
				i = b + 1;
			} else i++;
		}
	}
	/* after reversal, combining marks sit before their base: move each base in front of its marks */
	for (i = 0; i < n; ) {
		if (is_mark(s[i])) {
			int e = i;
			while (e < n && is_mark(s[e])) e++;
			if (e < n) {
				uint32_t base_c = s[e];
				memmove(&s[i + 1], &s[i], (size_t)(e - i) * sizeof(uint32_t));
				s[i] = base_c;
			}
			i = e + 1;
		} else i++;
	}
}

int ac_to_visual(const uint32_t* in, int n, uint32_t* out, int max) {
	uint32_t tmp[MAXN];
	int m;
	if (n > MAXN) n = MAXN;
	if (max > MAXN) max = MAXN;
	m = shape(in, n, tmp, MAXN);
	reorder(tmp, m);
	if (m > max) m = max;
	memcpy(out, tmp, (size_t)m * sizeof(uint32_t));
	return m;
}
