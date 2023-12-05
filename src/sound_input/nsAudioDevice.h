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
// �^���f�[�^���j�b�g
//
class typDataUnit
{
private:
	//�i�[�f�[�^
	char *data;
	//�f�[�^�̑傫��(Byte)
	DWORD DataBytes;
private:
	//�R�s�[
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
		//�R�s�[
		if(him.DataBytes > 0)
		{
			DataBytes = him.DataBytes;
			memcpy(data,him.data,DataBytes);
		}
	}
public:
	//�R���X�g���N�^
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
	//�R�s�[�R���X�g���N�^
	typDataUnit(const typDataUnit& him)
	:typDataUnit()
	{
		//�R�s�[
		_copy(him);
	}
	//�f�X�g���N�^
	~typDataUnit()
	{
		delete [] data;
	}
public:
	typDataUnit& operator = (const typDataUnit& him)
	{
		//�R�s�[
		_copy(him);

		return *this;
	}
public:
	//�T�C�Y�̎擾
	int size()
	{
		return DataBytes;
	}
	//�l�̐ݒ�
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
	//�i�[�f�[�^�̐擪�A�h���X�𓾂�
	const char *ptr()
	{
		return (char *)data;
	}
};

//
// �󂯓n���f�[�^���j�b�g
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
// �I�[�f�B�I�f�o�C�X�N���X(�X�e���I�~�L�T�[)
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
	//�X�e���I�~�L�T�[�n
	HWAVEIN hWIn;
	long    WaveBufferSize;
	short   lpWave[BLOCK_NUMS][24000];
	DWORD   RecStsrtTime;
	//�I�[�f�B�I �f�[�^�̌`��
	WAVEFORMATEX wf;
	//�g�`�I�[�f�B�I �o�b�t�@�[
	WAVEHDR      wh[BLOCK_NUMS];
	//�������͂̃R�[���o�b�N�����̉�
	bool isInputSound;
	//�����f�[�^�����C�x���g�^�C�}�[
	TTimer *DrawTimer;
	//�擾���������f�[�^�̍X�V�t���O
	bool isAudioUpdate;
	//�R�[���o�b�N���b�N
	std::mutex callback_mtx_;
protected:
	//����
	typSoundSource soundSourceType;
	//�ŏI�G���[������
	std::string last_error;
	// �󂯓n���f�[�^���j�b�g
	typSendDataUnit sendUnit;
	//�����f�[�^�擾�C�x���g�n���h���[�̊֐��|�C���^
	TAudioDataEvent FAudioDataEvent;
	// �^���f�[�^���j�b�g
	typDataUnit unit;
	//�^���o�b�t�@���X�g
	std::vector<typDataUnit> g_data;
	//�^�����H
	bool IsRecording;
	//�ŏI�X���b�h���|�C���g�ʉߎ���
	DWORD last_thread_time;
protected:
	//�^���p��
	void continueAudioInput(WAVEHDR& Now_wh);
	// �X�e���I�f�[�^�����E�������ă��m�����ϊ����ăo�b�t�@�ɃZ�b�g
	int SetMonoBufferFromStereo(int lpWork[2][24000],short *lpData,int StereoBytesRecorded);
	//�������͌p��
	void ContinueRecord(WAVEHDR& Now_wh);
protected:
	//�����f�[�^�擾�^�C�}�[�C�x���g�n���h���[
	virtual void __fastcall AudioDataEventTimer(TObject *Sender);
private:
	//�������͂̃R�[���o�b�N�֐�(�X�e���I�~�L�T�[)
	static void CALLBACK waveInProc(HWAVEIN hwi,UINT uMsg,DWORD_PTR dwInstance,DWORD_PTR dwParam1,DWORD_PTR dwParam2);
public:
	//�R���X�g���N�^
	nsAudioDevice();
	//�f�X�g���N�^
	virtual ~nsAudioDevice();
protected:
	//�擾�f�[�^�𓾂�
	virtual bool getAudioData(int *sample_num,int *lpData[2]);
public:
	//������
	virtual bool init();
	//�I��
	virtual bool end();
	//�^���J�n
	virtual bool startRecord();
	//�^���I��
	virtual bool endRecord();
	//�Ō�̃G���[�̃��b�Z�[�W��������擾����
	virtual std::string get_last_error();
	//�������͊J�n
	virtual bool startAudioInput();
	//�^���f�[�^����
	virtual bool clearRecord();
	//�f�[�^�擾���̃C�x���g�n���h���Z�b�g
	virtual bool setAudioDataEvent(TAudioDataEvent event);
	//Wave�t�@�C���ɕۑ�
	virtual bool writeWaveFile(wchar_t *fileName);
	//�w���X�`�F�b�N
	virtual bool healthCheck();
	//�����^�C�v
	virtual typSoundSource getSoundSourceType();
public:
	//�P�b������̃T���v����
	long getSamplesPerSec();
	//�P�b������̃o�C�g��
	long getAvgBytesPerSec();
	//�o�b�t�@��
	long getBufferLength();
};

#endif
