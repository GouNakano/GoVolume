//---------------------------------------------------------------------------
#pragma hdrstop

#include "nsLoopbackAudio.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

//-------------------------------------------------------------
//コンストラクタ
//-------------------------------------------------------------
nsLoopbackAudio::nsLoopbackAudio()
:nsAudioDevice()
{

}
//----------------------------------------------------------
//デストラクタ
//----------------------------------------------------------
__fastcall nsLoopbackAudio::~nsLoopbackAudio()
{
}
//----------------------------------------------------------
//初期化
//----------------------------------------------------------
bool nsLoopbackAudio::init()
{
	HRESULT hr;

	//オブジェクトのポインタ初期化
	enumerator      = nullptr;
	device          = nullptr;
	audio_client    = nullptr;
	capture_client  = nullptr;
	soundInputTimer = nullptr;
	//enumeratorのCOMオブジェクトの作成
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
	if(FAILED(hr))
	{
		last_error = "IMMDeviceEnumeratorの生成に失敗しました。";
		return false;
	}
	//GetDefaultAudioEndpoint メソッドは、指定されたデータ フローの方向とロールの既定のオーディオ エンドポイントを取得します。
	hr = enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device);
	if(FAILED(hr))
	{
		release();
		last_error = "既定のオーディオエンドポイントの取得に失敗しました。";
		return false;
	}
	hr = device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void **)&audio_client);
	if (FAILED(hr))
	{
		release();
		last_error = "オーディオデバイスを作動させる事に失敗しました。";
		return false;
	}
	WAVEFORMATEX *mix_format = nullptr;
	hr = audio_client->GetMixFormat(&mix_format);
	if (FAILED(hr))
	{
		release();
		last_error = "ミックスフォーマットの取得に失敗しました。";
		return false;
	}
	//ウェーブフォーマットを確保
	memcpy(&wf,mix_format,sizeof(wf));

	sampling_rate  = mix_format->nSamplesPerSec;
	num_channels   = mix_format->nChannels;
	bit_per_sample = mix_format->wBitsPerSample;
	//監視時間までの必要なサンプル数
	intervalSampleNumber = (sampling_rate * num_channels * INTERVAL)/1000;
	//Initialize メソッドは、オーディオ ストリームを初期化します。
	hr = audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		INTERVAL * 2 * 10000,
		0,
		mix_format,
		nullptr
	);
	//GetBufferSize メソッドは、エンドポイントバッファーのサイズ (最大容量) を取得します。
	hr = audio_client->GetBufferSize(&bufferFrameCount);
	if(FAILED(hr))
	{
		release();
		last_error = "エンドポイントバッファーのサイズの取得失敗しました。";
		return false;
	}
	//CoTaskMemAlloc 関数または CoTaskMemRealloc 関数の呼び出しによって以前に割り当てられたタスク メモリのブロックを解放します。
	CoTaskMemFree(mix_format);
	if(FAILED(hr))
	{
		release();
		last_error = "オーディオクライアントの初期化に失敗しました。";
		return false;
	}
	//GetService メソッドは、オーディオ クライアント オブジェクトから追加のサービスにアクセスします。
	hr = audio_client->GetService(IID_PPV_ARGS(&capture_client));
	if(FAILED(hr))
	{
		release();
		last_error = "キャプチャークライアントの取得に失敗しました。";
		return false;
	}
	//録音中OFF
	IsRecording = false;
	//waveファイル作成用ウェーブフォーマット設定の写しを得る
	wf2.wFormatTag      = WAVE_FORMAT_PCM;
	wf2.nChannels       = wf.Format.nChannels;
	wf2.nSamplesPerSec  = wf.Format.nSamplesPerSec;
	wf2.wBitsPerSample  = 16;
	wf2.nBlockAlign     = ((wf2.wBitsPerSample / 8) * wf2.nChannels);
	wf2.nAvgBytesPerSec = (wf2.nSamplesPerSec * wf2.nBlockAlign);
	wf2.cbSize          = sizeof(WAVEFORMATEX);
	//音源タイプ(ループバック)
	soundSourceType = tssLoopBack;
	//音声入力タイマーのエラーフラグ
	isSoundInputTimerError = false;
	//音声入力タイマー内部処理終了フラグ
	isEndsoundInputTimer = false;
	//音声入力タイマー
	soundInputTimer = new TTimer(nullptr);
	soundInputTimer->Enabled  = false;
	soundInputTimer->Interval = INTERVAL/2;
	soundInputTimer->OnTimer  = SoundInputEventTimer;

	return true;
}
//-----------------------------------
//音声入力開始
//-----------------------------------
bool nsLoopbackAudio::startAudioInput()
{
	HRESULT hr;
	//稼働中チェック
	if(isAudioClientActive == true)
	{
		//稼働中なので処理しない
		release();
		last_error = "録音は実行中なので、新たに録音を実行する事は出来ません。";
		return false;
	}
	//Start メソッドは、オーディオ ストリームを開始します。
	hr = audio_client->Start();
	if(FAILED(hr))
	{
		release();
		last_error = "録音は実行中なので、新たに録音を実行する事は出来ません。";
		return false;
	}
	//AudioClient稼働状態アクティブ
	isAudioClientActive = true;
	//最終スレッド内ポイント通過時間
	last_thread_time = GetTickCount();
	//音声入力処理をリクエストする
	soundInputTimer->Enabled = true;

	return true;
}
//-----------------------------------
//リソース解放
//-----------------------------------
bool nsLoopbackAudio::release()
{
	try
	{
		//リソースを確保した順番の逆順で解放する
		if(soundInputTimer != nullptr)
		{
			delete soundInputTimer;
			soundInputTimer = nullptr;
		}
		if(capture_client != nullptr)
		{
			capture_client->Release();
			capture_client = nullptr;
		}
		if(audio_client != nullptr)
		{
			audio_client->Release();
			audio_client = nullptr;
		}
		if(device != nullptr)
		{
			device->Release();
			device = nullptr;
		}
		if(enumerator != nullptr)
		{
			enumerator->Release();
			enumerator = nullptr;
		}
	}
	catch(Exception& e)
	{
		return false;
	}
	return true;
}
//----------------------------------------------------------
//終了
//----------------------------------------------------------
bool nsLoopbackAudio::end()
{
	HRESULT hr;
	//例外検査
	try
	{
		//音声入力タイマーを止める
		soundInputTimer->Enabled = false;
		//音声入力処理を止める
		isEndsoundInputTimer = true;
		for(int Cnt = 0;Cnt < 16;Cnt++)
		{
			if(isEndsoundInputTimer == false)
			{
				break;
			}
			Sleep(10);
			Application->ProcessMessages();
		}
		soundInputTimer->Enabled = false;
		//オーディオ ストリームを停止
		if(audio_client != nullptr)
		{
			//Stop メソッドは、オーディオ ストリームを停止します。
			hr = audio_client->Stop();
			audio_client = nullptr;
			//チェック
			if(FAILED(hr))
			{
				last_error = "オーディオクライアントの停止に失敗しました。";
				return false;
			}
			//AudioClient稼働状態非アクティブ
			isAudioClientActive = false;
		}
		//リソース解放
		if(release() == false)
		{
			return false;
		}
	}
	catch(Exception& e)
	{
		return false;
	}

	return true;
}
//-------------------------------------------------------------
//録音開始
//-------------------------------------------------------------
bool nsLoopbackAudio::startRecord()
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
bool nsLoopbackAudio::endRecord()
{
	//録音を止める
	IsRecording = false;

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
bool nsLoopbackAudio::writeWaveFile(wchar_t *fileName)
{
	HMMIO    hmmio;
	MMCKINFO mmckRiff;
	MMCKINFO mmckFmt;
	MMCKINFO mmckData;

	//wavファイルオープン
	hmmio = mmioOpenW(fileName,nullptr,MMIO_CREATE | MMIO_WRITE);
	if(hmmio == nullptr)
	{
		return false;
	}

	//wavファイルヘッダー書き込み
	mmckRiff.fccType = mmioStringToFOURCC(TEXT("WAVE"), 0);
	mmioCreateChunk(hmmio, &mmckRiff, MMIO_CREATERIFF);

	mmckFmt.ckid = mmioStringToFOURCC(TEXT("fmt "), 0);
	mmioCreateChunk(hmmio, &mmckFmt, 0);
	mmioWrite(hmmio, (char *)&wf2,sizeof(WAVEFORMATEX));
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
//----------------------------------------------------------
//ヘルスチェック
//----------------------------------------------------------
bool nsLoopbackAudio::healthCheck()
{
	//エラーフラグチェック
	if(isSoundInputTimerError == true)
	{
		return false;
	}

	return true;
}
//----------------------------------------------------------
//音源タイプ
//----------------------------------------------------------
typSoundSource nsLoopbackAudio::getSoundSourceType()
{
	return tssLoopBack;
}
//-----------------------------------
//音声入力タイマーハンドラー
//-----------------------------------
void __fastcall nsLoopbackAudio::SoundInputEventTimer(TObject *Sender)
{

	HRESULT            hr;
	BYTE              *fragment;
	UINT32             num_frames_available;
	DWORD              flags;
	std::vector<short> wave_data;
	std::vector<short> sum_wave_data;
	float              fval;
	double             dval;
	short              sval;
	float             *wave_float;
	short             *wave_short;

	//音声入力タイマーを止める
	soundInputTimer->Enabled = false;
	//音声入力タイマーで処理するか
	if(isEndsoundInputTimer == true)
	{
		//音声入力タイマーで処理しない
		return;
	}
	//パケット処理
	try
	{
		try
		{
			//パケットサイズ
			UINT32 packetLength = 0;
			//パケットループに入る前の休止
			Sleep(10);
			//合計したwave_data初期化
			sum_wave_data.clear();
			//パケットサイズ0の回数
			int packet0 = 0;
			//必要なサンプル数を得るまでループ
			while(sum_wave_data.size() < intervalSampleNumber)
			{
				//音声入力タイマー内部処理終了フラグチェック
				if(isEndsoundInputTimer == true)
				{
					//終了
					isEndsoundInputTimer   = false;
					isSoundInputTimerError = false;
					return;
				}
				//パケットに残りがある分だけループ
				hr = capture_client->GetNextPacketSize(&packetLength);
				//音声入力エラーチェック
				if(FAILED(hr))
				{
					//音声入力タイマーのエラーフラグON
					isSoundInputTimerError = true;
					return;
				}
				//パケットサイズが0なら0で埋めて取り込み完了とする
				if(packetLength == 0)
				{
					if(packet0 > 4)
					{
						//0で埋める
						sum_wave_data.resize(intervalSampleNumber);
						std::fill(sum_wave_data.begin(),sum_wave_data.end(),0);
						hr = capture_client->ReleaseBuffer(0);
						if(FAILED(hr))
						{
							isSoundInputTimerError = true;
							return;
						}
						//音声入力タイマーのエラーフラグOFF
						isSoundInputTimerError = false;
						break;
					}
					packet0++;
					Sleep(10);
					Application->ProcessMessages();
					continue;
				}

				//パケットでループ
				while(packetLength > 0)
				{
					//パケットサイズ0の回数リセット
					packet0 = 0;
					//バッファを得る
					fragment             = nullptr;
					num_frames_available = 0;
					flags                = 0;
					hr = capture_client->GetBuffer(&fragment,&num_frames_available,&flags,nullptr,nullptr);
					if(FAILED(hr))
					{
						//録音スレッドのエラーフラグON
						isSoundInputTimerError = true;
						return;
					}
					//パケットサイズを得る
					int packet_size = (sizeof(BYTE) * wf.Format.wBitsPerSample * num_frames_available * num_channels)/8;
					//パケットが無音かチェックする
					if((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
					{
						//無音なのでゼロで埋める
						memset(fragment,0,packet_size);
					}
					//パケット内のデータは、前のパケットのデバイス位置と相関してないかチェック
					else if((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0)
					{
						//バッファ解放
						hr = capture_client->ReleaseBuffer(num_frames_available);
						continue;
					}
					//waveデータの確保
					if(wave_data.size() != num_frames_available*num_channels)
					{
						wave_data.resize(num_frames_available*num_channels);
					}

					//ビット数による場合分け
					if(wf.Format.wBitsPerSample == 32)
					{
						//floatのwaveデータのアドレスを得る
						wave_float = reinterpret_cast<float *>(fragment);
						//floatのwaveデータをshortに変換する
						for(int cnt = 0;cnt < num_frames_available*num_channels;cnt++)
						{
							//サンプルビットによる切り替え
							if(wf.Format.wBitsPerSample == 32)
							{
								try
								{
									//32ビットfloatから変換して16ビットの範囲(16ビット量子化)でセット
									dval = wave_float[cnt];
									dval *= 32768.0;
									sval  = static_cast<short>(dval);
									//リストの作成
									wave_data[cnt] = sval;
								}
								catch(Exception& e)
								{
									if(cnt > 0)
									{
										wave_data[cnt] = wave_data[cnt-1];
									}
									else
									{
										wave_data[cnt] = 0;
									}
								}
							}
						}
					}
					else if(wf.Format.wBitsPerSample == 16)
					{
						//shortのwaveデータのアドレスを得る
						wave_short = reinterpret_cast<short *>(fragment);
						//waveデータセットする
						for(int cnt = 0;cnt < num_frames_available*2;cnt++)
						{
							//16ビットshort
							sval = wave_short[cnt];

							wave_data[cnt] = sval;
						}
					}
					else
					{
						//音声入力タイマーのエラーフラグON
						isSoundInputTimerError = true;
						return;
					}
					//ウェーブ情報のシーケンスコンテナ連結
					sum_wave_data.insert(sum_wave_data.end(),wave_data.begin(),wave_data.end());
					//ReleaseBuffer メソッドはバッファーを解放します。
					hr = capture_client->ReleaseBuffer(num_frames_available);
					if(FAILED(hr))
					{
						isSoundInputTimerError = true;
						return;
					}
					//パケットに残りがある分だけループ
					capture_client->GetNextPacketSize(&packetLength);
				}
			}
			//タイミング調整
			DWORD et = GetTickCount();
			DWORD rt = et - last_thread_time;
			if(rt < INTERVAL)
			{
				DWORD tm = INTERVAL - rt;
				Sleep(tm);
			}
			//音声データのオブジェクト作成
			unit.set((char *)sum_wave_data.data(),sizeof(short)*sum_wave_data.size());
			//録音中ならバッファに保存
			if(IsRecording == true)
			{
				g_data.push_back(unit);
			}
			//ステレオモノラル変換してバッファにセット
			sendUnit.NowActiveBufferSize = SetMonoBufferFromStereo(sendUnit.lpWork,(short *)(unit.ptr()),unit.size());
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
			//最終スレッド内ポイント通過時間
			last_thread_time = GetTickCount();
		}
		catch(Exception& e)
		{
			//音声入力エラー
			isSoundInputTimerError = true;
			return;
		}
		//音声入力タイマーを再開
		if(soundInputTimer != nullptr)
		{
			soundInputTimer->Enabled = true;
		}
	}
	__finally
	{
		//音声入力タイマー内部処理終了フラグOFF
		isEndsoundInputTimer = false;
	}
}

