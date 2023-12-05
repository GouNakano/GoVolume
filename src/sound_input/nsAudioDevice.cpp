//---------------------------------------------------------------------------
#pragma hdrstop

#include <functional>
#include "nsDebug.h"
#include "MainFrm.h"
#include "nsAudioDevice.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

//----------------------------------------------------------
//コンストラクタ
//----------------------------------------------------------
nsAudioDevice::nsAudioDevice()
:INTERVAL(125) //監視時間
{
	//ハンドルをクリア
	hWIn = nullptr;
	//音声データ処理イベントタイマー削除
	DrawTimer = nullptr;
	//取得した音声データの更新フラグ
	isAudioUpdate = false;
	//音声データ取得イベントハンドラーの関数ポインタ
	FAudioDataEvent = nullptr;
	//音声入力のコールバック処理の可否
	isInputSound = false;
}
//----------------------------------------------------------
//デストラクタ
//----------------------------------------------------------
nsAudioDevice::~nsAudioDevice()
{
}
//----------------------------------------------------------
//初期化
//----------------------------------------------------------
bool nsAudioDevice::init()
{
	//初期化
	hWIn = nullptr;
	//バッファ確保
	wf.wFormatTag      = WAVE_FORMAT_PCM;
	wf.nChannels       = 2;
	wf.nSamplesPerSec  = 44100;
	wf.wBitsPerSample  = 16;
	wf.nBlockAlign     = ((wf.wBitsPerSample / 8) * wf.nChannels);
	wf.nAvgBytesPerSec = (wf.nSamplesPerSec * wf.nBlockAlign);
	wf.cbSize          = sizeof(wf);
	//録音中OFF
	IsRecording = false;
	//バッファのサイズ
	WaveBufferSize  = (INTERVAL * wf.nAvgBytesPerSec) / 1000;

	for(int nBlock = 0; nBlock < BLOCK_NUMS;nBlock++)
	{
		//波形オーディオ バッファーを識別するために使用されるヘッダー
		memset(&wh[nBlock],0,sizeof(wh[nBlock]));
		wh[nBlock].lpData          = (char *)lpWave[nBlock];
		wh[nBlock].dwBufferLength  = WaveBufferSize;
		wh[nBlock].dwBytesRecorded = 0;
		wh[nBlock].dwUser          = 0;
		wh[nBlock].dwFlags         = 0;
		wh[nBlock].dwLoops         = 1;
		wh[nBlock].lpNext          = nullptr;
		wh[nBlock].reserved        = 0;
	}

	//音声入力時のイベントポインタ
	FAudioDataEvent = nullptr;
	//音源タイプ(ステレオミキサー)
	soundSourceType = tssStereoMixer;
	//音声データ処理イベントタイマー
	DrawTimer = new TTimer(nullptr);
	DrawTimer->Enabled  = false;
	DrawTimer->Interval = INTERVAL/4;
	DrawTimer->OnTimer  = AudioDataEventTimer;

	return true;
}
//-----------------------------------
//最後のエラー文字列を得る(ループバック音源)
//-----------------------------------
std::string nsAudioDevice::get_last_error()
{
	return last_error;
}
//----------------------------------------------------------
//音声データ取得タイマーイベントハンドラー
//----------------------------------------------------------
void __fastcall nsAudioDevice::AudioDataEventTimer(TObject *Sender)
{
	//タイマーを止める
	DrawTimer->Enabled = false;
	//音声データの処理呼び出し
	try
	{
		//ロックする
		std::lock_guard<std::mutex> lock(callback_mtx_);
		//データが更新されているか
		if(isAudioUpdate == false)
		{
			//更新されていないので処理しない
			return;
		}
		//音声データ取得イベントハンドラー呼び出し
		if(FAudioDataEvent != nullptr)
		{
			int sample_num;
			int *lpData[2];
			//取得データを得る
			if(getAudioData(&sample_num,lpData) == true)
			{
				FAudioDataEvent(this,sample_num,lpData);
			}
		}
	}
	__finally
	{
		//取得した音声データの更新フラグOFF
		isAudioUpdate = false;
		//タイマーを再開
		DrawTimer->Enabled = true;
	}
}
//----------------------------------------------------------
//終了
//----------------------------------------------------------
bool nsAudioDevice::end()
{
	//音声入力のコールバック処理の可否
	isInputSound = false;
	//音声データ処理イベントタイマー終了
	DrawTimer->Enabled = false;
	DrawTimer->OnTimer = nullptr;
	//ロック解放待ち
	std::lock_guard<std::mutex> lock(callback_mtx_);
	//音声データ処理イベントタイマー削除
	delete DrawTimer;
	DrawTimer = nullptr;
	// waveIn 終了
	waveInStop(hWIn);
	for(int cnt=0; cnt< BLOCK_NUMS; cnt++)
	{
		wh[cnt].lpData = nullptr;
		waveInUnprepareHeader(hWIn,&wh[cnt], sizeof(WAVEHDR));
	}
	//音声入力を閉じる
	waveInReset(hWIn);
	waveInClose(hWIn);
	hWIn = nullptr;

	return true;
}
//----------------------------------------------------------
//音声入力開始
//----------------------------------------------------------
bool nsAudioDevice::startAudioInput()
{
	//波形オーディオ入力デバイスを開く
	MMRESULT hr = waveInOpen(&hWIn,WAVE_MAPPER,&wf,(DWORD_PTR)waveInProc,(DWORD_PTR)this,CALLBACK_FUNCTION);

	if(hr != MMSYSERR_NOERROR)
	{
		return false;
	}
	//波形オーディオ入力用のバッファー
	for(int Cnt = 0;Cnt < BLOCK_NUMS;Cnt++)
	{
		hr = waveInPrepareHeader(hWIn,&wh[Cnt],sizeof(WAVEHDR));
		hr = waveInAddBuffer    (hWIn,&wh[Cnt],sizeof(WAVEHDR));
	}
	//指定された波形オーディオ入力デバイスで入力を開始します
	hr = waveInStart(hWIn);
	//描画処理をリクエストする
	DrawTimer->Enabled = true;
	//音声入力のコールバック処理の可否
	isInputSound = true;
	//録音開始時間
	RecStsrtTime = GetTickCount();
	//最終コールバック内ポイント通過時間初期化
	last_thread_time = GetTickCount();

	return true;
}
//----------------------------------------------------------
//１秒当たりのサンプル数
//----------------------------------------------------------
long nsAudioDevice::getSamplesPerSec()
{
	return wf.nSamplesPerSec;
}
//----------------------------------------------------------
//１秒当たりのバイト数
//----------------------------------------------------------
long nsAudioDevice::getAvgBytesPerSec()
{
	return wf.nAvgBytesPerSec;
}
//----------------------------------------------------------
//バッファ長
//----------------------------------------------------------
long nsAudioDevice::getBufferLength()
{
	return wh[0].dwBufferLength;
}
//----------------------------------------------------------
//データ取得時のイベントハンドラセット
//----------------------------------------------------------
bool nsAudioDevice::setAudioDataEvent(TAudioDataEvent event)
{
	FAudioDataEvent = event;

	return true;
}
//----------------------------------------------------------
//ヘルスチェック
//----------------------------------------------------------
bool nsAudioDevice::healthCheck()
{
	//最終スレッド内ポイント通過時間と現在の時間の差をチェック
	DWORD now = GetTickCount();
	if(now - last_thread_time > INTERVAL*4)
	{
		return false;
	}
	return true;
}
//----------------------------------------------------------
//音源タイプ
//----------------------------------------------------------
typSoundSource nsAudioDevice::getSoundSourceType()
{
	return tssStereoMixer;
}
//----------------------------------------------------------
//音声入力のコールバック関数
//----------------------------------------------------------
void CALLBACK nsAudioDevice::waveInProc(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2)
{
	//コールバック元オブジェクトを得る
	nsAudioDevice *pAudioDevice = reinterpret_cast<nsAudioDevice *>(dwInstance);
	//最終コールバック内ポイント通過時間更新
	pAudioDevice->last_thread_time = GetTickCount();
	//音声入力のコールバック処理の可否
	if(pAudioDevice->isInputSound == false)
	{
		return;
	}

	switch(uMsg)
	{
		case WIM_OPEN:
		{
			//デバイスオープン
			break;
		}
		case WIM_CLOSE:
		{
			//デバイスクローズ
			break;
		}
		case WIM_DATA:
		{
			int call_idx;
			//ロックする
			std::lock_guard<std::mutex> lock(pAudioDevice->callback_mtx_);

			//コールバックした波形オーディオ バッファー
			WAVEHDR *whdr = (WAVEHDR *)dwParam1;
			//波形オーディオ バッファーのインデックスを得る
			if(whdr->lpData == (char *)pAudioDevice->lpWave[0])
			{
				call_idx = 0;
			}
			else
			{
				call_idx = 1;
			}
			//音声データ処理
			if((pAudioDevice->wh[call_idx].dwFlags & WHDR_DONE) == WHDR_DONE)
			{
				//音声データのオブジェクト作成
				pAudioDevice->unit.set(pAudioDevice->wh[call_idx].lpData,pAudioDevice->wh[call_idx].dwBytesRecorded);

				//録音中ならバッファに保存
				if(pAudioDevice->IsRecording == true)
				{
					pAudioDevice->g_data.push_back(pAudioDevice->unit);
				}
				//ステレオモノラル変換してバッファにセット
				pAudioDevice->sendUnit.NowActiveBufferSize = pAudioDevice->SetMonoBufferFromStereo(pAudioDevice->sendUnit.lpWork,(short *)(pAudioDevice->unit.ptr()),pAudioDevice->unit.size());
				//録音継続
				pAudioDevice->ContinueRecord(pAudioDevice->wh[call_idx]);
				//取得した音声データの更新フラグON
				pAudioDevice->isAudioUpdate = true;
			}

			break;
		}
		default:
		{
			break;
		}
	}
}
//-------------------------------------------------------------
//音声入力継続
//-------------------------------------------------------------
void nsAudioDevice::ContinueRecord(WAVEHDR& Now_wh)
{
	waveInUnprepareHeader(hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInPrepareHeader  (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInAddBuffer      (hWIn,&Now_wh,sizeof(WAVEHDR));
	//録音開始時間
	RecStsrtTime = ::GetTickCount();
}
//-------------------------------------------------------------
// ステレオデータを左右分離してモノラル変換してバッファにセット
//-------------------------------------------------------------
int nsAudioDevice::SetMonoBufferFromStereo(int lpWork[2][24000],short *lpData,int StereoBytesRecorded)
{
	//モノラル変換後のバッファサイズ
	int MonoBufSize = StereoBytesRecorded / (2 * sizeof(short));
	//左右の量子化値を格納
	for(int Cnt = 0;Cnt < MonoBufSize;Cnt++)
	{
		lpWork[0][Cnt]  = static_cast<int>(lpData[Cnt*2]);
		lpWork[1][Cnt]  = static_cast<int>(lpData[Cnt*2+1]);
	}
	return MonoBufSize;
}
//-------------------------------------------------------------
//取得データを得る
//-------------------------------------------------------------
bool nsAudioDevice::getAudioData(int *sample_num,int *lpData[2])
{
	*sample_num = sendUnit.NowActiveBufferSize;
	lpData[0]   = sendUnit.lpWork[0];
	lpData[1]   = sendUnit.lpWork[1];

	return true;
}
//-------------------------------------------------------------
//録音継続
//-------------------------------------------------------------
void nsAudioDevice::continueAudioInput(WAVEHDR& Now_wh)
{
	waveInUnprepareHeader(hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInPrepareHeader  (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInAddBuffer      (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInStart          (hWIn);
}
//-------------------------------------------------------------
//録音開始
//-------------------------------------------------------------
bool nsAudioDevice::startRecord()
{
	//録音バッファリスト消去
	g_data.clear();
	//録音中
	IsRecording = true;

	return true;
}
//-------------------------------------------------------------
//録音終了
//-------------------------------------------------------------
bool nsAudioDevice::endRecord()
{
	//録音を止める
	IsRecording = false;

	return true;
}
//-------------------------------------------------------------
//録音データ消去
//-------------------------------------------------------------
bool nsAudioDevice::clearRecord()
{
	//録音データ消去
	g_data.clear();

	return true;
}
//-------------------------------------------------------------
//  機能     ：Waveファイルに保存
//
//  関数定義 ：BOOL WriteWaveFile(LPTSTR lpszFileName)
//
//  アクセスレベル ：public
//
//  引数     ：
//
//  戻り値   ：
//
//  作成者　 ：
//
//  改定者   ：
//-------------------------------------------------------------
bool nsAudioDevice::writeWaveFile(wchar_t *fileName)
{
	HMMIO    hmmio;
	MMCKINFO mmckRiff;
	MMCKINFO mmckFmt;
	MMCKINFO mmckData;

	//ファイルオープン
	hmmio = mmioOpenW(fileName,nullptr,MMIO_CREATE | MMIO_WRITE);
	if(hmmio == nullptr)
	{
		return false;
	}

	mmckRiff.fccType = mmioStringToFOURCC(TEXT("WAVE"), 0);
	mmioCreateChunk(hmmio, &mmckRiff, MMIO_CREATERIFF);

	mmckFmt.ckid = mmioStringToFOURCC(TEXT("fmt "), 0);
	mmioCreateChunk(hmmio, &mmckFmt, 0);

	mmioWrite (hmmio, reinterpret_cast<char *>(&wf), sizeof(WAVEFORMATEX));
	mmioAscend(hmmio, &mmckFmt, 0);

	mmckData.ckid = mmioStringToFOURCC(TEXT("data"), 0);
	mmioCreateChunk(hmmio, &mmckData, 0);

	//データユニット毎に保存を行う
	for(int Cnt = 0;Cnt < g_data.size();Cnt++)
	{
		//ユニットを得る
		typDataUnit& unit = g_data[Cnt];
		//先頭アドレス
		const char *lpWaveData = unit.ptr();
		//ファイルへの書き込み
		mmioWrite(hmmio, (char *)lpWaveData,unit.size());
	}

	mmioAscend(hmmio, &mmckData, 0);
	mmioAscend(hmmio, &mmckRiff, 0);
	mmioClose(hmmio, 0);

	return true;
}

