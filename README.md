# MIDI stack-chan

スタックチャンのPWMサーボの調整及びテストを行うためのアプリケーションをベースに、MIDIシーケンサーを追加しました。
任意のSMFファイルを再生することができます。

https://x.com/riraosan_0901/status/1779121404706967791

## 対応キット

BOOTHで頒布している [ｽﾀｯｸﾁｬﾝ M5GoBottom版 組み立てキット](https://mongonta.booth.pm/)の動作確認及び組立時の設定用に開発しました。ピンの設定を変えることによりスタックチャン基板にも対応可能です。

## Synth Unit（MIDI音源ユニット）

[Synth Unit](https://docs.m5stack.com/ja/unit/Unit-Synth)はPort.Cに接続してください。ポートがない場合、M5BUSよりシリアル通信ポート(HardwareSerial2 port)を引き出して使用してください。その具体的な方法については割愛します。
SoftwareSerialライブラリを使用して任意のポートで通信可能だとは思いますが、未検証です。

## サーボのピンの設定

CoreS3はPort.C(G18, G17)、Core2はPort.C(G13,G14)、Fireは Port.A(G22,G21)、Core1は Port.C(G16,G17)を使うようになっています。違うピンを使用する場合は下記の箇所を書き換えてください。
https://github.com/mongonta0716/stack-chan-tester/blob/main/src/main.cpp#L7-L35

Basic, Fireでは、SPIポートとサーボポートが共有となってしまい、SDカードの読み込みが非常に不安定となります。
サーボ動作とともにMIDIファイルを再生したい場合は、Core2を使用してください（未確認TODO）

## サーボのオフセット調整

SG90系のPWMサーボは個体差が多く、90°を指定しても少しずれる場合があります。その場合は下記のオフセット値を調整してください。(90°からの角度（±）を設定します。)
調整する値は、ボタンAの長押しでオフセットを調整するモードに入るので真っ直ぐになる数値を調べてください。

https://github.com/mongonta0716/stack-chan-tester/blob/main/Stackchan_tester/Stackchan_tester.ino#L27-L28

## 使い方

SDカードにファイル名がplaydat0.mid～playdat9.midのSMFファイルを保存して、M5Stack本体に差し込んでください。
電源をONすると自動的に再生が始まります。

* ボタンA： X軸、Y軸のサーボを90°に回転します。固定前に90°にするときに使用してください。
* ボタンB： X軸は0〜180, Y軸は90〜50まで動きます。<br>ダブルクリックすると、Groveの5V(ExtPower)出力のON/OFFを切り替えます。Stack-chan_Takao_Baseの後ろから給電をチェックする場合OFFにします。
* ボタンC： ランダムで動きます。
  * ボタンC: ランダムモードの停止
* ボタンAの長押し：オフセットを調整して調べるモードに入ります。
  * ボタンA: オフセットを減らす
  * ボタンB: X軸とY軸の切り替え
  * ボタンC: オフセットを増やす
  * ボタンB長押し: 調整モードの終了

## CoreS3のボタン操作

CoreS3はCore2のBtnA、B、Cの部分がカメラやマイクに変わってしまったためボタンの扱いが変わりました。画面を縦に3分割して左：BtnA、中央：BtnB、右：BtnCとなっています。

## 必要なライブラリ（動作確認済バージョン）

> 最新の情報はplatformio.iniを確認してください。最新で動かない場合はライブラリのバージョンを合わせてみてください。

- [M5Unified](https://github.com/m5stack/M5Unified) v0.1.6で確認
- [M5Stack-Avatar](https://github.com/meganetaaan/m5stack-avatar) v0.8.2で確認。v0.7.4以前はM5Unifiedに対応していないのでビルド時にM5の二重定義エラーが出ます。
- [ServoEasing](https://github.com/ArminJo/ServoEasing) v3.1.0で確認
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) v0.13.0で確認。CoreS3はv0.13.0じゃないと動かない場合があります。

## ビルド方法

VSCodeのエクステンションであるPlatformIOでビルドしてください。

## ｽﾀｯｸﾁｬﾝについて

ｽﾀｯｸﾁｬﾝは[ししかわさん](https://github.com/meganetaaan)が公開しているオープンソースのプロジェクトです。

https://github.com/meganetaaan/stack-chan

## author

Takao Akaki

modified by riraosan

## LICENSE

MIT
