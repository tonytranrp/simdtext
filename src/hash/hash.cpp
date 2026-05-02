#include "simdtext/hash.hpp"

namespace simdtext {

namespace {

// CRC32 table (polynomial 0xEDB88320)
static constexpr uint32_t crc32_table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
    0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
    0x35B5A8FA,0x42B2986C,0xDBBBBBD6,0xACBCCB40,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
    0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
    0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
    0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D87CC,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
    0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
    0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
    0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
    0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEEDECC5,0x47D7857F,0x30D0D6E9,
    0xBDDA5D6E,0xCADD4D9A,0x53DBE8D0,0x24C49E46,0xBAD03C05,0xCDD70693,0x54DE5729,0x23D967BF,
    0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

// CRC32C table (Castagnoli polynomial 0x82F63B78)
static constexpr uint32_t crc32c_table[256] = {
    0x00000000u,0xF26B8303u,0xE13B70F7u,0x1350F3F4u,0xC79A971Fu,0x35F1141Cu,0x26A1E7E8u,0xD4CA64EBu,
    0x8AD958CFu,0x78B2DBCCu,0x6BE22838u,0x9989AB3Bu,0x4D43CFD0u,0xBF284CD3u,0xAC78BF27u,0x5E133C24u,
    0x105EC76Fu,0xE235446Cu,0xF165B798u,0x030E349Bu,0xD7C45070u,0x25AFD373u,0x36FF2087u,0xC494A384u,
    0x9A879FA0u,0x68EC1CA3u,0x7BBCEF57u,0x89D76C54u,0x5D1D08BFu,0xAF768BBCu,0xBC267848u,0x4E4DFB4Bu,
    0x20BD8EDEu,0xD2D60DDDu,0xC186FE29u,0x33ED7D2Au,0xE72719C1u,0x154C9AC2u,0x061C6936u,0xF477EA35u,
    0xAA64D611u,0x580F5512u,0x4B5FA6E6u,0xB93425E5u,0x6DFE410Eu,0x9F95C20Du,0x8CC531F9u,0x7EAEB2FAu,
    0x30E349B1u,0xC288CAB2u,0xD1D83946u,0x23B3BA45u,0xF779DEAEu,0x05125DADu,0x1642AE59u,0xE4292D5Au,
    0xBA3A117Eu,0x4851927Du,0x5B016189u,0xA96AE28Au,0x7DA08661u,0x8FCB0562u,0x9C9BF696u,0x6EF07595u,
    0x417B1DBCu,0xB3109EBFu,0xA0406D4Bu,0x522BEE48u,0x86E18AA3u,0x748A09A0u,0x67DAFA54u,0x95B17957u,
    0xCBA24573u,0x39C9C670u,0x2A993584u,0xD8F2B687u,0x0C38D26Cu,0xFE53516Fu,0xED03A29Bu,0x1F682198u,
    0x5125DAD3u,0xA34E59D0u,0xB01EAA24u,0x42752927u,0x96BF4DCCu,0x64D4CECFu,0x77843D3Bu,0x85EFBE38u,
    0xDBFC821Cu,0x2997011Fu,0x3AC7F2EBu,0xC8AC71E8u,0x1C661503u,0xEE0D9600u,0xFD5D65F4u,0x0F36E6F7u,
    0x61C69362u,0x93AD1061u,0x80FDE395u,0x72966096u,0xA65C047Du,0x5437877Eu,0x4767748Au,0xB50CF789u,
    0xEB1FCBADu,0x197448AEu,0x0A24BB5Au,0xF84F3859u,0x2C855CB2u,0xDEEEDFB1u,0xCDBE2C45u,0x3FD5AF46u,
    0x7198540Du,0x83F3D70Eu,0x90A324FAu,0x62C8A7F9u,0xB602C312u,0x44694011u,0x5739B3E5u,0xA55230E6u,
    0xFB410CC2u,0x092A8FC1u,0x1A7A7C35u,0xE811FF36u,0x3CDB9BDDu,0xCEB018DEu,0xDDE0EB2Au,0x2F8B6829u,
    0x82F63B78u,0x709DB87Bu,0x63CD4B8Fu,0x91A6C88Cu,0x456CAC67u,0xB7072F64u,0xA457DC90u,0x563C5F93u,
    0x082F63B7u,0xFA44E0B4u,0xE9141340u,0x1B7F9043u,0xCFB5F4A8u,0x3DDE77ABu,0x2E8E845Fu,0xDCE5075Cu,
    0x92A8FC17u,0x60C37F14u,0x73938CE0u,0x81F80FE3u,0x55326B08u,0xA759E80Bu,0xB4091BFFu,0x466298FCu,
    0x1871A4D8u,0xEA1A27DBu,0xF94AD42Fu,0x0B21572Cu,0xDFEB33C7u,0x2D80B0C4u,0x3ED04330u,0xCCBBC033u,
    0xA24BB5A6u,0x502036A5u,0x4370C551u,0xB11B4652u,0x65D122B9u,0x97BAA1BAu,0x84EA524Eu,0x7681D14Du,
    0x2892ED69u,0xDAF96E6Au,0xC9A99D9Eu,0x3BC21E9Du,0xEF087A76u,0x1D63F975u,0x0E330A81u,0xFC588982u,
    0xB21572C9u,0x407EF1CAu,0x532E023Eu,0xA145813Du,0x758FE5D6u,0x87E466D5u,0x94B49521u,0x66DF1622u,
    0x38CC2A06u,0xCAA7A905u,0xD9F75AF1u,0x2B9CD9F2u,0xFF56BD19u,0x0D3D3E1Au,0x1E6DCDEEu,0xEC064EEDu,
    0xC38D26C4u,0x31E6A5C7u,0x22B65633u,0xD0DDD530u,0x0417B1DBu,0xF67C32D8u,0xE52CC12Cu,0x1747422Fu,
    0x49547E0Bu,0xBB3FFD08u,0xA86F0EFCu,0x5A048DFFu,0x8ECEE914u,0x7CA56A17u,0x6FF599E3u,0x9D9E1AE0u,
    0xD3D3E1ABu,0x21B862A8u,0x32E8915Cu,0xC083125Fu,0x144976B4u,0xE622F5B7u,0xF5720643u,0x07198540u,
    0x590AB964u,0xAB613A67u,0xB831C993u,0x4A5A4A90u,0x9E902E7Bu,0x6CFBAD78u,0x7FAB5E8Cu,0x8DC0DD8Fu,
    0xE330A81Au,0x115B2B19u,0x020BD8EDu,0xF0605BEEu,0x24AA3F05u,0xD6C1BC06u,0xC5914FF2u,0x37FACCF1u,
    0x69E9F0D5u,0x9B8273D6u,0x88D28022u,0x7AB90321u,0xAE7367CAu,0x5C18E4C9u,0x4F48173Du,0xBD23943Eu,
    0xF36E6F75u,0x0105EC76u,0x12551F82u,0xE03E9C81u,0x34F4F86Au,0xC69F7B69u,0xD5CF889Du,0x27A40B9Eu,
    0x79B737BAu,0x8BDCB4B9u,0x988C474Du,0x6AE7C44Eu,0xBE2DA0A5u,0x4C4623A6u,0x5F16D052u,0xAD7D5351u,
};

} // anonymous namespace

uint32_t crc32(std::string_view data) noexcept {
    // CRC32 uses the standard polynomial (0xEDB88320) — no hardware instruction for this.
    // Software table lookup is the only option on x86.
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ static_cast<unsigned char>(c)) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t crc32c(std::string_view data) noexcept {
    if (__builtin_cpu_supports("sse4.2")) {
        // CRC32C (Castagnoli) has hardware support via SSE4.2
        uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
        size_t len = data.size();
        while (len >= 8) {
            uint64_t val;
            __builtin_memcpy(&val, ptr, 8);
            // Use inline asm to avoid needing -msse4.2 compile flag
            // The crc32b/crc32q instructions are CRC32C (Castagnoli)
            __asm__ __volatile__("crc32q %1, %0" : "+r"(crc) : "rm"(val));
            ptr += 8; len -= 8;
        }
        uint32_t crc32_val = static_cast<uint32_t>(crc);
        while (len >= 4) {
            uint32_t val;
            __builtin_memcpy(&val, ptr, 4);
            __asm__ __volatile__("crc32l %1, %0" : "+r"(crc32_val) : "rm"(val));
            ptr += 4; len -= 4;
        }
        while (len >= 1) {
            __asm__ __volatile__("crc32b %1, %0" : "+r"(crc32_val) : "rm"(*ptr));
            ptr++; len--;
        }
        return crc32_val ^ 0xFFFFFFFF;
    }
    // Software fallback
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc = crc32c_table[(crc ^ static_cast<unsigned char>(c)) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint64_t xxhash64(std::string_view data) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    size_t len = data.size();

    constexpr uint64_t PRIME1 = 11400714785074694791ULL;
    constexpr uint64_t PRIME2 = 14029467366897019727ULL;
    constexpr uint64_t PRIME3 = 1609587929392839161ULL;
    constexpr uint64_t PRIME4 = 9650029242287828579ULL;
    constexpr uint64_t PRIME5 = 2870177450012600261ULL;

    uint64_t h64;

    if (len >= 32) {
        uint64_t v1 = PRIME1 + PRIME2;
        uint64_t v2 = PRIME2;
        uint64_t v3 = 0;
        uint64_t v4 = static_cast<uint64_t>(-static_cast<int64_t>(PRIME1));

        auto round = [](uint64_t acc, uint64_t val) {
            acc += val * PRIME2;
            acc = (acc << 31) | (acc >> 33);
            return acc * PRIME1;
        };

        const uint8_t* limit = ptr + len - 32;
        do {
            uint64_t k1, k2, k3, k4;
            __builtin_memcpy(&k1, ptr, 8); ptr += 8;
            __builtin_memcpy(&k2, ptr, 8); ptr += 8;
            __builtin_memcpy(&k3, ptr, 8); ptr += 8;
            __builtin_memcpy(&k4, ptr, 8); ptr += 8;
            v1 = round(v1, k1);
            v2 = round(v2, k2);
            v3 = round(v3, k3);
            v4 = round(v4, k4);
        } while (ptr <= limit);

        h64 = ((v1 << 1) | (v1 >> 63)) + ((v2 << 7) | (v2 >> 57)) +
              ((v3 << 12) | (v3 >> 52)) + ((v4 << 18) | (v4 >> 46));

        h64 = (h64 ^ round(0, v1)) * PRIME1 + PRIME4;
        h64 = (h64 ^ round(0, v2)) * PRIME1 + PRIME4;
        h64 = (h64 ^ round(0, v3)) * PRIME1 + PRIME4;
        h64 = (h64 ^ round(0, v4)) * PRIME1 + PRIME4;
    } else {
        h64 = PRIME5;
    }

    h64 += len;

    const uint8_t* end = reinterpret_cast<const uint8_t*>(data.data()) + len;
    while (ptr + 8 <= end) {
        uint64_t k1;
        __builtin_memcpy(&k1, ptr, 8); ptr += 8;
        uint64_t folded = k1 * PRIME2;
        folded = (folded << 31) | (folded >> 33);
        h64 ^= folded * PRIME1;
        h64 = ((h64 << 27) | (h64 >> 37)) * PRIME1 + PRIME4;
    }

    if (ptr + 4 <= end) {
        uint32_t k1;
        __builtin_memcpy(&k1, ptr, 4); ptr += 4;
        h64 ^= k1 * PRIME1;
        h64 = ((h64 << 23) | (h64 >> 41)) * PRIME2 + PRIME3;
    }

    while (ptr < end) {
        h64 ^= (*ptr++) * PRIME5;
        h64 = ((h64 << 11) | (h64 >> 53)) * PRIME1;
    }

    h64 ^= h64 >> 33;
    h64 *= PRIME2;
    h64 ^= h64 >> 29;
    h64 *= PRIME3;
    h64 ^= h64 >> 32;

    return h64;
}

uint64_t wyhash(std::string_view data) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    size_t len = data.size();

    constexpr uint64_t P0 = 0xA0761D6478BD642FULL;
    constexpr uint64_t P1 = 0xE7037ED1A0B428DBULL;
    constexpr uint64_t P2 = 0x8EBD8583D0F5D0CDULL;
    // P3 not used in this implementation

    auto wymix = [](uint64_t a, uint64_t b) -> uint64_t {
        __uint128_t r = static_cast<__uint128_t>(a) * b;
        return static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64);
    };

    auto r8 = [](const uint8_t* p) -> uint64_t {
        uint64_t v; __builtin_memcpy(&v, p, 8); return v;
    };
    auto r4 = [](const uint8_t* p) -> uint32_t {
        uint32_t v; __builtin_memcpy(&v, p, 4); return v;
    };

    uint64_t seed = 0;
    uint64_t a, b;

    if (len <= 16) {
        if (len >= 4) {
            a = (static_cast<uint64_t>(r4(ptr)) << 32) | r4(ptr + ((len >> 3) << 2));
            b = (static_cast<uint64_t>(r4(ptr + len - 4)) << 32) | r4(ptr + len - 4 - ((len >> 3) << 2));
        } else if (len > 0) {
            a = (static_cast<uint64_t>(ptr[0]) << 16) |
                (static_cast<uint64_t>(ptr[len >> 1]) << 8) |
                static_cast<uint64_t>(ptr[len - 1]);
            b = 0;
        } else {
            a = 0; b = 0;
        }
    } else {
        uint64_t i = len;
        if (i > 48) {
            uint64_t see1 = seed, see2 = seed;
            do {
                seed = wymix(r8(ptr) ^ P0, r8(ptr + 8) ^ seed);
                see1 = wymix(r8(ptr + 16) ^ P1, r8(ptr + 24) ^ see1);
                see2 = wymix(r8(ptr + 32) ^ P2, r8(ptr + 40) ^ see2);
                ptr += 48; i -= 48;
            } while (i > 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed = wymix(r8(ptr) ^ P0, r8(ptr + 8) ^ seed);
            ptr += 16; i -= 16;
        }
        a = r8(ptr + i - 16);
        b = r8(ptr + i - 8);
    }

    return wymix(P2 ^ len, wymix(a ^ P0, b ^ P1 ^ seed));
}

} // namespace simdtext
