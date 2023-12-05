//---------------------------------------------------------------------------
#pragma hdrstop

#include <functional>
#include "nsDebug.h"
#include "MainFrm.h"
#include "nsAudioDevice.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

//----------------------------------------------------------
//�R���X�g���N�^
//----------------------------------------------------------
nsAudioDevice::nsAudioDevice()
:INTERVAL(125) //�Ď�����
{
	//�n���h�����N���A
	hWIn = nullptr;
	//�����f�[�^�����C�x���g�^�C�}�[�폜
	DrawTimer = nullptr;
	//�擾���������f�[�^�̍X�V�t���O
	isAudioUpdate = false;
	//�����f�[�^�擾�C�x���g�n���h���[�̊֐��|�C���^
	FAudioDataEvent = nullptr;
	//�������͂̃R�[���o�b�N�����̉�
	isInputSound = false;
}
//----------------------------------------------------------
//�f�X�g���N�^
//----------------------------------------------------------
nsAudioDevice::~nsAudioDevice()
{
}
//----------------------------------------------------------
//������
//----------------------------------------------------------
bool nsAudioDevice::init()
{
	//������
	hWIn = nullptr;
	//�o�b�t�@�m��
	wf.wFormatTag      = WAVE_FORMAT_PCM;
	wf.nChannels       = 2;
	wf.nSamplesPerSec  = 44100;
	wf.wBitsPerSample  = 16;
	wf.nBlockAlign     = ((wf.wBitsPerSample / 8) * wf.nChannels);
	wf.nAvgBytesPerSec = (wf.nSamplesPerSec * wf.nBlockAlign);
	wf.cbSize          = sizeof(wf);
	//�^����OFF
	IsRecording = false;
	//�o�b�t�@�̃T�C�Y
	WaveBufferSize  = (INTERVAL * wf.nAvgBytesPerSec) / 1000;

	for(int nBlock = 0; nBlock < BLOCK_NUMS;nBlock++)
	{
		//�g�`�I�[�f�B�I �o�b�t�@�[�����ʂ��邽�߂Ɏg�p�����w�b�_�[
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

	//�������͎��̃C�x���g�|�C���^
	FAudioDataEvent = nullptr;
	//�����^�C�v(�X�e���I�~�L�T�[)
	soundSourceType = tssStereoMixer;
	//�����f�[�^�����C�x���g�^�C�}�[
	DrawTimer = new TTimer(nullptr);
	DrawTimer->Enabled  = false;
	DrawTimer->Interval = INTERVAL/4;
	DrawTimer->OnTimer  = AudioDataEventTimer;

	return true;
}
//-----------------------------------
//�Ō�̃G���[������𓾂�(���[�v�o�b�N����)
//-----------------------------------
std::string nsAudioDevice::get_last_error()
{
	return last_error;
}
//----------------------------------------------------------
//�����f�[�^�擾�^�C�}�[�C�x���g�n���h���[
//----------------------------------------------------------
void __fastcall nsAudioDevice::AudioDataEventTimer(TObject *Sender)
{
	//�^�C�}�[���~�߂�
	DrawTimer->Enabled = false;
	//�����f�[�^�̏����Ăяo��
	try
	{
		//���b�N����
		std::lock_guard<std::mutex> lock(callback_mtx_);
		//�f�[�^���X�V����Ă��邩
		if(isAudioUpdate == false)
		{
			//�X�V����Ă��Ȃ��̂ŏ������Ȃ�
			return;
		}
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
	}
	__finally
	{
		//�擾���������f�[�^�̍X�V�t���OOFF
		isAudioUpdate = false;
		//�^�C�}�[���ĊJ
		DrawTimer->Enabled = true;
	}
}
//----------------------------------------------------------
//�I��
//----------------------------------------------------------
bool nsAudioDevice::end()
{
	//�������͂̃R�[���o�b�N�����̉�
	isInputSound = false;
	//�����f�[�^�����C�x���g�^�C�}�[�I��
	DrawTimer->Enabled = false;
	DrawTimer->OnTimer = nullptr;
	//���b�N����҂�
	std::lock_guard<std::mutex> lock(callback_mtx_);
	//�����f�[�^�����C�x���g�^�C�}�[�폜
	delete DrawTimer;
	DrawTimer = nullptr;
	// waveIn �I��
	waveInStop(hWIn);
	for(int cnt=0; cnt< BLOCK_NUMS; cnt++)
	{
		wh[cnt].lpData = nullptr;
		waveInUnprepareHeader(hWIn,&wh[cnt], sizeof(WAVEHDR));
	}
	//�������͂����
	waveInReset(hWIn);
	waveInClose(hWIn);
	hWIn = nullptr;

	return true;
}
//----------------------------------------------------------
//�������͊J�n
//----------------------------------------------------------
bool nsAudioDevice::startAudioInput()
{
	//�g�`�I�[�f�B�I���̓f�o�C�X���J��
	MMRESULT hr = waveInOpen(&hWIn,WAVE_MAPPER,&wf,(DWORD_PTR)waveInProc,(DWORD_PTR)this,CALLBACK_FUNCTION);

	if(hr != MMSYSERR_NOERROR)
	{
		return false;
	}
	//�g�`�I�[�f�B�I���͗p�̃o�b�t�@�[
	for(int Cnt = 0;Cnt < BLOCK_NUMS;Cnt++)
	{
		hr = waveInPrepareHeader(hWIn,&wh[Cnt],sizeof(WAVEHDR));
		hr = waveInAddBuffer    (hWIn,&wh[Cnt],sizeof(WAVEHDR));
	}
	//�w�肳�ꂽ�g�`�I�[�f�B�I���̓f�o�C�X�œ��͂��J�n���܂�
	hr = waveInStart(hWIn);
	//�`�揈�������N�G�X�g����
	DrawTimer->Enabled = true;
	//�������͂̃R�[���o�b�N�����̉�
	isInputSound = true;
	//�^���J�n����
	RecStsrtTime = GetTickCount();
	//�ŏI�R�[���o�b�N���|�C���g�ʉߎ��ԏ�����
	last_thread_time = GetTickCount();

	return true;
}
//----------------------------------------------------------
//�P�b������̃T���v����
//----------------------------------------------------------
long nsAudioDevice::getSamplesPerSec()
{
	return wf.nSamplesPerSec;
}
//----------------------------------------------------------
//�P�b������̃o�C�g��
//----------------------------------------------------------
long nsAudioDevice::getAvgBytesPerSec()
{
	return wf.nAvgBytesPerSec;
}
//----------------------------------------------------------
//�o�b�t�@��
//----------------------------------------------------------
long nsAudioDevice::getBufferLength()
{
	return wh[0].dwBufferLength;
}
//----------------------------------------------------------
//�f�[�^�擾���̃C�x���g�n���h���Z�b�g
//----------------------------------------------------------
bool nsAudioDevice::setAudioDataEvent(TAudioDataEvent event)
{
	FAudioDataEvent = event;

	return true;
}
//----------------------------------------------------------
//�w���X�`�F�b�N
//----------------------------------------------------------
bool nsAudioDevice::healthCheck()
{
	//�ŏI�X���b�h���|�C���g�ʉߎ��Ԃƌ��݂̎��Ԃ̍����`�F�b�N
	DWORD now = GetTickCount();
	if(now - last_thread_time > INTERVAL*4)
	{
		return false;
	}
	return true;
}
//----------------------------------------------------------
//�����^�C�v
//----------------------------------------------------------
typSoundSource nsAudioDevice::getSoundSourceType()
{
	return tssStereoMixer;
}
//----------------------------------------------------------
//�������͂̃R�[���o�b�N�֐�
//----------------------------------------------------------
void CALLBACK nsAudioDevice::waveInProc(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2)
{
	//�R�[���o�b�N���I�u�W�F�N�g�𓾂�
	nsAudioDevice *pAudioDevice = reinterpret_cast<nsAudioDevice *>(dwInstance);
	//�ŏI�R�[���o�b�N���|�C���g�ʉߎ��ԍX�V
	pAudioDevice->last_thread_time = GetTickCount();
	//�������͂̃R�[���o�b�N�����̉�
	if(pAudioDevice->isInputSound == false)
	{
		return;
	}

	switch(uMsg)
	{
		case WIM_OPEN:
		{
			//�f�o�C�X�I�[�v��
			break;
		}
		case WIM_CLOSE:
		{
			//�f�o�C�X�N���[�Y
			break;
		}
		case WIM_DATA:
		{
			int call_idx;
			//���b�N����
			std::lock_guard<std::mutex> lock(pAudioDevice->callback_mtx_);

			//�R�[���o�b�N�����g�`�I�[�f�B�I �o�b�t�@�[
			WAVEHDR *whdr = (WAVEHDR *)dwParam1;
			//�g�`�I�[�f�B�I �o�b�t�@�[�̃C���f�b�N�X�𓾂�
			if(whdr->lpData == (char *)pAudioDevice->lpWave[0])
			{
				call_idx = 0;
			}
			else
			{
				call_idx = 1;
			}
			//�����f�[�^����
			if((pAudioDevice->wh[call_idx].dwFlags & WHDR_DONE) == WHDR_DONE)
			{
				//�����f�[�^�̃I�u�W�F�N�g�쐬
				pAudioDevice->unit.set(pAudioDevice->wh[call_idx].lpData,pAudioDevice->wh[call_idx].dwBytesRecorded);

				//�^�����Ȃ�o�b�t�@�ɕۑ�
				if(pAudioDevice->IsRecording == true)
				{
					pAudioDevice->g_data.push_back(pAudioDevice->unit);
				}
				//�X�e���I���m�����ϊ����ăo�b�t�@�ɃZ�b�g
				pAudioDevice->sendUnit.NowActiveBufferSize = pAudioDevice->SetMonoBufferFromStereo(pAudioDevice->sendUnit.lpWork,(short *)(pAudioDevice->unit.ptr()),pAudioDevice->unit.size());
				//�^���p��
				pAudioDevice->ContinueRecord(pAudioDevice->wh[call_idx]);
				//�擾���������f�[�^�̍X�V�t���OON
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
//�������͌p��
//-------------------------------------------------------------
void nsAudioDevice::ContinueRecord(WAVEHDR& Now_wh)
{
	waveInUnprepareHeader(hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInPrepareHeader  (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInAddBuffer      (hWIn,&Now_wh,sizeof(WAVEHDR));
	//�^���J�n����
	RecStsrtTime = ::GetTickCount();
}
//-------------------------------------------------------------
// �X�e���I�f�[�^�����E�������ă��m�����ϊ����ăo�b�t�@�ɃZ�b�g
//-------------------------------------------------------------
int nsAudioDevice::SetMonoBufferFromStereo(int lpWork[2][24000],short *lpData,int StereoBytesRecorded)
{
	//���m�����ϊ���̃o�b�t�@�T�C�Y
	int MonoBufSize = StereoBytesRecorded / (2 * sizeof(short));
	//���E�̗ʎq���l���i�[
	for(int Cnt = 0;Cnt < MonoBufSize;Cnt++)
	{
		lpWork[0][Cnt]  = static_cast<int>(lpData[Cnt*2]);
		lpWork[1][Cnt]  = static_cast<int>(lpData[Cnt*2+1]);
	}
	return MonoBufSize;
}
//-------------------------------------------------------------
//�擾�f�[�^�𓾂�
//-------------------------------------------------------------
bool nsAudioDevice::getAudioData(int *sample_num,int *lpData[2])
{
	*sample_num = sendUnit.NowActiveBufferSize;
	lpData[0]   = sendUnit.lpWork[0];
	lpData[1]   = sendUnit.lpWork[1];

	return true;
}
//-------------------------------------------------------------
//�^���p��
//-------------------------------------------------------------
void nsAudioDevice::continueAudioInput(WAVEHDR& Now_wh)
{
	waveInUnprepareHeader(hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInPrepareHeader  (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInAddBuffer      (hWIn,&Now_wh,sizeof(WAVEHDR));
	waveInStart          (hWIn);
}
//-------------------------------------------------------------
//�^���J�n
//-------------------------------------------------------------
bool nsAudioDevice::startRecord()
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
bool nsAudioDevice::endRecord()
{
	//�^�����~�߂�
	IsRecording = false;

	return true;
}
//-------------------------------------------------------------
//�^���f�[�^����
//-------------------------------------------------------------
bool nsAudioDevice::clearRecord()
{
	//�^���f�[�^����
	g_data.clear();

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
bool nsAudioDevice::writeWaveFile(wchar_t *fileName)
{
	HMMIO    hmmio;
	MMCKINFO mmckRiff;
	MMCKINFO mmckFmt;
	MMCKINFO mmckData;

	//�t�@�C���I�[�v��
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

