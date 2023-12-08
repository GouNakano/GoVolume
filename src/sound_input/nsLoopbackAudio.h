//---------------------------------------------------------------------------
#ifndef nsLoopbackAudioH
#define nsLoopbackAudioH
//---------------------------------------------------------------------------
#include "GoVolumeDef.h"
#include "nsAudioDevice.h"

//
// �I�[�f�B�I�f�o�C�X�N���X(���[�v�o�b�N)
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
	//�������̓^�C�}�[�̃G���[�t���O
	bool isSoundInputTimerError;
	//AudioClient�ғ����
	bool isAudioClientActive;
	//�E�F�[�u�t�H�[�}�b�g
	WAVEFORMATEXTENSIBLE wfex;
	//wave�t�@�C���쐬�p�E�F�[�u�t�H�[�}�b�g
	WAVEFORMATEX wf2;
	//�^�����ʂ̐���wav�t�@�C����
	std::string wave_file;
	//�G���h�|�C���g�o�b�t�@�[�̃T�C�Y
	UINT32 bufferFrameCount;
	//�Ď����Ԃ܂ł̕K�v�ȃT���v����
	int intervalSampleNumber;
	//�������̓^�C�}�[
	TTimer *soundInputTimer;
	//�������̓^�C�}�[���������I���t���O
	bool isEndsoundInputTimer;

	CRITICAL_SECTION   critical_section;

private:
	//���\�[�X���
	bool release();
protected:
	//�������̓^�C�}�[�n���h���[
	void __fastcall SoundInputEventTimer(TObject *Sender);
public:
	//�R���X�g���N�^
	nsLoopbackAudio();
	//�f�X�g���N�^
	virtual ~nsLoopbackAudio();
public:
	//������
	virtual bool init();
	//�������͊J�n
	virtual bool startAudioInput();
	//�������͏I��
	virtual bool end();
	//�^���J�n
	virtual bool startRecord();
	//�^���I��
	virtual bool endRecord();
	//Wave�t�@�C���ɕۑ�
	virtual bool writeWaveFile(wchar_t *fileName);
	//�w���X�`�F�b�N
	virtual bool healthCheck();
	//�����^�C�v
	virtual typSoundSource getSoundSourceType();
};

#endif
