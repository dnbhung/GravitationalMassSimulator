#ifndef CIRBUFFER_H
#define CIRBUFFER_H

#include <SDL3/SDL.h>


/* Circular buffer*/
struct cirBuffer
{
    int writePointer;
    int readPointer;

    int count;
    int capacity;

    struct SDL_FRect *buffer;
};

void writeCirBuffer(struct cirBuffer *wishedBuffer, struct SDL_FRect passedRect);
struct SDL_FRect *readCirBuffer(struct cirBuffer *wishedBuffer);

#endif