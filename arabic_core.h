#ifndef ARABIC_CORE_H
#define ARABIC_CORE_H
#include <stdint.h>

/* Decode UTF-8 -> codepoints. Returns count, or -1 if the bytes are not valid UTF-8. */
int ac_utf8_decode(const unsigned char* s, int n, uint32_t* out, int max);
/* Like ac_utf8_decode but never fails: invalid bytes become U+FFFD. */
int ac_utf8_decode_lenient(const unsigned char* s, int n, uint32_t* out, int max);
/* Encode codepoints -> UTF-8. Returns byte count (0 if it does not fit). */
int ac_utf8_encode(const uint32_t* cp, int n, unsigned char* out, int max);
/* Length in bytes of the UTF-8 sequence starting with this lead byte (1 for ASCII, 0 if invalid lead). */
int ac_utf8_seqlen(unsigned char lead);

/* Logical text -> visual text: Arabic joining (presentation forms), lam-alef ligatures,
   and bidi reordering. Output is ready to draw left-to-right with a font that has the
   Arabic presentation forms. Returns count written. */
int ac_to_visual(const uint32_t* in, int n, uint32_t* out, int max);

/* Does this byte string (chat line) contain a valid multi-byte UTF-8 sequence >= U+0100?
   Used to tell "tunnelled" lines from ordinary CP437 text. */
int ac_is_tunnelled(const unsigned char* s, int n);

/* Largest prefix (in bytes, <= maxBytes) of a UTF-8 string that ends on a codepoint boundary. */
int ac_utf8_prefix(const unsigned char* s, int n, int maxBytes);

#endif
