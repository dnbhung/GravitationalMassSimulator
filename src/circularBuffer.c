#include "circularBuffer.h"

void writeCirBuffer(struct cirBuffer *wishedBuffer, struct SDL_FRect passedRect)
{
    wishedBuffer->buffer[wishedBuffer->writePointer] = passedRect;
    wishedBuffer->writePointer = (wishedBuffer->writePointer + 1) % wishedBuffer->capacity;

    if (wishedBuffer->count < wishedBuffer->capacity)
    {
        ++wishedBuffer->count;
    }
}

struct SDL_FRect *readCirBuffer(struct cirBuffer *wishedBuffer)
{
    struct SDL_FRect *value = &wishedBuffer->buffer[wishedBuffer->readPointer];
    wishedBuffer->readPointer = (wishedBuffer->readPointer + 1) % wishedBuffer->capacity;
    return value;
}
