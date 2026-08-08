// main.c -- Simple Breakout / Block Breaker using SDL2 (C)
// Compile (MSYS2 mingw64):
// gcc main.c -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -o main.exe
// Don't forget to copy SDL2.dll from C:/msys64/mingw64/bin to the exe folder.

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_W 800
#define WINDOW_H 600

#define PADDLE_W 120
#define PADDLE_H 16
#define PADDLE_Y_MARGIN 40
#define PADDLE_SPEED 600.0f // pixels/sec

#define BALL_SIZE 12
#define BALL_SPEED 360.0f // initial speed in px/sec

#define BRICK_ROWS 5
#define BRICK_COLS 10
#define BRICK_PADDING 6
#define BRICK_TOP_MARGIN 60
#define BRICK_LEFT_MARGIN 40
#define BRICK_HEIGHT 20

typedef struct {
    SDL_Rect rect;
    int alive;
    SDL_Color color;
} Brick;

static Brick bricks[BRICK_ROWS * BRICK_COLS];

typedef struct {
    SDL_Rect rect;
    float vx, vy;
    int active;
} Ball;

typedef struct {
    SDL_Rect rect;
} Paddle;

static Paddle paddle;
static Ball ball;
static int score = 0;
static int lives = 3;
static int bricks_left = 0;

void init_bricks() {
    int idx = 0;
    int available_width = WINDOW_W - 2 * BRICK_LEFT_MARGIN;
    int brick_w = (available_width - (BRICK_COLS - 1) * BRICK_PADDING) / BRICK_COLS;
    for (int r = 0; r < BRICK_ROWS; ++r) {
        for (int c = 0; c < BRICK_COLS; ++c) {
            Brick *b = &bricks[idx++];
            b->rect.w = brick_w;
            b->rect.h = BRICK_HEIGHT;
            b->rect.x = BRICK_LEFT_MARGIN + c * (brick_w + BRICK_PADDING);
            b->rect.y = BRICK_TOP_MARGIN + r * (BRICK_HEIGHT + BRICK_PADDING);
            b->alive = 1;
            // color by row
            switch (r % 5) {
                case 0: b->color = (SDL_Color){200, 60, 60, 255}; break;
                case 1: b->color = (SDL_Color){60, 180, 75, 255}; break;
                case 2: b->color = (SDL_Color){60, 140, 200, 255}; break;
                case 3: b->color = (SDL_Color){200, 150, 60, 255}; break;
                case 4: b->color = (SDL_Color){160, 60, 200, 255}; break;
            }
        }
    }
    bricks_left = BRICK_ROWS * BRICK_COLS;
    score = 0;
}

void reset_paddle_and_ball() {
    // paddle in middle bottom
    paddle.rect.w = PADDLE_W;
    paddle.rect.h = PADDLE_H;
    paddle.rect.x = (WINDOW_W - PADDLE_W) / 2;
    paddle.rect.y = WINDOW_H - PADDLE_Y_MARGIN;

    // ball sits above paddle and is inactive until space pressed
    ball.rect.w = BALL_SIZE;
    ball.rect.h = BALL_SIZE;
    ball.rect.x = paddle.rect.x + (paddle.rect.w - BALL_SIZE) / 2;
    ball.rect.y = paddle.rect.y - BALL_SIZE - 2;
    ball.vx = 0;
    ball.vy = 0;
    ball.active = 0;
}

void launch_ball() {
    if (ball.active) return;
    // launch with upward vector; slight random horizontal bias
    float angle = ((rand() % 40) - 20) * (3.1415926f / 180.0f); // -20..20 deg
    ball.vx = BALL_SPEED * SDL_cosf(angle);
    ball.vy = -BALL_SPEED * SDL_sinf(1.5707963f - angle); // mostly upward
    // simpler: set vy negative and vx random small:
    if (ball.vx == 0) ball.vx = 60.0f;
    ball.vy = -BALL_SPEED;
    ball.active = 1;
}

void reset_game() {
    lives = 3;
    init_bricks();
    reset_paddle_and_ball();
}

int rect_intersect(const SDL_Rect *a, const SDL_Rect *b) {
    return !(a->x + a->w <= b->x || b->x + b->w <= a->x || a->y + a->h <= b->y || b->y + b->h <= a->y);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Breakout (SDL2)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    reset_game();

    int running = 1;
    Uint32 last_ticks = SDL_GetTicks();
    const Uint32 FRAME_DELAY = 16; // ~60fps

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        float dt = (frame_start - last_ticks) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f; // clamp for large hitches
        last_ticks = frame_start;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
                else if (ev.key.keysym.sym == SDLK_SPACE) {
                    launch_ball();
                } else if (ev.key.keysym.sym == SDLK_r) {
                    reset_game();
                }
            }
        }

        // input state for paddle
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        float move = 0.0f;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) move -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) move += 1.0f;
        paddle.rect.x += (int)(move * PADDLE_SPEED * dt);
        // clamp
        if (paddle.rect.x < 0) paddle.rect.x = 0;
        if (paddle.rect.x + paddle.rect.w > WINDOW_W) paddle.rect.x = WINDOW_W - paddle.rect.w;

        // if ball not active, follow paddle
        if (!ball.active) {
            ball.rect.x = paddle.rect.x + (paddle.rect.w - ball.rect.w) / 2;
            ball.rect.y = paddle.rect.y - ball.rect.h - 2;
        } else {
            // move ball
            float nx = ball.rect.x + ball.vx * dt;
            float ny = ball.rect.y + ball.vy * dt;
            SDL_Rect nextRect = { (int)nx, (int)ny, ball.rect.w, ball.rect.h };

            // wall collisions
            if (nextRect.x <= 0) {
                ball.vx = fabsf(ball.vx);
                nx = 0;
            } else if (nextRect.x + nextRect.w >= WINDOW_W) {
                ball.vx = -fabsf(ball.vx);
                nx = WINDOW_W - nextRect.w;
            }
            if (nextRect.y <= 0) {
                ball.vy = fabsf(ball.vy);
                ny = 0;
            }

            // paddle collision
            SDL_Rect ballRect = { (int)nx, (int)ny, ball.rect.w, ball.rect.h };
            if (rect_intersect(&ballRect, &paddle.rect) && ball.vy > 0) {
                // reflect and change angle based on hit position
                float hitPos = ( (ballRect.x + ballRect.w/2.0f) - (paddle.rect.x + paddle.rect.w/2.0f) ) / (paddle.rect.w/2.0f);
                // hitPos in [-1,1]
                float angle = hitPos * (75.0f * 3.1415926f / 180.0f); // -75..75 degrees
                float speed = SDL_sqrtf(ball.vx*ball.vx + ball.vy*ball.vy);
                ball.vx = speed * SDL_sinf(angle);
                ball.vy = -fabsf(speed * SDL_cosf(angle)); // go upwards
                ny = paddle.rect.y - ballRect.h - 1;
            }

            // brick collisions - check all bricks
            for (int i = 0; i < BRICK_ROWS * BRICK_COLS; ++i) {
                Brick *b = &bricks[i];
                if (!b->alive) continue;
                if (rect_intersect(&ballRect, &b->rect)) {
                    // simple: invert y velocity and kill brick
                    b->alive = 0;
                    bricks_left--;
                    score += 10;
                    ball.vy = -ball.vy;
                    // adjust position to avoid sticking
                    if (ball.vy > 0) { // moving down after invert
                        ny = b->rect.y + b->rect.h + 1;
                    } else {
                        ny = b->rect.y - ballRect.h - 1;
                    }
                    break; // handle one brick per frame
                }
            }

            // apply new pos
            ball.rect.x = (int)nx;
            ball.rect.y = (int)ny;

            // bottom - lose life
            if (ball.rect.y > WINDOW_H) {
                lives--;
                ball.active = 0;
                reset_paddle_and_ball();
                if (lives <= 0) {
                    // game over
                    char title[128];
                    snprintf(title, sizeof(title), "Game Over! Score: %d - Press R to restart, ESC to quit", score);
                    SDL_SetWindowTitle(window, title);
                }
            }
        } // end ball active

        // win?
        if (bricks_left <= 0) {
            char title[128];
            snprintf(title, sizeof(title), "You Win! Score: %d - Press R to restart, ESC to quit", score);
            SDL_SetWindowTitle(window, title);
            ball.active = 0;
        } else {
            // update window title with score/lives
            char title[64];
            snprintf(title, sizeof(title), "Breakout - Score: %d  Lives: %d  Bricks: %d", score, lives, bricks_left);
            SDL_SetWindowTitle(window, title);
        }

        // render
        SDL_SetRenderDrawColor(ren, 20, 20, 30, 255);
        SDL_RenderClear(ren);

        // draw bricks
        for (int i = 0; i < BRICK_ROWS * BRICK_COLS; ++i) {
            Brick *b = &bricks[i];
            if (!b->alive) continue;
            SDL_SetRenderDrawColor(ren, b->color.r, b->color.g, b->color.b, 255);
            SDL_RenderFillRect(ren, &b->rect);
            // brick border
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
            SDL_RenderDrawRect(ren, &b->rect);
        }

        // draw paddle
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderFillRect(ren, &paddle.rect);

        // draw ball
        SDL_SetRenderDrawColor(ren, 255, 255, 120, 255);
        SDL_RenderFillRect(ren, &ball.rect);

        // present
        SDL_RenderPresent(ren);

        // frame cap
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (FRAME_DELAY > frame_time) SDL_Delay(FRAME_DELAY - frame_time);
    } // main loop

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
