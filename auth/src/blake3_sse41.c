// Code from the BLAKE3 team GitHub repository, with minor modifications.
#include "blake3_impl.h"
#ifdef __x86_64__
#include <immintrin.h>

#define DEGREE 4
extern const uint32_t *IV;

#define _mm_shuffle_ps2(a, b, c)                                                                   \
    (_mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), (c))))

INLINE __m128i loadu(const uint8_t src[16]) {
    return _mm_loadu_si128((const __m128i *)src);
}

INLINE void storeu(__m128i src, uint8_t dest[16]) {
    _mm_storeu_si128((__m128i *)dest, src);
}

INLINE __m128i addv(__m128i a, __m128i b) {
    return _mm_add_epi32(a, b);
}

// Note that clang-format doesn't like the name "xor" for some reason.
INLINE __m128i xorv(__m128i a, __m128i b) {
    return _mm_xor_si128(a, b);
}

INLINE __m128i set4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return _mm_setr_epi32((int32_t)a, (int32_t)b, (int32_t)c, (int32_t)d);
}

INLINE __m128i rot16(__m128i x) {
    return _mm_shuffle_epi8(x, _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2));
}

INLINE __m128i rot12(__m128i x) {
    return xorv(_mm_srli_epi32(x, 12), _mm_slli_epi32(x, 32 - 12));
}

INLINE __m128i rot8(__m128i x) {
    return _mm_shuffle_epi8(x, _mm_set_epi8(12, 15, 14, 13, 8, 11, 10, 9, 4, 7, 6, 5, 0, 3, 2, 1));
}

INLINE __m128i rot7(__m128i x) {
    return xorv(_mm_srli_epi32(x, 7), _mm_slli_epi32(x, 32 - 7));
}

INLINE void g1(__m128i *row0, __m128i *row1, __m128i *row2, __m128i *row3, __m128i m) {
    *row0 = addv(addv(*row0, m), *row1);
    *row3 = xorv(*row3, *row0);
    *row3 = rot16(*row3);
    *row2 = addv(*row2, *row3);
    *row1 = xorv(*row1, *row2);
    *row1 = rot12(*row1);
}

INLINE void g2(__m128i *row0, __m128i *row1, __m128i *row2, __m128i *row3, __m128i m) {
    *row0 = addv(addv(*row0, m), *row1);
    *row3 = xorv(*row3, *row0);
    *row3 = rot8(*row3);
    *row2 = addv(*row2, *row3);
    *row1 = xorv(*row1, *row2);
    *row1 = rot7(*row1);
}

// Note the optimization here of leaving row1 as the unrotated row, rather than
// row0. All the message loads below are adjusted to compensate for this. See
// discussion at https://github.com/sneves/blake2-avx2/pull/4
INLINE void diagonalize(__m128i *row0, __m128i *row2, __m128i *row3) {
    *row0 = _mm_shuffle_epi32(*row0, _MM_SHUFFLE(2, 1, 0, 3));
    *row3 = _mm_shuffle_epi32(*row3, _MM_SHUFFLE(1, 0, 3, 2));
    *row2 = _mm_shuffle_epi32(*row2, _MM_SHUFFLE(0, 3, 2, 1));
}

INLINE void undiagonalize(__m128i *row0, __m128i *row2, __m128i *row3) {
    *row0 = _mm_shuffle_epi32(*row0, _MM_SHUFFLE(0, 3, 2, 1));
    *row3 = _mm_shuffle_epi32(*row3, _MM_SHUFFLE(1, 0, 3, 2));
    *row2 = _mm_shuffle_epi32(*row2, _MM_SHUFFLE(2, 1, 0, 3));
}

INLINE void compress_pre(__m128i rows[4], const uint32_t cv[8],
    const uint8_t block[BLAKE3_BLOCK_LEN], uint8_t block_len, uint64_t counter, uint8_t flags) {
    rows[0] = loadu((uint8_t *)&cv[0]);
    rows[1] = loadu((uint8_t *)&cv[4]);
    rows[2] = set4(IV[0], IV[1], IV[2], IV[3]);
    rows[3] =
        set4(counter_low(counter), counter_high(counter), (uint32_t)block_len, (uint32_t)flags);

    __m128i m0 = loadu(&block[sizeof(__m128i) * 0]);
    __m128i m1 = loadu(&block[sizeof(__m128i) * 1]);
    __m128i m2 = loadu(&block[sizeof(__m128i) * 2]);
    __m128i m3 = loadu(&block[sizeof(__m128i) * 3]);

    __m128i t0, t1, t2, t3, tt;

    // Round 1. The first round permutes the message words from the original
    // input order, into the groups that get mixed in parallel.
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(2, 0, 2, 0));  //  6  4  2  0
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 3, 1));  //  7  5  3  1
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(2, 0, 2, 0));  // 14 12 10  8
    t2 = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2, 1, 0, 3));    // 12 10  8 14
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 1, 3, 1));  // 15 13 11  9
    t3 = _mm_shuffle_epi32(t3, _MM_SHUFFLE(2, 1, 0, 3));    // 13 11  9 15
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 2. This round and all following rounds apply a fixed permutation
    // to the message words from the round before.
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 3
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 4
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 5
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 6
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 7
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
}

INLINE void compress_pre_opt(__m128i rows[4], const uint32_t cv[8], const uint8_t block1[32],
    const uint8_t block2[32], uint8_t block_len, uint64_t counter, uint8_t flags) {
    rows[0] = loadu((uint8_t *)&cv[0]);
    rows[1] = loadu((uint8_t *)&cv[4]);
    rows[2] = set4(IV[0], IV[1], IV[2], IV[3]);
    rows[3] =
        set4(counter_low(counter), counter_high(counter), (uint32_t)block_len, (uint32_t)flags);

    __m128i m0 = loadu(&block1[sizeof(__m128i) * 0]);
    __m128i m1 = loadu(&block1[sizeof(__m128i) * 1]);
    __m128i m2 = loadu(&block2[sizeof(__m128i) * 0]);
    __m128i m3 = loadu(&block2[sizeof(__m128i) * 1]);

    __m128i t0, t1, t2, t3, tt;

    // Round 1. The first round permutes the message words from the original
    // input order, into the groups that get mixed in parallel.
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(2, 0, 2, 0));  //  6  4  2  0
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 3, 1));  //  7  5  3  1
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(2, 0, 2, 0));  // 14 12 10  8
    t2 = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2, 1, 0, 3));    // 12 10  8 14
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 1, 3, 1));  // 15 13 11  9
    t3 = _mm_shuffle_epi32(t3, _MM_SHUFFLE(2, 1, 0, 3));    // 13 11  9 15
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 2. This round and all following rounds apply a fixed permutation
    // to the message words from the round before.
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 3
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 4
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 5
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 6
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
    m0 = t0;
    m1 = t1;
    m2 = t2;
    m3 = t3;

    // Round 7
    t0 = _mm_shuffle_ps2(m0, m1, _MM_SHUFFLE(3, 1, 1, 2));
    t0 = _mm_shuffle_epi32(t0, _MM_SHUFFLE(0, 3, 2, 1));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t0);
    t1 = _mm_shuffle_ps2(m2, m3, _MM_SHUFFLE(3, 3, 2, 2));
    tt = _mm_shuffle_epi32(m0, _MM_SHUFFLE(0, 0, 3, 3));
    t1 = _mm_blend_epi16(tt, t1, 0xCC);
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t1);
    diagonalize(&rows[0], &rows[2], &rows[3]);
    t2 = _mm_unpacklo_epi64(m3, m1);
    tt = _mm_blend_epi16(t2, m2, 0xC0);
    t2 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(1, 3, 2, 0));
    g1(&rows[0], &rows[1], &rows[2], &rows[3], t2);
    t3 = _mm_unpackhi_epi32(m1, m3);
    tt = _mm_unpacklo_epi32(m2, t3);
    t3 = _mm_shuffle_epi32(tt, _MM_SHUFFLE(0, 1, 3, 2));
    g2(&rows[0], &rows[1], &rows[2], &rows[3], t3);
    undiagonalize(&rows[0], &rows[2], &rows[3]);
}
void blake3_compress_in_place_sse41(uint32_t cv[8], const uint8_t block[BLAKE3_BLOCK_LEN],
    uint8_t block_len, uint64_t counter, uint8_t flags) {
    __m128i rows[4];
    compress_pre(rows, cv, block, block_len, counter, flags);
    storeu(xorv(rows[0], rows[2]), (uint8_t *)&cv[0]);
    storeu(xorv(rows[1], rows[3]), (uint8_t *)&cv[4]);
}
void blake3_compress_xof_sse41_opt(const uint32_t cv[8], const uint8_t *block1,
    const uint8_t *block2, uint8_t block_len, uint64_t counter, uint8_t flags, uint8_t out[64]) {
    __m128i rows[4];
    compress_pre_opt(rows, cv, block1, block2, block_len, counter, flags);
    storeu(xorv(rows[0], rows[2]), &out[0]);
    storeu(xorv(rows[1], rows[3]), &out[16]);
}
void blake3_compress_xof_sse41(const uint32_t cv[8], const uint8_t block[BLAKE3_BLOCK_LEN],
    uint8_t block_len, uint64_t counter, uint8_t flags, uint8_t out[64]) {
    __m128i rows[4];
    compress_pre(rows, cv, block, block_len, counter, flags);
    storeu(xorv(rows[0], rows[2]), &out[0]);
    storeu(xorv(rows[1], rows[3]), &out[16]);
}

#endif
