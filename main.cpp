

// main.cpp - Gold Miner with welcome -> goal screen -> gameplay + hook logic
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// #include <SDL3/SDL_ttf.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846 // Hằng số Pi để tính góc quay hook
#endif

const int SCREEN_WIDTH = 600;
const int SCREEN_HEIGHT = 450;

// Cửa sổ và renderer chính
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

// Hàm load ảnh thành texture
SDL_Texture *loadTexture(const char *path) {
  SDL_Surface *surface = IMG_Load(path);
  if (!surface) {
    std::cerr << "IMG_Load failed: " << SDL_GetError() << "\n";
    return nullptr;
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_DestroySurface(surface);
  return texture;
}

// Cấu trúc vật thể game
struct GameObject {
  SDL_Texture *texture = nullptr;
  SDL_FRect rect{};
  bool visible = true;
};

// Trạng thái game
enum class GameState { Welcome, Ready, Playing, GameOver };

GameState gameState = GameState::Welcome; // Trạng thái khởi đầu

// Các đối tượng game chính
std::vector<GameObject> golds;
std::vector<GameObject> stones; // Danh sách các viên đá
GameObject background, welcomeBtn, goalImg, playBackground, winImg, loseImg,
    hook;

// Biến trạng thái móc và game
float angle = 0.0f, angleSpeed = 1.0f;
bool fang = false, shou = false, timeUp = false;
float timeLeft = 180.0f;
int money = 0;

// Biến điều khiển chuyển động của móc
float hookLength = 0.0f;
float hookMaxLength = 350.0f;
float hookSpeed = 4.0f;
GameObject *hookedGold = nullptr; // Cục vàng đang dính móc

// Kiểm tra chuột có trong vùng hình chữ nhật không
bool pointInRect(const SDL_FPoint *pt, const SDL_FRect *rect) {
  return pt->x >= rect->x && pt->x <= (rect->x + rect->w) && pt->y >= rect->y &&
         pt->y <= (rect->y + rect->h);
}

// Kiểm tra va chạm giữa 2 vật thể
bool checkCollision(const SDL_FRect &a, const SDL_FRect &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

// Khởi tạo SDL và tạo cửa sổ
bool init() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    return false;
  }
  window = SDL_CreateWindow("Gold Miner", SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, nullptr);
  SDL_SetRenderLogicalPresentation(renderer, SCREEN_WIDTH, SCREEN_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  return true;
}

// Load toàn bộ asset hình ảnh vào game
void loadAssets() {
  background = {loadTexture("image/welcome.png"),
                {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  welcomeBtn = {loadTexture("image/welcomego.png"), {60, 110, 120, 60}};
  goalImg = {loadTexture("image/goal.png"),
             {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  playBackground = {loadTexture("image/game.png"),
                    {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  hook = {loadTexture("image/hook.png"), {291, 63, 40, 40}};
  winImg = {loadTexture("image/win.png"), {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  loseImg = {loadTexture("image/lose.png"),
             {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};

  // Khởi tạo random
  srand((unsigned int)time(0));
  std::vector<SDL_FRect> allRects;

  // Hàm sinh vị trí ngẫu nhiên không trùng
  auto randomRect = [&](float w, float h) {
    SDL_FRect rect;
    int tries = 0;
    do {
      rect.x = 50 + rand() % (SCREEN_WIDTH - (int)w - 100);
      rect.y = 120 + rand() % (SCREEN_HEIGHT - (int)h - 150);
      rect.w = w;
      rect.h = h;
      tries++;
      if (tries > 100)
        break; // Tránh vòng lặp vô hạn
    } while (
        std::any_of(allRects.begin(), allRects.end(), [&](const SDL_FRect &r) {
          return checkCollision(rect, r);
        }));
    allRects.push_back(rect);
    return rect;
  };

  // Thêm nhiều vàng và đá hơn
  golds.clear();
  stones.clear();

  // Thêm 5 cục vàng nhỏ
  for (int i = 0; i < 5; ++i) {
    golds.push_back(
        {loadTexture("image/goldsmall.png"), randomRect(19.0f, 18.0f)});
  }

  // Thêm 3 cục vàng vừa
  for (int i = 0; i < 3; ++i) {
    golds.push_back(
        {loadTexture("image/goldmid.png"), randomRect(33.0f, 30.0f)});
  }

  // Thêm 2 cục vàng to
  for (int i = 0; i < 2; ++i) {
    golds.push_back(
        {loadTexture("image/goldbig.png"), randomRect(77.0f, 73.0f)});
  }

  // Thêm 6 viên đá loại A
  for (int i = 0; i < 6; ++i) {
    stones.push_back(
        {loadTexture("image/stonea.png"), randomRect(30.0f, 28.0f)});
  }

  // Thêm 4 viên đá loại B
  for (int i = 0; i < 4; ++i) {
    stones.push_back(
        {loadTexture("image/stoneb.png"), randomRect(47.0f, 43.0f)});
  }
}

// Cập nhật logic game mỗi frame
void updateGame(float deltaTime) {
  if (gameState != GameState::Playing || timeUp)
    return;

  timeLeft -= deltaTime;
  if (timeLeft <= 0.0f) {
    timeLeft = 0.0f;
    timeUp = true;
    gameState = GameState::GameOver;
  }

  if (!fang) {
    // Móc chưa bắn: quay trái phải
    angle += angleSpeed;
    if (angle > 75.0f || angle < -75.0f)
      angleSpeed = -angleSpeed;
  } else {
    // Tính hướng đi của móc theo góc xoay
    float rad = (angle + 90.0f) * M_PI / 180.0f;
    float dx = cos(rad);
    float dy = sin(rad);

    // Tính hookMaxLength DỰA THEO GÓC, chỉ khi vừa mới bắn
    if (hookLength == 0.0f) {
      float t = (SCREEN_HEIGHT - 63.0f) / dy;

      float endX = 291.0f + dx * t;
      float endY = 63.0f + dy * t;

      // Nếu vượt mép ngang màn hình, điều chỉnh lại t
      if (endX < 0.0f || endX > SCREEN_WIDTH) {
        if (endX < 0.0f)
          t = (0.0f - 291.0f) / dx;
        else
          t = (SCREEN_WIDTH - 291.0f) / dx;

        endX = 291.0f + dx * t;
        endY = 63.0f + dy * t;
      }

      float diffX = endX - 291.0f;
      float diffY = endY - 63.0f;
      hookMaxLength = sqrtf(diffX * diffX + diffY * diffY);
    }

    // Nếu đang đi xuống và chưa va chạm
    if (!shou) {
      hook.rect.x += dx * hookSpeed;
      hook.rect.y += dy * hookSpeed;

      // Tính chiều dài dây thật sự bằng khoảng cách Euclid
      SDL_FPoint tip = {hook.rect.x + hook.rect.w / 2.0f, hook.rect.y};
      SDL_FPoint start = {291.0f, 63.0f};
      float dxHook = tip.x - start.x;
      float dyHook = tip.y - start.y;
      hookLength = sqrtf(dxHook * dxHook + dyHook * dyHook);

      // Kiểm tra va chạm với vàng
      for (auto &g : golds) {
        if (g.visible && checkCollision(hook.rect, g.rect)) {
          shou = true;
          hookedGold = &g;

          // Giảm tốc kéo nếu vàng nhỏ
          hookSpeed = (g.rect.w < 30.0f) ? 2.0f : 1.5f;
          break;
        }
      }

      // Nếu chưa móc vàng thì thử đá
      if (!shou) {
        for (auto &s : stones) {
          if (s.visible && checkCollision(hook.rect, s.rect)) {
            shou = true;
            hookedGold = &s;

            // Đá thì tốc kéo chậm hơn
            hookSpeed = 1.0f;
            break;
          }
        }
      }

      // Nếu móc đi quá dài mà không dính gì
      if (hookLength >= hookMaxLength - 1.0f) {
        shou = true;
        hookedGold = nullptr; // không vớ được gì hết
        hookSpeed = 4.0f;     // reset để kéo về nhanh
      }
    }
    // Nếu đã dính vật rồi → kéo ngược lại
    else {
      hook.rect.x -= dx * hookSpeed;
      hook.rect.y -= dy * hookSpeed;
      hookLength -= hookSpeed;

      if (hookLength <= 0.0f) {
        fang = false;
        shou = false;
        hookLength = 0.0f;
        hook.rect.x = 270;
        hook.rect.y = 63;

        if (hookedGold) {
          hookedGold->visible = false;
          bool isGold = false;
          // Chỉ cộng điểm nếu là vàng
          for (auto &g : golds) {
            if (&g == hookedGold) {
              money += 100;
              isGold = true;
              break;
            }
          }
          if (!isGold) {
            money += 30; // Đá cộng ít hơn
          }
          hookedGold = nullptr;
          hookSpeed = 4.0f; // reset tốc độ móc lại ban đầu
        }
      }
    }
  }
}

// Vẽ toàn bộ giao diện game lên màn hình
void renderGame() {
  SDL_RenderClear(renderer);

  if (gameState == GameState::Welcome) {
    SDL_RenderTexture(renderer, background.texture, nullptr, &background.rect);
    SDL_RenderTexture(renderer, welcomeBtn.texture, nullptr, &welcomeBtn.rect);
  } else if (gameState == GameState::Ready) {
    SDL_RenderTexture(renderer, goalImg.texture, nullptr, &goalImg.rect);
  } else if (gameState == GameState::Playing) {
    // 1. Vẽ background của màn chơi
    SDL_RenderTexture(renderer, playBackground.texture, nullptr,
                      &playBackground.rect);

    // 2. Vẽ các cục vàng còn hiện
    for (auto &g : golds)
      if (g.visible)
        SDL_RenderTexture(renderer, g.texture, nullptr, &g.rect);
    for (auto &s : stones)
      if (s.visible)
        SDL_RenderTexture(renderer, s.texture, nullptr, &s.rect);

    // 3. Tính điểm pivot (gốc xoay) là đỉnh móc
    SDL_FPoint pivot = {hook.rect.w / 2.0f, 0.0f};

    // 4. Tính điểm đầu móc (nơi dây sẽ nối vào)
    float tipX = hook.rect.x + pivot.x;
    float tipY = hook.rect.y + pivot.y;

    // 5. Vẽ dây móc từ tời đến đỉnh móc
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Dây màu đen
    if (!fang && hookLength <= 0.0f) {
      SDL_RenderLine(renderer, 291, 63, 291, 63); // Không kéo thì dây gọn
    } else {
      SDL_RenderLine(renderer, 291, 63, tipX, tipY);
    }

    // 6. Vẽ móc xoay quanh đỉnh
    SDL_RenderTextureRotated(renderer, hook.texture, nullptr, &hook.rect, angle,
                             &pivot, SDL_FLIP_NONE);
  }

  if (gameState == GameState::GameOver) {
    SDL_RenderTexture(renderer,
                      (money >= 650 ? winImg.texture : loseImg.texture),
                      nullptr, &(money >= 650 ? winImg.rect : loseImg.rect));
  }

  SDL_RenderPresent(renderer);
}

// Giải phóng bộ nhớ khi kết thúc
void cleanup() {
  for (auto &g : golds)
    SDL_DestroyTexture(g.texture);
  SDL_DestroyTexture(background.texture);
  SDL_DestroyTexture(welcomeBtn.texture);
  SDL_DestroyTexture(goalImg.texture);
  SDL_DestroyTexture(playBackground.texture);
  SDL_DestroyTexture(hook.texture);
  SDL_DestroyTexture(winImg.texture);
  SDL_DestroyTexture(loseImg.texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

// Hàm main - vòng lặp game chính
int main(int argc, char *argv[]) {
  if (!init())
    return -1;
  loadAssets();

  Uint64 lastTick = SDL_GetTicks();
  bool running = true;
  SDL_Event e;

  while (running) {
    Uint64 current = SDL_GetTicks();
    float deltaTime = (current - lastTick) / 1000.0f;
    lastTick = current;

    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        running = false;

      if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { // thay đổi làm hết lỗi nút bắt đầu0000000000000
        // Chuyển đổi tọa độ chuột sang hệ logic
        float logicX = 0, logicY = 0;
        SDL_RenderCoordinatesFromWindow(renderer, e.button.x, e.button.y, &logicX, &logicY);

        SDL_FPoint mp = {logicX, logicY};

        if (gameState == GameState::Welcome &&
            pointInRect(&mp, &welcomeBtn.rect))
          gameState = GameState::Ready;
        else if (gameState == GameState::Ready)
          gameState = GameState::Playing;
        else if (gameState == GameState::Playing && !fang)
          fang = true; // Bắt đầu móc
      }
    }

    updateGame(deltaTime);
    renderGame();
    SDL_Delay(16);
  }

  cleanup();
  return 0;
}
