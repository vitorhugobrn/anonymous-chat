#pragma once

#include <vector>
#include <cstdint>

#include <openssl/sha.h>
#include <openssl/evp.h>

static std::vector<uint8_t> generateSHA256Bytes(const void *src, size_t srcLen) {
    const EVP_MD *digest = EVP_sha256();
    unsigned int mdLen = EVP_MD_size(digest);
    std::vector<uint8_t> md(mdLen);
    EVP_MD_CTX *ctx(EVP_MD_CTX_new());
    EVP_DigestInit(ctx, digest);
    EVP_DigestUpdate(ctx, src, srcLen);
    EVP_DigestFinal(ctx, md.data(), &mdLen);
    EVP_MD_CTX_free(ctx);
    return md;
}