#include "rt.h"

typedef struct { const u8 *p; u32 n; } rec_t;

/* Record format (https://sqlite.org/fileformat2.html#record_format). */
typedef struct { u64 t; const u8 *p; } col_t;

static inline const u8 *rd_varint(const u8 *p, const u8 *end, u64 *v) {
  u64 x = 0;
  for (int i = 0; i < 8; i++) {
    if (p >= end) return 0;
    x = x << 7 | (p[0] & 0x7f);
    if (!(*p++ & 0x80)) { *v = x; return p; }
  }
  if (p >= end) return 0;
  *v = x << 8 | *p++;
  return p;
}
static inline u32 serial_len(u64 t) {
  static const u8 n[12] = {0, 1, 2, 3, 4, 6, 8, 8, 0, 0, 0, 0};
  return t < 12 ? n[t] : (u32)((t - 12) / 2);
}
/* Decode the first ncol columns. Returns 0, or -1 if the record is malformed or shorter. */
static inline int rec_cols(const rec_t *r, int ncol, col_t *c) {
  const u8 *end = r->p + r->n;
  u64 hsz;
  const u8 *h = rd_varint(r->p, end, &hsz);
  if (!h || hsz > r->n) return -1;
  const u8 *hend = r->p + hsz, *body = hend;
  for (int i = 0; i < ncol; i++) {
    if (h >= hend) return -1;
    h = rd_varint(h, hend, &c[i].t);
    if (!h) return -1;
    c[i].p = body;
    body += serial_len(c[i].t);
  }
  return body <= end ? 0 : -1;
}
/* Project one field without constructing a column array or reading trailing serial types.
 * The generated ordinal is schema-indexed; serial types/lengths remain checked here. */
static inline int rec_field(const rec_t *r, u32 ordinal, col_t *c) {
  const u8 *end = r->p + r->n;
  u64 hsz;
  const u8 *h = rd_varint(r->p, end, &hsz);
  if (!h || hsz > r->n || hsz < (u64)(h - r->p)) return -1;
  const u8 *hend = r->p + hsz;
  u64 off = hsz;
  for (u32 i = 0; i <= ordinal; i++) {
    u64 t;
    h = rd_varint(h, hend, &t);
    if (!h || t > 2ull * UINT32_MAX + 13ull) return -1;
    u32 len = serial_len(t);
    if (off > r->n || len > r->n - off) return -1;
    if (i == ordinal) { c->t = t; c->p = r->p + off; return 0; }
    off += len;
  }
  return -1;
}
static inline int col_is_int(const col_t *c) { return (c->t >= 1 && c->t <= 6) || c->t == 8 || c->t == 9; }
static inline int col_is_text(const col_t *c) { return c->t >= 13 && (c->t & 1); }
static inline u32 col_len(const col_t *c) { return serial_len(c->t); }
static inline i64 col_int(const col_t *c) {
  const u8 *p = c->p;
  switch (c->t) {
  case 1: return (int8_t)p[0];
  case 2: return (int16_t)((u16)p[0] << 8 | p[1]);
  case 3: return (int32_t)((u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8) >> 8;
  case 4: return (int32_t)((u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]);
  case 5: return (i64)((u64)(int16_t)((u16)p[0] << 8 | p[1]) << 32 | (u64)((u32)p[2] << 24 | (u32)p[3] << 16 | (u32)p[4] << 8 | p[5]));
  case 6: { u64 v = 0; for (int i = 0; i < 8; i++) v = v << 8 | p[i]; return (i64)v; }
  case 9: return 1;
  default: return 0;
  }
}

/* A schema-selected text projection immediately after an INTEGER PRIMARY KEY alias.
 * The common encoding has one-byte header length and serial type. Guards establish that
 * encoding on this record; long headers/types take the general checked decoder. */
static inline int rec_text_after_rowid(const rec_t *r, const u8 **text, u32 *len) {
  const u8 *p = r->p;
  if (r->n >= 3 && p[0] >= 3 && p[0] < 128 && p[1] == 0 &&
      p[2] >= 13 && p[2] < 128 && (p[2] & 1)) {
    u32 off = p[0], n = (p[2] - 13u) >> 1;
    if (off > r->n || n > r->n - off) return -1;
    *text = p + off; *len = n;
    return 0;
  }
  col_t c;
  if (rec_field(r, 1, &c) || !col_is_text(&c)) return -1;
  *text = c.p; *len = col_len(&c);
  return 0;
}
