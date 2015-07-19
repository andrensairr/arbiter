#include <arbiter/util/crypto.hpp>

#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))

namespace arbiter
{
namespace crypto
{
namespace
{
    const std::size_t block(64);

    std::vector<char> append(
            const std::vector<char>& a,
            const std::vector<char>& b)
    {
        std::vector<char> out(a);
        out.insert(out.end(), b.begin(), b.end());
        return out;
    }

    std::vector<char> append(
            const std::vector<char>& a,
            const std::string& b)
    {
        return append(a, std::vector<char>(b.begin(), b.end()));
    }

    // SHA1 implementation:
    //      https://github.com/B-Con/crypto-algorithms
    //
    // HMAC:
    //      https://en.wikipedia.org/wiki/Hash-based_message_authentication_code
    //
    typedef struct
    {
        uint8_t data[64];
        uint32_t datalen;
        unsigned long long bitlen;
        uint32_t state[5];
        uint32_t k[4];
    } SHA1_CTX;

    void sha1_transform(SHA1_CTX *ctx, const uint8_t* data)
    {
        uint32_t a, b, c, d, e, i, j, t, m[80];

        for (i = 0, j = 0; i < 16; ++i, j += 4)
        {
            m[i] =
                (data[j] << 24) + (data[j + 1] << 16) +
                (data[j + 2] << 8) + (data[j + 3]);
        }

        for ( ; i < 80; ++i)
        {
            m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
            m[i] = (m[i] << 1) | (m[i] >> 31);
        }

        a = ctx->state[0];
        b = ctx->state[1];
        c = ctx->state[2];
        d = ctx->state[3];
        e = ctx->state[4];

        for (i = 0; i < 20; ++i) {
            t = ROTLEFT(a, 5) + ((b & c) ^ (~b & d)) + e + ctx->k[0] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 40; ++i) {
            t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[1] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 60; ++i) {
            t = ROTLEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d)) + e +
                ctx->k[2] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 80; ++i) {
            t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[3] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }

        ctx->state[0] += a;
        ctx->state[1] += b;
        ctx->state[2] += c;
        ctx->state[3] += d;
        ctx->state[4] += e;
    }

    void sha1_init(SHA1_CTX *ctx)
    {
        ctx->datalen = 0;
        ctx->bitlen = 0;
        ctx->state[0] = 0x67452301;
        ctx->state[1] = 0xEFCDAB89;
        ctx->state[2] = 0x98BADCFE;
        ctx->state[3] = 0x10325476;
        ctx->state[4] = 0xc3d2e1f0;
        ctx->k[0] = 0x5a827999;
        ctx->k[1] = 0x6ed9eba1;
        ctx->k[2] = 0x8f1bbcdc;
        ctx->k[3] = 0xca62c1d6;
    }

    void sha1_update(SHA1_CTX *ctx, const uint8_t* data, size_t len)
    {
        for (std::size_t i(0); i < len; ++i)
        {
            ctx->data[ctx->datalen] = data[i];
            ++ctx->datalen;
            if (ctx->datalen == 64)
            {
                sha1_transform(ctx, ctx->data);
                ctx->bitlen += 512;
                ctx->datalen = 0;
            }
        }
    }

    void sha1_final(SHA1_CTX *ctx, uint8_t* hash)
    {
        uint32_t i;

        i = ctx->datalen;

        // Pad whatever data is left in the buffer.
        if (ctx->datalen < 56)
        {
            ctx->data[i++] = 0x80;
            while (i < 56)
                ctx->data[i++] = 0x00;
        }
        else
        {
            ctx->data[i++] = 0x80;
            while (i < 64)
                ctx->data[i++] = 0x00;
            sha1_transform(ctx, ctx->data);
            std::memset(ctx->data, 0, 56);
        }

        // Append to the padding the total message's length in bits and
        // transform.
        ctx->bitlen += ctx->datalen * 8;
        ctx->data[63] = ctx->bitlen;
        ctx->data[62] = ctx->bitlen >> 8;
        ctx->data[61] = ctx->bitlen >> 16;
        ctx->data[60] = ctx->bitlen >> 24;
        ctx->data[59] = ctx->bitlen >> 32;
        ctx->data[58] = ctx->bitlen >> 40;
        ctx->data[57] = ctx->bitlen >> 48;
        ctx->data[56] = ctx->bitlen >> 56;
        sha1_transform(ctx, ctx->data);

        // Since this implementation uses little endian byte ordering and MD
        // uses big endian, reverse all the bytes when copying the final state
        // to the output hash.
        for (i = 0; i < 4; ++i)
        {
            hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        }
    }

    std::vector<char> sha1(const std::vector<char>& data)
    {
        SHA1_CTX ctx;
        std::vector<char> out(20);

        sha1_init(&ctx);
        sha1_update(
                &ctx,
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size());
        sha1_final(&ctx, reinterpret_cast<uint8_t*>(out.data()));

        return out;
    }

    std::string sha1(const std::string& data)
    {
        auto hashed(sha1(std::vector<char>(data.begin(), data.end())));
        return std::string(hashed.begin(), hashed.end());
    }

} // unnamed namespace

std::vector<char> hmacSha1(std::string key, const std::string message)
{
    if (key.size() > block) key = sha1(key);
    if (key.size() < block) key.insert(key.end(), block - key.size(), 0);

    std::vector<char> okeypad(block, 0x5c);
    std::vector<char> ikeypad(block, 0x36);

    for (std::size_t i(0); i < block; ++i)
    {
        okeypad[i] ^= key[i];
        ikeypad[i] ^= key[i];
    }

    return sha1(append(okeypad, sha1(append(ikeypad, message))));
}

} // namespace crypto
} // namespace arbiter