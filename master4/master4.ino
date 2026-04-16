#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>

#define IR_RECV_PIN 2
#define BUZZER_PIN 8

LiquidCrystal_I2C lcd(0x20, 16, 2);
DFRobotDFPlayerMini myDFPlayer;

const int JUDGE_LINE_POS = 0;
const int NOTE_START_POS = 15;
const unsigned long SCROLL_SPEED = 120;

//得分分值
const int SCORE_PERFECT = 100;
const int SCORE_GOOD = 50;

//判定时长
const int JUDGE_WINDOW_PERFECT = 180;
const int JUDGE_WINDOW_GOOD = 350;

//播放“0001.mp3”
#define TRACK_BGM 1

//音符数组
const unsigned long noteHitTimes[] = {
  3200,    
  4800,   
  6400,
  8000,
  9600,
  11200,
  12000,
  12800,
  13600,
  14400,
  15200,
  16000,
  17600,
  19200,
  20800,
  22400,
  24000,
  24800,
  25600,
  26000,
  26400,
  27200,
  27600,
  28000,
  28800,
  30400,
  32000,
  33600,
  34400,
  35200,
  36000,
  36800,
  38400,
  40000,
  40800,
  41600,
  42400,
  43200,
  44000,
  44800,
  45200,
  45600,
  46400,
  47200,
  48000,
  48400,
  48800,
  49600,
  50400,
  51200,
  51600,
  52000,
  52400,
  52800,
  53200,
  54000,
  54200,
  54800
};

const byte totalNotes = sizeof(noteHitTimes) / sizeof(noteHitTimes[0]);

//参数：延时参数，用于微调第一个音符出现的时间
long GLOBAL_TIME_OFFSET = -150; 


struct Note {
  int position;
  unsigned long moveTime;
  bool active;
  unsigned long hitTime;
};
#define MAX_NOTES 6
Note notes[MAX_NOTES];
byte activeNoteCount = 0;

unsigned long gameStartTime = 0;
int score = 0;
bool gameRunning = false;

// 显示变量
char judgeText[9] = "";
unsigned long judgeTextTime = 0;
const unsigned long JUDGE_DISPLAY_TIME = 300;

byte nextNoteIndex = 0;


void spawnNote(unsigned long hitTime);
void updateNotes();
void drawScreen();
void judgeNote();
void startGame();
void endGame();
void playBuzzer(int frequency, int duration);
void updateJudgeDisplay();
bool initMP3();


void setup() {
  IrReceiver.begin(IR_RECV_PIN, false);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  lcd.init();
  lcd.backlight();
  lcd.setBacklight(20);
  
  lcd.clear();
  lcd.print("Press any key");
  lcd.setCursor(0, 1);
  lcd.print("to start");
  
  if (!initMP3()) {
    lcd.clear();
    lcd.print("MP3 Error");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");
    while(1) {
      playBuzzer(300, 200);
      delay(1000);
    }
  }
  
  playBuzzer(1000, 200);
}


bool initMP3() {
  Serial.begin(9600);
  delay(1000);
  
  if (!myDFPlayer.begin(Serial)) {
    return false;
  }
  
  delay(200);
  myDFPlayer.volume(15);
  delay(200);
  
  return true;
}


void loop() {
  if (IrReceiver.decode()) {
    unsigned long keyCode = IrReceiver.decodedIRData.decodedRawData;
    if (keyCode != 0 && keyCode != 0xFFFFFFFF) {
      if (!gameRunning) {
        startGame();
      } else {
        playBuzzer(1000, 100);
        judgeNote();
      }
    }
    IrReceiver.resume();
  }
  
  if (gameRunning) {
    unsigned long currentTime = millis() - gameStartTime;
    
    //检查是否需要生成新音符
    if (nextNoteIndex < totalNotes) {
      unsigned long hitTime = noteHitTimes[nextNoteIndex] + GLOBAL_TIME_OFFSET;
      
      //计算音符生成时间
      int distance = NOTE_START_POS - JUDGE_LINE_POS;
      unsigned long travelTime = distance * SCROLL_SPEED;
      unsigned long spawnTime = hitTime - travelTime;
      
      if (currentTime >= spawnTime) {
        spawnNote(hitTime);
        nextNoteIndex++;
      }
    }
    
    updateNotes();
    updateJudgeDisplay();
    
    static unsigned long lastDrawTime = 0;
    if (millis() - lastDrawTime >= 80) {
      drawScreen();
      lastDrawTime = millis();
    }
    
    if (nextNoteIndex >= totalNotes && activeNoteCount == 0) {
      delay(500);
      endGame();
    }
  }
}


void startGame() {
  lcd.clear();
  gameRunning = true;
  gameStartTime = millis();
  score = 0;
  nextNoteIndex = 0;
  activeNoteCount = 0;
  judgeText[0] = '\0';
  
  for (byte i = 0; i < MAX_NOTES; i++) {
    notes[i].active = false;
  }
  
  myDFPlayer.play(TRACK_BGM);
  playBuzzer(1500, 150);
  
  lcd.print("Go!");
  delay(500);
  lcd.clear();
}

void spawnNote(unsigned long hitTime) //遍历音符之后找到还没有出现的第一个音符使其出现{
  for (byte i = 0; i < MAX_NOTES; i++) {
    if (!notes[i].active) {
      notes[i].position = NOTE_START_POS;
      notes[i].moveTime = millis() + SCROLL_SPEED;
      notes[i].active = true;
      notes[i].hitTime = hitTime;
      activeNoteCount++;
      break;
    }
  }
}

void updateNotes() {
  unsigned long currentMillis = millis();
  
  for (byte i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active && currentMillis >= notes[i].moveTime) //判断音符是否活着且还在屏幕上{
      notes[i].position--;//音符的移动
      notes[i].moveTime = currentMillis + SCROLL_SPEED;
      
      if (notes[i].position < 0) {
        notes[i].active = false;
        activeNoteCount--;
      }
    }
  }
}

void drawScreen() {
  char line1[17] = "|               ";
  line1[16] = '\0';
  
  for (byte i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active && notes[i].position >= 0 && notes[i].position < 16) {
      if (line1[notes[i].position] != '|') {
        line1[notes[i].position] = 'o';
      }
    }
  }
  //做了一个“ ”与“o”的遍历实现移动
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(score);
  
  lcd.setCursor(8, 1);
  if (judgeText[0] != '\0') {
    lcd.print(judgeText);
  } else {
    lcd.print("        ");//打印perfect good和miss
  }
}

void judgeNote() {
  if (!gameRunning) return;
  
  unsigned long currentTime = millis() - gameStartTime;
  char bestIndex = -1;
  long minTimeDiff = JUDGE_WINDOW_GOOD + 1;
  
  // 寻找最接近的活动音符
  for (char i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active) {
      long timeDiff = (long)currentTime - (long)notes[i].hitTime;
      long absTimeDiff = abs(timeDiff);
      
      if (absTimeDiff < minTimeDiff) {
        minTimeDiff = absTimeDiff;
        bestIndex = i;
      }
    }
  }
  
  if (bestIndex != -1) {
    long timeDiff = (long)currentTime - (long)notes[bestIndex].hitTime;
    long absTimeDiff = abs(timeDiff);//精准计算时间差
    
    notes[bestIndex].active = false;
    activeNoteCount--;
    
    if (absTimeDiff <= JUDGE_WINDOW_PERFECT) {
      score += SCORE_PERFECT;
      playBuzzer(1500, 100);
      strcpy(judgeText, "PERFECT");
    } else if (absTimeDiff <= JUDGE_WINDOW_GOOD) {
      score += SCORE_GOOD;
      playBuzzer(1200, 100);
      strcpy(judgeText, "GOOD");
    } else {
      // 时间差过大，判定为MISS但不会移除音符
      playBuzzer(500, 80);
      strcpy(judgeText, "MISS");
      // MISS不扣分，音符继续移动
      notes[bestIndex].active = true;  // 恢复音符状态
      activeNoteCount++;
    }
  } else {
    // 没有找到活动音符，按空键处理
    playBuzzer(500, 80);
    strcpy(judgeText, "MISS");
  }
  
  judgeTextTime = millis() + JUDGE_DISPLAY_TIME;
}

void updateJudgeDisplay() {
  if (millis() > judgeTextTime && judgeText[0] != '\0') {
    judgeText[0] = '\0';
  }
}

void endGame() {
  gameRunning = false;
  myDFPlayer.stop();
  
  lcd.clear();
  lcd.print("MUSIC FINISHED!");
  lcd.setCursor(0, 1);
  lcd.print("Score:");
  lcd.print(score);
  
  playBuzzer(600, 300);
  delay(500);
  playBuzzer(400, 400);
  
  delay(2000);
  
  lcd.clear();
  lcd.print("Press any key");
  lcd.setCursor(0, 1);
  lcd.print("to restart");
}

void playBuzzer(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
}