#include <Wire.h>
#include <U8g2lib.h>
#include <Bounce2.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

// ==================== 정의 ====================

DFRobotDFPlayerMini myDFPlayer;

// 스위치 개수
#define NUM_SWITCHES 8

// LED 설정 (핀 번호 11로 변경하여 스위치 핀과 충돌 방지)
#define LED_PIN     11
#define LED_COUNT   4

// 가변저항 핀
#define POT_PIN     A2


#define COMMON_SWITCH_PIN 12

// 조이스틱 버튼 핀
#define JOYSTICK_BUTTON_PIN 2

// 스위치 핀 배열 (전역으로 선언)
const int switchPins[NUM_SWITCHES] = {3, 4, 5, 6, 7, 8, 9, 10};

// NeoPixel 객체
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// U8g2 디스플레이 객체 (I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Bounce 객체들
Bounce switches[NUM_SWITCHES];
Bounce commonSwitchDebouncer;
Bounce joystickButton;

// 메뉴 항목
const char* menuItems[] = {
    "1. Apartment Game",
    "2. Baskin Robbins 31",
    "3. Up-Down Game",
    "4. GGSB Game",
    "5. Show Ranking",
    "6. Reset Ranking"
};

// 게임 상태
enum GameState {
    MENU,
    APARTMENT_GAME,
    BASKIN_ROBBINS_31_GAME,
    UP_DOWN_GAME,
    GGSB_GAME,
    SHOW_RANKING,
    RESET_RANKING
};
GameState gameState = MENU;

// 플레이어 점수 (4명)
int playerScores[4] = { 0, 0, 0, 0 };

// ==================== GGSB 게임 관련 정의 ====================

// 플레이어 정보 구조체 (score 필드 제거)
struct GGSB_Player {
    int id;
    const char* name;
    uint32_t color;
    int oddSwitchPin;  // 홀수 번호 스위치 (예: 3, 5, 7, 9)
    int evenSwitchPin; // 짝수 번호 스위치 (예: 4, 6, 8, 10)
    bool active;
};

// 플레이어 초기화
GGSB_Player GGSB_players[4] = {
    {1, "PLAYER 1", 0xFF0000, 3, 4, true},  // 빨간색
    {2, "PLAYER 2", 0xFFFF00, 5, 6, true},  // 노란색
    {3, "PLAYER 3", 0x0000FF, 7, 8, true},  // 파란색
    {4, "PLAYER 4", 0x800080, 9, 10, true}  // 보라색
};

// 게임 시간 설정
float GGSB_timeLimit = 2.0;          // 초기 제한 시간 (초)
const float GGSB_timeDecrement = 0.1; // 시간 감소량 (초)
const float GGSB_minTimeLimit = 0.5;  // 최소 제한 시간 (초)

// 활성 플레이어 수
int GGSB_activePlayerCount = 4;

// 랜덤 시드 초기화 여부
bool GGSB_randomSeedInitialized = false;


// ==================== 설정 함수 ====================
void setup() {
    
    // 시리얼 통신 시작 (디버깅용)
    Serial.begin(9600);
    Serial1.begin(9600);
    myDFPlayer.begin(Serial1);

    // 디스플레이 초기화
    u8g2.begin();
    // u8g2.enableUTF8Print(); // 필요 시 활성화

    // NeoPixel 초기화
    leds.begin();
    leds.show(); // 초기에는 모든 LED 끄기

    // 조이스틱 버튼 초기화
    joystickButton.attach(JOYSTICK_BUTTON_PIN, INPUT_PULLUP);
    joystickButton.interval(25); // 디바운스 시간 설정

    // 공용 스위치 초기화
    commonSwitchDebouncer.attach(COMMON_SWITCH_PIN, INPUT_PULLUP);
    commonSwitchDebouncer.interval(25); // 디바운스 시간 설정

   // 스위치 핀 초기화 (이미 전역으로 선언됨)
    for (int i = 0; i < NUM_SWITCHES; i++) {
        pinMode(switchPins[i], INPUT_PULLUP);
    }

    // 각 스위치 초기화
    for (int i = 0; i < NUM_SWITCHES; i++) {
        switches[i].attach(switchPins[i], INPUT_PULLUP);
        switches[i].interval(25); // 디바운스 시간 설정
    }
}

// ==================== 메인 루프 ====================
void loop() {
    if (gameState == MENU) {
        // 메뉴를 실행하고 선택된 메뉴 인덱스를 가져옴
        int selectedMenu = runMenu();

        // 선택된 메뉴에 따라 해당 기능 호출
        switch (selectedMenu) {
        case 0:
            gameState = APARTMENT_GAME;
            break;
        case 1:
            gameState = BASKIN_ROBBINS_31_GAME;
            break;
        case 2:
            gameState = UP_DOWN_GAME;
            break;
        case 3:
            gameState = GGSB_GAME;
            break;
        case 4:
            showRanking();
            break;
        case 5:
            resetRanking();
            break;
        default:
            Serial.println("Invalid Menu Selection!");
            break;
        }
    }
    else if (gameState == APARTMENT_GAME) {
        playApartmentGame();
        gameState = MENU;
    }
    else if (gameState == BASKIN_ROBBINS_31_GAME) {
        playBaskinRobbinsGame();
        gameState = MENU;
    }
    else if (gameState == UP_DOWN_GAME) {
        playUpDownGame();
        gameState = MENU;
    }
    else if (gameState == GGSB_GAME) {
       playGGSBGame();
       gameState = MENU;
    }
    // 다른 게임 상태 추가 가능
}

// ==================== 메뉴 함수 ====================

// 메뉴를 실행하고 선택된 메뉴 인덱스를 반환
int runMenu() {
    int menuIndex = 0;
    const int totalItems = sizeof(menuItems) / sizeof(menuItems[0]);
    bool menuActive = true;

    while (menuActive) {
        displayMenu(menuIndex);

        // 조이스틱 Y축 입력으로 메뉴 탐색 (A1 핀 연결 가정)
        int joystickY = analogRead(A1);
        if (joystickY < 400) {
            menuIndex = (menuIndex > 0) ? menuIndex - 1 : totalItems - 1;
            delay(150); // 디바운스 딜레이
        }
        else if (joystickY > 600) {
            menuIndex = (menuIndex < totalItems - 1) ? menuIndex + 1 : 0;
            delay(150); // 디바운스 딜레이
        }

        // 조이스틱 버튼 상태 업데이트
        joystickButton.update();
        if (joystickButton.fell()) {
            menuActive = false;
        }
    }

    return menuIndex;
}

// OLED에 메뉴를 표시
void displayMenu(int menuIndex) {
    const int visibleItems = 3;
    const int totalItems = sizeof(menuItems) / sizeof(menuItems[0]);
    int startItem = menuIndex - menuIndex % visibleItems;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // 적절한 폰트 선택

    for (int i = 0; i < visibleItems; i++) {
        int itemIndex = startItem + i;

        if (itemIndex >= totalItems) break;

        int y = 16 + i * 16; // 수직 간격 조정
        if (itemIndex == menuIndex) {
            u8g2.drawBox(0, y - 12, 128, 16); // 선택된 항목 강조
            u8g2.setDrawColor(0); // 텍스트 색상 검정
        }
        else {
            u8g2.setDrawColor(1); // 텍스트 색상 흰색
        }
        u8g2.setCursor(5, y);
        u8g2.print(menuItems[itemIndex]);
    }

    u8g2.setDrawColor(1); // 드로우 색상 초기화
    u8g2.sendBuffer();
}
// ==================== Apartment Game 함수 ====================
void playApartmentGame() {

    // DFPlayer 설정
    myDFPlayer.volume(30); // 볼륨 설정 (0~30)
    //myDFPlayer.play(1);    // 첫 번째 트랙 재생 (파일 이름: 0001.mp3)
    myDFPlayer.loop(1);     // 반복 재생 시작
    const char* colors[] = {
        "Red", "Orange", "Yellow", "Green",
        "Blue", "Indigo", "Violet", "None"
    };

    const uint32_t colorValues[] = {
          0xFF0000, // Red
          0xFFFFFF, // Orange
          0xFFFF00, // Yellow
          0x00FF00, // Green
          0x0000FF, // Blue
          0xA52A2A, // Brown인데 핑크로보임
          0x8B00FF, // Violet
          0x000000  // None (Black)
    };

    const uint8_t playerColorsLocal[4][2] = {
        {0, 1}, // Player 1: Red, Orange
        {2, 3}, // Player 2: Yellow, Green
        {4, 5}, // Player 3: Blue, Brown인데 핑크로보임
        {6, 7}  // Player 4: Violet, None
    };

    uint8_t selectedColors[NUM_SWITCHES];
    uint8_t colorCount = 0;
    bool isColorSelected[8] = { false };

    leds.clear();
    leds.show();

    // 게임 초기화
    colorCount = 0;
    memset(isColorSelected, false, sizeof(isColorSelected));

    // 카운트다운 표시
    for (int i = 3; i >= 1; i--) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB24_tr); // 큰 폰트
        u8g2.setCursor(50, 32); // 중앙에 가까운 위치로 조정
        u8g2.print(i);
        u8g2.sendBuffer();
        delay(1000);
    }

    // 메시지 표시: "All players, press your switches!"
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // 일반 폰트
    u8g2.setCursor(10, 30); // 중앙에 가까운 위치로 조정
    u8g2.println("All players,");
    u8g2.setCursor(10, 45); // 두 번째 줄 위치 조정
    u8g2.println("press your switches!");
    u8g2.sendBuffer();
    delay(1000); // 메시지 표시 시간 추가

    // 스위치 입력 대기 (최대 3초)
    unsigned long startTime = millis();
    while ((colorCount < NUM_SWITCHES) && (millis() - startTime < 3000)) {
        for (int i = 0; i < NUM_SWITCHES; i++) {
            switches[i].update();
            if (switches[i].fell() && !isColorSelected[i]) {
                isColorSelected[i] = true;
                selectedColors[colorCount] = i;
                colorCount++;
                Serial.print("Color selected: ");
                Serial.println(colors[i]);
                delay(100); // 디바운싱 딜레이
            }
        }
    }

    // 모든 색상이 선택되지 않은 경우, 나머지를 랜덤으로 채움
    if (colorCount < NUM_SWITCHES) {
        for (int i = colorCount; i < NUM_SWITCHES; i++) {
            int randomColor;
            do {
                randomColor = random(0, 8);
            } while (isColorSelected[randomColor]);
            selectedColors[i] = randomColor;
            isColorSelected[randomColor] = true;
            colorCount++;
            Serial.print("Random color assigned: ");
            Serial.println(colors[randomColor]);
        }
    }

    // "Game Start!" 메시지 표시
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr); // 중간 폰트
    u8g2.setCursor(10, 32); // 중앙에 가까운 위치로 조정
    u8g2.println("Game Start!");
    u8g2.sendBuffer();
    delay(1500);

    // 처음 4개의 색상을 LED에 표시
    for (int i = 0; i < LED_COUNT; i++) {
        apartment_setColor(i, selectedColors[i], colorValues);
    }
    leds.show();

    // 층 선택
    int floorNumber = 1; // 기본 층
    bool floorSelected = false;

    // 가변저항 값 안정화를 위한 배열
    int potValues[10] = { 0 };
    int potIndex = 0;

    while (!floorSelected) {
        // 가변저항 값 읽기 및 안정화
        int potValue = analogRead(POT_PIN);
        potValues[potIndex] = potValue;
        potIndex = (potIndex + 1) % 10;
        int potAvg = 0;
        for (int i = 0; i < 10; i++) {
            potAvg += potValues[i];
        }
        potAvg /= 10;

        floorNumber = map(potAvg, 0, 1023, 1, 50);
        floorNumber = constrain(floorNumber, 1, 50);

        // OLED에 표시
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(10, 32);
        u8g2.print("Which floor? ");
        u8g2.print(floorNumber);
        u8g2.sendBuffer();

        // 공용 스위치 확인
        commonSwitchDebouncer.update();
        if (commonSwitchDebouncer.fell()) {
            floorSelected = true;
        }

        delay(100); // 빠른 루핑 방지
    }

    // 선택 완료 메시지 표시
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(20, 32);
    u8g2.print(floorNumber);
    u8g2.println(" Floor selected!");
    u8g2.sendBuffer();
    delay(1000);

    // 색상 순서 회전 및 디스플레이 업데이트
    for (int i = 1; i <= floorNumber; i++) {
        apartment_rotateColors(selectedColors, NUM_SWITCHES);

        // LED 업데이트
        for (int j = 0; j < LED_COUNT; j++) {
            apartment_setColor(j, selectedColors[j], colorValues);
        }
        leds.show();

        // OLED 디스플레이 업데이트
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(40, 32);
        u8g2.print(i);
        u8g2.println(" Floor!");
        u8g2.sendBuffer();

        delay(500); // 0.5초 딜레이
    }

    // 최종 결과 계산
    int loserColorIndex = selectedColors[0];
    int loserPlayer = apartment_findPlayer(loserColorIndex, playerColorsLocal, 4);

    if (loserPlayer != -1) {
        // 점수 업데이트 (패배자는 점수 받지 않음)
        for (int i = 0; i < 4; i++) {
            if (i != loserPlayer) {
                playerScores[i] += 50;
            }
        }
        // 결과 OLED에 표시
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(20, 20); // 메시지가 잘리지 않게 위치 조정
        u8g2.print("Loser! Player ");
        u8g2.print(loserPlayer + 1);
        u8g2.println("!");
        u8g2.setCursor(10, 40); // 두 번째 줄 위치 조정
        u8g2.println("Others +50 pts");
        u8g2.sendBuffer();
    }
    else {
        // 패배자 없음
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(30, 32); // 중앙에 가까운 위치로 조정
        u8g2.println("No Loser Found!");
        u8g2.sendBuffer();
    }

    // 게임 종료 후 메뉴로 복귀
    delay(5000); // 결과를 볼 수 있는 시간 제공
    gameState = MENU;

    // 3초 후 LED 끄기
    delay(3000); // 3초 대기
    leds.clear(); // 네오픽셀 LED 끄기
    leds.show(); // 변경사항 적용
    myDFPlayer.stop();
}

// ==================== 헬퍼 함수 ====================

// 특정 LED에 색상을 설정하는 함수 (Apartment Game용)
void apartment_setColor(int ledIndex, int colorIndex, const uint32_t* colorValues) {
    if (ledIndex >= LED_COUNT) return; // 범위 초과 방지
    uint32_t color = colorValues[colorIndex];
    leds.setPixelColor(ledIndex, color);
}

// selectedColors 배열의 색상 순서를 회전시키는 함수 (Apartment Game용)
void apartment_rotateColors(uint8_t* selectedColors, int size) {
    uint8_t firstColor = selectedColors[0];
    for (int i = 0; i < size - 1; i++) {
        selectedColors[i] = selectedColors[i + 1];
    }
    selectedColors[size - 1] = firstColor;
}

// 패배자 찾기 함수 (Apartment Game용)
int apartment_findPlayer(int colorIndex, const uint8_t playerColorsLocal[4][2], int numPlayers) {
    for (int i = 0; i < numPlayers; i++) {
        for (int j = 0; j < 2; j++) {
            if (playerColorsLocal[i][j] == colorIndex) {
                return i;
            }
        }
    }
    return -1;
}
// 메인 게임 함수: Up-Down Game
void playUpDownGame() {

    // DFPlayer 설정
    myDFPlayer.volume(30); // 볼륨 설정 (0~30)
    //myDFPlayer.play(2);    // 첫 번째 트랙 재생 (파일 이름: 0001.mp3)
    myDFPlayer.loop(2);     // 반복 재생 시작

    // Up-Down 게임 관련 변수 선언
    int switchPinsUpDown[4] = {3, 5, 7, 9};  // 1번~4번 플레이어에 해당하는 스위치 핀
    int upDownRandomNumber;
    int playerAttempts[4] = { 0, 0, 0, 0 };  // 플레이어별 시도 횟수 초기화
    int currentPlayer = 0;                  // 현재 플레이어 인덱스 (0~3)
    int playerOrder[4] = { 0, 1, 2, 3 };    // 플레이어 순위

    // Up-Down 게임 초기화
    upDown_initializeGame(upDownRandomNumber, playerAttempts, currentPlayer);

    // 초기 화면 표시
    upDown_displayMessage("UP-DOWN GAME", 2500);

    // 플레이어별로 게임 진행
    for (currentPlayer = 0; currentPlayer < 4; currentPlayer++) {
        bool correct = false;
        upDownRandomNumber = random(1, 101);
        
        // **정답 숫자를 시리얼 모니터에 출력**
        Serial.print("Player ");
        Serial.print(currentPlayer + 1);
        Serial.print("의 정답 숫자: ");
        Serial.println(upDownRandomNumber);

        Serial.print("Player ");
        Serial.print(currentPlayer + 1);
        Serial.println("가 1에서 100 사이의 숫자를 맞추기 시작합니다.");

        while (!correct) {
            // 플레이어 준비 화면
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.setCursor(10, 20);
            u8g2.print("Player " + String(currentPlayer + 1) + " Ready!");
            u8g2.setCursor(10, 40);
            u8g2.print("Choose a number!");
            u8g2.sendBuffer();
            delay(2000);

            // 숫자 선택
            int guessNumber = upDown_getPlayerGuess(POT_PIN, currentPlayer);  // 각 플레이어에 맞는 스위치 처리
            playerAttempts[currentPlayer]++;
            
            // **플레이어의 추측 숫자를 시리얼 모니터에 출력**
            Serial.print("Player ");
            Serial.print(currentPlayer + 1);
            Serial.print("가 추측한 숫자: ");
            Serial.println(guessNumber);

            // 결과 판단
            if (guessNumber < upDownRandomNumber) {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_ncenB08_tr);
                u8g2.setCursor(0, 10);
                u8g2.print(guessNumber);
                u8g2.println(" -> UP!");
                u8g2.setCursor(0, 20);
                u8g2.println("Choose a higher number!");
                u8g2.sendBuffer();
                delay(1000);
            }
            else if (guessNumber > upDownRandomNumber) {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_ncenB08_tr);
                u8g2.setCursor(0, 10);
                u8g2.print(guessNumber);
                u8g2.println(" -> DOWN!");
                u8g2.setCursor(0, 20);
                u8g2.println("Choose a lower number!");
                u8g2.sendBuffer();
                delay(1000);
            }
            else {
                // 정답
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_helvB12_tr); // 큰 폰트로 변경
                u8g2.setCursor(0, 20);
                u8g2.print("Correct!");
                u8g2.setCursor(0, 40);
                u8g2.print("Number: " + String(guessNumber));
                u8g2.sendBuffer();
                delay(5000);
                
                // **정답을 맞춘 플레이어와 정답 숫자를 시리얼 모니터에 출력**
                Serial.print("Player ");
                Serial.print(currentPlayer + 1);
                Serial.print("가 정답을 맞혔습니다! 정답 숫자: ");
                Serial.println(upDownRandomNumber);
                
                correct = true;
            }
        }
    }

    // 모든 플레이어가 게임을 완료하면 점수 계산
    // 시도 횟수에 따라 정렬 (오름차순)
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (playerAttempts[playerOrder[i]] > playerAttempts[playerOrder[j]]) {
                int temp = playerOrder[i];
                playerOrder[i] = playerOrder[j];
                playerOrder[j] = temp;
            }
        }
    }

    // 점수 부여
    playerScores[playerOrder[0]] += 50;
    playerScores[playerOrder[1]] += 25;
    playerScores[playerOrder[2]] += 10;
    playerScores[playerOrder[3]] += 0;

    // 최종 점수 화면 표시
    upDown_displayFinalScores(playerOrder, playerAttempts, playerScores);

    // 게임 상태 초기화 및 메뉴로 복귀
    gameState = MENU;
}

// 게임 초기화 함수
void upDown_initializeGame(int& randomNumber, int playerAttempts[], int& currentPlayer) {
    randomNumber = random(1, 101); // 랜덤 숫자 생성 (1~100)
    for (int i = 0; i < 4; i++) {
        playerAttempts[i] = 0;
    }
    currentPlayer = 0; // 첫 번째 플레이어부터 시작
}

// 플레이어의 숫자 선택을 처리하는 함수
int upDown_getPlayerGuess(int potPin, int playerIndex) {
    int switchPins[4] = {3, 5, 7, 9};  // 1번~4번 플레이어에 해당하는 스위치 핀
    int guessNumber = 0;
    while (true) {
        // 가변저항 값을 읽어 숫자 선택
        int potValue = analogRead(potPin);  
        guessNumber = map(potValue, 0, 1023, 1, 100); // 1~100 범위로 숫자 매핑
        guessNumber = constrain(guessNumber, 1, 100);  // 1과 100 사이로 제한

        // 화면에 현재 숫자 표시
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(0, 10);
        u8g2.print("Current Number: ");
        u8g2.println(guessNumber);
        u8g2.setCursor(0, 30);
        u8g2.println("Press your switch");
        u8g2.setCursor(0, 40);
        u8g2.println("to confirm!");
        u8g2.sendBuffer();

        // 해당 플레이어의 스위치가 눌렸는지 확인 (각 플레이어의 스위치 핀을 사용)
        if (digitalRead(switchPins[playerIndex]) == LOW) { // 해당 플레이어의 스위치가 눌렸을 때 (LOW로 설정됨)
            delay(500); // 노이즈 방지 및 디바운싱
            break; // 숫자 확정
        }
    }
    return guessNumber; // 확정된 숫자 반환
}

// 화면에 메시지를 표시하는 함수 (u8g2 사용)
void upDown_displayMessage(const String& message, unsigned long delayTime) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr); // 일반 폰트
    u8g2.setCursor(0, 32); // 적절한 위치로 조정
    u8g2.println(message);
    u8g2.sendBuffer();
    if (delayTime > 0) {
        delay(delayTime);
    }
}

// 최종 점수를 표시하는 함수 (u8g2 사용, 스크롤링 구현)
void upDown_displayFinalScores(int playerOrder[], int playerAttempts[], int playerScores[]) {
    // 표시할 모든 라인 정의
    const int totalLines = 6;
    const int lineHeight = 12; // 폰트 크기에 따라 조정 필요

    String lines[totalLines];
    lines[0] = "GAME OVER!";
    for (int i = 0; i < 4; i++) {
        lines[i + 1] = "P" + String(playerOrder[i] + 1) + ": " + String(playerScores[playerOrder[i]]) + "pts, " + String(playerAttempts[playerOrder[i]]) + "att";
    }
    lines[5] = "Returning to Menu!";

    // 전체 텍스트의 높이 계산
    int totalHeight = totalLines * lineHeight;

    // 스크롤 시작 및 종료 위치 설정
    int startY = 64; // 디스플레이 하단에서 시작
    int endY = -totalHeight; // 텍스트가 모두 올라가도록

    // 스크롤 속도 및 딜레이 설정
    const int scrollSpeed = 1; // 한 번에 스크롤할 픽셀 수
    const int scrollDelay = 50; // 스크롤 간 딜레이 (밀리초)

    // 스크롤 루프
    for (int offset = startY; offset >= endY; offset -= scrollSpeed) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        for (int i = 0; i < totalLines; i++) {
            int y = offset + i * lineHeight;
            // 텍스트가 화면 내에 있을 때만 표시
            if (y > 0 && y < 64) {
                u8g2.setCursor(10, y);
                u8g2.print(lines[i]);
            }
        }

        u8g2.sendBuffer();
        delay(scrollDelay);
    }

    // 추가적인 지연 시간 (5초)
    delay(5000);

    myDFPlayer.stop(); // 소리 정지
}



//베라
void playBaskinRobbinsGame() {

    // DFPlayer 설정
    myDFPlayer.volume(30); // 볼륨 설정 (0~30)
    //myDFPlayer.play(3);    // 첫 번째 트랙 재생 (파일 이름: 0001.mp3)
    myDFPlayer.loop(3);
//    Serial.println("Starting Baskin Robbins 31 Game...");
    #define playerCount 4

    // 스위치 핀 배열 (플레이어마다 하나씩 연결된 스위치 핀)
    int playerSwitchPins[] = {3, 5, 7, 9}; // 스위치 핀 예시 (하드웨어에 맞게 설정)

    //int playerScores[playerCount] = {0, 0, 0, 0}; // 각 플레이어의 점수 배열
     // 시드 초기화 (이 값은 아날로그 핀에 연결된 값으로 설정할 수 있습니다)
    randomSeed(analogRead(0));  // A0 핀의 값으로 시드 초기화
    int currentPlayer = random(0, playerCount);  // 현재 플레이어 (0 ~ playerCount-1)
    int currentCount = 0;   // 31 게임의 현재 카운트
    int playerPressCount = 0; // 현재 플레이어의 버튼 누른 횟수
    bool gameActive = true;  // 게임 활성화 상태

    // 버튼 상태 관리
    int playerButtonStates[playerCount] = {HIGH, HIGH, HIGH, HIGH};
    int lastPlayerButtonStates[playerCount] = {HIGH, HIGH, HIGH, HIGH};
    unsigned long lastPlayerDebounceTimes[playerCount] = {0, 0, 0, 0};
    unsigned long debounceDelay = 50;  // 디바운스 딜레이 (밀리초)

    int commonSwitchPin = 12;  // 공용 스위치 핀
    int commonButtonState = HIGH;
    int lastCommonButtonState = HIGH;
    unsigned long lastCommonDebounceTime = 0;

    // 카운트다운
    startCountdown();

    // "시작 플레이어" 표시
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(20, 30);
    u8g2.print("Player ");
    u8g2.print(currentPlayer + 1);  // 시작 플레이어 표시
    u8g2.setCursor(20, 50);
    u8g2.print(" Starts!");
    u8g2.sendBuffer();
    delay(1500);  // 1.5초 대기

    // 게임 루프
    while (gameActive) {
        unsigned long currentTime = millis();

        // 현재 플레이어의 버튼 상태 읽기
        int playerPin = playerSwitchPins[currentPlayer];
        int readingPlayer = digitalRead(playerPin);

        // 플레이어 버튼 디바운싱 처리
        if (readingPlayer != lastPlayerButtonStates[currentPlayer]) {
            lastPlayerDebounceTimes[currentPlayer] = currentTime;
        }

        if ((currentTime - lastPlayerDebounceTimes[currentPlayer]) > debounceDelay) {
            if (readingPlayer != playerButtonStates[currentPlayer]) {
                playerButtonStates[currentPlayer] = readingPlayer;

                if (playerButtonStates[currentPlayer] == LOW) {
                    // 현재 플레이어의 버튼 눌림 처리
                    playerPressCount++;
                    currentCount++;
                    updateBaskinRobbinsDisplay(currentPlayer, currentCount);  // 디스플레이 갱신

                    if (currentCount >= 31) {
                        // 게임 종료 처리
                        gameActive = false;
                        gameOverBaskinRobbins(currentPlayer, playerScores);
                    }

                    if (playerPressCount >= 3) {
                        // 3번 누르면 다음 플레이어로 변경
                        currentPlayer = (currentPlayer + 1) % playerCount;
                        playerPressCount = 0;  // 버튼 누른 횟수 리셋
                        updateBaskinRobbinsDisplay(currentPlayer, currentCount);
                    }
                }
            }
        }

        lastPlayerButtonStates[currentPlayer] = readingPlayer;

        // 공용 스위치 처리
        int readingCommon = digitalRead(COMMON_SWITCH_PIN);

        // 공용 스위치 디바운싱 처리
        if (readingCommon != lastCommonButtonState) {
            lastCommonDebounceTime = currentTime;
        }

        if ((currentTime - lastCommonDebounceTime) > debounceDelay) {
            if (readingCommon != commonButtonState) {
                commonButtonState = readingCommon;

                if (commonButtonState == LOW) {
                    // 짧게 눌렀을 때 처리 (플레이어 변경)
                    if (playerPressCount >= 1 && playerPressCount < 3) {
                        currentPlayer = (currentPlayer + 1) % playerCount;  // 플레이어 변경
                        playerPressCount = 0;  // 버튼 눌린 횟수 초기화
                        updateBaskinRobbinsDisplay(currentPlayer, currentCount);  // 디스플레이 갱신
                    }
                }
            }
        }

        lastCommonButtonState = readingCommon;
    }
}

// 카운트다운 함수
void startCountdown() {
    for (int i = 3; i > 0; i--) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.setCursor(50, 30);
        u8g2.print(i);
        u8g2.sendBuffer();
        delay(1000);  // 1초 대기
    }

    // "Start!" 메시지 출력
    u8g2.clearBuffer();
    u8g2.setCursor(40, 30);
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.print("Start!");
    u8g2.sendBuffer();
    delay(1000);  // "Start!" 1초 대기
}

// 디스플레이 업데이트 함수
void updateBaskinRobbinsDisplay(int currentPlayer, int currentCount) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);

    // 현재 플레이어 표시
    u8g2.setCursor(0, 20);
    u8g2.print("Player ");
    u8g2.print(currentPlayer + 1);

    // 현재 카운트 표시
    u8g2.setCursor(0, 45);
    u8g2.print("Count: ");
    u8g2.print(currentCount);

    u8g2.sendBuffer();
}

// 게임 오버 처리 함수
void gameOverBaskinRobbins(int currentPlayer, int playerScores[]) {
    // 패배한 플레이어 표시 (3초)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);  // 기본 폰트
    u8g2.setCursor(0, 20);
    u8g2.print("Game Over");

    u8g2.setCursor(0, 45);
    u8g2.print("P");
    u8g2.print(currentPlayer + 1);
    u8g2.print(" Lost");

    u8g2.sendBuffer();
    delay(3000);  // 3초 대기

    // 점수 업데이트 (모든 플레이어의 점수 표시) (3초)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);  // 작은 폰트 사용
    u8g2.setCursor(0, 20);

    // 플레이어 점수를 세로로 나열
    for (int i = 0; i < playerCount; i++) {
        if (i != currentPlayer) { // 패배한 플레이어 제외
            playerScores[i] += 50; // 나머지 플레이어에게 50점 추가
        }
        
        u8g2.print("P");
        u8g2.print(i + 1);
        u8g2.print(": ");
        u8g2.print(playerScores[i]);
        u8g2.print(" pts");
        u8g2.setCursor(0, 20 + (i + 1) * 10);  // 다음 줄로 이동
    }

    u8g2.sendBuffer();
    delay(3000);  // 3초 대기 후 메뉴로 이동

    // 게임 종료 후 메뉴로 돌아가기
    gameState = MENU;  // 게임이 끝나면 메뉴 화면으로 돌아가기
    displayMenu(gameState);  // 메뉴 표시
    myDFPlayer.stop();
}
// ==================== GGSB 게임 함수 ====================
void playGGSBGame() {
    myDFPlayer.volume(30);  // 볼륨 설정 (0~30)
    // 랜덤 시드 초기화 (한 번만)
    if (!GGSB_randomSeedInitialized) {
        randomSeed(analogRead(A0));
        GGSB_randomSeedInitialized = true;
    }

    // 플레이어 상태 초기화
    for (int i = 0; i < 4; i++) {
        GGSB_players[i].active = true;
    }
    GGSB_activePlayerCount = 4;

    // 제한 시간 초기화
    GGSB_timeLimit = 2.0;

    // NeoPixel 초기화
    leds.begin();
    leds.show(); // 초기에는 모든 LED 끄기

    // 게임 시작: 카운트다운 및 시작 메시지 표시
    GGSB_displayCountdown();
    GGSB_displayStart();

    // 게임 루프
    while (true) {
        // 현재 활성 플레이어가 있는지 확인
        if (GGSB_activePlayerCount < 1) {
            GGSB_displayGameOver();
            GGSB_displayScores();
            delay(2000);
            GGSB_resetNeoPixels();
            GGSB_clearDisplay();
            gameState = MENU;
            break;
        }

        // 활성 플레이어 중 랜덤으로 한 명 선택
        int selectedPlayerId = GGSB_selectRandomPlayer();
        if (selectedPlayerId == -1) {
            GGSB_displayGameOver();
            GGSB_displayScores();
            delay(2000);
            GGSB_resetNeoPixels();
            GGSB_clearDisplay();
            gameState = MENU;
            break;
        }

        // BOOM! 메시지 및 NeoPixel 설정
        GGSB_displayBoom(selectedPlayerId);
        myDFPlayer.play(4); // 0004번 재생 (빵 소리)

        // 시간 측정 시작
        unsigned long startTime = millis();

        // 플레이어 응답 확인
        bool targetPressed, leftPressed, rightPressed;
        bool allCorrect = GGSB_checkPlayerResponses(selectedPlayerId, startTime, GGSB_timeLimit, targetPressed, leftPressed, rightPressed);

        if (allCorrect) {
            GGSB_displayNice();
            // 시간 제한 감소
            GGSB_timeLimit = max(GGSB_timeLimit - GGSB_timeDecrement, GGSB_minTimeLimit);
        } else {
            // Find selectedIdx
            int selectedIdx = -1;
            for (int i = 0; i < 4; i++) {
                if (GGSB_players[i].id == selectedPlayerId) {
                    selectedIdx = i;
                    break;
                }
            }

            // Collect loser IDs
            int loserIds[3];
            int numLosers = 0;

            if (!targetPressed && selectedIdx != -1) {
                loserIds[numLosers++] = selectedPlayerId;
            }

            if (selectedIdx != -1) {
                int leftIdx = (selectedIdx == 0) ? 3 : selectedIdx - 1;
                int rightIdx = (selectedIdx == 3) ? 0 : selectedIdx + 1;

                if (!leftPressed) {
                    loserIds[numLosers++] = GGSB_players[leftIdx].id;
                }
                if (!rightPressed) {
                    loserIds[numLosers++] = GGSB_players[rightIdx].id;
                }
            }

            // Display losing players and play sounds
            for (int i = 0; i < numLosers; i++) {
                GGSB_displayLose(loserIds[i]);

                // 소리 재생 로직
                // 빨간색 플레이어가 지목당한 경우 (leftPressed 관련 없음)
                if (loserIds[i] == 1) {
                    // 빨간색 플레이어의 왼쪽 스위치(홀수) 눌림과 관련 없음
                    // 이미 LOSE! 메시지가 표시되었으므로 추가 소리 필요 없음
                }
                // 보라색(4)과 노란색(2) 플레이어의 오른쪽 스위치 눌림
                // else if (loserIds[i] == 2 || loserIds[i] == 4) {
                //     myDFPlayer.play(5); // 0005번 재생 (으악 소리)
                // }

                delay(2000); // 2초 동안 패배자 표시 유지
            }

            // 점수 업데이트
            GGSB_updateScores(loserIds, numLosers);

            // 게임 종료
            GGSB_displayGameOver();
            GGSB_displayScores();
            delay(2000);
            GGSB_resetNeoPixels();
            GGSB_clearDisplay();
            gameState = MENU;
            break;
        }

        GGSB_resetNeoPixels();
    }
}

// 3, 2, 1 카운트다운 표시
void GGSB_displayCountdown() {
    for (int i = 3; i > 0; i--) {
        u8g2.clearBuffer();
        u8g2.setCursor(60, 32);
        u8g2.print(i);
        u8g2.sendBuffer();
        delay(1000);
    }
}

// "Game Start!" 메시지 표시
void GGSB_displayStart() {
    u8g2.clearBuffer();
    u8g2.setCursor(30, 32);
    u8g2.print("Game Start!");
    u8g2.sendBuffer();
    delay(1000);
}

// "BOOM!" 메시지와 NeoPixel 설정
void GGSB_displayBoom(int playerId) {
    u8g2.clearBuffer();
    u8g2.setCursor(50, 20);
    u8g2.print("BOOM!");
    u8g2.sendBuffer();

    // 선택된 플레이어의 색으로 NeoPixel 설정
    uint32_t selectedColor = 0;
    for (int i = 0; i < 4; i++) {
        if (GGSB_players[i].id == playerId) {
            selectedColor = GGSB_players[i].color;
            break;
        }
    }
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, selectedColor);
    }
    leds.show();
}

// "NICE!" 메시지 표시
void GGSB_displayNice() {
    u8g2.clearBuffer();
    u8g2.setCursor(50, 32);
    u8g2.print("NICE!");
    u8g2.sendBuffer();
    delay(1000);
}

// "PLAYER X LOSE!" 메시지 표시 및 NeoPixel 설정
void GGSB_displayLose(int playerId) {
    u8g2.clearBuffer();
    u8g2.setCursor(20, 32);
    u8g2.print("PLAYER ");
    u8g2.print(playerId);
    u8g2.print(" LOSE!");
    u8g2.sendBuffer();

    // NeoPixel 설정 (패배자 색상만 표시)
    uint32_t loseColor = 0;
    for (int i = 0; i < 4; i++) {
        if (GGSB_players[i].id == playerId) {
            loseColor = GGSB_players[i].color;
            break;
        }
    }
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, loseColor);
    }
    leds.show();
}

// 게임 종료 메시지 표시
void GGSB_displayGameOver() {
    u8g2.clearBuffer();
    u8g2.setCursor(40, 32);
    u8g2.print("Game Over!");
    u8g2.sendBuffer();
    delay(2000);
}

// 최종 점수 표시
void GGSB_displayScores() {
    u8g2.clearBuffer();
    u8g2.setCursor(10, 10);
    u8g2.print("Final Scores:");
    for (int i = 0; i < 4; i++) {
        u8g2.setCursor(10, 20 + i * 10);
        u8g2.print("Player ");
        u8g2.print(GGSB_players[i].id);
        u8g2.print(": ");
        u8g2.print(playerScores[i]); // playerScores 배열 사용
    }
    u8g2.sendBuffer();
}

// 모든 NeoPixel 끄기
void GGSB_resetNeoPixels() {
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, 0); // 모두 끄기
    }
    leds.show();
}

// 게임 종료 후 디스플레이 초기화
void GGSB_clearDisplay() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

// 활성 플레이어 중 랜덤으로 한 명 선택
int GGSB_selectRandomPlayer() {
    int activeCount = 0;
    for (int i = 0; i < 4; i++) {
        if (GGSB_players[i].active) activeCount++;
    }

    if (activeCount == 0) {
        return -1; // 모든 플레이어가 비활성화됨
    }

    int randNum = random(0, activeCount);
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (GGSB_players[i].active) {
            if (count == randNum) {
                return GGSB_players[i].id; // 플레이어 ID 반환
            }
            count++;
        }
    }
    return -1; // 선택 실패
}

// 플레이어 응답 확인
bool GGSB_checkPlayerResponses(int selectedPlayerId, unsigned long startTime, float timeLimit, bool& targetPressed, bool& leftPressed, bool& rightPressed) {
    // 지목된 플레이어와 인접 플레이어의 인덱스 찾기 (0부터)
    int selectedIdx = -1;
    for (int i = 0; i < 4; i++) {
        if (GGSB_players[i].id == selectedPlayerId) {
            selectedIdx = i;
            break;
        }
    }
    if (selectedIdx == -1) return false; // 오류

    // 원형 배열 가정: 왼쪽, 오른쪽 인덱스 계산
    int leftIdx = (selectedIdx == 0) ? 3 : selectedIdx - 1;
    int rightIdx = (selectedIdx == 3) ? 0 : selectedIdx + 1;

    // 필요한 스위치 핀
    int targetSwitchPin = GGSB_players[selectedIdx].oddSwitchPin;
    int leftSwitchPin = GGSB_players[leftIdx].evenSwitchPin;
    int rightSwitchPin = GGSB_players[rightIdx].evenSwitchPin;

    // 스위치 인덱스 찾기
    int targetSwitchIdx = findSwitchIndex(targetSwitchPin);
    int leftSwitchIdx = findSwitchIndex(leftSwitchPin);
    int rightSwitchIdx = findSwitchIndex(rightSwitchPin);

    if (targetSwitchIdx == -1 || leftSwitchIdx == -1 || rightSwitchIdx == -1) {
        Serial.println("Error: Switch index not found.");
        return false;
    }

    targetPressed = false;
    leftPressed = false;
    rightPressed = false;

    // 응답 대기 루프
    while ((millis() - startTime) / 1000.0 < timeLimit) {
        // 모든 스위치 업데이트
        for (int i = 0; i < NUM_SWITCHES; i++) {
            switches[i].update();
        }

        // 지목된 플레이어의 홀수 스위치 눌림 확인
        if (!targetPressed && switches[targetSwitchIdx].fell()) {
            targetPressed = true;
            Serial.println("Target player pressed the correct switch.");
        }

        // 왼쪽 인접 플레이어의 짝수 스위치 눌림 확인
        if (!leftPressed && switches[leftSwitchIdx].fell()) {
            leftPressed = true;
            Serial.println("Left adjacent player pressed the correct switch.");
            // 보라색과 노란색 플레이어는 오른쪽 스위치를 눌러야 함
            // 으악 소리 재생
            myDFPlayer.play(5); // 0005번 재생 (으악 소리)
        }

        // 오른쪽 인접 플레이어의 짝수 스위치 눌림 확인
        if (!rightPressed && switches[rightSwitchIdx].fell()) {
            rightPressed = true;
            Serial.println("Right adjacent player pressed the correct switch.");
            // 보라색과 노란색 플레이어는 오른쪽 스위치를 눌러야 함
            // 으악 소리 재생
            myDFPlayer.play(5); // 0005번 재생 (으악 소리)
        }

        // 잘못된 스위치 눌림 확인
        for (int i = 0; i < NUM_SWITCHES; i++) {
            // 필요한 스위치가 아닌 경우
            if (switchPins[i] != targetSwitchPin && switchPins[i] != leftSwitchPin && switchPins[i] != rightSwitchPin) {
                if (switches[i].fell()) {
                    // 잘못된 스위치 눌림
                    // 해당 스위치를 누른 플레이어 찾기
                    int wrongPlayerId = -1;
                    for (int p = 0; p < 4; p++) {
                        if (switchPins[i] == GGSB_players[p].oddSwitchPin || switchPins[i] == GGSB_players[p].evenSwitchPin) {
                            if (GGSB_players[p].active) {
                                wrongPlayerId = GGSB_players[p].id;
                                break;
                            }
                        }
                    }
                    if (wrongPlayerId != -1) {
                        Serial.print("Wrong switch pressed by Player ");
                        Serial.println(wrongPlayerId);
                        // 패배자 ID 배열에 추가하지 않고 즉시 패배 처리
                        return false;
                    }
                }
            }
        }

        // 모든 필요한 스위치가 눌렸는지 확인
        if (targetPressed && leftPressed && rightPressed) {
            return true;
        }

        delay(10); // 짧은 지연으로 CPU 부담 줄이기
    }

    // 시간 초과 처리: 어떤 스위치가 눌리지 않았는지 확인하여 패배 처리
    return false;
}

// 패배자 처리 및 점수 업데이트
void GGSB_updateScores(int loserIds[], int numLosers) {
    // 패배한 플레이어 비활성화 및 점수 0점 처리
    for (int i = 0; i < numLosers; i++) {
        for (int p = 0; p < 4; p++) {
            if (GGSB_players[p].id == loserIds[i]) {
                GGSB_players[p].active = false;
                GGSB_activePlayerCount--;
                // 패배한 플레이어의 점수를 0으로 설정
                playerScores[p] = 0;
                break;
            }
        }
    }

    // 나머지 활성 플레이어에게 +50점 추가
    for (int p = 0; p < 4; p++) {
        if (GGSB_players[p].active) {
            playerScores[p] += 50; // playerScores 배열 사용
        }
    }
}

// ==================== 헬퍼 함수 ====================

// 특정 핀 번호의 스위치 인덱스를 찾는 함수
int findSwitchIndex(int pin) {
    for (int i = 0; i < NUM_SWITCHES; i++) {
        if (switchPins[i] == pin) {
            return i;
        }
    }
    return -1; // 찾지 못한 경우
}

// 랭킹을 표시하는 함수
void showRanking() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 10);
    u8g2.println("Player Rankings:");
    for (int i = 0; i < 4; i++) {
        u8g2.setCursor(0, 20 + i * 10);
        u8g2.print("P");
        u8g2.print(i + 1);
        u8g2.print(": ");
        u8g2.print(playerScores[i]);
        u8g2.println(" pts");
    }
    u8g2.sendBuffer();
    delay(5000); // 5초간 표시
}

// 랭킹을 리셋하는 함수
void resetRanking() {
    for (int i = 0; i < 4; i++) {
        playerScores[i] = 0; // 점수 초기화
    }

    // 게임에서 점수 초기화 후 화면 표시
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(20, 32);
    u8g2.println("Scores Reset!");
    u8g2.sendBuffer();
    delay(2000);
}
