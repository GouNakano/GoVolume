//---------------------------------------------------------------------------
#pragma hdrstop

#include "nsLoopbackAudio.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

//-------------------------------------------------------------
//�R���X�g���N�^
//-------------------------------------------------------------
nsLoopbackAudio::nsLoopbackAudio()
:nsAudioDevice()
{

}
//----------------------------------------------------------
//�f�X�g���N�^
//----------------------------------------------------------
__fastcall nsLoopbackAudio::~nsLoopbackAudio()
{
}
//----------------------------------------------------------
//������
//----------------------------------------------------------
bool nsLoopbackAudio::init()
{
	HRESULT hr;

	//�I�u�W�F�N�g�̃|�C���^������
	enumerator      = nullptr;
	device          = nullptr;
	audio_client    = nullptr;
	capture_client  = nullptr;
	soundInputTimer = nullptr;
	//enumerator��COM�I�u�W�F�N�g�̍쐬
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
	if(FAILED(hr))
	{
		last_error = "IMMDeviceEnumerator�̐����Ɏ��s���܂����B";
		return false;
	}
	//GetDefaultAudioEndpoint ���\�b�h�́A�w�肳�ꂽ�f�[�^ �t���[�̕����ƃ��[���̊���̃I�[�f�B�I �G���h�|�C���g���擾���܂��B
	hr = enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device);
	if(FAILED(hr))
	{
		release();
		last_error = "����̃I�[�f�B�I�G���h�|�C���g�̎擾�Ɏ��s���܂����B";
		return false;
	}
	hr = device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void **)&audio_client);
	if (FAILED(hr))
	{
		release();
		last_error = "�I�[�f�B�I�f�o�C�X���쓮�����鎖�Ɏ��s���܂����B";
		return false;
	}
	WAVEFORMATEX *mix_format = nullptr;
	hr = audio_client->GetMixFormat(&mix_format);
	if (FAILED(hr))
	{
		release();
		last_error = "�~�b�N�X�t�H�[�}�b�g�̎擾�Ɏ��s���܂����B";
		return false;
	}
	//�E�F�[�u�t�H�[�}�b�g���m��
	memcpy(&wf,mix_format,sizeof(wf));

	sampling_rate  = mix_format->nSamplesPerSec;
	num_channels   = mix_format->nChannels;
	bit_per_sample = mix_format->wBitsPerSample;
	//�Ď����Ԃ܂ł̕K�v�ȃT���v����
	intervalSampleNumber = (sampling_rate * num_channels * INTERVAL)/1000;
	//Initialize ���\�b�h�́A�I�[�f�B�I �X�g���[�������������܂��B
	hr = audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		INTERVAL * 2 * 10000,
		0,
		mix_format,
		nullptr
	);
	//GetBufferSize ���\�b�h�́A�G���h�|�C���g�o�b�t�@�[�̃T�C�Y (�ő�e��) ���擾���܂��B
	hr = audio_client->GetBufferSize(&bufferFrameCount);
	if(FAILED(hr))
	{
		release();
		last_error = "�G���h�|�C���g�o�b�t�@�[�̃T�C�Y�̎擾���s���܂����B";
		return false;
	}
	//CoTaskMemAlloc �֐��܂��� CoTaskMemRealloc �֐��̌Ăяo���ɂ���ĈȑO�Ɋ��蓖�Ă�ꂽ�^�X�N �������̃u���b�N��������܂��B
	CoTaskMemFree(mix_format);
	if(FAILED(hr))
	{
		release();
		last_error = "�I�[�f�B�I�N���C�A���g�̏������Ɏ��s���܂����B";
		return false;
	}
	//GetService ���\�b�h�́A�I�[�f�B�I �N���C�A���g �I�u�W�F�N�g����ǉ��̃T�[�r�X�ɃA�N�Z�X���܂��B
	hr = audio_client->GetService(IID_PPV_ARGS(&capture_client));
	if(FAILED(hr))
	{
		release();
		last_error = "�L���v�`���[�N���C�A���g�̎擾�Ɏ��s���܂����B";
		return false;
	}
	//�^����OFF
	IsRecording = false;
	//wave�t�@�C���쐬�p�E�F�[�u�t�H�[�}�b�g�ݒ�̎ʂ��𓾂�
	wf2.wFormatTag      = WAVE_FORMAT_PCM;
	wf2.nChannels       = wf.Format.nChannels;
	wf2.nSamplesPerSec  = wf.Format.nSamplesPerSec;
	wf2.wBitsPerSample  = 16;
	wf2.nBlockAlign     = ((wf2.wBitsPerSample / 8) * wf2.nChannels);
	wf2.nAvgBytesPerSec = (wf2.nSamplesPerSec * wf2.nBlockAlign);
	wf2.cbSize          = sizeof(WAVEFORMATEX);
	//�����^�C�v(���[�v�o�b�N)
	soundSourceType = tssLoopBack;
	//�������̓^�C�}�[�̃G���[�t���O
	isSoundInputTimerError = false;
	//�������̓^�C�}�[���������I���t���O
	isEndsoundInputTimer = false;
	//�������̓^�C�}�[
	soundInputTimer = new TTimer(nullptr);
	soundInputTimer->Enabled  = false;
	soundInputTimer->Interval = INTERVAL/2;
	soundInputTimer->OnTimer  = SoundInputEventTimer;

	return true;
}
//-----------------------------------
//�������͊J�n
//-----------------------------------
bool nsLoopbackAudio::startAudioInput()
{
	HRESULT hr;
	//�ғ����`�F�b�N
	if(isAudioClientActive == true)
	{
		//�ғ����Ȃ̂ŏ������Ȃ�
		release();
		last_error = "�^���͎��s���Ȃ̂ŁA�V���ɘ^�������s���鎖�͏o���܂���B";
		return false;
	}
	//Start ���\�b�h�́A�I�[�f�B�I �X�g���[�����J�n���܂��B
	hr = audio_client->Start();
	if(FAILED(hr))
	{
		release();
		last_error = "�^���͎��s���Ȃ̂ŁA�V���ɘ^�������s���鎖�͏o���܂���B";
		return false;
	}
	//AudioClient�ғ���ԃA�N�e�B�u
	isAudioClientActive = true;
	//�ŏI�X���b�h���|�C���g�ʉߎ���
	last_thread_time = GetTickCount();
	//�������͏��������N�G�X�g����
	soundInputTimer->Enabled = true;

	return true;
}
//-----------------------------------
//���\�[�X���
//-----------------------------------
bool nsLoopbackAudio::release()
{
	try
	{
		//���\�[�X���m�ۂ������Ԃ̋t���ŉ������
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
//�I��
//----------------------------------------------------------
bool nsLoopbackAudio::end()
{
	HRESULT hr;
	//��O����
	try
	{
		//�������̓^�C�}�[���~�߂�
		soundInputTimer->Enabled = false;
		//�������͏������~�߂�
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
		//�I�[�f�B�I �X�g���[�����~
		if(audio_client != nullptr)
		{
			//Stop ���\�b�h�́A�I�[�f�B�I �X�g���[�����~���܂��B
			hr = audio_client->Stop();
			audio_client = nullptr;
			//�`�F�b�N
			if(FAILED(hr))
			{
				last_error = "�I�[�f�B�I�N���C�A���g�̒�~�Ɏ��s���܂����B";
				return false;
			}
			//AudioClient�ғ���Ԕ�A�N�e�B�u
			isAudioClientActive = false;
		}
		//���\�[�X���
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
//�^���J�n
//-------------------------------------------------------------
bool nsLoopbackAudio::startRecord()
{
	//�^���o�b�t�@���X�g����
	g_data.clear();
	//�^����
	IsRecording = true;

	return true;
}
//-------------------------------------------------------------
//�^���I��
//-------------------------------------------------------------
bool nsLoopbackAudio::endRecord()
{
	//�^�����~�߂�
	IsRecording = false;

	return true;
}
//-------------------------------------------------------------
//  �@�\     �FWave�t�@�C���ɕۑ�
//
//  �֐���` �FBOOL WriteWaveFile(LPTSTR lpszFileName)
//
//  �A�N�Z�X���x�� �Fpublic
//
//  ����     �F
//
//  �߂�l   �F
//
//  �쐬�ҁ@ �F
//
//  �����   �F
//-------------------------------------------------------------
bool nsLoopbackAudio::writeWaveFile(wchar_t *fileName)
{
	HMMIO    hmmio;
	MMCKINFO mmckRiff;
	MMCKINFO mmckFmt;
	MMCKINFO mmckData;

	//wav�t�@�C���I�[�v��
	hmmio = mmioOpenW(fileName,nullptr,MMIO_CREATE | MMIO_WRITE);
	if(hmmio == nullptr)
	{
		return false;
	}

	//wav�t�@�C���w�b�_�[��������
	mmckRiff.fccType = mmioStringToFOURCC(TEXT("WAVE"), 0);
	mmioCreateChunk(hmmio, &mmckRiff, MMIO_CREATERIFF);

	mmckFmt.ckid = mmioStringToFOURCC(TEXT("fmt "), 0);
	mmioCreateChunk(hmmio, &mmckFmt, 0);
	mmioWrite(hmmio, (char *)&wf2,sizeof(WAVEFORMATEX));
	mmioAscend(hmmio, &mmckFmt, 0);

	mmckData.ckid = mmioStringToFOURCC(TEXT("data"), 0);
	mmioCreateChunk(hmmio, &mmckData, 0);

	//�f�[�^���j�b�g���ɕۑ����s��
	for(int Cnt = 0;Cnt < g_data.size();Cnt++)
	{
		//���j�b�g�𓾂�
		typDataUnit& unit = g_data[Cnt];
		//�擪�A�h���X
		const char *lpWaveData = unit.ptr();
		//�t�@�C���ւ̏�������
		mmioWrite(hmmio, (char *)lpWaveData,unit.size());
	}

	mmioAscend(hmmio, &mmckData, 0);
	mmioAscend(hmmio, &mmckRiff, 0);
	mmioClose(hmmio, 0);

	return true;
}
//----------------------------------------------------------
//�w���X�`�F�b�N
//----------------------------------------------------------
bool nsLoopbackAudio::healthCheck()
{
	//�G���[�t���O�`�F�b�N
	if(isSoundInputTimerError == true)
	{
		return false;
	}

	return true;
}
//----------------------------------------------------------
//�����^�C�v
//----------------------------------------------------------
typSoundSource nsLoopbackAudio::getSoundSourceType()
{
	return tssLoopBack;
}
//-----------------------------------
//�������̓^�C�}�[�n���h���[
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

	//�������̓^�C�}�[���~�߂�
	soundInputTimer->Enabled = false;
	//�������̓^�C�}�[�ŏ������邩
	if(isEndsoundInputTimer == true)
	{
		//�������̓^�C�}�[�ŏ������Ȃ�
		return;
	}
	//�p�P�b�g����
	try
	{
		try
		{
			//�p�P�b�g�T�C�Y
			UINT32 packetLength = 0;
			//�p�P�b�g���[�v�ɓ���O�̋x�~
			Sleep(10);
			//���v����wave_data������
			sum_wave_data.clear();
			//�p�P�b�g�T�C�Y0�̉�
			int packet0 = 0;
			//�K�v�ȃT���v�����𓾂�܂Ń��[�v
			while(sum_wave_data.size() < intervalSampleNumber)
			{
				//�������̓^�C�}�[���������I���t���O�`�F�b�N
				if(isEndsoundInputTimer == true)
				{
					//�I��
					isEndsoundInputTimer   = false;
					isSoundInputTimerError = false;
					return;
				}
				//�p�P�b�g�Ɏc�肪���镪�������[�v
				hr = capture_client->GetNextPacketSize(&packetLength);
				//�������̓G���[�`�F�b�N
				if(FAILED(hr))
				{
					//�������̓^�C�}�[�̃G���[�t���OON
					isSoundInputTimerError = true;
					return;
				}
				//�p�P�b�g�T�C�Y��0�Ȃ�0�Ŗ��߂Ď�荞�݊����Ƃ���
				if(packetLength == 0)
				{
					if(packet0 > 4)
					{
						//0�Ŗ��߂�
						sum_wave_data.resize(intervalSampleNumber);
						std::fill(sum_wave_data.begin(),sum_wave_data.end(),0);
						hr = capture_client->ReleaseBuffer(0);
						if(FAILED(hr))
						{
							isSoundInputTimerError = true;
							return;
						}
						//�������̓^�C�}�[�̃G���[�t���OOFF
						isSoundInputTimerError = false;
						break;
					}
					packet0++;
					Sleep(10);
					Application->ProcessMessages();
					continue;
				}

				//�p�P�b�g�Ń��[�v
				while(packetLength > 0)
				{
					//�p�P�b�g�T�C�Y0�̉񐔃��Z�b�g
					packet0 = 0;
					//�o�b�t�@�𓾂�
					fragment             = nullptr;
					num_frames_available = 0;
					flags                = 0;
					hr = capture_client->GetBuffer(&fragment,&num_frames_available,&flags,nullptr,nullptr);
					if(FAILED(hr))
					{
						//�^���X���b�h�̃G���[�t���OON
						isSoundInputTimerError = true;
						return;
					}
					//�p�P�b�g�T�C�Y�𓾂�
					int packet_size = (sizeof(BYTE) * wf.Format.wBitsPerSample * num_frames_available * num_channels)/8;
					//�p�P�b�g���������`�F�b�N����
					if((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
					{
						//�����Ȃ̂Ń[���Ŗ��߂�
						memset(fragment,0,packet_size);
					}
					//�p�P�b�g���̃f�[�^�́A�O�̃p�P�b�g�̃f�o�C�X�ʒu�Ƒ��ւ��ĂȂ����`�F�b�N
					else if((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0)
					{
						//�o�b�t�@���
						hr = capture_client->ReleaseBuffer(num_frames_available);
						continue;
					}
					//wave�f�[�^�̊m��
					if(wave_data.size() != num_frames_available*num_channels)
					{
						wave_data.resize(num_frames_available*num_channels);
					}

					//�r�b�g���ɂ��ꍇ����
					if(wf.Format.wBitsPerSample == 32)
					{
						//float��wave�f�[�^�̃A�h���X�𓾂�
						wave_float = reinterpret_cast<float *>(fragment);
						//float��wave�f�[�^��short�ɕϊ�����
						for(int cnt = 0;cnt < num_frames_available*num_channels;cnt++)
						{
							//�T���v���r�b�g�ɂ��؂�ւ�
							if(wf.Format.wBitsPerSample == 32)
							{
								try
								{
									//32�r�b�gfloat����ϊ�����16�r�b�g�͈̔�(16�r�b�g�ʎq��)�ŃZ�b�g
									dval = wave_float[cnt];
									dval *= 32768.0;
									sval  = static_cast<short>(dval);
									//���X�g�̍쐬
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
						//short��wave�f�[�^�̃A�h���X�𓾂�
						wave_short = reinterpret_cast<short *>(fragment);
						//wave�f�[�^�Z�b�g����
						for(int cnt = 0;cnt < num_frames_available*2;cnt++)
						{
							//16�r�b�gshort
							sval = wave_short[cnt];

							wave_data[cnt] = sval;
						}
					}
					else
					{
						//�������̓^�C�}�[�̃G���[�t���OON
						isSoundInputTimerError = true;
						return;
					}
					//�E�F�[�u���̃V�[�P���X�R���e�i�A��
					sum_wave_data.insert(sum_wave_data.end(),wave_data.begin(),wave_data.end());
					//ReleaseBuffer ���\�b�h�̓o�b�t�@�[��������܂��B
					hr = capture_client->ReleaseBuffer(num_frames_available);
					if(FAILED(hr))
					{
						isSoundInputTimerError = true;
						return;
					}
					//�p�P�b�g�Ɏc�肪���镪�������[�v
					capture_client->GetNextPacketSize(&packetLength);
				}
			}
			//�^�C�~���O����
			DWORD et = GetTickCount();
			DWORD rt = et - last_thread_time;
			if(rt < INTERVAL)
			{
				DWORD tm = INTERVAL - rt;
				Sleep(tm);
			}
			//�����f�[�^�̃I�u�W�F�N�g�쐬
			unit.set((char *)sum_wave_data.data(),sizeof(short)*sum_wave_data.size());
			//�^�����Ȃ�o�b�t�@�ɕۑ�
			if(IsRecording == true)
			{
				g_data.push_back(unit);
			}
			//�X�e���I���m�����ϊ����ăo�b�t�@�ɃZ�b�g
			sendUnit.NowActiveBufferSize = SetMonoBufferFromStereo(sendUnit.lpWork,(short *)(unit.ptr()),unit.size());
			//�����f�[�^�擾�C�x���g�n���h���[�Ăяo��
			if(FAudioDataEvent != nullptr)
			{
				int sample_num;
				int *lpData[2];
				//�擾�f�[�^�𓾂�
				if(getAudioData(&sample_num,lpData) == true)
				{
					FAudioDataEvent(this,sample_num,lpData);
				}
			}
			//�ŏI�X���b�h���|�C���g�ʉߎ���
			last_thread_time = GetTickCount();
		}
		catch(Exception& e)
		{
			//�������̓G���[
			isSoundInputTimerError = true;
			return;
		}
		//�������̓^�C�}�[���ĊJ
		if(soundInputTimer != nullptr)
		{
			soundInputTimer->Enabled = true;
		}
	}
	__finally
	{
		//�������̓^�C�}�[���������I���t���OOFF
		isEndsoundInputTimer = false;
	}
}

