#ifndef TEXTLABEL_H
#define TEXTLABEL_H

#include <SDL3/SDL.h>

struct TextLabel
{
    char *text;
    SDL_Texture *Texture;
    SDL_FRect dst;
};

struct TextLabelList
{
    int NumItems;
    int Capacity;
    struct TextLabel *Data;
};


int AddTextLabel(struct TextLabelList *WishedList, struct TextLabel PassedObject);
void ClearTextLabels(struct TextLabelList *WishedList);

#endif
