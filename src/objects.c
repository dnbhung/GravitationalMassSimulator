#include "objects.h"

int ClearObjects(struct ObjectList *WishedList)
{
    for (int i = 0; i < WishedList->NumItems; ++i)
    {
        struct Object *obj = &WishedList->Data[i];
        if (obj->trailBuffer.buffer != NULL)
        {
            SDL_free(obj->trailBuffer.buffer);
            obj->trailBuffer.buffer = NULL;
        }
        obj->trailBuffer.capacity = NUMBER_OF_TRAIL_PARTICLES;
        obj->trailBuffer.count = 0;
        obj->trailBuffer.readPointer = 0;
        obj->trailBuffer.writePointer = 0;
    }

    SDL_free(WishedList->Data);
    WishedList->Data = NULL;
    WishedList->NumItems = 0;
    WishedList->Capacity = 10;
}

/* This function adds an item into a provided list. If the list is full, it would automatically reallocate room for 10 more objects.*/
int AddObject(struct ObjectList *WishedList, struct Object PassedObject)
{
    if (WishedList->NumItems == WishedList->Capacity)
    {
        struct Object *ptr;
        WishedList->Capacity += 10;

        ptr = SDL_realloc(WishedList->Data, WishedList->Capacity * sizeof(struct Object));
        if (ptr == NULL)
        {
            return -1;
        }
        WishedList->Data = ptr;
    }

    WishedList->Data[WishedList->NumItems] = PassedObject;
    return WishedList->NumItems++;
}

