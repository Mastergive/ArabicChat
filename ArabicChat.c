/*
 * ArabicChat - ClassiCube client plugin: type and read Arabic / Persian / Urdu (RTL) chat.
 *
 * How it works (the stock client is limited to CP437 bytes, so this has to be a bit clever):
 *   typing : /client arabic opens an input bar. Raw Unicode keystrokes are captured from
 *            InputEvents.Press (before the client's CP437 filter), shown shaped + right-to-left
 *            using a SYSTEM font, and sent as UTF-8 bytes inside normal chat lines.
 *   reading: incoming chat lines that contain such UTF-8 bytes are decoded, shaped, reordered
 *            and drawn by this plugin with the system font (a small overlay under the stock chat).
 *   BOTH players need this plugin, and the server must pass bytes >= 0x80 through unchanged.
 *
 * Platforms: Windows (GDI) and Linux (FreeType + fontconfig). Build: see README.md.  Written against ClassiCube 1.3.8 headers (plugin API version 1).
 */
#ifdef _WIN32
	#define CC_API   __declspec(dllimport)
	#define CC_VAR   __declspec(dllimport)
	#define EXPORT   __declspec(dllexport)
#else
	#define CC_API
	#define CC_VAR
	#define EXPORT   __attribute__((visibility("default")))
#endif

#include "src/Bitmap.h"
#include "src/Chat.h"
#include "src/Commands.h"
#include "src/Constants.h"
#include "src/Drawer2D.h"
#include "src/Event.h"
#include "src/Game.h"
#include "src/Graphics.h"
#include "src/Gui.h"
#include "src/Input.h"
#include "src/Options.h"
#include "src/Platform.h"
#if defined(__has_include)
	#if __has_include("src/String_.h")
		#include "src/String_.h"    /* current ClassiCube sources */
	#elif __has_include("src/Strings.h")
		#include "src/Strings.h"
	#else
		#include "src/String.h"
	#endif
#else
	#include "src/String_.h"
#endif
#include <string.h>

#include "arabic_core.h"
#if defined(_WIN32) || defined(__linux__)
	#define AC_HAVE_TEXT 1
	#include "sys_text.h"   /* gdi_text.c (Windows) or ft_text.c (Linux) */
#endif

/* ---- tweakables ---------------------------------------------------------------------- */
#define AC_MAX_CP     200        /* max characters in the input bar */
#define AC_LINES      6          /* decoded incoming lines kept on screen */
#define AC_SHOW_MS    15000      /* how long an incoming line stays visible */
#define AC_CHUNK      60         /* max UTF-8 bytes per chat line sent (server limit is 64) */
#define AC_PRIO_HUD   20         /* Gui_Add priorities; raise if the overlay is drawn under something */
#define AC_PRIO_INPUT 51

/* ---- state --------------------------------------------------------------------------- */
static uint32_t inBuf[AC_MAX_CP];
static int      inLen;
static cc_bool  typing, inDirty;
static cc_uint64 openedAt;
static int      caretOn;
static double   caretTimer;
static struct Texture inTex;

struct Line { uint32_t cp[AC_MAX_CP]; int n; cc_uint64 at; struct Texture tex; cc_bool built; };
static struct Line lines[AC_LINES];
static int lineHead;     /* index of next slot to write */
static cc_bool hudAdded;

static struct Screen hudScreen, inScreen;
static struct ScreenVTABLE hudVT, inVT;

/* timing via the monotonic stopwatch (independent of DateTime_* API changes) */
static cc_uint64 NowTicks(void) { return Stopwatch_Measure(); }
static int ElapsedMs(cc_uint64 since) { return (int)(Stopwatch_ElapsedMicroseconds(since, Stopwatch_Measure()) / 1000); }

/* ---- text -> texture ------------------------------------------------------------------ */
static int FontPx(void)  { return Options_GetInt("arabicchat-size", 8, 72, 20); }
static void FontFace(char* dst, int cap) {
	cc_string opt; char buf[64]; int n;
	opt = String_Init(buf, 0, 64);
	#ifdef _WIN32
	Options_Get("arabicchat-font", &opt, "Segoe UI");
#else
	Options_Get("arabicchat-font", &opt, "sans-serif");
#endif
	n = opt.length < cap - 1 ? opt.length : cap - 1;
	memcpy(dst, opt.buffer, n); dst[n] = '\0';
}

static void FreeTex(struct Texture* t) {
	if (t->ID) Gfx_DeleteTexture(&t->ID);
	t->ID = 0;
}

/* Builds a texture for logical text. barW > 0 draws a dark input bar of that width behind it. */
static cc_bool BuildTexture(struct Texture* tex, const uint32_t* logical, int n, int barW, cc_bool rtl) {
#ifdef AC_HAVE_TEXT
	uint32_t vis[AC_MAX_CP + 4]; uint16_t u16[AC_MAX_CP + 4];
	unsigned char* mask; int w, h, m, i, x, y, tw, th, ox, oy;
	char face[64]; struct Context2D ctx;

	m = ac_to_visual(logical, n, vis, AC_MAX_CP + 4);
	if (m <= 0) return false;
	for (i = 0; i < m; i++) u16[i] = vis[i] > 0xFFFF ? '?' : (uint16_t)vis[i];
	FontFace(face, sizeof(face));
	if (!sys_render_mask(u16, m, face, FontPx(), &mask, &tw, &th)) return false;

	w = barW > 0 ? (barW > tw + 16 ? barW : tw + 16) : tw + 1;
	h = barW > 0 ? th + 8 : th + 1;
	Context2D_Alloc(&ctx, w, h);
	ox = barW > 0 ? (rtl ? w - tw - 8 : 8) : 0;
	oy = barW > 0 ? 4 : 0;

	for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
		int tx = x - ox, ty = y - oy, a = 0, sa = 0;
		if (tx >= 0 && tx < tw && ty >= 0 && ty < th)         a  = mask[ty * tw + tx];
		if (tx - 1 >= 0 && tx - 1 < tw && ty - 1 >= 0 && ty - 1 < th) sa = mask[(ty - 1) * tw + (tx - 1)];
		if (a > 0)                    ctx.bmp.scan0[y * ctx.bmp.width + x] = BitmapCol_Make(255, 255, 255, a);
		else if (sa > 0)              ctx.bmp.scan0[y * ctx.bmp.width + x] = BitmapCol_Make(0, 0, 0, sa * 3 / 4);
		else if (barW > 0)            ctx.bmp.scan0[y * ctx.bmp.width + x] = BitmapCol_Make(0, 0, 0, 150);
		else                          ctx.bmp.scan0[y * ctx.bmp.width + x] = BitmapCol_Make(0, 0, 0, 0);
	}
	sys_free_mask(mask);
	Context2D_MakeTexture(tex, &ctx);
	Context2D_Free(&ctx);
	return true;
#else
	(void)tex; (void)logical; (void)n; (void)barW; (void)rtl;
	return false; /* TODO: macOS backend (CoreText) */
#endif
}

static cc_bool IsRtlCp(uint32_t c) { return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFEFF); }
static cc_bool ParaIsRtl(const uint32_t* s, int n) {
	int i;
	for (i = 0; i < n; i++) {
		if (IsRtlCp(s[i]) && !(s[i] >= 0x0660 && s[i] <= 0x0669)) return true;
		if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')) return false;
	}
	return false;
}

/* ---- drawing -------------------------------------------------------------------------- */
/* Texture_Render / Gfx_Draw2DTexture are NOT exported by the client binary, so plugins can't link
 * against them. Instead we draw the quad ourselves with a private dynamic VB, using only exported
 * Gfx_* functions (Gfx_CreateDynamicVb, Gfx_LockDynamicVb, Gfx_BindIb, Gfx_SetVertexFormat, ...). */
static GfxResourceID quadVb;

static void FreeQuadVb(void) {
	if (quadVb) Gfx_DeleteDynamicVb(&quadVb);
	quadVb = 0;
}

static void DrawTex(struct Texture* t, int x, int y) {
	struct VertexTextured* v;
	float x1, y1, x2, y2;

	t->x = (short)x; t->y = (short)y;
	if (!t->ID) return;
	if (!quadVb) {                      /* created lazily; freed again on context loss */
		quadVb = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, 4);
		if (!quadVb) return;
	}

	x1 = (float)t->x;               y1 = (float)t->y;
	x2 = (float)(t->x + t->width);  y2 = (float)(t->y + t->height);

	Gfx_BindTexture(t->ID);
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);

	v = (struct VertexTextured*)Gfx_LockDynamicVb(quadVb, VERTEX_FORMAT_TEXTURED, 4);
	v[0].x = x1; v[0].y = y1; v[0].z = 0; v[0].Col = PACKEDCOL_WHITE; v[0].U = t->uv.u1; v[0].V = t->uv.v1;
	v[1].x = x2; v[1].y = y1; v[1].z = 0; v[1].Col = PACKEDCOL_WHITE; v[1].U = t->uv.u2; v[1].V = t->uv.v1;
	v[2].x = x2; v[2].y = y2; v[2].z = 0; v[2].Col = PACKEDCOL_WHITE; v[2].U = t->uv.u2; v[2].V = t->uv.v2;
	v[3].x = x1; v[3].y = y2; v[3].z = 0; v[3].Col = PACKEDCOL_WHITE; v[3].U = t->uv.u1; v[3].V = t->uv.v2;
	Gfx_UnlockDynamicVb(quadVb);

	Gfx_BindIb(Gfx.DefaultIb);          /* the client's shared 0,1,2 / 0,2,3 quad index buffer */
	Gfx_DrawVb_IndexedTris(4);
}

static void Hud_Render(void* s, float delta) {
	int i, k, y, x = Options_GetInt("arabicchat-x", 0, 4000, 10);
	y = Options_GetInt("arabicchat-y", 0, 4000, 140);
	(void)s; (void)delta;
	for (k = 0; k < AC_LINES; k++) {
		i = (lineHead + k) % AC_LINES;               /* oldest first */
		if (lines[i].n == 0 || ElapsedMs(lines[i].at) > AC_SHOW_MS) continue;
		if (!lines[i].built) lines[i].built = BuildTexture(&lines[i].tex, lines[i].cp, lines[i].n, 0, false);
		if (!lines[i].built) continue;
		DrawTex(&lines[i].tex, x, y);
		y += lines[i].tex.height + 2;
	}
}

static void In_Update(void* s, float delta) {
	(void)s;
	caretTimer += delta;
	if (caretTimer > 0.5) { caretTimer = 0; caretOn = !caretOn; inDirty = true; }
}

static void In_Render(void* s, float delta) {
	uint32_t tmp[AC_MAX_CP + 2]; int n;
	(void)s; (void)delta;
	if (inDirty || !inTex.ID) {
		FreeTex(&inTex);
		memcpy(tmp, inBuf, inLen * sizeof(uint32_t)); n = inLen;
		tmp[n++] = caretOn ? '|' : ' ';
		BuildTexture(&inTex, tmp, n, Game.Width * 6 / 10, ParaIsRtl(inBuf, inLen));
		inDirty = false;
	}
	if (inTex.ID) DrawTex(&inTex, (Game.Width - inTex.width) / 2, Game.Height - inTex.height - 60);
}

/* ---- screen plumbing (stock screens use these as no-ops) ------------------------------ */
static void  NullFn(void* s)                              { (void)s; }
static void  NullUpd(void* s, float d)                    { (void)s; (void)d; }
static int   NullKeyPress(void* s, char c)                { (void)s; (void)c; return 0; }
static int   NullText(void* s, const cc_string* t)        { (void)s; (void)t; return 0; }
static int   NullPtrDown(void* s, int id, int x, int y)   { (void)s; (void)id; (void)x; (void)y; return 0; }
static void  NullPtrUp(void* s, int id, int x, int y)     { (void)s; (void)id; (void)x; (void)y; }
static int   NullScroll(void* s, float d)                 { (void)s; (void)d; return 0; }
static int   NullInDown(void* s, int k, void* dev)        { (void)s; (void)k; (void)dev; return 0; }
static void  NullInUp(void* s, int k, void* dev)          { (void)s; (void)k; (void)dev; }
static int   NullPad(void* s, void* u)                    { (void)s; (void)u; return 0; }

static void  Hud_Lost(void* s) {
	int i; (void)s;
	FreeQuadVb();
	for (i = 0; i < AC_LINES; i++) { FreeTex(&lines[i].tex); lines[i].built = false; }
}
static void  In_Lost(void* s) { (void)s; FreeQuadVb(); FreeTex(&inTex); inDirty = true; }

static void CloseInput(void);
static void SendInput(void) {
	unsigned char utf[AC_MAX_CP * 4]; int ul, off = 0;
	ul = ac_utf8_encode(inBuf, inLen, utf, sizeof(utf));
	while (off < ul) {
		char buf[STRING_SIZE]; cc_string s; int take, i;
		take = ac_utf8_prefix(utf + off, ul - off, AC_CHUNK);
		if (take <= 0) break;
		s = String_Init(buf, 0, STRING_SIZE);
		for (i = 0; i < take; i++) String_Append(&s, (char)utf[off + i]);
		Chat_Send(&s, true);
		off += take;
	}
}

static int In_KeyDown(void* s, int key, void* dev) {
	(void)s; (void)dev;
	if (key == CCKEY_ESCAPE) { CloseInput(); return 1; }
	if (key == CCKEY_ENTER || key == CCKEY_KP_ENTER) {
		if (ElapsedMs(openedAt) < 300) return 1; /* the Enter that ran the command */
		if (inLen > 0) SendInput();
		CloseInput();
		return 1;
	}
	if (key == CCKEY_BACKSPACE) { if (inLen > 0) { inLen--; inDirty = true; } return 1; }
	return 1; /* swallow everything else while typing */
}

/* Raw Unicode keystrokes: raised by the window layer before the CP437 conversion. */
static void OnRawPress(void* obj, int cp) {
	(void)obj;
	if (!typing || ElapsedMs(openedAt) < 50) return;
	if (cp < 32 || cp == 127 || inLen >= AC_MAX_CP - 1) return;
	inBuf[inLen++] = (uint32_t)cp;
	inDirty = true;
}

static void OpenInput(void) {
	if (typing) return;
	inLen = 0; typing = true; inDirty = true; caretOn = 1; caretTimer = 0;
	openedAt = NowTicks();
	Gui_Add(&inScreen, AC_PRIO_INPUT);
}
static void CloseInput(void) {
	if (!typing) return;
	typing = false;
	Gui_Remove(&inScreen);
	FreeTex(&inTex);
}

/* ---- incoming chat -------------------------------------------------------------------- */
static void OnChat(void* obj, const cc_string* msg, int msgType) {
	unsigned char raw[STRING_SIZE * 2]; int i, rn = 0;
	struct Line* L;
	(void)obj;
	if (msgType != 0 || !msg->length) return;
	if (!ac_is_tunnelled((const unsigned char*)msg->buffer, msg->length)) return;

	for (i = 0; i < msg->length && rn < (int)sizeof(raw); i++) {          /* drop &x colour codes */
		if (msg->buffer[i] == '&' && i + 1 < msg->length) { i++; continue; }
		raw[rn++] = (unsigned char)msg->buffer[i];
	}
	L = &lines[lineHead];
	FreeTex(&L->tex); L->built = false;
	L->n  = ac_utf8_decode_lenient(raw, rn, L->cp, AC_MAX_CP);
	L->at = NowTicks();
	lineHead = (lineHead + 1) % AC_LINES;
}

/* ---- command + component hooks -------------------------------------------------------- */
static void Cmd_Arabic(const cc_string* args, int argsCount) { (void)args; (void)argsCount; OpenInput(); }
static struct ChatCommand arabicCmd = {
	"Arabic", Cmd_Arabic, 0,
	{ "&a/client arabic", "&eOpens the Arabic/RTL input bar. Enter = send, Esc = cancel." }
};

static void Plugin_Init(void) {
	memset(&hudVT, 0, sizeof(hudVT)); memset(&inVT, 0, sizeof(inVT));
	hudVT.Init = (void*)NullFn;  hudVT.Update = (void*)NullUpd; hudVT.Free = (void*)NullFn;
	hudVT.Render = (void*)Hud_Render; hudVT.BuildMesh = (void*)NullFn;
	hudVT.HandlesInputDown = (void*)NullInDown; hudVT.OnInputUp = (void*)NullInUp;
	hudVT.HandlesKeyPress = (void*)NullKeyPress; hudVT.HandlesTextChanged = (void*)NullText;
	hudVT.HandlesPointerDown = (void*)NullPtrDown; hudVT.OnPointerUp = (void*)NullPtrUp;
	hudVT.HandlesPointerMove = (void*)NullPtrDown; hudVT.HandlesMouseScroll = (void*)NullScroll;
	hudVT.Layout = (void*)NullFn; hudVT.ContextLost = (void*)Hud_Lost; hudVT.ContextRecreated = (void*)NullFn;
	hudVT.HandlesPadAxis = (void*)NullPad;
	inVT = hudVT;
	inVT.Update = (void*)In_Update; inVT.Render = (void*)In_Render;
	inVT.HandlesInputDown = (void*)In_KeyDown; inVT.ContextLost = (void*)In_Lost;

	memset(&hudScreen, 0, sizeof(hudScreen)); memset(&inScreen, 0, sizeof(inScreen));
	hudScreen.VTABLE = &hudVT;
	inScreen.VTABLE  = &inVT;
	inScreen.grabsInput = true; inScreen.blocksWorld = true;

	Event_Register((struct Event_Void*)&ChatEvents.ChatReceived, NULL, (Event_Void_Callback)OnChat);
	Event_Register((struct Event_Void*)&InputEvents.Press,       NULL, (Event_Void_Callback)OnRawPress);
	Commands_Register(&arabicCmd);
}
static void Plugin_Free(void) { FreeQuadVb(); }
static void Plugin_Reset(void) {
	int i;
	CloseInput();
	for (i = 0; i < AC_LINES; i++) { lines[i].n = 0; FreeTex(&lines[i].tex); lines[i].built = false; }
}
static void Plugin_OnNewMap(void) { }
static void Plugin_OnNewMapLoaded(void) {
	if (!hudAdded) { Gui_Add(&hudScreen, AC_PRIO_HUD); hudAdded = true; } /* game/GUI is up by now */
}

EXPORT int Plugin_ApiVersion = 1;
EXPORT struct IGameComponent Plugin_Component = {
	Plugin_Init, Plugin_Free, Plugin_Reset, Plugin_OnNewMap, Plugin_OnNewMapLoaded
};
