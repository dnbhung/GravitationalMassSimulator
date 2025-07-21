#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <math.h>

#include "objects.h"
#include "circularBuffer.h"
#include "textLabel.h"

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;

#define NUMBER_OF_BALLS 10
#define PI 3.14159265f

Uint64 lastTime;
struct ObjectList ObjectContainer;
struct TextLabelList TextContainer;

int WindowHeight;
int WindowWidth;
static int collision = 1;
static int dragging = 0;
static int paused = 0;
static int helpPanel = 1;

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

/* This function calculates the distance between 2 points using Pythagorean theorem*/
float distance(float x1, float y1, float x2, float y2)
{
    return sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

/* This function draws a circle*/
void DrawCircle(const float size, float x, float y)
{

    int detail = (int)(4 + SDL_logf(size + 1.0f) * 15.0f);
    if (detail > 360)
        detail = 360;

    float step = 2.0f * PI / detail;

    int i = 0;
    struct SDL_FPoint *cirPoints = SDL_malloc(detail * sizeof(struct SDL_FPoint));

    for (float angle = 0; angle < 2.0f * PI; angle += step)
    {
        float px = x + SDL_cosf(angle) * size;
        float py = y + SDL_sinf(angle) * size;
        cirPoints[i] = (struct SDL_FPoint){px, py};
        ++i;
        if (i >= detail)
            break;
    }
    SDL_RenderPoints(renderer, cirPoints, detail);
    SDL_free(cirPoints);
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
        SDL_Log("Adaptive VSync not supported, trying normal VSync");
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
        ClearObjects(&ObjectContainer);
        SDL_Log("Cannot allocate buffer for Text Container.");
        return SDL_APP_FAILURE;
    }

    struct TextLabel guides[7] = {
        (struct TextLabel){
            .text = "M - Spawn object at cursor",
            .dst = (SDL_FRect){100, 100, 250, 25}},
        (struct TextLabel){
            .text = "N - Toggle collision",
            .dst = (SDL_FRect){100, 125, 200, 25}},
        (struct TextLabel){
            .text = "P - Despawn all objects",
            .dst = (SDL_FRect){100, 150, 250, 25}},
        (struct TextLabel){
            .text = "O - Continue/Pause the simulation",
            .dst = (SDL_FRect){100, 175, 350, 25}},
        (struct TextLabel){
            .text = "H - Toggle this panel",
            .dst = (SDL_FRect){100, 200, 200, 25}},
        (struct TextLabel){
            .text = "Drag your mouse to move cam",
            .dst = (SDL_FRect){100, 225, 275, 25}},
        (struct TextLabel){
            .text = "Scroll to zoom",
            .dst = (SDL_FRect){100, 250, 150, 25}},

        };

    int guideSize = sizeof(guides) / sizeof(guides[0]);

    for (int i = 0; i < guideSize; ++i)
    {
        struct TextLabel *Label = &guides[i];

        SDL_Surface *text = TTF_RenderText_Blended(font, Label->text, 0, color);
        SDL_Texture *texture;
        if (text)
        {
            texture = SDL_CreateTextureFromSurface(renderer, text);
            SDL_DestroySurface(text);

            Label->Texture = texture;
        }
        if (!texture)
        {
            SDL_Log("Couldn't create text: %s\n", SDL_GetError());
        }
        else
        {
            AddTextLabel(&TextContainer, *Label);
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

    /*When M is pressed, add object to simulation*/
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

        circle.trailBuffer.capacity = NUMBER_OF_TRAIL_PARTICLES;
        circle.trailBuffer.count = 0;
        circle.trailBuffer.readPointer = 0;
        circle.trailBuffer.writePointer = 0;
        circle.trailBuffer.buffer = SDL_malloc(circle.trailBuffer.capacity * sizeof(struct SDL_FRect));

        AddObject(&ObjectContainer, circle);
    }
    /* Otherwise, if N is pressed, toggle collision*/
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_N)
    {
        // Toggle collision
        collision = !collision; // toggle 0 - 1
    }
    /* Otherwise, if P is pressed, delete all objects from simulation*/
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_P)
    {
        // Delete all objects
        ClearObjects(&ObjectContainer);
        // Reset capacity after successful malloc
        ObjectContainer.Capacity = NUMBER_OF_BALLS;
        ObjectContainer.Data = SDL_malloc(ObjectContainer.Capacity * sizeof(struct Object));
        if (!ObjectContainer.Data)
        {
            SDL_Log("Failed to reallocate ObjectContainer after ClearObjects.");
            return SDL_APP_FAILURE;
        }
    }
    /* Otherwise, if O is pressed, pause/continue the simulation*/
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_O)
    {
        paused = !paused;
    }
    /* Otherwise, if H is pressed, toggle the help panel*/
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_H)
    {
        helpPanel = !helpPanel;
    }

    /* If mouse button is down, start dragging*/
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        dragging = 1;
        previousMouseX = event->button.x;
        previousMouseY = event->button.y;
    }
    /* Otherwise, if mouse button is up, stop dragging*/
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        dragging = 0;
    }
    /* Otherwise, if the mouse wheel is scrolling, apply zoom accordingly*/
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
    /* If mouse is moving and is dragging, apply cameraRoot*/
    /* TODO: fix world space to camera space math*/
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
    /* If window is being resized, update window sizes*/
    if (event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        WindowWidth = event->window.data1;
        WindowHeight = event->window.data2;

        /* Update cameraRoot accordingly*/
        cameraRootX = CameraX + (WindowWidth * 0.5f) / zoom;
        cameraRootY = CameraY + (WindowHeight * 0.5f) / zoom;
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

void calcPhysicsBetween2Objects(struct Object *selfObject, struct Object *otherObject, float dt)
{
    float distanceBetweenObject = distance(otherObject->x, otherObject->y, selfObject->x, selfObject->y);

    if (distanceBetweenObject <= selfObject->size + otherObject->size) // Collision, neuron activation, DOPAMINE RELEASED
    {
        if (!collision)
        {
            return;
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
        return;
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

void renderTrailForObject(struct Object *selfObject)
{
    int start = (selfObject->trailBuffer.writePointer - selfObject->trailBuffer.count + selfObject->trailBuffer.capacity) % selfObject->trailBuffer.capacity;
    selfObject->trailBuffer.readPointer = start;

    Uint8 PrevAlpha = 0;

    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    for (int i = 0; i < selfObject->trailBuffer.count; ++i)
    {
        struct SDL_FRect *trail = readCirBuffer(&selfObject->trailBuffer);

        /* Calculate relative coordinates*/
        float TrailRelativeX = cameraRootX - trail->x;
        float TrailRelativeY = cameraRootY - trail->y;

        /* Apply zoom*/
        TrailRelativeX *= zoom;
        TrailRelativeY *= zoom;
        float TrailSizeX = trail->w * (zoom + 0.5f);
        float TrailSizeY = trail->h * (zoom + 0.5f);

        if (!(
                TrailRelativeX + TrailSizeX < 0 || TrailRelativeX - TrailSizeX > WindowWidth ||
                TrailRelativeY + TrailSizeY < 0 || TrailRelativeY - TrailSizeY > WindowHeight))
        {
            SDL_FRect screenRect = {
                .x = TrailRelativeX,
                .y = TrailRelativeY,
                .w = TrailSizeX,
                .h = TrailSizeY}; // Create a copy

            int steps = (selfObject->trailBuffer.readPointer - selfObject->trailBuffer.writePointer + selfObject->trailBuffer.capacity) % selfObject->trailBuffer.capacity;
            Uint8 alphaval;

            if (steps == 0)
            {
                alphaval = 0; // Fully transparent if there are no steps.
            }
            else if (steps == selfObject->trailBuffer.capacity - 1)
            {
                alphaval = 255; // Fully opaque when we are one step from being full.
            }
            else
            {
                alphaval = (Uint8)(255.0f * ((float)steps / (float)selfObject->trailBuffer.capacity));
            }

            if (alphaval != PrevAlpha)
            {
                SDL_SetRenderDrawColor(renderer, r, g, b, alphaval);
                PrevAlpha = alphaval;
            }

            SDL_RenderFillRect(renderer, &screenRect);
        }
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void renderObject(struct Object *selfObject)
{
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

void renderText(float dt)
{
    for (int i = 0; i < TextContainer.NumItems; ++i)
    {
        if (helpPanel)
        {
            SDL_RenderTexture(renderer, TextContainer.Data[i].Texture, NULL, &TextContainer.Data[i].dst);
        }
    }
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
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

        if (!paused)
        {
            for (int j = i + 1; j < ObjectContainer.NumItems; ++j)
            {
                struct Object *otherObject = &ObjectContainer.Data[j];
                calcPhysicsBetween2Objects(selfObject, otherObject, dt);
            }

            float prevX = selfObject->x;
            float prevY = selfObject->y;

            selfObject->x += selfObject->dx * dt; // Apply dx
            selfObject->y += selfObject->dy * dt; // Apply dy

            float steps = distance(selfObject->x, selfObject->y, prevX, prevY);
            float jumps = 2.0f / (steps / 2.0f);

            for (float t = 0; t < 1; t += jumps)
            {
                float lerpX = prevX + (selfObject->x - prevX) * t;
                float lerpY = prevY + (selfObject->y - prevY) * t;
                /* Append trail*/
                writeCirBuffer(&selfObject->trailBuffer, (SDL_FRect){lerpX - 1.0f, lerpY - 1.0f, 2.0f, 2.0f});
            }
        }

        /* Render trail*/
        renderTrailForObject(selfObject);

        /* Render object*/
        renderObject(selfObject);
    }

    // Render text
    renderText(dt);

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
        ClearObjects(&ObjectContainer);
    }
    if (TextContainer.Data != NULL)
    {
        ClearTextLabels(&TextContainer);
    }
}