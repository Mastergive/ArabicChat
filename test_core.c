#include "arabic_core.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(name, cond) do { if (!(cond)) { printf("FAIL: %s\n", name); fails++; } else printf("ok:   %s\n", name); } while (0)

static int vis(const char* utf8, uint32_t* out) {
	uint32_t cp[256]; int n = ac_utf8_decode((const unsigned char*)utf8, (int)strlen(utf8), cp, 256);
	if (n < 0) return -1;
	return ac_to_visual(cp, n, out, 256);
}
static int eq(const uint32_t* a, int n, const uint32_t* b, int m) { return n == m && !memcmp(a, b, n * 4); }

int main(void) {
	uint32_t o[256]; int n;

	/* marhaba: م ر ح ب ا -> visual (LTR) FE8E FE92 FEA3 FEAE FEE3 */
	n = vis("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7", o);
	{ uint32_t e[] = {0xFE8E,0xFE92,0xFEA3,0xFEAE,0xFEE3}; CHECK("marhaba shaping+RTL", eq(o,n,e,5)); }

	/* lam-alef isolated */
	n = vis("\xD9\x84\xD8\xA7", o);
	{ uint32_t e[] = {0xFEFB}; CHECK("lam-alef isolated ligature", eq(o,n,e,1)); }

	/* ba + lam-alef: ب (initial) + ligature final -> visual: FEFC FE91 */
	n = vis("\xD8\xA8\xD9\x84\xD8\xA7", o);
	{ uint32_t e[] = {0xFEFC,0xFE91}; CHECK("beh+lam-alef", eq(o,n,e,2)); }

	/* Latin prefix keeps LTR, Arabic reversed in place: "Ali: " + marhaba */
	n = vis("Ali: \xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7", o);
	{ uint32_t e[] = {'A','l','i',':',' ',0xFE8E,0xFE92,0xFEA3,0xFEAE,0xFEE3}; CHECK("mixed LTR paragraph", eq(o,n,e,10)); }

	/* digits in RTL paragraph stay in order and land on the left */
	n = vis("\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 123", o);
	{ uint32_t e[] = {'1','2','3',' ',0xFE8E,0xFE92,0xFEA3,0xFEAE,0xFEE3}; CHECK("digits in RTL", eq(o,n,e,9)); }

	/* Persian: پ alone iso FB56 ; گ initial + ی final: "گی" -> FBFD FB94 */
	n = vis("\xD9\xBE", o); { uint32_t e[] = {0xFB56}; CHECK("peh isolated", eq(o,n,e,1)); }
	n = vis("\xDA\xAF\xDB\x8C", o); { uint32_t e[] = {0xFBFD,0xFB94}; CHECK("gaf+farsi yeh", eq(o,n,e,2)); }

	/* diacritic stays after its base: ب + fatha(064E) alone */
	n = vis("\xD8\xA8\xD9\x8E", o);
	{ uint32_t e[] = {0xFE8F,0x064E}; CHECK("mark after base", eq(o,n,e,2)); }

	/* UTF-8 roundtrip + validity + tunnel detection */
	{ const char* s = "hi \xD9\x85\xD8\xB1"; uint32_t cp[32]; unsigned char b[64]; int c = ac_utf8_decode((const unsigned char*)s,(int)strlen(s),cp,32);
	  int bl = ac_utf8_encode(cp,c,b,64);
	  CHECK("utf8 roundtrip", bl == (int)strlen(s) && !memcmp(b,s,bl));
	  CHECK("tunnelled detect (arabic)", ac_is_tunnelled((const unsigned char*)s,(int)strlen(s)));
	  CHECK("not tunnelled (plain)", !ac_is_tunnelled((const unsigned char*)"hello",5));
	  CHECK("not tunnelled (lone CP437 high byte)", !ac_is_tunnelled((const unsigned char*)"caf\x82",4)); }
	CHECK("invalid utf8 rejected", ac_utf8_decode((const unsigned char*)"\xD9", 1, o, 8) == -1);

	/* prefix never splits a codepoint */
	{ const char* s = "\xD9\x85\xD8\xB1\xD8\xAD"; /* 3 letters, 6 bytes */
	  CHECK("prefix boundary", ac_utf8_prefix((const unsigned char*)s,6,5) == 4); }

	{ const unsigned char mixed[] = { 'a', 0xD9, 0x85, 0x82, 'b' }; uint32_t cp[8];
	  int k = ac_utf8_decode_lenient(mixed, 5, cp, 8);
	  CHECK("lenient decode", k == 4 && cp[0]=='a' && cp[1]==0x645 && cp[2]==0xFFFD && cp[3]=='b'); }
	printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
	return fails != 0;
}
