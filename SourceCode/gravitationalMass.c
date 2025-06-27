#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <math.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;

const unsigned short NUMBER_OF_BALLS = 10;
const float PI = 3.14159265f;

Uint64 lastTime;
struct ObjectList ObjectContainer;
struct TextLabelList TextContainer;

int WindowHeight;
int WindowWidth;
static int collision = 1;
static int dragging = 0;

float CameraX = 0;
float CameraY = 0;

float cameraRootX;
float cameraRootY;

float previousMouseX;
float previousMouseY;

float zoom = 1.0f;
const float zoomStep = 0.1f;

float maximumZoom = 10.0f;
float minimumZoom = 0.05f;

/* Circular buffer*/
struct cirBuffer
{
    int writePointer;
    int readPointer;

    int count;
    int capacity;

    struct SDL_FRect *buffer;
};

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

struct TextLabel
{
    SDL_Texture *Texture;
    SDL_FRect dst;
    float lifetime;
};

struct TextLabelList
{
    int NumItems;
    int Capacity;
    struct TextLabel *Data;
};

/* This function calculates the distance between 2 points using Pythagorean theorem*/
float distance(float x1, float y1, float x2, float y2)
{
    return sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

/* This function draws a circle*/
void DrawCircle(const float size, float x, float y)
{

    int detail = (int)(4 + SDL_logf(size + 1.0f) * 10.0f);
    if (detail > 360)
        detail = 360;

    float step = 2.0f * PI / detail;
    for (float angle = 0; angle < 2.0f * PI; angle += step)
    {
        float px = x + SDL_cosf(angle) * size;
        float py = y + SDL_sinf(angle) * size;
        SDL_RenderPoint(renderer, (int)px, (int)py);
    }
}
/* This function adds an item into a provided list. If the list is full, it would automatically reallocate room for 10 more objects.*/
int AddItem(struct ObjectList *WishedList, struct Object PassedObject)
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

int AddItem_TL(struct TextLabelList *WishedList, struct TextLabel PassedObject)
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

void ClearList(struct ObjectList *WishedList)
{
    for (int i = 0; i < WishedList->NumItems; ++i)
    {
        struct Object *obj = &WishedList->Data[i];
        if (obj->trailBuffer.buffer != NULL)
        {
            SDL_free(obj->trailBuffer.buffer);
            obj->trailBuffer.buffer = NULL;
        }
        obj->trailBuffer.capacity = 100;
        obj->trailBuffer.count = 0;
        obj->trailBuffer.readPointer = 0;
        obj->trailBuffer.writePointer = 0;
    }

    SDL_free(WishedList->Data);
    WishedList->Data = NULL;
    WishedList->NumItems = 0;
    WishedList->Capacity = 10;
}

void ClearList_TL(struct TextLabelList *WishedList)
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

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Simulation", "1.0", "com.hung.simulation");

    SDL_Color color = {255, 255, 255, SDL_ALPHA_OPAQUE};

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Gravitational Masses Simulation", 2000, 1000, SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE))
    {
        SDL_Log("Adaptive VSync not supported, trying normal VSync...");
        if (!SDL_SetRenderVSync(renderer, 1))
        {
            SDL_Log("Couldn't enable any VSync mode: %s", SDL_GetError());
        }
    }
    if (!TTF_Init())
    {
        SDL_Log("Couldn't initiate TTF: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    font = TTF_OpenFont("font.ttf", 24);
    if (!font)
    {
        SDL_Log("Couldn't open font: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // SDL_SetWindowFullscreen(window, 1);
    SDL_GetWindowSize(window, &WindowWidth, &WindowHeight);

    cameraRootX = CameraX + WindowWidth * 0.5f;
    cameraRootY = CameraY + WindowHeight * 0.5f;
    // Create a container to store our lovely objects
    ObjectContainer.Capacity = NUMBER_OF_BALLS;
    ObjectContainer.NumItems = 0;
    ObjectContainer.Data = SDL_malloc(ObjectContainer.Capacity * sizeof(struct Object));

    if (ObjectContainer.Data == NULL)
    {
        SDL_Log("Cannot allocate buffer for Object Container.");
        return SDL_APP_FAILURE;
    }

    TextContainer.Capacity = 10;
    TextContainer.NumItems = 0;
    TextContainer.Data = SDL_malloc(ObjectContainer.Capacity * sizeof(struct TextLabel));

    if (TextContainer.Data == NULL)
    {
        ClearList(&ObjectContainer);
        SDL_Log("Cannot allocate buffer for Text Container.");
        return SDL_APP_FAILURE;
    }

    /* Create the text */

    char guides[5][50] = {
        "M - Spawn object at cursor ",
        "N - Toggle collision       ",
        "P - Despawn all objects    ",
        "Drag your mouse to move cam",
        "Scroll to zoom             ",
    };

    for (int i = 0; i < 5; ++i)
    {

        SDL_Surface *text = TTF_RenderText_Blended(font, guides[i], 0, color);
        SDL_Texture *texture;
        if (text)
        {
            texture = SDL_CreateTextureFromSurface(renderer, text);
            SDL_DestroySurface(text);
            struct TextLabel myLabel;

            myLabel.dst = (SDL_FRect){100, 100 + (25 * i), 250, 25};
            myLabel.Texture = texture;
            myLabel.lifetime = 5.0f;

            AddItem_TL(&TextContainer, myLabel);
        }
        if (!texture)
        {
            ClearList(&ObjectContainer);
            ClearList_TL(&TextContainer);
            SDL_Log("Couldn't create text: %s\n", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_M)
    {
        // Add an object to the simulation
        struct Object circle;

        circle.dx = 0.0f;
        circle.dy = 0.0f;
        // A random size 0.0f - 30.0f
        circle.size = (SDL_randf() + 1.0f) * 15.0f;

        float MouseX;
        float MouseY;
        SDL_GetMouseState(&MouseX, &MouseY);
        // Calculate the positiom based on mouse pos and cameraRoot
        circle.y = cameraRootY - MouseY / zoom;
        circle.x = cameraRootX - MouseX / zoom;
        // Calculate the mass according to the size
        circle.mass = circle.size * circle.size * PI * 8;

        circle.trailBuffer.capacity = 100;
        circle.trailBuffer.count = 0;
        circle.trailBuffer.readPointer = 0;
        circle.trailBuffer.writePointer = 0;
        circle.trailBuffer.buffer = SDL_malloc(circle.trailBuffer.capacity * sizeof(struct SDL_FRect));

        AddItem(&ObjectContainer, circle);
    }
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_N)
    {
        // Toggle collision
        collision = !collision; // toggle 0 - 1
    }
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_P)
    {
        // Delete all objects
        ClearList(&ObjectContainer);
        // Reset capacity after successful malloc
        ObjectContainer.Capacity = NUMBER_OF_BALLS;
        ObjectContainer.Data = SDL_malloc(ObjectContainer.Capacity * sizeof(struct Object));
        if (!ObjectContainer.Data)
        {
            SDL_Log("Failed to reallocate ObjectContainer after ClearList.");
            return SDL_APP_FAILURE;
        }
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        dragging = 1;
        previousMouseX = event->button.x;
        previousMouseY = event->button.y;
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        dragging = 0;
    }
    else if (event->type == SDL_EVENT_MOUSE_WHEEL)
    {
        if (event->wheel.y > 0)
        {
            zoom += zoomStep;
        }
        else if (event->wheel.y < 0)
        {
            zoom -= zoomStep;
        }

        if (zoom < minimumZoom)
            zoom = minimumZoom; // limit min zoom
        if (zoom > maximumZoom)
            zoom = maximumZoom; // limit max zoom

        /* Update cameraRoot accordingly*/
        cameraRootX = CameraX + (WindowWidth * 0.5f) / zoom;
        cameraRootY = CameraY + (WindowHeight * 0.5f) / zoom;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION && dragging)
    {
        /* Find difference in X and Y*/
        float dx = event->motion.x - previousMouseX;
        float dy = event->motion.y - previousMouseY;
        /* Apply changes*/
        CameraX += dx / zoom;
        CameraY += dy / zoom;
        /* Update cameraRoot accordingly*/
        cameraRootX = CameraX + (WindowWidth * 0.5f) / zoom;
        cameraRootY = CameraY + (WindowHeight * 0.5f) / zoom;

        previousMouseX = event->motion.x;
        previousMouseY = event->motion.y;
    }

    if (event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        WindowWidth = event->window.data1;
        WindowHeight = event->window.data2;
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    int i;

    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    float dt = (now - lastTime) / (float)freq;
    lastTime = now;
    if (dt > 0.05f)
        dt = 0.05f; // cap at 50ms (20 FPS)

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 1, 1, 1, SDL_ALPHA_OPAQUE); /* grey, full alpha (full opacity) */
    SDL_RenderClear(renderer);                                   /* start with a blank canvas. */

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE); /* while, full alpha */

    // Render and handle Objects
    for (int i = 0; i < ObjectContainer.NumItems; ++i)
    {
        struct Object *selfObject = &ObjectContainer.Data[i];

        for (int j = i + 1; j < ObjectContainer.NumItems; ++j)
        {
            struct Object *otherObject = &ObjectContainer.Data[j];
            float distanceBetweenObject = distance(otherObject->x, otherObject->y, selfObject->x, selfObject->y);

            if (distanceBetweenObject <= selfObject->size + otherObject->size) // Collision, neuron activation, DOPAMINE RELEASED
            {
                if (!collision)
                {
                    continue;
                }

                float m1 = selfObject->mass;
                float m2 = otherObject->mass;

                // 1D elastic collision formula for dx (repeat for dy)
                float newDx1 = (selfObject->dx * (m1 - m2) + 2 * m2 * otherObject->dx) / (m1 + m2);
                float newDx2 = (otherObject->dx * (m2 - m1) + 2 * m1 * selfObject->dx) / (m1 + m2);

                float newDy1 = (selfObject->dy * (m1 - m2) + 2 * m2 * otherObject->dy) / (m1 + m2);
                float newDy2 = (otherObject->dy * (m2 - m1) + 2 * m1 * selfObject->dy) / (m1 + m2);

                selfObject->dx = newDx1;
                selfObject->dy = newDy1;
                otherObject->dx = newDx2;
                otherObject->dy = newDy2;
                continue;
            }

            /* Newton's Law of Universal Gravitation*/
            float force = 1000.0f * ((selfObject->mass * otherObject->mass) / (distanceBetweenObject * distanceBetweenObject) + 50.0f);

            /* Normalizing DirectionX and DirectionY*/
            float invDist = 1.0f / distanceBetweenObject;
            float directionX = (otherObject->x - selfObject->x) * invDist;
            float directionY = (otherObject->y - selfObject->y) * invDist;

            /* Finding acceleration with a formula derived from Newton's second law */
            float selfAccel = force / selfObject->mass;
            selfObject->dx += directionX * selfAccel * dt;
            selfObject->dy += directionY * selfAccel * dt;

            float otherAccel = force / otherObject->mass;
            otherObject->dx -= directionX * otherAccel * dt;
            otherObject->dy -= directionY * otherAccel * dt;
        }

        /* Append trail*/
        writeCirBuffer(&selfObject->trailBuffer, (SDL_FRect){selfObject->x - 1.0f, selfObject->y - 1.0f, 2.0f, 2.0f});
        /* Render trail*/

        int start = (selfObject->trailBuffer.writePointer - selfObject->trailBuffer.count + selfObject->trailBuffer.capacity) % selfObject->trailBuffer.capacity;
        selfObject->trailBuffer.readPointer = start;

        for (int i = 0; i < selfObject->trailBuffer.count; ++i)
        {
            struct SDL_FRect *trail = readCirBuffer(&selfObject->trailBuffer);

            /* Calculate relative coordinates*/
            float TrailRelativeX = cameraRootX - trail->x;
            float TrailRelativeY = cameraRootY - trail->y;

            /* Apply zoom*/
            TrailRelativeX *= zoom;
            TrailRelativeY *= zoom;
            float TrailSizeX = trail->w;
            float TrailSizeY = trail->h;

            if (!(
                    TrailRelativeX + TrailSizeX < 0 || TrailRelativeX - TrailSizeX > WindowWidth ||
                    TrailRelativeY + TrailSizeY < 0 || TrailRelativeY - TrailSizeY > WindowHeight))
            {
                SDL_FRect screenRect = {
                    .x = TrailRelativeX,
                    .y = TrailRelativeY,
                    .w = TrailSizeX,
                    .h = TrailSizeY}; // Create a copy

                Uint8 r, g, b, a;
                int steps = (selfObject->trailBuffer.readPointer - 1 - selfObject->trailBuffer.writePointer + selfObject->trailBuffer.capacity) % selfObject->trailBuffer.capacity;
                Uint8 alphaval = 255.0f * (steps / (float)selfObject->trailBuffer.capacity);

                SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
                SDL_SetRenderDrawColor(renderer, r, g, b, alphaval);
                SDL_RenderFillRect(renderer, &screenRect);
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
            }
        }

        selfObject->x += selfObject->dx * dt; // Apply dx
        selfObject->y += selfObject->dy * dt; // Apply dy

        /* Calculate relative coordinates*/
        float ObjectRelativeX = cameraRootX - selfObject->x;
        float ObjectRelativeY = cameraRootY - selfObject->y;

        /* Apply zoom*/
        ObjectRelativeX *= zoom;
        ObjectRelativeY *= zoom;
        float ObjectSize = selfObject->size * zoom;

        /* Check if object is out-of-bound, if yes then don't render*/
        if (!(
                ObjectRelativeX + ObjectSize < 0 || ObjectRelativeX - ObjectSize > WindowWidth ||
                ObjectRelativeY + ObjectSize < 0 || ObjectRelativeY - ObjectSize > WindowHeight))
        {
            /* Render the object*/
            DrawCircle(ObjectSize, ObjectRelativeX, ObjectRelativeY);
        }
    }

    // Render text
    for (int i = 0; i < TextContainer.NumItems; ++i)
    {
        TextContainer.Data[i].lifetime -= dt;

        if (TextContainer.Data[i].lifetime <= 0.0f)
        {
            SDL_DestroyTexture(TextContainer.Data[i].Texture);

            // Shift left to remove item
            for (int j = i + 1; j < TextContainer.NumItems; ++j)
            {
                TextContainer.Data[j - 1] = TextContainer.Data[j];
            }
            TextContainer.NumItems--;
        }
        else
        {
            SDL_RenderTexture(renderer, TextContainer.Data[i].Texture, NULL, &TextContainer.Data[i].dst);
        }
    }

    SDL_RenderPresent(renderer); /* put it all on the screen! */

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
    /* Clean up heap space:D*/
    if (ObjectContainer.Data != NULL)
    {
        ClearList(&ObjectContainer);
    }
    if (TextContainer.Data != NULL)
    {
        ClearList_TL(&TextContainer);
    }
}