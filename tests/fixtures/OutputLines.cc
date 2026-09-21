#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

namespace {

unsigned long parseBoundedValue(
        const char *text,
        const char *name,
        unsigned long maximum) {
    errno = 0;
    char *end = nullptr;
    const unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > maximum) {
        fprintf(stderr, "Invalid %s: %s\n", name, text);
        exit(1);
    }
    return value;
}

} // anonymous namespace

int main(int argc, char *argv[]) {
    if (argc > 3) {
        fprintf(stderr, "Usage: %s [line-count [payload-width]]\n", argv[0]);
        return 1;
    }

    const unsigned long lineCount = argc >= 2
        ? parseBoundedValue(argv[1], "line count", 1000000)
        : 100;
    const unsigned long payloadWidth = argc >= 3
        ? parseBoundedValue(argv[2], "payload width", 1048576)
        : 78;
    const std::string payload(payloadWidth, 'X');

    for (unsigned long line = 1; line <= lineCount; ++line) {
        printf("%lu %s\n", line, payload.c_str());
    }
    return 0;
}
