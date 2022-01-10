# HTTPServer

Utility library provides main features for working with web server.

## Features

- Easy configuration
- URI resolve by regex pattern
- Request validation and parsing
- Response formatting helpers
- Chunked response handling

### Add as CPM project dependency

How to add CPM to the project, check the [link](https://github.com/cpm-cmake/CPM.cmake)

```cmake
CPMAddPackage(
        NAME HTTPServer
        GITHUB_REPOSITORY ximtech/HTTPServer
        GIT_TAG origin/main)

target_link_libraries(${PROJECT_NAME} HTTPServer)
```

```cmake
add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})
# For Clion STM32 plugin generated Cmake use 
target_link_libraries(${PROJECT_NAME}.elf HTTPServer)
```

## Usage

- ***Request URI handling***

```c

void handleRoot(ServerContext *context, HTTPParser *request) {
    assert_string_equal("/", request->uriPath);
    assert_int(HTTP_GET, ==, request->method);
}

void handlePostId(ServerContext *context, HTTPParser *request) {
    printf("%s\n", request->uriPath);
    assert_int(HTTP_POST, ==, request->method);
}

void handleNotFound(ServerContext *context, HTTPParser *request) {}


int main() {

    ServerConfiguration configuration = {0};
    configuration.serverPort = 80;
    configuration.serverTimeoutMs = 5000;
    configuration.rxDataBufferSize = 5000;
    configuration.defaultHandler = handleNotFound;
    
    ServerContext *context = initHTTPServerContext(&configuration);
    if (context == NULL) {
        printf("Failed to initialize server\n");
        return -1;
    }
    
    addUrlMapping(context, "^/$", HTTP_GET, handleRoot);    // handles only "/" query
    addUrlMapping(context, "^/api/\\d+/test$", HTTP_POST, handlePostId); // handles queries like: "/api/1/test"
}
```

- ***Chunked response***
```c
int main() {

    char buffer[BUFFER_SIZE] = {0};
    char *response = "test1, test2, test3, test4";
    int chunkSize = 15;

    // Split request to chunks
    ChunkedResponse chunk = prepareChunkedResponse(chunkSize, response, strlen(response));
    while(hasNextHTTPChunk(&chunk, buffer)) {
        printf("%s", buffer);
    }
    
    // Result:
    /*   
    "8\r\n"
    "test1, t\r\n"

    "8\r\n"
    "est2, te\r\n"

    "a\r\n"
    "st3, test4\r\n"

    "0\r\n"
    */
}
```

- ***Format HTTP headers***
```c
int main() {

    char buffer[BUFFER_SIZE] = {0};
    HashMap headers = getHashMapInstance(16);
    hashMapPut(headers, "header1", "value");
    hashMapPut(headers, "header2", "value");
    hashMapPut(headers, "header3", "value");

    formatHTTPServerHeaders(buffer, headers);
    assert_string_equal("header1: value\r\n"
                        "header2: value\r\n"
                        "header3: value\r\n\r\n", buffer);
    hashMapDelete(headers);
}
```