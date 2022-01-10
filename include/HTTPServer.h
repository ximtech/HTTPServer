#pragma once

#include "HTTPHeader.h"
#include "HTTPMethod.h"
#include "HTTPParser.h"
#include "IPAddress.h"
#include "MACAddress.h"
#include "Vector.h"
#include "Regex.h"

#define SERVER_HTTP_VERSION "1.1"
#define HTTP_VERSION_HEADER  "HTTP/1.1"
#define SERVER_NAME "ESP8266/1.0"

#define SERVER_INITIAL_URL_MAPPING_COUNT 8

typedef struct ServerContext ServerContext;
typedef void (*RequestHandlerFunction)(ServerContext *context, HTTPParser *request);

typedef struct ServerConfiguration {
    uint16_t serverPort;
    uint32_t serverTimeoutMs;
    uint32_t rxDataBufferSize;
    Vector urlMappings;
    RequestHandlerFunction defaultHandler;
} ServerConfiguration;

struct ServerContext {
    ServerConfiguration *configuration;
    char *txDataBufferPointer;
    bool isServerRunning;
    IPAddress requestIP;
    uint32_t socketId;
};

typedef struct ChunkedResponse {
    const char *bodyPointer;
    uint32_t chunkDataLength;
    uint32_t bodyLength;
    uint32_t chunkSize;
    bool isLastChunk;
} ChunkedResponse;


ServerContext *initHTTPServerContext(ServerConfiguration *configuration);
void addUrlMapping(ServerContext *context, const char *uriPattern, HTTPMethod method, RequestHandlerFunction handlerFunction);
RequestHandlerFunction handleIncomingServerRequest(ServerContext *context, HTTPParser *request);

uint32_t formatHTTPServerStatusLine(char *buffer, HTTPStatus status);
uint32_t getHTTPServerHeadersLength(HashMap headers);
void formatHTTPServerHeaders(char *buffer, HashMap headers);
void formatHTTPServerResponseBody(ServerContext *context, const char *responseBody);

ChunkedResponse prepareChunkedResponse(uint32_t chunkSize, const char *bodyContent, uint32_t bodyLength);
bool hasNextHTTPChunk(ChunkedResponse *chunk, char *responseBuffer);

void deleteHTTPServer(ServerContext *context);