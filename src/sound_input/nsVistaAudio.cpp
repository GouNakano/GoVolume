//---------------------------------------------------------------------------
#include <SysUtils.hpp>
#pragma hdrstop

#include "nsVistaAudio.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

	GUID IID_IAudioEndpointVolume_ = Sysutils::StringToGUID(L"{5CDF2C82-841E-4546-9722-0CF74078229A}");
	GUID IID_IAudioLoudness_       = Sysutils::StringToGUID(L"{7D8B1437-DD53-4350-9C1B-1EE2890BD938}");


void nsVistaAudio::Init()
{
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator    = __uuidof(IMMDeviceEnumerator);
	HRESULT hr;

	//CoCreateInstance �֐��́A�I�u�W�F�N�g���쐬���邽�߂̔ėp�I�ȃ��J�j�Y����񋟂��܂��B
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator,nullptr,CLSCTX_ALL, IID_IMMDeviceEnumerator,(void**)&Enumerator);
	if(hr != S_OK)
	{
		Enumerator = nullptr;
		return;
	}
	//GetDefaultAudioEndpoint ���\�b�h�́A�w�肳�ꂽ�f�[�^ �t���[�̕����ƃ��[���̊���̃I�[�f�B�I �G���h�|�C���g���擾���܂��B
	hr = Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &Endpoint);
	if(hr != S_OK)
	{
		Endpoint = nullptr;
		return;
	}
	clsid = IID_IAudioEndpointVolume_;

	hr = Endpoint->Activate(clsid, CLSCTX_ALL,nullptr,(void **)&AudioEndpointVolume);
	if(hr != S_OK)
	{
		AudioEndpointVolume = nullptr;
		return;
	}
}

double nsVistaAudio::GetVolume()
{
	float   Vol;
	HRESULT hr;

	hr = AudioEndpointVolume->GetMasterVolumeLevelScalar(&Vol);

	if(hr != S_OK)
	{
		return 65535;
	}
	return Vol;
}

bool nsVistaAudio::GetMute()
{
	int     Mute;
	HRESULT hr;

	hr = AudioEndpointVolume->GetMute(&Mute);

	if(hr != S_OK)
	{
		return true;
	}
	return (Mute != 0);
}


int nsVistaAudio::SetVolume(double Vol)
{
	int Result = 0;
	HRESULT hr;

	hr = AudioEndpointVolume->SetMasterVolumeLevelScalar(Vol,&GUID_NULL);

	if(hr != S_OK)
	{
		return -1;
	}
	return 0;
}

int nsVistaAudio::SetMute(bool Mute)
{
	HRESULT hr;

	hr = AudioEndpointVolume->SetMute(int(Mute),&GUID_NULL);

	if(hr != S_OK)
	{
		return -1;
	}
	return 0;
}

