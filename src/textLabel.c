#include "textLabel.h"
#include <stddef.h>

void ClearTextLabels(struct TextLabelList *WishedList)
{
    for (int i = 0; i < WishedList->NumItems; ++i)
    {
        SDL_DestroyTexture(WishedList->Data[i].Texture);
    }
    SDL_free(WishedList->Data);
    WishedList->Data = NULL;
    WishedList->NumItems = 0;
    WishedList->Capacity = 10;
}

int AddTextLabel(struct TextLabelList *WishedList, struct TextLabel PassedObject)
{
    if (WishedList->NumItems == WishedList->Capacity)
    {
        struct TextLabel *ptr;
        WishedList->Capacity += 10;

        ptr = SDL_realloc(WishedList->Data, WishedList->Capacity * sizeof(struct TextLabel));
        if (ptr == NULL)
        {
            return -1;
        }
        WishedList->Data = ptr;
    }

    WishedList->Data[WishedList->NumItems] = PassedObject;
    return WishedList->NumItems++;
}