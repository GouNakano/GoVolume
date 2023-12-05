//---------------------------------------------------------------------------
#ifndef nsAudioDeviceH
#define nsAudioDeviceH
#include <vcl.h>
#include <vector>
#include <map>
#include <thread>
#include <deque>
#include <mutex>
#include <mutex>
#include <windows.h>
#include <mmeapi.h>
#include <mmiscapi.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include "GoVolumeDef.h"
//---------------------------------------------------------------------------
//
// 録音データユニット
//
class typDataUnit
{
private:
	//格納データ
	char *data;
	//データの大きさ(Byte)
	DWORD DataBytes;
private:
	//コピー
	void _copy(const typDataUnit& him)
	{
		if(data != nullptr)
		{
			if(DataBytes != him.DataBytes)
			{
				delete [] data;
				data = nullptr;

				if(him.DataBytes > 0)
				{
					data = new char[him.DataBytes];
				}
			}
		}
		else
		{
			if(him.DataBytes > 0)
			{
				data = new char[him.DataBytes];
			}
		}
		//コピー
		if(him.DataBytes > 0)
		{
			DataBytes = him.DataBytes;
			memcpy(data,him.data,DataBytes);
		}
	}
public:
	//コンストラクタ
	typDataUnit()
	{
		data      = nullptr;
		DataBytes = 0;
	}

	typDataUnit(DWORD Bytes)
	:typDataUnit()
	{
		data      = new char[Bytes];
		DataBytes = Bytes;
		memset(data,0,DataBytes);
	}
	//コピーコンストラクタ
	typDataUnit(const typDataUnit& him)
	:typDataUnit()
	{
		//コピー
		_copy(him);
	}
	//デストラクタ
	~typDataUnit()
	{
		delete [] data;
	}
public:
	typDataUnit& operator = (const typDataUnit& him)
	{
		//コピー
		_copy(him);

		return *this;
	}
public:
	//サイズの取得
	int size()
	{
		return DataBytes;
	}
	//値の設定
	bool set(char *src,int sz)
	{
		if(sz != DataBytes)
		{
			if(data != nullptr)
			{
				delete [] data;
				data = nullptr;
				DataBytes = 0;
			}
		}
		if(sz > 0)
		{
			DataBytes = sz;

			if(data == nullptr)
			{
				data = new char[DataBytes];
			}
			memcpy(data,src,DataBytes);
		}
		return true;
	}
	//格納データの先頭アドレスを得る
	const char *ptr()
	{
		return (char *)data;
	}
};

//
// 受け渡しデータユニット
//
class typSendDataUnit
{
public:
	int NowActiveBufferSize;
	int lpWork[2][24000];
public:
	typSendDataUnit()
	{
		NowActiveBufferSize = 0;
		memset(lpWork,0,sizeof(lpWork));
	}
};


//
// オーディオデバイスクラス(ステレオミキサー)
//
class nsAudioDevice
{
public:
	typedef void (__closure *TAudioDataEvent)(nsAudioDevice *Sender,int sample_num,int *lpData[2]);
private:
	const static int BLOCK_NUMS = 2;
protected:
	const int INTERVAL;
private:
	//ステレオミキサー系
	HWAVEIN hWIn;
	long    WaveBufferSize;
	short   lpWave[BLOCK_NUMS][24000];
	DWORD   RecStsrtTime;
	//オーディオ データの形式
	WAVEFORMATEX wf;
	//波形オーディオ バッファー
	WAVEHDR      wh[BLOCK_NUMS];
	//音声入力のコールバック処理の可否
	bool isInputSound;
	//音声データ処理イベントタイマー
	TTimer *DrawTimer;
	//取得した音声データの更新フラグ
	bool isAudioUpdate;
	//コールバックロック
	std::mutex callback_mtx_;
protected:
	//共通
	typSoundSource soundSourceType;
	//最終エラー文字列
	std::string last_error;
	// 受け渡しデータユニット
	typSendDataUnit sendUnit;
	//音声データ取得イベントハンドラーの関数ポインタ
	TAudioDataEvent FAudioDataEvent;
	// 録音データユニット
	typDataUnit unit;
	//録音バッファリスト
	std::vector<typDataUnit> g_data;
	//録音中？
	bool IsRecording;
	//最終スレッド内ポイント通過時間
	DWORD last_thread_time;
protected:
	//録音継続
	void continueAudioInput(WAVEHDR& Now_wh);
	// ステレオデータを左右分離してモノラル変換してバッファにセット
	int SetMonoBufferFromStereo(int lpWork[2][24000],short *lpData,int StereoBytesRecorded);
	//音声入力継続
	void ContinueRecord(WAVEHDR& Now_wh);
protected:
	//音声データ取得タイマーイベントハンドラー
	virtual void __fastcall AudioDataEventTimer(TObject *Sender);
private:
	//音声入力のコールバック関数(ステレオミキサー)
	static void CALLBACK waveInProc(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2);
public:
	//コンストラクタ
	nsAudioDevice();
	//デストラクタ
	virtual ~nsAudioDevice();
protected:
	//取得データを得る
	virtual bool getAudioData(int *sample_num,int *lpData[2]);
public:
	//初期化
	virtual bool init();
	//終了
	virtual bool end();
	//録音開始
	virtual bool startRecord();
	//録音終了
	virtual bool endRecord();
	//最後のエラーのメッセージ文字列を取得する
	virtual std::string get_last_error();
	//音声入力開始
	virtual bool startAudioInput();
	//録音データ消去
	virtual bool clearRecord();
	//データ取得時のイベントハンドラセット
	virtual bool setAudioDataEvent(TAudioDataEvent event);
	//Waveファイルに保存
	virtual bool writeWaveFile(wchar_t *fileName);
	//ヘルスチェック
	virtual bool healthCheck();
	//音源タイプ
	virtual typSoundSource getSoundSourceType();
public:
	//１秒当たりのサンプル数
	long getSamplesPerSec();
	//１秒当たりのバイト数
	long getAvgBytesPerSec();
	//バッファ長
	long getBufferLength();
};

#endif
