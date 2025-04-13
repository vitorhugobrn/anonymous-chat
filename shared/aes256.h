#pragma once

#include <vector>
#include <cstdint>
#include <wmmintrin.h>

#define AES256_KEYLEN   64
#define AES256_BLOCKLEN 16

struct AES256 {
    __m128i roundKeys[28];
};

#define AES256_KEYROUND_EVEN(i, rcon)                             \
    key = ctx->roundKeys[i - 2];                                  \
    gen = _mm_aeskeygenassist_si128(ctx->roundKeys[i - 1], rcon); \
    gen = _mm_shuffle_epi32(gen, 0xFF);                           \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    ctx->roundKeys[i] = _mm_xor_si128(key, gen);

#define AES256_KEYROUND_ODD(i)                                    \
    key = ctx->roundKeys[i - 2];                                  \
    gen = _mm_aeskeygenassist_si128(ctx->roundKeys[i - 1], 0x00); \
    gen = _mm_shuffle_epi32(gen, 0xAA);                           \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));             \
    ctx->roundKeys[i] = _mm_xor_si128(key, gen)

static void AES256Init(struct AES256 *ctx, const void *k) {
    __m128i key, gen;
    ctx->roundKeys[0] = _mm_loadu_si128((const __m128i *)(k));
    ctx->roundKeys[1] = _mm_loadu_si128((const __m128i *)(k) + 1);

    AES256_KEYROUND_EVEN(2, 0x01);
    AES256_KEYROUND_ODD(3);
    AES256_KEYROUND_EVEN(4, 0x02);
    AES256_KEYROUND_ODD(5);
    AES256_KEYROUND_EVEN(6, 0x04);
    AES256_KEYROUND_ODD(7);
    AES256_KEYROUND_EVEN(8, 0x08);
    AES256_KEYROUND_ODD(9);
    AES256_KEYROUND_EVEN(10, 0x10);
    AES256_KEYROUND_ODD(11);
    AES256_KEYROUND_EVEN(12, 0x20);
    AES256_KEYROUND_ODD(13);
    AES256_KEYROUND_EVEN(14, 0x40);

    ctx->roundKeys[15] = _mm_aesimc_si128(ctx->roundKeys[13]);
    ctx->roundKeys[16] = _mm_aesimc_si128(ctx->roundKeys[12]);
    ctx->roundKeys[17] = _mm_aesimc_si128(ctx->roundKeys[11]);
    ctx->roundKeys[18] = _mm_aesimc_si128(ctx->roundKeys[10]);
    ctx->roundKeys[19] = _mm_aesimc_si128(ctx->roundKeys[ 9]);
    ctx->roundKeys[20] = _mm_aesimc_si128(ctx->roundKeys[ 8]);
    ctx->roundKeys[21] = _mm_aesimc_si128(ctx->roundKeys[ 7]);
    ctx->roundKeys[22] = _mm_aesimc_si128(ctx->roundKeys[ 6]);
    ctx->roundKeys[23] = _mm_aesimc_si128(ctx->roundKeys[ 5]);
    ctx->roundKeys[24] = _mm_aesimc_si128(ctx->roundKeys[ 4]);
    ctx->roundKeys[25] = _mm_aesimc_si128(ctx->roundKeys[ 3]);
    ctx->roundKeys[26] = _mm_aesimc_si128(ctx->roundKeys[ 2]);
    ctx->roundKeys[27] = _mm_aesimc_si128(ctx->roundKeys[ 1]);
}

static void AES256Encrypt(struct AES256 *ctx, void *outputBlock, const void *inputBlock) {
    __m128i state = _mm_loadu_si128((const __m128i *)(inputBlock));
    state = _mm_xor_si128(state, ctx->roundKeys[0]);

    for (int i = 1; i < 14; ++i)
        state = _mm_aesenc_si128(state, ctx->roundKeys[i]);

    state = _mm_aesenclast_si128(state, ctx->roundKeys[14]);
    _mm_storeu_si128((__m128i_u *)(outputBlock), state);
}

static void AES256Decrypt(struct AES256 *ctx, void *outputBlock, const void *inputBlock) {
    __m128i state = _mm_loadu_si128((const __m128i *)(inputBlock));
    state = _mm_xor_si128(state, ctx->roundKeys[14]);

    for (int i = 15; i < 28; ++i)
        state = _mm_aesdec_si128(state, ctx->roundKeys[i]);

    state = _mm_aesdeclast_si128(state, ctx->roundKeys[0]);
    _mm_storeu_si128((__m128i_u *)(outputBlock), state);
}

static size_t AES256EncryptMessage(struct AES256 *ctx, uint8_t *outputBlocks, const char *inputText, size_t inputLength) {
    size_t numBlocks = (inputLength + 15) / 16;
    size_t totalEncryptedSize = numBlocks * 16;

    std::vector<uint8_t> paddedInput(totalEncryptedSize, 0);
    memcpy(paddedInput.data(), inputText, inputLength);

    for (size_t i = 0; i < numBlocks; i++)
        AES256Encrypt(ctx, outputBlocks + (i * 16), paddedInput.data() + (i * 16));

    return totalEncryptedSize;
}

static void AES256DecryptMessage(struct AES256 *ctx, uint8_t *outputText, const uint8_t *inputBlocks, size_t encryptedSize) {
    size_t numBlocks = encryptedSize / 16;

    for (size_t i = 0; i < numBlocks; i++)
        AES256Decrypt(ctx, outputText + (i * 16), inputBlocks + (i * 16));
}