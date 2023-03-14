#include "hash_util.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace sylar {

#define	ROTL(x, r) ((x << r) | (x >> (32 - r)))

static inline uint32_t fmix32(uint32_t h)
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  
  return h;
}

uint32_t murmur3_hash(const void* data, const uint32_t& size, const uint32_t & seed){
  if (!data) return 0;
  
  const char* str = (const char*)data;
  uint32_t s, h = seed,
          seed1 = 0xcc9e2d51,
          seed2 = 0x1b873593,
          * ptr = (uint32_t *)str;
  
  // handle begin blocks
  int len = size;
  int blk = len / 4;
  for (int i = 0; i < blk; i++){
    s  = ptr[i];
    s *= seed1;
    s  = ROTL(s, 15);
    s *= seed2;
    
    h ^= s;
    h  = ROTL(h, 13);
    h *= 5;
    h += 0xe6546b64;
  }

  // handle tail
  s = 0;
  uint8_t * tail = (uint8_t *)(str + blk * 4);
  switch(len & 3)
  {
    case 3: s |= tail[2] << 16;
    case 2: s |= tail[1] << 8;
    case 1: s |= tail[0];
      
      s *= seed1;
      s  = ROTL(s, 15);
      s *= seed2;
      h ^= s;
  };
  
  return fmix32(h ^ len);
}


uint32_t murmur3_hash(const char * str, const uint32_t & seed){
  if (!str) return 0;
  
  uint32_t s, h = seed,
          seed1 = 0xcc9e2d51,
          seed2 = 0x1b873593,
          * ptr = (uint32_t *)str;
  
  // handle begin blocks
  int len = (int)strlen(str);
  int blk = len / 4;
  for (int i = 0; i < blk; i++){
    s  = ptr[i];
    s *= seed1;
    s  = ROTL(s, 15);
    s *= seed2;
    
    h ^= s;
    h  = ROTL(h, 13);
    h *= 5;
    h += 0xe6546b64;
  }

  // handle tail
  s = 0;
  uint8_t * tail = (uint8_t *)(str + blk * 4);
  switch(len & 3)
  {
    case 3: s |= tail[2] << 16;
    case 2: s |= tail[1] << 8;
    case 1: s |= tail[0];
      
      s *= seed1;
      s  = ROTL(s, 15);
      s *= seed2;
      h ^= s;
  };
  
  return fmix32(h ^ len);
}

uint32_t quick_hash(const char * str){
    unsigned int h = 0;
    for (; *str; str++){
        h = 31 * h + *str;
    }
    return h;
}

uint32_t quick_hash(const void* tmp, uint32_t size){
    const char* str = (const char*)tmp;
    unsigned int h = 0;
    for(uint32_t i = 0; i < size; ++i){
        h = 31 * h + *str;
    }
    return h;
}

uint64_t murmur3_hash64(const void* str, const uint32_t& size,  const uint32_t & seed, const uint32_t& seed2){
    return (((uint64_t)murmur3_hash(str, size, seed)) << 32 | murmur3_hash(str, size, seed2));
}
uint64_t murmur3_hash64(const char * str, const uint32_t & seed, const uint32_t& seed2){
    return (((uint64_t)murmur3_hash(str, seed)) << 32 | murmur3_hash(str, seed2));
}

std::string base64decode(const std::string &src){
    std::string result;
    result.resize(src.size() * 3 / 4);
    char *writeBuf = &result[0];

    const char* ptr = src.c_str();
    const char* end = ptr + src.size();

    while(ptr < end){
        int i = 0;
        int padding = 0;
        int packed = 0;
        for(; i < 4 && ptr < end; ++i, ++ptr){
            if(*ptr == '='){
                ++padding;
                packed <<= 6;
                continue;
            }

            // padding with "=" only
            if (padding > 0){
                return "";
            }

            int val = 0;
            if(*ptr >= 'A' && *ptr <= 'Z'){
                val = *ptr - 'A';
            } else if(*ptr >= 'a' && *ptr <= 'z'){
                val = *ptr - 'a' + 26;
            } else if(*ptr >= '0' && *ptr <= '9'){
                val = *ptr - '0' + 52;
            } else if(*ptr == '+'){
                val = 62;
            } else if(*ptr == '/'){
                val = 63;
            }else{
                return ""; // invalid character
            }

            packed = (packed << 6) | val;
        }
        if (i != 4){
            return "";
        }
        if (padding > 0 && ptr != end){
            return "";
        }
        if (padding > 2){
            return "";
        }

        *writeBuf++ = (char)((packed >> 16) & 0xff);
        if(padding != 2){
            *writeBuf++ = (char)((packed >> 8) & 0xff);
        }
        if(padding == 0){
            *writeBuf++ = (char)(packed & 0xff);
        }
    }

    result.resize(writeBuf - result.c_str());
    return result;
}

std::string base64encode(const std::string& data){
    return base64encode(data.c_str(), data.size());
}

std::string base64encode(const void* data, size_t len){
    const char* base64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string ret;
    ret.reserve(len * 4 / 3 + 2);

    const unsigned char* ptr = (const unsigned char*)data;
    const unsigned char* end = ptr + len;

    while(ptr < end){
        unsigned int packed = 0;
        int i = 0;
        int padding = 0;
        for(; i < 3 && ptr < end; ++i, ++ptr){
            packed = (packed << 8) | *ptr;
        }
        if(i == 2){
            padding = 1;
        } else if (i == 1){
            padding = 2;
        }
        for(; i < 3; ++i){
            packed <<= 8;
        }

        ret.append(1, base64[packed >> 18]);
        ret.append(1, base64[(packed >> 12) & 0x3f]);
        if(padding != 2){
            ret.append(1, base64[(packed >> 6) & 0x3f]);
        }
        if(padding == 0){
            ret.append(1, base64[packed & 0x3f]);
        }
        ret.append(padding, '=');
    }

    return ret;
}

std::string md5(const std::string &data){
    return hexstring_from_data(md5sum(data).c_str(), MD5_DIGEST_LENGTH);
}

std::string sha1(const std::string &data){
    return hexstring_from_data(sha1sum(data).c_str(), SHA_DIGEST_LENGTH);
}

std::string md5sum(const void *data, size_t len){
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    std::string result;
    result.resize(MD5_DIGEST_LENGTH);
    MD5_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string md5sum(const std::string &data){
    return md5sum(data.c_str(), data.size());
}

std::string sha0sum(const void *data, size_t len){
    // SHA_CTX ctx;
    // SHA_Init(&ctx);
    // SHA_Update(&ctx, data, len);
    // std::string result;
    // result.resize(SHA_DIGEST_LENGTH);
    // SHA_Final((unsigned char*)&result[0], &ctx);
    // return result;
    
    //todo: 原来是SHA_CTX，报错，后面的openssl版本不支持这个函数了，因此将其改为SHA1_XX
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string sha0sum(const std::string & data){
    return sha0sum(data.c_str(), data.length());
}

std::string sha1sum(const void *data, size_t len){
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string sha1sum(const std::string &data){
    return sha1sum(data.c_str(), data.size());
}

struct xorStruct {
    xorStruct(char value) : m_value(value){}
    char m_value;
    char operator()(char in) const { return in ^ m_value; }
};

template <class CTX,
    int (*Init)(CTX *),
    int (*Update)(CTX *, const void *, size_t),
    int (*Final)(unsigned char *, CTX *),
    unsigned int B, unsigned int L>
std::string hmac(const std::string &text, const std::string &key){
    std::string keyLocal = key;
    CTX ctx;
    if (keyLocal.size() > B){
        Init(&ctx);
        Update(&ctx, keyLocal.c_str(), keyLocal.size());
        keyLocal.resize(L);
        Final((unsigned char *)&keyLocal[0], &ctx);
    }
    keyLocal.append(B - keyLocal.size(), '\0');
    std::string ipad = keyLocal, opad = keyLocal;
    std::transform(ipad.begin(), ipad.end(), ipad.begin(), xorStruct(0x36));
    std::transform(opad.begin(), opad.end(), opad.begin(), xorStruct(0x5c));
    Init(&ctx);
    Update(&ctx, ipad.c_str(), B);
    Update(&ctx, text.c_str(), text.size());
    std::string result;
    result.resize(L);
    Final((unsigned char *)&result[0], &ctx);
    Init(&ctx);
    Update(&ctx, opad.c_str(), B);
    Update(&ctx, result.c_str(), L);
    Final((unsigned char *)&result[0], &ctx);
    return result;
}

std::string hmac_md5(const std::string &text, const std::string &key){
    return hmac<MD5_CTX,
        &MD5_Init,
        &MD5_Update,
        &MD5_Final,
        MD5_CBLOCK, MD5_DIGEST_LENGTH>
        (text, key);
}

std::string hmac_sha1(const std::string &text, const std::string &key){
    return hmac<SHA_CTX,
        &SHA1_Init,
        &SHA1_Update,
        &SHA1_Final,
        SHA_CBLOCK, SHA_DIGEST_LENGTH>
        (text, key);
}

std::string
hmac_sha256(const std::string &text, const std::string &key){
    return hmac<SHA256_CTX,
        &SHA256_Init,
        &SHA256_Update,
        &SHA256_Final,
        SHA256_CBLOCK, SHA256_DIGEST_LENGTH>
        (text, key);
}

void
hexstring_from_data(const void *data, size_t len, char *output){
    const unsigned char *buf = (const unsigned char *)data;
    size_t i, j;
    for (i = j = 0; i < len; ++i){
        char c;
        c = (buf[i] >> 4) & 0xf;
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
        c = (buf[i] & 0xf);
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
    }
}

std::string
hexstring_from_data(const void *data, size_t len){
    if (len == 0){
        return std::string();
    }
    std::string result;
    result.resize(len * 2);
    hexstring_from_data(data, len, &result[0]);
    return result;
}

std::string hexstring_from_data(const std::string &data){
    return hexstring_from_data(data.c_str(), data.size());
}

void data_from_hexstring(const char *hexstring, size_t length, void *output){
    unsigned char *buf = (unsigned char *)output;
    unsigned char byte;
    if (length % 2 != 0){
        throw std::invalid_argument("data_from_hexstring length % 2 != 0");
    }
    for (size_t i = 0; i < length; ++i){
        switch (hexstring[i]){
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                byte = (hexstring[i] - 'a' + 10) << 4;
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                byte = (hexstring[i] - 'A' + 10) << 4;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                byte = (hexstring[i] - '0') << 4;
                break;
            default:
                throw std::invalid_argument("data_from_hexstring invalid hexstring");
        }
        ++i;
        switch (hexstring[i]){
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                byte |= hexstring[i] - 'a' + 10;
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                byte |= hexstring[i] - 'A' + 10;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                byte |= hexstring[i] - '0';
                break;
            default:
                throw std::invalid_argument("data_from_hexstring invalid hexstring");
        }
        *buf++ = byte;
    }
}

std::string data_from_hexstring(const char *hexstring, size_t length){
    if (length % 2 != 0){
        throw std::invalid_argument("data_from_hexstring length % 2 != 0");
    }
    if (length == 0){
        return std::string();
    }
    std::string result;
    result.resize(length / 2);
    data_from_hexstring(hexstring, length, &result[0]);
    return result;
}

std::string data_from_hexstring(const std::string &hexstring){
    return data_from_hexstring(hexstring.c_str(), hexstring.size());
}

std::string replace(const std::string &str1, char find, char replaceWith){
    auto str = str1;
    size_t index = str.find(find);
    while (index != std::string::npos){
        str[index] = replaceWith;
        index = str.find(find, index + 1);
    }
    return str;
}

std::string replace(const std::string &str1, char find, const std::string &replaceWith){
    auto str = str1;
    size_t index = str.find(find);
    while (index != std::string::npos){
        str = str.substr(0, index) + replaceWith + str.substr(index + 1);
        index = str.find(find, index + replaceWith.size());
    }
    return str;
}

std::string replace(const std::string &str1, const std::string &find, const std::string &replaceWith){
    auto str = str1;
    size_t index = str.find(find);
    while (index != std::string::npos){
        str = str.substr(0, index) + replaceWith + str.substr(index + find.size());
        index = str.find(find, index + replaceWith.size());
    }
    return str;
}

std::vector<std::string> split(const std::string &str, char delim, size_t max){
    std::vector<std::string> result;
    if (str.empty()){
        return result;
    }

    size_t last = 0;
    size_t pos = str.find(delim);
    while (pos != std::string::npos){
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find(delim, last);
    }
    result.push_back(str.substr(last));
    return result;
}

std::vector<std::string> split(const std::string &str, const char *delims, size_t max){
    std::vector<std::string> result;
    if (str.empty()){
        return result;
    }

    size_t last = 0;
    size_t pos = str.find_first_of(delims);
    while (pos != std::string::npos){
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find_first_of(delims, last);
    }
    result.push_back(str.substr(last));
    return result;
}

std::string random_string(size_t len, const std::string& chars){
    if(len == 0 || chars.empty()){
        return "";
    }
    std::string rt;
    rt.resize(len);
    int count = chars.size();
    for(size_t i = 0; i < len; ++i){
        rt[i] = chars[rand() % count];
    }
    return rt;
}

}
