#ifndef OBJECTS_H
#define OBJECTS_H

#include "circularBuffer.h"
#include <stddef.h>

#define NUMBER_OF_TRAIL_PARTICLES 150

/* This structure defines an Object.*/
struct Object
{
    float x;
    float y;

    float dx;
    float dy;

    float size;
    float mass;

    struct cirBuffer trailBuffer;
};

/* This structure defines an Object container, used to contain objects. */
struct ObjectList
{
    int NumItems;
    int Capacity;
    struct Object *Data;
};

/* This function adds an item into a provided list. If the list is full, it would automatically reallocate room for 10 more objects.*/
int AddObject(struct ObjectList *WishedList, struct Object PassedObject);
int ClearObjects(struct ObjectList *WishedList);



#endif