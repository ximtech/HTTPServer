#pragma once

#include "BaseTestTemplate.h"
#include "HTTPServer.h"

#define BUFFER_SIZE 256

static void handleNotFound(ServerContext *context, HTTPParser *request);

static void *testSetup(const MunitParameter params[], void *userData) {
    ServerConfiguration *configuration = malloc(sizeof(struct ServerConfiguration));
    configuration->rxDataBufferSize = BUFFER_SIZE;
    configuration->defaultHandler = handleNotFound;
    ServerContext *context = initHTTPServerContext(configuration);
    assert_not_null(context);
    return context;
}

static void handleRoot(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/", request->uriPath);
    assert_int(HTTP_GET, ==, request->method);
}

static void handlePutId(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/api/123/test", request->uriPath);
    assert_int(HTTP_PUT, ==, request->method);
}

static void handlePostId(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/api/4321/test", request->uriPath);
    assert_int(HTTP_POST, ==, request->method);
}

static void handlePostTest(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/api/tEsT/other", request->uriPath);
    assert_int(HTTP_POST, ==, request->method);
}

static void handleNotFound(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/api/unknown/other", request->uriPath);
    assert_int(HTTP_HEAD, ==, request->method);
}


static MunitResult httpServerHandleTest(const MunitParameter params[], void *data) {
    ServerContext *context = data;

    addUrlMapping(context, "^/$", HTTP_GET, handleRoot);
    addUrlMapping(context, "^/api/\\d+/test$", HTTP_PUT, handlePutId);
    addUrlMapping(context, "^/api/\\d+/test$", HTTP_POST, handlePostId);
    addUrlMapping(context, "^/api/[Tt][Ee][Ss][Tt]/other$", HTTP_POST, handlePostTest);
    assert_uint32(4, ==, getVectorSize(context->configuration->urlMappings));

    HTTPParser *httpParser = getHttpParserInstance();
    RequestHandlerFunction handle;
    char requestBuffer[BUFFER_SIZE] = {0};

    strcpy(requestBuffer, "GET / HTTP/1.1\n\n");
    parseHttpBuffer(requestBuffer, httpParser, HTTP_REQUEST);
    handle = handleIncomingServerRequest(context, httpParser);
    assert_true(handle == handleRoot);
    handle(context, httpParser);

    strcpy(requestBuffer, "PUT /api/123/test HTTP/1.1\n\n");
    parseHttpBuffer(requestBuffer, httpParser, HTTP_REQUEST);
    handle = handleIncomingServerRequest(context, httpParser);
    assert_true(handle == handlePutId);
    handle(context, httpParser);

    strcpy(requestBuffer, "POST /api/4321/test HTTP/1.1\n\n");
    parseHttpBuffer(requestBuffer, httpParser, HTTP_REQUEST);
    handle = handleIncomingServerRequest(context, httpParser);
    assert_true(handle == handlePostId);
    handle(context, httpParser);

    strcpy(requestBuffer, "POST /api/tEsT/other HTTP/1.1\n\n");
    parseHttpBuffer(requestBuffer, httpParser, HTTP_REQUEST);
    handle = handleIncomingServerRequest(context, httpParser);
    assert_true(handle == handlePostTest);
    handle(context, httpParser);

    strcpy(requestBuffer, "HEAD /api/unknown/other HTTP/1.1\n\n");
    parseHttpBuffer(requestBuffer, httpParser, HTTP_REQUEST);
    handle = handleIncomingServerRequest(context, httpParser);
    assert_true(handle == handleNotFound);
    handle(context, httpParser);

    deleteHttpParser(httpParser);
    return MUNIT_OK;
}

static MunitResult httpServerResponseStatusTest(const MunitParameter params[], void *data) {
    char buffer[BUFFER_SIZE] = {0};
    ServerContext *context = data;
    context->txDataBufferPointer = buffer;
    formatHTTPServerStatusLine(context->txDataBufferPointer, HTTP_OK);
    assert_string_equal("HTTP/1.1 200 OK\r\n", buffer);
    return MUNIT_OK;
}

static MunitResult httpServerHeaderLengthTest(const MunitParameter params[], void *data) {
    HashMap headers = getHashMapInstance(16);
    hashMapPut(headers, "header1", "value");
    hashMapPut(headers, "header2", "value");
    hashMapPut(headers, "header3", "value");
    assert_uint32(50, ==, getHTTPServerHeadersLength(headers));
    hashMapDelete(headers);
    return MUNIT_OK;
}

static MunitResult httpServerHeaderFormatTest(const MunitParameter params[], void *data) {
    char buffer[BUFFER_SIZE] = {0};
    ServerContext *context = data;
    context->txDataBufferPointer = buffer;

    HashMap headers = getHashMapInstance(16);
    hashMapPut(headers, "header1", "value");
    hashMapPut(headers, "header2", "value");
    hashMapPut(headers, "header3", "value");

    formatHTTPServerHeaders(context->txDataBufferPointer, headers);
    assert_string_equal("header1: value\r\n"
                        "header2: value\r\n"
                        "header3: value\r\n\r\n", buffer);

    hashMapDelete(headers);
    return MUNIT_OK;
}


static MunitResult httpServerBodyFormatTest(const MunitParameter params[], void *data) {
    char buffer[BUFFER_SIZE] = {0};
    ServerContext *context = data;
    context->txDataBufferPointer = buffer;
    formatHTTPServerResponseBody(context, "body");
    assert_string_equal("body\r\n\r\n", buffer);
    return MUNIT_OK;
}

static MunitResult httpServerChunkedResponseTest(const MunitParameter params[], void *data) {
    char buffer[BUFFER_SIZE] = {0};
    char *response = "test1, test2, test3, test4";

    ChunkedResponse chunk = prepareChunkedResponse(15, response, strlen(response));
    assert_true(hasNextHTTPChunk(&chunk, buffer));
    assert_string_equal("8\r\ntest1, t\r\n", buffer);

    assert_true(hasNextHTTPChunk(&chunk, buffer));
    assert_string_equal("8\r\nest2, te\r\n", buffer);

    assert_true(hasNextHTTPChunk(&chunk, buffer));
    assert_string_equal("a\r\nst3, test4\r\n", buffer);

    assert_true(hasNextHTTPChunk(&chunk, buffer));
    assert_string_equal("0\r\n", buffer);

    assert_false(hasNextHTTPChunk(&chunk, buffer));
    return MUNIT_OK;
}

static void testTearDown(void *context) {
    ServerContext *ctx = context;
    deleteHTTPServer(ctx);
    free(ctx->configuration);
}

static MunitTest httpServerTests[] = {
        {.name = "Test OK - should correctly handle request uri", .test = httpServerHandleTest, .setup = testSetup, .tear_down = testTearDown},
        {.name = "Test OK - should correctly format response status string", .test = httpServerResponseStatusTest, .setup = testSetup, .tear_down = testTearDown},
        {.name = "Test OK - should correctly calculate header length", .test = httpServerHeaderLengthTest},
        {.name = "Test OK - should correctly format response headers", .test = httpServerHeaderFormatTest, .setup = testSetup, .tear_down = testTearDown},
        {.name = "Test OK - should correctly format response body", .test = httpServerBodyFormatTest, .setup = testSetup, .tear_down = testTearDown},
        {.name = "Test OK - should correctly format chunked response", .test = httpServerChunkedResponseTest},
        END_OF_TESTS
};

static const MunitSuite httpServerTestSuite = {
        .prefix = "HTTP server: ",
        .tests = httpServerTests,
        .suites = NULL,
        .iterations = 1,
        .options = MUNIT_SUITE_OPTION_NONE
};