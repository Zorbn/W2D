#include "renderer.h"
#include "sprite.h"

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Cannot initialize SDL");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("WGPU C", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 640, 480,
                                          SDL_WINDOW_RESIZABLE);

    if (!window) {
        printf("Cannot create window");
        return 1;
    }

    Renderer renderer = rendererCreate(window, "shader.wgsl");

    SpriteBatch spriteBatch =
        spriteBatchCreate(10, "test.png", &renderer, (SpriteBatchOptions){
                .textureFilteringMode = TextureFilteringModeNearest,
                .textureWrapMode = TextureWrapModeRepeat,
        });
    spriteBatchAdd(&spriteBatch, (Sprite){
                                     .x = 8.0f,
                                     .y = 8.0f,
                                     .z = -1.0f,
                                     .width = 160.0f,
                                     .height = 160.0f,
                                     .texX = 8.0f,
                                     .texWidth = 16.0f,
                                     .texHeight = 16.0f,
                                 });
    spriteBatchAdd(&spriteBatch, (Sprite){
                                     .x = 0.0f,
                                     .y = 0.0f,
                                     .z = 0.0f,
                                     .width = 64.0f,
                                     .height = 64.0f,
                                     .texX = 8.0f,
                                     .texY = 8.0f,
                                     .texWidth = 8.0f,
                                     .texHeight = 8.0f,
                                 });

    bool isRunning = true;
    while (isRunning) {
        rendererBegin(&renderer, 0.4f, 0.6f, 0.9f);

        spriteBatchDraw(&spriteBatch, &renderer);

        rendererEnd(&renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            }
        }
    }

    SDL_Quit();

    return 0;
}