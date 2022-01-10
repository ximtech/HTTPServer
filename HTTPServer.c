#include "HTTPServer.h"

#define CR_LF_LENGTH 2
#define CHUNK_CR_LF_COUNT 4
#define LINE_END_LENGTH 1
#define HEADER_FORMAT_LENGTH 4 // ": " and "\r\n"

typedef struct URLMapping {
    Regex uriPattern;
    HTTPMethod method;
    RequestHandlerFunction handlerFunction;
} URLMapping;

static void formatChunk(char *responseBuffer, const char *dataPointer, uint32_t dataLen, uint32_t chunkSize);


ServerContext *initHTTPServerContext(ServerConfiguration *configuration) {
    if (configuration == NULL ||
        configuration->rxDataBufferSize == 0 ||
        configuration->defaultHandler == NULL) {
        return NULL;    // not properly configured
    }

    ServerContext *context = malloc(sizeof(struct ServerContext));
    if (context == NULL) return NULL;   // failed to create server context
    context->configuration = configuration;
    context->txDataBufferPointer = NULL;
    context->isServerRunning = false;
    context->socketId = 0;
    context->requestIP = ipAddressOf(0, 0, 0, 0);
    configuration->urlMappings = getVectorInstance(SERVER_INITIAL_URL_MAPPING_COUNT);
    return context;
}

void addUrlMapping(ServerContext *context, const char *uriPattern, HTTPMethod method, RequestHandlerFunction handlerFunction) {
    if (uriPattern == NULL || handlerFunction == NULL || context == NULL) return;
    URLMapping *urlMapping = malloc(sizeof(struct URLMapping));
    if (urlMapping == NULL) return;

    urlMapping->method = method;
    urlMapping->handlerFunction = handlerFunction;
    regexCompile(&urlMapping->uriPattern, uriPattern);
    if (!urlMapping->uriPattern.isPatternValid) {
        free(urlMapping);
        return;
    }
    vectorAdd(context->configuration->urlMappings, urlMapping);
}

RequestHandlerFunction handleIncomingServerRequest(ServerContext *context, HTTPParser *request) {
    if (request->parserStatus == HTTP_PARSE_OK && isStringNotBlank(request->uriPath)) {
        for (uint32_t i = 0; i < getVectorSize(context->configuration->urlMappings); i++) {
            URLMapping *urlMapping = vectorGet(context->configuration->urlMappings, i);
            Matcher matcher = regexMatch(&urlMapping->uriPattern, request->uriPath);
            if (matcher.isFound && request->method == urlMapping->method) {
                return urlMapping->handlerFunction;
            }
        }
        return context->configuration->defaultHandler;
    }
    return NULL;
}

uint32_t formatHTTPServerStatusLine(char *buffer, HTTPStatus status) {
    return sprintf(buffer, "%s %d %s\r\n", HTTP_VERSION_HEADER, status, getHttpStatusCodeMeaning(status));
}

uint32_t getHTTPServerHeadersLength(HashMap headers) {
    uint32_t result = 0;
    HashMapIterator iterator = getHashMapIterator(headers);
    while (hashMapHasNext(&iterator)) {
        result += strlen(iterator.key);
        result += strlen(iterator.value);
        result += HEADER_FORMAT_LENGTH;
    }
    result += CR_LF_LENGTH;
    return result;
}

void formatHTTPServerHeaders(char *buffer, HashMap headers) {
    HashMapIterator iterator = getHashMapIterator(headers);
    while (hashMapHasNext(&iterator)) {
        strcat(buffer, iterator.key);
        strcat(buffer, ": ");
        strcat(buffer, iterator.value);
        strcat(buffer, "\r\n");
    }
    strcat(buffer, "\r\n");
}

void formatHTTPServerResponseBody(ServerContext *context, const char *responseBody) {
    if (isStringNotBlank(responseBody)) {
        strcat(context->txDataBufferPointer, responseBody);
        strcat(context->txDataBufferPointer, "\r\n\r\n");
    }
}

ChunkedResponse prepareChunkedResponse(uint32_t chunkSize, const char *bodyContent, uint32_t bodyLength) {
    ChunkedResponse chunk = {0};
    chunk.bodyLength = bodyLength;
    chunk.chunkSize = chunkSize;
    chunk.bodyPointer = bodyContent;

    uint8_t chunkDigitCount = 0;
    while (chunkSize != 0) {
        chunkSize /= 10;
        chunkDigitCount++;
    }
    chunk.chunkDataLength = chunk.chunkSize - CHUNK_CR_LF_COUNT - LINE_END_LENGTH - chunkDigitCount;
    return chunk;
}

bool hasNextHTTPChunk(ChunkedResponse *chunk, char *responseBuffer) {
    if (chunk->bodyLength > 0) {
        if (chunk->chunkSize > chunk->bodyLength) {
            formatChunk(responseBuffer, chunk->bodyPointer, chunk->bodyLength, chunk->chunkSize);
            chunk->bodyPointer += chunk->bodyLength;
            chunk->bodyLength = 0;
            return true;
        }

        formatChunk(responseBuffer, chunk->bodyPointer, chunk->chunkDataLength, chunk->chunkSize);
        chunk->bodyPointer += chunk->chunkDataLength;
        chunk->bodyLength -= chunk->chunkDataLength;
        return true;
    }

    if (!chunk->isLastChunk) {
        formatChunk(responseBuffer, NULL, 0, chunk->chunkSize);
        chunk->isLastChunk = true;
        return true;
    }
    return false;
}

void deleteHTTPServer(ServerContext *context) {
    if (context != NULL) {
        ServerConfiguration *configuration = context->configuration;
        if (configuration != NULL) {
            for (uint32_t i = 0; i < getVectorSize(configuration->urlMappings); i++) {
                free(vectorGet(configuration->urlMappings, i));
            }
            vectorDelete(configuration->urlMappings);
            configuration->urlMappings = NULL;
        }
        free(context);
    }
}

static void formatChunk(char *responseBuffer, const char *dataPointer, uint32_t dataLen, uint32_t chunkSize) {
    memset(responseBuffer, 0, chunkSize);
    sprintf(responseBuffer, "%lx\r\n", dataLen);    // Hexadecimal chunk size format

    if (dataPointer != NULL) {
        strncat(responseBuffer, dataPointer, dataLen);
        strcat(responseBuffer, "\r\n");
    }
}
