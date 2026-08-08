// main.c -- Bullet Hell mini demo (C, SDL2) with quadtree & object pooling
// Optional audio: compile with -DUSE_MIXER and link -lSDL2_mixer
//
// Compile examples:
//  (no audio)
//  gcc main.c -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -o bullethell.exe
//
//  (with audio)
//  gcc main.c -DUSE_MIXER -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_mixer -o bullethell.exe

#include <SDL.h>
#ifdef USE_MIXER
#include <SDL_mixer.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_W 800
#define WINDOW_H 600

// bullet settings
#define MAX_BULLETS 2000
#define BULLET_RADIUS 3
#define BULLET_SPEED 180.0f

// particle settings
#define MAX_PARTICLES 400
#define PARTICLE_LIFE 0.6f

// quadtree settings
#define QT_CAPACITY 8
#define QT_MAX_LEVEL 6

typedef struct {
    float x, y;
    float vx, vy;
    int alive;
    float life; // optional life (unused now)
    SDL_Color color;
} Bullet;

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    int alive;
} Particle;

typedef struct Quadtree {
    SDL_Rect bounds;
    int level;
    Bullet* items[QT_CAPACITY];
    int count;
    struct Quadtree* children[4];
} Quadtree;

typedef struct {
    float x, y;
    float w, h;
} AABB;

// global pools
static Bullet bullets[MAX_BULLETS];
static Particle particles[MAX_PARTICLES];

// player
typedef struct {
    float x, y;
    float w, h;
    int alive;
} Player;

static Player player;

// timing
static Uint32 last_ticks = 0;

// audio chunks (optional)
#ifdef USE_MIXER
static Mix_Chunk *sfx_shot = NULL;
static Mix_Chunk *sfx_hit = NULL;
#endif

// ---------------------- helper funcs ----------------------
static float clampf(float v, float a, float b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

static int aabb_intersect(const AABB *a, const AABB *b) {
    return !(a->x + a->w < b->x || b->x + b->w < a->x || a->y + a->h < b->y || b->y + b->h < a->y);
}

static int circle_aabb_collision(float cx, float cy, float r, const AABB *b) {
    // clamp circle center to AABB, compute distance
    float closestX = clampf(cx, b->x, b->x + b->w);
    float closestY = clampf(cy, b->y, b->y + b->h);
    float dx = cx - closestX;
    float dy = cy - closestY;
    return (dx*dx + dy*dy) <= r*r;
}

// ---------------------- object pool ----------------------
static void bullets_init() {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        bullets[i].alive = 0;
    }
}

static Bullet* bullet_spawn(float x, float y, float vx, float vy, SDL_Color color) {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].alive) {
            bullets[i].alive = 1;
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = vx;
            bullets[i].vy = vy;
            bullets[i].life = 5.0f;
            bullets[i].color = color;
            return &bullets[i];
        }
    }
    return NULL; // pool full
}

static void particles_init() {
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].alive = 0;
}

static void particle_spawn(float x, float y, float vx, float vy, float life) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].alive) {
            particles[i].alive = 1;
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].life = life;
            return;
        }
    }
}

// ---------------------- quadtree ----------------------
// --- 安全な qt_create_node (malloc チェック・ items 初期化) ---
static Quadtree* qt_create_node(int level, SDL_Rect bounds) {
    Quadtree* q = (Quadtree*)malloc(sizeof(Quadtree));
    if (!q) {
        fprintf(stderr, "qt_create_node: malloc failed\n");
        return NULL;
    }
    q->bounds = bounds;
    q->level = level;
    q->count = 0;
    for (int i = 0; i < 4; ++i) q->children[i] = NULL;
    for (int i = 0; i < QT_CAPACITY; ++i) q->items[i] = NULL;
    return q;
}

// --- 安全な再帰解放（子を再帰で解放） ---
static void qt_free(Quadtree* q) {
    if (!q) return;
    for (int i = 0; i < 4; ++i) {
        if (q->children[i]) {
            qt_free(q->children[i]);
            // qt_free 自体 frees child, so just null the pointer
            q->children[i] = NULL;
        }
    }
    free(q);
}

// --- クリア（子は保持） ---
static void qt_clear(Quadtree* q) {
    if (!q) return;
    q->count = 0;
    for (int i = 0; i < QT_CAPACITY; ++i) q->items[i] = NULL;
    for (int i = 0; i < 4; ++i) {
        if (q->children[i]) qt_clear(q->children[i]);
    }
}

// --- subdivide: 子作成時に malloc チェック ---
static void qt_subdivide(Quadtree* q) {
    if (!q) return;
    int x = q->bounds.x, y = q->bounds.y;
    int w = q->bounds.w / 2, h = q->bounds.h / 2;
    int lvl = q->level + 1;
    q->children[0] = qt_create_node(lvl, (SDL_Rect){x,   y,   w, h});
    q->children[1] = qt_create_node(lvl, (SDL_Rect){x+w, y,   w, h});
    q->children[2] = qt_create_node(lvl, (SDL_Rect){x,   y+h, w, h});
    q->children[3] = qt_create_node(lvl, (SDL_Rect){x+w, y+h, w, h});
    // if any child is NULL (malloc failed), free those already created
    for (int i = 0; i < 4; ++i) {
        if (!q->children[i]) {
            for (int j = 0; j < 4; ++j) {
                if (q->children[j]) { qt_free(q->children[j]); q->children[j] = NULL; }
            }
            break;
        }
    }
}

// --- index 決定はそのまま （NULLガード追加） ---
static int qt_get_index(Quadtree* q, Bullet* b) {
    if (!q || !b) return -1;
    int midX = q->bounds.x + q->bounds.w / 2;
    int midY = q->bounds.y + q->bounds.h / 2;
    int left = (b->x + BULLET_RADIUS*0.5f) < midX;
    int right = (b->x - BULLET_RADIUS*0.5f) >= midX;
    int top = (b->y + BULLET_RADIUS*0.5f) < midY;
    int bottom = (b->y - BULLET_RADIUS*0.5f) >= midY;

    if (top) {
        if (left) return 0;
        if (right) return 1;
    } else if (bottom) {
        if (left) return 2;
        if (right) return 3;
    }
    return -1;
}

// --- 重要: 安全な qt_insert（配列境界を越えない） ---
static void qt_insert(Quadtree* q, Bullet* b) {
    if (!q || !b) return;

    // 既に分割済みなら子に入れられるか試す
    if (q->children[0] != NULL) {
        int idx = qt_get_index(q, b);
        if (idx != -1) {
            qt_insert(q->children[idx], b);
            return;
        }
    }

    // このノードの空きがあれば格納
    if (q->count < QT_CAPACITY) {
        q->items[q->count++] = b;
        return;
    }

    // 空きが無ければ分割（可能なら）して再度挿入を試みる
    if (q->children[0] == NULL && q->level < QT_MAX_LEVEL) {
        qt_subdivide(q);
    }
    if (q->children[0] != NULL) {
        int idx = qt_get_index(q, b);
        if (idx != -1) {
            qt_insert(q->children[idx], b);
            return;
        }
    }

    // フォールバック：このノードに格納するが、配列境界は越えない
    // （すでに満杯なら挿入を諦める）
    if (q->count < QT_CAPACITY) {
        q->items[q->count++] = b;
    } else {
        // ここに来るのは稀。必要なら別の戦略（動的配列等）を検討。
    }
}

static void qt_retrieve_list(Quadtree* q, AABB *area, Bullet** out_list, int *out_count, int max_out) {
    if (!q || *out_count >= max_out) return;
    AABB nodeAABB = { (float)q->bounds.x, (float)q->bounds.y, (float)q->bounds.w, (float)q->bounds.h };
    if (!aabb_intersect(&nodeAABB, area)) return;
    // add items from this node
    for (int i = 0; i < q->count && *out_count < max_out; ++i) {
        Bullet* b = q->items[i];
        if (b && b->alive) out_list[(*out_count)++] = b;
    }
    // recurse
    for (int i = 0; i < 4; ++i) {
        if (q->children[i]) qt_retrieve_list(q->children[i], area, out_list, out_count, max_out);
    }
}

// ---------------------- game systems ----------------------
static Quadtree* global_qt = NULL;

static void spawn_spiral(float cx, float cy, int num, float speed, float phase_offset) {
    // spawn bullets in spiral with phase
    float t = SDL_GetTicks() / 1000.0f;
    for (int i = 0; i < num; ++i) {
        float ang = (i * (2.0f * M_PI / num)) + phase_offset + t * 2.0f; // rotating spiral
        float vx = cosf(ang) * speed;
        float vy = sinf(ang) * speed;
        SDL_Color col = {255, 200, 60, 255};
        bullet_spawn(cx, cy, vx, vy, col);
    }
}

static void spawn_fan(float cx, float cy, int n, float spread_deg, float base_angle_deg, float speed) {
    float base = base_angle_deg * (M_PI/180.0f);
    float spread = spread_deg * (M_PI/180.0f);
    for (int i = 0; i < n; ++i) {
        float a = base - spread/2.0f + (spread * i) / (n>1?(n-1):1);
        bullet_spawn(cx, cy, cosf(a)*speed, sinf(a)*speed, (SDL_Color){180, 80, 200, 255});
    }
}

static void spawn_enemy_patterns(float dt) {
    // Example spawner: top-center enemy shoots spirals and fans periodically
    static float timer_spiral = 0.0f;
    static float timer_fan = 0.0f;
    timer_spiral += dt;
    timer_fan += dt;
    float cx = WINDOW_W * 0.5f;
    float cy = 120.0f;
    if (timer_spiral > 0.35f) {
        spawn_spiral(cx, cy, 22, 140.0f, (float)rand() / RAND_MAX * 6.28f);
#ifdef USE_MIXER
        if (sfx_shot) Mix_PlayChannel(-1, sfx_shot, 0);
#endif
        timer_spiral = 0.0f;
    }
    if (timer_fan > 1.1f) {
        // rotating fan
        float t = SDL_GetTicks() / 1000.0f;
        spawn_fan(cx - 120.0f, cy + 40.0f, 16, 60.0f, t*60.0f, 180.0f);
        spawn_fan(cx + 120.0f, cy + 40.0f, 16, 60.0f, -t*60.0f, 180.0f);
#ifdef USE_MIXER
        if (sfx_shot) Mix_PlayChannel(-1, sfx_shot, 0);
#endif
        timer_fan = 0.0f;
    }
}

// ---------------------- update & render ----------------------
static void reset_game_state() {
    // reset bullets and particles and player
    for (int i = 0; i < MAX_BULLETS; ++i) bullets[i].alive = 0;
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].alive = 0;
    player.w = 20; player.h = 20;
    player.x = WINDOW_W*0.5f - player.w*0.5f;
    player.y = WINDOW_H - 80;
    player.alive = 1;
}

static void update(float dt) {
    // clear & rebuild quadtree each frame
    if (!global_qt) {
        SDL_Rect root = {0,0,WINDOW_W,WINDOW_H};
        global_qt = qt_create_node(0, root);
    }
    qt_clear(global_qt);

    // spawn patterns
    spawn_enemy_patterns(dt);

    // insert bullets into quadtree and update positions
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].alive) continue;
        bullets[i].x += bullets[i].vx * dt;
        bullets[i].y += bullets[i].vy * dt;
        bullets[i].life -= dt;
        if (bullets[i].x < -20 || bullets[i].x > WINDOW_W + 20 || bullets[i].y < -20 || bullets[i].y > WINDOW_H + 20 || bullets[i].life <= 0.0f) {
            bullets[i].alive = 0;
            continue;
        }
        // insert into quadtree
        qt_insert(global_qt, &bullets[i]);
    }

    // update particles
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].alive) continue;
        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].life -= dt;
        if (particles[i].life <= 0.0f) particles[i].alive = 0;
    }

    // player movement
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    float speed = 300.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) speed = 120.0f; // slow for precision
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) player.x -= speed * dt;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) player.x += speed * dt;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) player.y -= speed * dt;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) player.y += speed * dt;
    // clamp
    if (player.x < 0) player.x = 0;
    if (player.x + player.w > WINDOW_W) player.x = WINDOW_W - player.w;
    if (player.y < 0) player.y = 0;
    if (player.y + player.h > WINDOW_H) player.y = WINDOW_H - player.h;

    // collision: query quadtree with player's AABB
    AABB playerAABB = { player.x, player.y, player.w, player.h };
    Bullet* candidates[256];
    int cand_count = 0;
    qt_retrieve_list(global_qt, &playerAABB, candidates, &cand_count, 256);
    for (int i = 0; i < cand_count; ++i) {
        Bullet* b = candidates[i];
        if (!b || !b->alive) continue;
        if (circle_aabb_collision(b->x, b->y, BULLET_RADIUS, &playerAABB)) {
            // hit!
            b->alive = 0;
            // spawn particles
            for (int p = 0; p < 12; ++p) {
                float ang = ((float)p / 12.0f) * 2.0f * M_PI;
                float sp = 60.0f + (rand()%80);
                particle_spawn(b->x, b->y, cosf(ang)*sp, sinf(ang)*sp, PARTICLE_LIFE*(0.6f + (rand()%40)/100.0f));
            }
#ifdef USE_MIXER
            if (sfx_hit) Mix_PlayChannel(-1, sfx_hit, 0);
#endif
            // game over: simply reset
            reset_game_state();
            break;
        }
    }
}

static void render_fill_circle(SDL_Renderer* ren, int cx, int cy, int r) {
    // naive circle fill (sufficient for tiny bullets)
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r*r - dy*dy));
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void render(SDL_Renderer* ren) {
    // clear
    SDL_SetRenderDrawColor(ren, 10, 10, 20, 255);
    SDL_RenderClear(ren);

    // draw particles
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].alive) continue;
        float t = particles[i].life / PARTICLE_LIFE;
        Uint8 alpha = (Uint8)(clampf(t,0,1) * 255.0f);
        SDL_SetRenderDrawColor(ren, 255, 200, 80, alpha);
        SDL_Rect r = { (int)particles[i].x - 2, (int)particles[i].y - 2, 4, 4 };
        SDL_RenderFillRect(ren, &r);
    }

    // draw bullets (use quad tree traversal optionally; here we iterate pool)
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].alive) continue;
        SDL_SetRenderDrawColor(ren, bullets[i].color.r, bullets[i].color.g, bullets[i].color.b, 255);
        // small filled circle
        render_fill_circle(ren, (int)bullets[i].x, (int)bullets[i].y, BULLET_RADIUS);
    }

    // draw player
    SDL_SetRenderDrawColor(ren, 180, 230, 255, 255);
    SDL_Rect pr = { (int)player.x, (int)player.y, (int)player.w, (int)player.h };
    SDL_RenderFillRect(ren, &pr);

    // HUD
    SDL_SetRenderDrawColor(ren, 255,255,255,255);
    // (no TTF here — textless HUD)

    SDL_RenderPresent(ren);
}

// ---------------------- audio init ----------------------
#ifdef USE_MIXER
static int audio_init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio error: %s", Mix_GetError());
        return 0;
    }
    // load sfx files if present
    sfx_shot = Mix_LoadWAV("shot.wav");
    sfx_hit  = Mix_LoadWAV("hit.wav");
    // missing files are OK; check for NULL before playing
    return 1;
}
#endif

// ---------------------- main ----------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

#ifdef USE_MIXER
    if (!audio_init()) {
        SDL_Log("Audio init failed (continuing without audio).");
    }
#endif

    SDL_Window* win = SDL_CreateWindow("Bullet Hell (mini) - SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    bullets_init();
    particles_init();
    reset_game_state();

    last_ticks = SDL_GetTicks();
    int running = 1;
    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_ticks = now;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
                else if (ev.key.keysym.sym == SDLK_r) reset_game_state();
            }
        }

        update(dt);
        render(ren);

        // small delay to cap CPU usage (we don't use vsync here)
        SDL_Delay(1);
    }

#ifdef USE_MIXER
    if (sfx_shot) { Mix_FreeChunk(sfx_shot); sfx_shot = NULL; }
    if (sfx_hit ) { Mix_FreeChunk(sfx_hit); sfx_hit  = NULL; }
    Mix_CloseAudio();
#endif

    if (global_qt) {
        qt_free(global_qt);
        global_qt = NULL;
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
