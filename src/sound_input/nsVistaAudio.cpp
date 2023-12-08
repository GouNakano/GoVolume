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

	//CoCreateInstance 関数は、オブジェクトを作成するための汎用的なメカニズムを提供します。
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator,nullptr,CLSCTX_ALL, IID_IMMDeviceEnumerator,(void**)&Enumerator);
	if(hr != S_OK)
	{
		Enumerator = nullptr;
		return;
	}
	//GetDefaultAudioEndpoint メソッドは、指定されたデータ フローの方向とロールの既定のオーディオ エンドポイントを取得します。
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

