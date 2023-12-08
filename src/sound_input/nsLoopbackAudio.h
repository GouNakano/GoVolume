//---------------------------------------------------------------------------
#ifndef nsLoopbackAudioH
#define nsLoopbackAudioH
//---------------------------------------------------------------------------
#include "GoVolumeDef.h"
#include "nsAudioDevice.h"

//
// オーディオデバイスクラス(ループバック)
//
class nsLoopbackAudio : public nsAudioDevice
{
private:
	WAVEFORMATEXTENSIBLE wf;
	IMMDeviceEnumerator *enumerator;
	IMMDevice           *device;
	IAudioClient        *audio_client;
	IAudioCaptureClient *capture_client;
	int                  sampling_rate;
	int                  num_channels;
	int                  bit_per_sample;
	UINT32               buffer_frame_count;
	//音声入力タイマーのエラーフラグ
	bool isSoundInputTimerError;
	//AudioClient稼働状態
	bool isAudioClientActive;
	//ウェーブフォーマット
	WAVEFORMATEXTENSIBLE wfex;
	//waveファイル作成用ウェーブフォーマット
	WAVEFORMATEX wf2;
	//録音結果の生成wavファイル名
	std::string wave_file;
	//エンドポイントバッファーのサイズ
	UINT32 bufferFrameCount;
	//監視時間までの必要なサンプル数
	int intervalSampleNumber;
	//音声入力タイマー
	TTimer *soundInputTimer;
	//音声入力タイマー内部処理終了フラグ
	bool isEndsoundInputTimer;

	CRITICAL_SECTION   critical_section;

private:
	//リソース解放
	bool release();
protected:
	//音声入力タイマーハンドラー
	void __fastcall SoundInputEventTimer(TObject *Sender);
public:
	//コンストラクタ
	nsLoopbackAudio();
	//デストラクタ
	virtual ~nsLoopbackAudio();
public:
	//初期化
	virtual bool init();
	//音声入力開始
	virtual bool startAudioInput();
	//音声入力終了
	virtual bool end();
	//録音開始
	virtual bool startRecord();
	//録音終了
	virtual bool endRecord();
	//Waveファイルに保存
	virtual bool writeWaveFile(wchar_t *fileName);
	//ヘルスチェック
	virtual bool healthCheck();
	//音源タイプ
	virtual typSoundSource getSoundSourceType();
};

#endif
