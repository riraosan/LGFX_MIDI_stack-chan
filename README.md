# stack-chan-tester
スタックチャンを作成するときに、サーボのテストを行うためのアプリケーションです。<br>
stack-chan test application for pwm servo

※ ArduinoFrameworkのみです。

# 使い方
ボタンA： X軸、Y軸のサーボを90°に回転します。
ボタンB： X軸は0〜180, Y軸は90〜50まで動きます。
ボタンC： ランダムで動きます。

# 必要なライブラリ
- [M5Stack-Avatar](https://github.com/meganetaaan/m5stack-avatar)
- [ServoEasing](https://github.com/ArminJo/ServoEasing)
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo)

# ビルド方法
 下記のリンクを参照してArduinoIDEでビルドできます。
 [初心者向けM5Stack Core2の始め方（ArduinoIDE編）](https://raspberrypi.mongonta.com/howto-start-m5stack-core2arduinoide/)

# 書き込み後再起動を繰り返す場合
 2021/12現在、Arduino-esp32のバージョンをv2.0.0以上にすると、書き込み後再起動を繰り返すという現象が見られます。

## 対処①
 ボードマネージャーでarduino-esp32 ver.1.0.6に戻してビルドする。

## 対処②
 librariesの中にあるm5stack-avatar/src/Avatar.cppを修正してください。修正箇所は下記のリンクにあります。
 - [fix platformio link error #66](https://github.com/meganetaaan/m5stack-avatar/pull/66/commits/f28efa87d482a730237565a666d67d7422e638f4)

# author
 Takao Akaki

# LICENSE
 MIT