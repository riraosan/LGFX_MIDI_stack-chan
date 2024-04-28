#include <Arduino.h>
#include <M5StackUpdater.h>
#include <M5Unified.h>
#include <SD.h>
#include <Ticker.h>

#include "IntervalCheck.h"
#include "IntervalCheckMicros.h"
#include "MidiPort.h"
#include "SmfFileAccess.h"
#include "SmfSeq.h"
#include "common.h"

SMF_SEQ_TABLE *pseqTbl;  // SMFシーケンサハンドル

TaskHandle_t taskHandle;

#if defined(ARDUINO_M5STACK_Core2)
// M5Stack Core2用のサーボの設定
// Port.A X:G33, Y:G32
// Port.C X:G13, Y:G14
// スタックチャン基板 X:G19, Y:G27
#define SERVO_PIN_X 33
#define SERVO_PIN_Y 32
#elif defined(ARDUINO_M5STACK_FIRE)
// M5Stack Fireの場合はPort.A(X:G22, Y:G21)のみです。
// I2Cと同時利用は不可
#define SERVO_PIN_X 22
#define SERVO_PIN_Y 21
#if SERVO_PIN_X == 22
// FireでPort.Aを使う場合は内部I2CをOffにする必要がある。
#define CORE_PORT_A
#endif

#elif defined(ARDUINO_M5Stack_Core_ESP32)
// M5Stack Basic/Gray/Go用の設定
// Port.A X:G22, Y:G21
// Port.C X:G16, Y:G17
// スタックチャン基板 X:G5, Y:G2
#define SERVO_PIN_X 22
#define SERVO_PIN_Y 21

#if SERVO_PIN_X == 22
// CoreでPort.Aを使う場合は内部I2CをOffにする必要がある。
#define CORE_PORT_A
#endif

#elif defined(ARDUINO_M5STACK_CORES3)
// M5Stack CoreS3用の設定
// ※暫定的にplatformio.iniにARDUINO_M5STACK_CORES3を定義しています。 Port.A X:G1
// Y:G2 Port.B X:G8 Y:G9 Port.C X:18 Y:17
#define SERVO_PIN_X 1
#define SERVO_PIN_Y 2
#include <gob_unifiedButton.hpp>  // 2023/5/12現在 M5UnifiedにBtnA等がないのでGobさんのライブラリを使用
goblib::UnifiedButton unifiedButton;
#elif defined(ARDUINO_M5STACK_DIAL)
// M5Stack Fireの場合はPort.A(X:G22, Y:G21)のみです。
// I2Cと同時利用は不可
#define SERVO_PIN_X 13
#define SERVO_PIN_Y 15
#endif

int servo_offset_x = 0;  // X軸サーボのオフセット（90°からの+-で設定）
int servo_offset_y = 0;  // Y軸サーボのオフセット（90°からの+-で設定）

#include <Avatar.h>  // https://github.com/meganetaaan/m5stack-avatar

#include <ServoEasing.hpp>  // https://github.com/ArminJo/ServoEasing

#include "formatString.hpp"  // https://gist.github.com/GOB52/e158b689273569357b04736b78f050d6

using namespace m5avatar;
Avatar avatar;

#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

#define SDU_APP_PATH "/stackchan_tester.bin"
#define TFCARD_CS_PIN 4

ServoEasing servo_x;
ServoEasing servo_y;

uint32_t mouth_wait = 2000;  // 通常時のセリフ入れ替え時間（msec）
uint32_t last_mouth_millis = 0;

const char *lyrics[] = {"BtnA:MoveTo90  ", "BtnB:ServoTest  ",
                        "BtnC:RandomMode  ", "BtnALong:AdjustMode"};
const int lyrics_size = sizeof(lyrics) / sizeof(char *);
int lyrics_idx = 0;

void moveX(int x, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_x.easeTo(x + servo_offset_x);
  } else {
    servo_x.easeToD(x + servo_offset_x, millis_for_move);
  }
}

void moveY(int y, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_y.easeTo(y + servo_offset_y);
  } else {
    servo_y.easeToD(y + servo_offset_y, millis_for_move);
  }
}

void moveXY(int x, int y, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_x.setEaseTo(x + servo_offset_x);
    servo_y.setEaseTo(y + servo_offset_y);
  } else {
    servo_x.setEaseToD(x + servo_offset_x, millis_for_move);
    servo_y.setEaseToD(y + servo_offset_y, millis_for_move);
  }
  // サーボが停止するまでウェイトします。
  synchronizeAllServosStartAndWaitForAllServosToStop();
}

void adjustOffset() {
  // サーボのオフセットを調整するモード
  servo_offset_x = 0;
  servo_offset_y = 0;
  moveXY(90, 90);
  bool adjustX = true;
  for (;;) {
#ifdef ARDUINO_M5STACK_CORES3
    unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
    M5.update();
    if (M5.BtnA.wasPressed()) {
      // オフセットを減らす
      if (adjustX) {
        servo_offset_x--;
      } else {
        servo_offset_y--;
      }
    }
    if (M5.BtnB.pressedFor(2000)) {
      // 調整モードを終了
      break;
    }
    if (M5.BtnB.wasPressed()) {
      // 調整モードのXとYを切り替え
      adjustX = !adjustX;
    }
    if (M5.BtnC.wasPressed()) {
      // オフセットを増やす
      if (adjustX) {
        servo_offset_x++;
      } else {
        servo_offset_y++;
      }
    }
    moveXY(90, 90);

    std::string s;

    if (adjustX) {
      s = formatString("%s:%d:BtnB:X/Y", "X", servo_offset_x);
    } else {
      s = formatString("%s:%d:BtnB:X/Y", "Y", servo_offset_y);
    }
    avatar.setSpeechText(s.c_str());
  }
}

void moveRandom(void *) {
  for (;;) {
    // ランダムモード
    int x = random(45, 135);  // 45〜135° でランダム
    int y = random(60, 90);   // 50〜90° でランダム
#ifdef ARDUINO_M5STACK_CORES3
    unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
    M5.update();
    if (M5.BtnC.wasPressed()) {
      break;
    }
    int delay_time = random(10);
    moveXY(x, y, 1000 + 100 * delay_time);
    delay(2000 + 500 * delay_time);
#if !defined(CORE_PORT_A)
    // Basic/M5Stack Fireの場合はバッテリー情報が取得できないので表示しない
    avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
#endif
    avatar.setSpeechText("");

    delay(1);
  }
}

void testServo() {
  for (int i = 0; i < 2; i++) {
    avatar.setSpeechText("X 90 -> 0  ");
    moveX(0);
    avatar.setSpeechText("X 0 -> 180  ");
    moveX(180);
    avatar.setSpeechText("X 180 -> 90  ");
    moveX(90);
    avatar.setSpeechText("Y 90 -> 50  ");
    moveY(50);
    avatar.setSpeechText("Y 50 -> 90  ");
    moveY(90);
  }
}

//----------------------------------------------------------------------
// MIDIポートアクセス関数定義
// MidiFunc.c/hから呼び出されるMIDI I/Fアクセス関数の実体を記述する。
// 以下ではハードウェアシリアルを使用した場合の例を記述。
#include "MidiPort.h"
int MidiPort_open() {
  Serial2.begin(D_MIDI_PORT_BPS);
  return (0);
}

void MidiPort_close() { Serial2.end(); }
int MidiPort_write(UCHAR data) {
#ifdef DUMPMIDI
  DPRINT("1:");
  int n = (int)data;
  DPRINTLN(n, HEX);
#else
  Serial2.write(data);
#endif
  return (1);
}

int MidiPort_writeBuffer(UCHAR *pData, ULONG Len) {
#ifdef DUMPMIDI
  int n;
  int i;
  DPRINT.print(Len);
  DPRINT.print(":");
  for (i = 0; i < Len; i++) {
    n = (int)pData[i];
    DPRINT.print(n, HEX);
  }
  DPRINTLN.println("");
#else
  Serial2.write(pData, Len);
#endif
  return (Len);
}
//----------------------------------------------------------------------
// SMFファイルアクセス関数定義
// SmfSeq.c/hから呼び出されるSMFファイルへのアクセス関数の実体を記述する。
// 以下ではSDカードシールドライブラリに対し、ファイルポインタで直接読み出し位置を指定する方法での例を記述。
// ライブラリ自体の初期化はsetup関数に記述している。
// #define D_SD_CHIP_SELECT_PIN   4
// #include <SPI.h>
// #include <SD.h>
#include "SmfFileAccess.h"

File s_FileHd;
bool SmfFileAccessOpen(UCHAR *Filename) {
  bool result = false;

  if (Filename != NULL) {
    s_FileHd = SD.open((const char *)Filename);

    result = s_FileHd.available();
  }
  return (result);
}

void SmfFileAccessClose() { s_FileHd.close(); }
bool SmfFileAccessRead(UCHAR *Buf, unsigned long Ptr) {
  bool result = true;
  if (Buf != NULL) {
    if (s_FileHd.position() != Ptr) {
      s_FileHd.seek(Ptr);
    }
    int data = s_FileHd.read();
    if (data >= 0) {
      *Buf = (UCHAR)data;
    } else {
      result = false;
    }
  }
  return (result);
}

bool SmfFileAccessReadNext(UCHAR *Buf) {
  bool result = true;
  if (Buf != NULL) {
    int data = s_FileHd.read();
    if (data >= 0) {
      *Buf = (UCHAR)data;
    } else {
      result = false;
    }
  }
  return (result);
}

int SmfFileAccessReadBuf(UCHAR *Buf, unsigned long Ptr, int Lng) {
  int result = 0;
  if (Buf != NULL) {
    if (s_FileHd.position() != Ptr) {
      s_FileHd.seek(Ptr);
    }

    int i;
    int data;
    for (i = 0; i < Lng; i++) {
      data = s_FileHd.read();
      if (data >= 0) {
        Buf[i] = (UCHAR)data;
        result++;
      } else {
        break;
      }
    }
  }
  return (result);
}

unsigned int SmfFileAccessSize() {
  unsigned int result = 0;
  result = s_FileHd.size();

  return (result);
}

inline void drawKey(int note, int ch, bool press) {
  // log_i("%d %d ", note, ch);

  if (ch == 0 && press == true) {
    avatar.setMouthOpenRatio(1.1);
  } else if (ch == 0 && press == false) {
    avatar.setMouthOpenRatio(0.0);
  }

  if (ch == 2 && press == true) {
    avatar.setMouthOpenRatio(0.7);
  } else if (ch == 2 && press == false) {
    avatar.setMouthOpenRatio(0.0);
  }
}

//----------------------------------------------------------------------
#define D_CH_OFFSET_PIN \
  3  // チャンネル番号オフセット（eVY1のGM音源としての演奏）
#define D_STATUS_LED 10  // 状態表示LED

int playdataCnt = 0;        // 選曲番号
#define D_PLAY_DATA_NUM 10  // SDカード格納ＳＭＦファイル数

IntervalCheck sButtonCheckInterval(100, true);
IntervalCheckMicros sTickProcInterval(ZTICK * 1000, true);
IntervalCheck sStatusLedCheckInterval(100, true);
unsigned int sLedPattern = 0x0f0f;
IntervalCheck sUpdateScreenInterval(500, true);

char filename[14] = {'/', 'p', 'l', 'a', 'y', 'd', 'a',
                     't', '0', '.', 'm', 'i', 'd', 0x00};

// SMFファイル名生成
//  playdat0.mid～playdat9.midの文字列を順次返す。
char *makeFilename() {
  int cnt = 0;
  //  char * filename = (char*)"playdat0.mid";
  for (cnt = 0; cnt < D_PLAY_DATA_NUM; cnt++) {
    filename[8] = 0x30 + playdataCnt;
    playdataCnt++;
    if (playdataCnt >= D_PLAY_DATA_NUM) {
      playdataCnt = 0;
    }
    if (SD.exists(filename) == true) {
      break;
    }
  }
  return (filename);
}

void midi_setup() {
  if (!SD.begin(TFCARD_CS_PIN, SPI, 80000000)) {
    DPRINTLN(F("Card failed, or not present"));
    // don't do anything more:
    delay(2000);
    return;
  }

  DPRINTLN(F("card initialized."));
  // すぐにファイルアクセスするとフォーマット破壊することがあったため待ち
  delay(2000);

  // コールバック関数を登録
  contents_run(drawKey);

  int Ret;
  pseqTbl = SmfSeqInit(ZTICK);
  if (pseqTbl == NULL) {
    delay(2000);
    return;
  }

  int chNoOffset = 0;

  // SMFファイル読込
  SmfSeqFileLoadWithChNoOffset(pseqTbl, (char *)makeFilename(), chNoOffset);
  // 発音中全キーノートオフ
  Ret = SmfSeqAllNoteOff(pseqTbl);
  // トラックテーブルリセット
  SmfSeqPlayResetTrkTbl(pseqTbl);
}

void setup() {
  auto cfg = M5.config();   // 設定用の情報を抽出
  cfg.output_power = true;  // Groveポートの出力をしない
  M5.begin(cfg);            // M5Stackをcfgの設定で初期化

  midi_setup();

#if defined(ARDUINO_M5STACK_CORES3)
  unifiedButton.begin(
      &M5.Display,　goblib::UnifiedButton::appearance_t::transparent_all);
#endif
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);
  M5_LOGI("Hello World");
  Serial.println("HelloWorldSerial");
  avatar.init();
#if defined(CORE_PORT_A)
  // M5Stack Fireの場合、Port.Aを使う場合は内部I2CをOffにする必要がある。
  avatar.setBatteryIcon(false);
  M5.In_I2C.release();
#else
  avatar.setBatteryIcon(true);
#endif
  if (servo_x.attach(SERVO_PIN_X, START_DEGREE_VALUE_X + servo_offset_x,
                     DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                     DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
    Serial.print("Error attaching servo x");
  }
  if (servo_y.attach(SERVO_PIN_Y, START_DEGREE_VALUE_Y + servo_offset_y,
                     DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                     DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
    Serial.print("Error attaching servo y");
  }
  servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
  servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);
  setSpeedForAllServos(60);
  last_mouth_millis = millis();

  xTaskCreatePinnedToCore(moveRandom, "servoTask", 4096, nullptr, 2,
                          &taskHandle, PRO_CPU_NUM);
}

int prePlayButtonStatus = HIGH;
int preFfButtonStatus = HIGH;
void midi_loop() {
  int Ret;

  // 定期起動処理
  if (sTickProcInterval.check() == true) {
    if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
      // 状態が演奏停止中以外の場合
      // 定期処理を実行
      Ret = SmfSeqTickProc(pseqTbl);
      // 処理が間に合わない場合のリカバリ
      while (sTickProcInterval.check() == true) {
        // 定期処理を実行
        Ret = SmfSeqTickProc(pseqTbl);
      }
      if (SmfSeqGetStatus(pseqTbl) == SMF_STAT_STOP) {
        // 状態が演奏停止中になった場合
        // 発音中全キーノートオフ
        Ret = SmfSeqAllNoteOff(pseqTbl);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        // ファイルクローズ
        SmfSeqEnd(pseqTbl);

        int chNoOffset = 0;

        pseqTbl = SmfSeqInit(ZTICK);
        // SMFファイル読込
        SmfSeqFileLoadWithChNoOffset(pseqTbl, (char *)makeFilename(),
                                     chNoOffset);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        // 演奏開始
        SmfSeqStart(pseqTbl);
      }
    }
  }

  // ボタン操作処理
  if (sButtonCheckInterval.check() == true) {
    M5.update();
    // スイッチ状態取得
    int buttonPlayStatus = M5.BtnB.wasPressed();
    if (prePlayButtonStatus != buttonPlayStatus) {
      // スイッチ状態が変化していた場合
      if (buttonPlayStatus == LOW) {
        // スイッチ状態がONの場合
        if (SmfSeqGetStatus(pseqTbl) == SMF_STAT_STOP) {
          // 演奏開始
          SmfSeqStart(pseqTbl);
        } else {
          // 演奏中なら演奏停止
          SmfSeqStop(pseqTbl);
          // 発音中全キーノートオフ
          Ret = SmfSeqAllNoteOff(pseqTbl);
        }
      }
    }
    // スイッチ状態保持
    prePlayButtonStatus = buttonPlayStatus;

    int buttonFfStatus = M5.BtnC.wasPressed();
    if (preFfButtonStatus != buttonFfStatus) {
      // スイッチ状態が変化していた場合
      if (preFfButtonStatus == LOW) {
        // スイッチ状態がONの場合
        bool playing = false;
        if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
          // 演奏中なら演奏停止
          SmfSeqStop(pseqTbl);
          // 発音中全キーノートオフ
          Ret = SmfSeqAllNoteOff(pseqTbl);
          // トラックテーブルリセット
          SmfSeqPlayResetTrkTbl(pseqTbl);
          // ファイルクローズ
          SmfSeqEnd(pseqTbl);
          playing = true;
        } else {
          playing = false;
        }

        int chNoOffset = 0;

        pseqTbl = SmfSeqInit(ZTICK);
        // SMFファイル読込
        SmfSeqFileLoadWithChNoOffset(pseqTbl, (char *)makeFilename(),
                                     chNoOffset);
        // 発音中全キーノートオフ
        Ret = SmfSeqAllNoteOff(pseqTbl);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        if (playing == true) {
          // 演奏開始
          SmfSeqStart(pseqTbl);
        }
      }
    }
    // スイッチ状態保持
    preFfButtonStatus = buttonFfStatus;
  }

  // 状態表示更新
  if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
    if (sStatusLedCheckInterval.check() == true) {
      unsigned int led = sLedPattern & 0x0001;
      if (led > 0) {
        // digitalWrite( D_STATUS_LED, HIGH );
      } else {
        // digitalWrite( D_STATUS_LED, LOW );
      }
      sLedPattern = (sLedPattern >> 1) | (led << 15);
    }
  } else {
    // digitalWrite( D_STATUS_LED, LOW );
  }
}

void loop() {
#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
  M5.update();
  if (M5.BtnA.pressedFor(2000)) {
    // サーボのオフセットを調整するモードへ
    adjustOffset();
  } else if (M5.BtnA.wasPressed()) {
    moveXY(90, 90);
  }

  if (M5.BtnB.wasSingleClicked()) {
    testServo();
  } else if (M5.BtnB.wasDoubleClicked()) {
    if (M5.Power.getExtOutput() == true) {
      M5.Power.setExtOutput(false);
      avatar.setSpeechText("ExtOutput Off");
    } else {
      M5.Power.setExtOutput(true);
      avatar.setSpeechText("ExtOutput On");
    }
    delay(2000);
    avatar.setSpeechText("");
  }
  if (M5.BtnC.pressedFor(5000)) {
    M5_LOGI("Will copy this sketch to filesystem");
    if (saveSketchToFS(SD, SDU_APP_PATH, TFCARD_CS_PIN)) {
      M5_LOGI("Copy Successful!");
    } else {
      M5_LOGI("Copy failed!");
    }
  } else if (M5.BtnC.wasPressed()) {
    // ランダムモードへ
    // moveRandom();
  }

  midi_loop();
  // delayを50msec程度入れないとCoreS3でバッテリーレベルと充電状態がおかしくなる。
  delay(0);
}
