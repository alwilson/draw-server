#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <string.h>

char *base64(const unsigned char *, int);

#endif /* WEBSOCKET_H */
