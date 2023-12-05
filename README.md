# 状況に応じてAI対応でPCの音量を自動的に調整する　GoVolume
![aaaa_20231205_133250](https://github.com/GouNakano/GoVolume/assets/56259253/0be64c0c-1571-4d09-9a45-9acdec9506cf)
## 概要

## 開発環境
1. OS  
  Windows10,11  
2. コンパイラ及びIDE  
 C++Builder 10.3 Comminuty Edition  
3. 使用フレームワーク  
   Windows .NET FrameworkのWASAPI  
   C++Builder付属のVCL  
4. 使用言語  
   C++(17)  
   一部にC++Builder独自のC++拡張部分を含む
   
## 実行環境
1. OS  
  Windows10,11  
2. 他に必要なソフト  
   無し

## ソースコード
  ソースコードはsrc/sound_inputフォルダに、
  音源から音波波形情報を取り込むクラスだけを公開しています。  
  そのほか部分につきましては、非公開とさせていただきますのでご了承ください。
  
## 音源読み取りクラスの使い方
  src/sound_inputフォルダには、  
  音源に応じて２クラスを用意しています。  

  1. nsAudioDeviceクラス
     ステレオミキサーを音源として、波形情報を取得するクラス。
  2. nsLoopbackAudioクラス
    ループバック音源から波形情報を取得するクラス。

  src/sound_input以下にある４ファイルを同一フォルダに置いて下さい。  
  nsAudioDevice.cppとnsLoopbackAudio.cppをビルドの対象にして下さい。

  ヘッダーファイルは以下の様に読み込みます。
```C++:test.cpp
#include "nsLoopbackAudio.h"
```
  nsAudioDevice.hは上記コードで自動的に読み込まれるので、  
  #includeの記述は必要ありません。

  二つのクラスは多態(polymorphism)を用いているので、  
  クラスの生成時以外は同一のコードで記述出来ます。  
  
```C++:test.cpp
#include "nsLoopbackAudio.h"
```
  


## 操作方法

1. ダウンロード  
   git cloneやzip形式で本リポジトリをダウンロード出来ます。  
   zip形式でダウンロードした場合は全て展開して下さい。  
   実行に必要な部分だけ必要な場合は、  
   GoVolumeのbinフォルダの32bitまたは64bitだけを  
   ダウンロード後に別の場所にコピーして下さい。  
2. 起動  
   GoVolume.exe をダブルクリックして起動してください。

   使用方法などについては、下記のGovolumeのページを参照してください。

   http://www.sakura-densan.com/govolume/index.htm

## ループバック音源について(Version 0.48の新機能 2023/12)  
  ループバック音源は、PCから発する音をそのまま受け取ります。
  そのため、今まで必要だったステレオミキサーの設定が不要となります。
  ループバック音源から音声データの取得は、
  Windows .NET FrameworkのWASAPIを用いることで実現しています。

  ループバック音源への切り替えは、GoVolumeの画面上で右クリックして
  表示されるメニューから、「音源(U)-ループバック(L)」を選択します。
  これまでのステレオミキサーを音源を継続して使用される場合は、
  メニューから、「音源(U)-ステレオミキサー(M)」を選択します。 

![s2](https://github.com/GouNakano/GoVolume/assets/56259253/4fcf82c3-e3d2-496a-9a90-6c8d0733d11a)




   
