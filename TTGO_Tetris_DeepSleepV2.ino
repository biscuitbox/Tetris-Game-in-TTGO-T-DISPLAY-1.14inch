//========================================================================
// TETRIS with TTGO T-Display
// - 부팅 시 로고 화면: 버튼 누를 때까지 대기 (1분 무입력 → 딥슬립)
// - 첫 화면(타이틀 화면)도 버튼 누를 때까지 대기 (1분 무입력 → 딥슬립)
// - 게임 화면에서는 첫 좌/우 입력 전에는 블록이 낙하하지 않음
//========================================================================

#include <SPI.h>
#include <TFT_eSPI.h>
#include "tet.h"
#include "esp_sleep.h"        // 딥슬립용

TFT_eSPI tft = TFT_eSPI(); 

uint16_t BlockImage[8][12][12];                            // Block
uint16_t backBuffer[220][110];                             // GAME AREA
const int Length = 11;     // the number of pixels for a side of a block
const int Width  = 10;     // the number of horizontal blocks
const int Height = 20;     // the number of vertical blocks
int screen[Width][Height] = {0}; //it shows color-numbers of all positions

struct Point {int X, Y;};
struct Block {Point square[4][4]; int numRotate, color;};
Point pos; Block block;
int rot, fall_cnt = 0;
bool started = false;              // ★ 낙하 시작 여부 (첫 좌/우 입력 후 true)
bool gameover = false;
boolean but_A = false, but_LEFT = false, but_RIGHT = false;
int game_speed = 20; // 25msec

Block blocks[7] = {
  {{{{-1,0},{0,0},{1,0},{2,0}},{{0,-1},{0,0},{0,1},{0,2}},
  {{0,0},{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0},{0,0}}},2,1},
  {{{{0,-1},{1,-1},{0,0},{1,0}},{{0,0},{0,0},{0,0},{0,0}},
  {{0,0},{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0},{0,0}}},1,2},
  {{{{-1,-1},{-1,0},{0,0},{1,0}},{{-1,1},{0,1},{0,0},{0,-1}},
  {{-1,0},{0,0},{1,0},{1,1}},{{1,-1},{0,-1},{0,0},{0,1}}},4,3},
  {{{{-1,0},{0,0},{0,1},{1,1}},{{0,-1},{0,0},{-1,0},{-1,1}},
  {{0,0},{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0},{0,0}}},2,4},
  {{{{-1,0},{0,0},{1,0},{1,-1}},{{-1,-1},{0,-1},{0,0},{0,1}},
  {{-1,1},{-1,0},{0,0},{1,0}},{{0,-1},{0,0},{0,1},{1,1}}},4,5},
  {{{{-1,1},{0,1},{0,0},{1,0}},{{0,-1},{0,0},{1,0},{1,1}},
  {{0,0},{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0},{0,0}}},2,6},
  {{{{-1,0},{0,0},{1,0},{0,-1}},{{0,-1},{0,0},{0,1},{-1,0}},
  {{-1,0},{0,0},{1,0},{0,1}},{{0,-1},{0,0},{0,1},{1,0}}},4,7}
};

extern uint8_t tetris_img[];   // 로고 이미지 배열
#define GREY 0x5AEB

int pom=0;
int pom2=0;
int pom3=0;
int pom4=0;

int score=0;
int lvl=1;

int leftButton=0;   // GPIO0
int rightButton=35; // GPIO35

// ★ 딥슬립 관련
unsigned long lastInputTime = 0;
const unsigned long SLEEP_TIMEOUT = 60000UL; // 1분
const gpio_num_t WAKE_PIN = GPIO_NUM_0;      // GPIO0 버튼으로 깨우기

// 함수 미리 선언
void Draw();
void PutStartPos();
void make_block( int n , uint16_t color );
void GetNextPosRot(Point* pnext_pos, int* pnext_rot);
void ReviseScreen(Point next_pos, int next_rot);
void DeleteLine();
void GameOver();
void ClearKeys();
bool KeyPadLoop();

//========================================================================
// 딥슬립 진입
//========================================================================
void goToDeepSleep() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 100);
  tft.println("Sleep...");

  delay(100);

  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0); // LOW에서 깨움
  esp_deep_sleep_start();
}

//========================================================================
// 버튼 눌릴 때까지 대기 (무입력 SLEEP_TIMEOUT 지나면 딥슬립)
//========================================================================
void waitForButtonWithSleep() {
  lastInputTime = millis();
  while (digitalRead(leftButton) == HIGH && digitalRead(rightButton) == HIGH) {
    if (millis() - lastInputTime > SLEEP_TIMEOUT) {
      goToDeepSleep();
    }
    delay(10);
  }
  // 버튼 눌렸으면
  lastInputTime = millis();
  delay(200); // 디바운스
}

//========================================================================
void setup(void) {
  // 버튼 입력 (풀업)
  pinMode(leftButton, INPUT_PULLUP);
  pinMode(rightButton, INPUT_PULLUP);
  // pinMode(37, INPUT_PULLUP); // M5Stack용 버튼, T-Display엔 없으면 주석

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);

  // ---------------- 1단계: 로고 화면 ----------------
  tft.pushImage(0, 0, 135, 240, tet);  // tet = 로고 이미지
  waitForButtonWithSleep();            // 로고에서 버튼 기다리기 + 딥슬립 타임아웃
  // --------------------------------------------------

  // ---------------- 2단계: 첫 화면(타이틀/설명) ----------------
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(20, 60, 2);
  tft.println("TETRIS");

  tft.setCursor(10, 90, 2);
  tft.println("Press any button");

  tft.setCursor(10, 110, 1);
  tft.println("Left: Move Left");

  tft.setCursor(10, 125, 1);
  tft.println("Right: Move Right");

  tft.setCursor(10, 140, 1);
  tft.println("Both: Rotate");

  waitForButtonWithSleep();            // 첫 화면에서도 버튼 기다리기
  // ----------------------------------------------------

  // ---------------- 3단계: 실제 게임 화면 초기화 ----------------
  tft.fillScreen(TFT_BLACK);  
  tft.drawLine(11,19,122,19,GREY);
  tft.drawLine(11,19,11,240,GREY);
  tft.drawLine(122,19,122,240,GREY);
  
  tft.drawString("SCORE:"+String(score),14,8,1);
  tft.drawString("LVL:"+String(lvl),88,8,1);

  // 블록 그래픽 준비
  make_block( 0, TFT_BLACK);        // Type No, Color
  make_block( 1, 0x00F0);       // DDDD     RED
  make_block( 2, 0xFBE4);       // DD,DD    PUPLE 
  make_block( 3, 0xFF00);       // D__,DDD  BLUE
  make_block( 4, 0xFF87);       // DD_,_DD  GREEN 
  make_block( 5, 0x87FF);       // __D,DDD  YELLO
  make_block( 6, 0xF00F);       // _DD,DD_  LIGHT GREEN
  make_block( 7, 0xF8FC);       // _D_,DDD  PINK

  PutStartPos(); // Start Position
  for (int i = 0; i < 4; ++i)
    screen[pos.X + block.square[rot][i].X][pos.Y + block.square[rot][i].Y] = block.color;
  Draw();        // Draw block

  started = false;   // ★ 첫 좌/우 입력 전까지 낙하 X
}

//========================================================================
void loop() {

  // ★ 1분 동안 입력이 없으면 딥슬립
  if (millis() - lastInputTime > SLEEP_TIMEOUT) {
    goToDeepSleep();
  }

  if (gameover){ 
    if(digitalRead(leftButton)==LOW || digitalRead(rightButton)==LOW)
    {
      lastInputTime = millis();  // 입력 시각 갱신

      for (int j = 0; j < Height; ++j)
        for (int i = 0; i < Width; ++i)
          screen[i][j] = 0;
      gameover=false;
      score=0;
      game_speed=20;
      lvl=1;
      fall_cnt = 0;
      started = false;          // 새 판도 첫 좌/우 입력 전까지 대기

      PutStartPos();                             // Start Position
      for (int i = 0; i < 4; ++i)
        screen[pos.X + block.square[rot][i].X][pos.Y + block.square[rot][i].Y] = block.color;

      tft.drawString("SCORE:"+String(score),14,8,1);
      tft.drawString("LVL:"+String(lvl),88,8,1);
      Draw();  
    }
    return;
  }

  if(!gameover){
    Point next_pos;
    int next_rot = rot;

    GetNextPosRot(&next_pos, &next_rot);

    // 아직 started=false이면, 첫 좌/우 입력만 기다리고 움직이지 않음
    if (!started) {
      // 화면 유지
    } else {
      ReviseScreen(next_pos, next_rot);
    }

    delay(game_speed);    // SPEED ADJUST
  }
}

//========================================================================
void Draw() {                               // Draw 120x240 in the center
  for (int i = 0; i < Width; ++i)
    for (int j = 0; j < Height; ++j)
      for (int k = 0; k < Length; ++k)
        for (int l = 0; l < Length; ++l)
          backBuffer[j * Length + l][i * Length + k] =
            BlockImage[screen[i][j]][k][l];

  tft.pushImage(12, 20, 110, 220, *backBuffer);
}

//========================================================================
void PutStartPos() {
  game_speed=20;
  pos.X = 4; pos.Y = 1;
  block = blocks[random(7)];
  rot = random(block.numRotate);
}

//========================================================================
bool GetSquares(Block block, Point pos, int rot, Point* squares) {
  bool overlap = false;
  for (int i = 0; i < 4; ++i) {
    Point p;
    p.X = pos.X + block.square[rot][i].X;
    p.Y = pos.Y + block.square[rot][i].Y;
    overlap |= p.X < 0 || p.X >= Width || p.Y < 0 || p.Y >= 
      Height || screen[p.X][p.Y] != 0;
    squares[i] = p;
  }
  return !overlap;
}

//========================================================================
void GameOver() {
  for (int i = 0; i < Width; ++i)
    for (int j = 0; j < Height; ++j)
      if (screen[i][j] != 0) screen[i][j] = 4;
  gameover = true;
}

//========================================================================
void ClearKeys() { but_A=false; but_LEFT=false; but_RIGHT=false;}

//========================================================================
bool KeyPadLoop(){
  // 왼쪽 버튼
  if(digitalRead(leftButton)==LOW && digitalRead(rightButton)==HIGH){
    lastInputTime = millis();           // 입력 시각 갱신
    if(pom==0) {
      pom=1;
      ClearKeys();
      but_LEFT =true;
      return true;
    }
  } else { pom=0; }
    
  // 오른쪽 버튼
  if(digitalRead(rightButton)==LOW && digitalRead(leftButton)==HIGH){
    lastInputTime = millis();           // 입력 시각 갱신
    if(pom2==0) {
      pom2=1;
      ClearKeys();
      but_RIGHT=true;
      return true;
    }
  } else { pom2=0; }

  // (선택) 37번 버튼이 실제로 있다면 여기 사용 가능
  /*
  if(digitalRead(37)==LOW){
    lastInputTime = millis();
    if(pom3==0){
      pom3=1; ClearKeys(); but_A = true; return true;
    }
  } else { pom3=0; }
  */

  // 두 버튼 동시에 → 회전
  if(digitalRead(rightButton)==LOW && digitalRead(leftButton)==LOW){
    lastInputTime = millis();           // 입력 시각 갱신
    if(pom4==0) {
      pom4=1;
      ClearKeys();
      but_A =true;
      return true;
    }
  } else { pom4=0; }
  
  return false;
}

//========================================================================
void GetNextPosRot(Point* pnext_pos, int* pnext_rot) {
  bool received = KeyPadLoop();

  // 아직 started=false 상태일 때:
  //  - 첫 왼쪽/오른쪽 입력이 들어오면 started=true 로 전환
  //  - 그 전에는 블록이 움직이지 않음
  if (!started) {
    if (but_LEFT || but_RIGHT) {
      started = true;           // 이제부터 낙하/이동 시작
    } else {
      return;                   // 계속 대기
    }
  }

  // 여기부터는 started == true
  pnext_pos->X = pos.X;
  pnext_pos->Y = pos.Y;

  // 낙하 타이밍
  if ((fall_cnt = (fall_cnt + 1) % 10) == 0) {
    pnext_pos->Y += 1;
  } else {
    if (but_LEFT) {
      but_LEFT = false; pnext_pos->X -= 1;
    }
    else if (but_RIGHT) {
      but_RIGHT = false; pnext_pos->X += 1;
    }
    else if (but_A) {
      but_A = false;
      *pnext_rot = (*pnext_rot + block.numRotate - 1)%block.numRotate; 
    }
  }
}

//========================================================================
void DeleteLine() {
  for (int j = 0; j < Height; ++j) {
    bool Delete = true;
    for (int i = 0; i < Width; ++i)
      if (screen[i][j] == 0) Delete = false;
    if (Delete)
    {
       score++;
       if(score%5==0)
       {
        lvl++;
        game_speed=game_speed-4;
        tft.drawString("LVL:"+String(lvl),88,8,1);
       }
       tft.drawString("SCORE:"+String(score),14,8,1);

       for (int k = j; k >= 1; --k) {
         for (int i = 0; i < Width; ++i) {
           screen[i][k] = screen[i][k - 1];
         }
       }
    }
  }
}

//========================================================================
void ReviseScreen(Point next_pos, int next_rot) {
  if (!started) return;
  Point next_squares[4];
  for (int i = 0; i < 4; ++i)
    screen[pos.X + block.square[rot][i].X][pos.Y + block.square[rot][i].Y] = 0;

  if (GetSquares(block, next_pos, next_rot, next_squares)) {
    for (int i = 0; i < 4; ++i)
      screen[next_squares[i].X][next_squares[i].Y] = block.color;
    pos = next_pos; rot = next_rot;
  }
  else {
    for (int i = 0; i < 4; ++i)
      screen[pos.X + block.square[rot][i].X][pos.Y + block.square[rot][i].Y] = block.color;
    if (next_pos.Y == pos.Y + 1) {
      DeleteLine();
      PutStartPos();
      if (!GetSquares(block, pos, rot, next_squares)) {
        for (int i = 0; i < 4; ++i)
          screen[pos.X + block.square[rot][i].X][pos.Y + block.square[rot][i].Y] = block.color;
        GameOver();
      } else {
        // 새 블록이 나타났을 때는 다시 첫 좌/우 입력을 기다릴지 여부 선택 가능
        // 지금은 계속 started=true 로 둠 (연속 플레이 느낌 유지)
      }
    }
  }
  Draw();
}

//========================================================================
void make_block( int n , uint16_t color ){            // Make Block color       
  for ( int i =0 ; i < 12; i++ )
    for ( int j =0 ; j < 12; j++ ){
      BlockImage[n][i][j] = color;                           // Block color
      if ( i == 0 || j == 0 ) BlockImage[n][i][j] = 0;       // TFT_BLACK Line
    } 
}
//========================================================================
