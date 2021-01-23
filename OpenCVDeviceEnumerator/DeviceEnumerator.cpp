#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <string>
#include <cstdio>
#include <cstdlib>

#include "DeviceEnumerator.h"

template <typename ... Args>
std::string fmt(const std::string &f, Args ... args)
{
  size_t l = snprintf(nullptr, 0, f.c_str(), args ...);
  std::vector<char> buf(l + 1);
  snprintf(&buf[0], l + 1, f.c_str(), args ...);
  return std::string(&buf[0], &buf[0] + l);
}

int DeviceEnumerator::dspConfig(IBaseFilter *pFilter)
{
  ICaptureGraphBuilder2 *pCapture = NULL;
  HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, 0, CLSCTX_INPROC,
    IID_ICaptureGraphBuilder2, (void **)&pCapture);
  if(SUCCEEDED(hr)){
    IAMStreamConfig *pConfig = NULL;
    hr = pCapture->FindInterface(&PIN_CATEGORY_CAPTURE, 0, pFilter,
      IID_IAMStreamConfig, (void **)&pConfig);
    if(SUCCEEDED(hr)){
      int cnt = 0, sz = 0;
      hr = pConfig->GetNumberOfCapabilities(&cnt, &sz);
      if(sz == sizeof(VIDEO_STREAM_CONFIG_CAPS)){
        for(int f = 0; f < cnt; ++f){
          VIDEO_STREAM_CONFIG_CAPS scc;
          AM_MEDIA_TYPE *pmt;
          hr = pConfig->GetStreamCaps(f, &pmt, (BYTE *)&scc);
          if(SUCCEEDED(hr)){
            if(pmt->majortype == MEDIATYPE_Video
#if 0
            && (pmt->subtype == MEDIASUBTYPE_RGB24
             || pmt->subtype == MEDIASUBTYPE_RGB32)
#endif
            && pmt->formattype == FORMAT_VideoInfo
            && pmt->cbFormat >= sizeof(VIDEOINFOHEADER)
            && pmt->pbFormat != NULL){
              VIDEOINFOHEADER *pVIH = (VIDEOINFOHEADER *)pmt->pbFormat;
              double ns = 100*1.0e-9;
              double frame = 1 / (ns * pVIH->AvgTimePerFrame);
              std::cout << fmt("%dx%d %7.2ffps\n",
                pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, frame);
#if 0
              pmt->pbFormat = (BYTE *)pVIH;
              hr = pConfig->SetFormat(pmt);
              if(FAILED(hr)){ std::cerr << "SetFormat failure" << std::endl; }
#endif
            }
          }
        }
      }
      pConfig->Release();
    }
    pCapture->Release();
  }
  return 0;
}

int DeviceEnumerator::dspNameAndPins(IMoniker *pMoniker)
{
  IBaseFilter *pFilter = NULL;
  HRESULT hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
  if(SUCCEEDED(hr)){
    FILTER_INFO f;
    pFilter->QueryFilterInfo(&f);
    std::cout << fmt("Filter: %016lx\n", (unsigned long long)pFilter);
    std::cout << fmt("Filter[%s]", toMBS(f.achName).c_str());
    if(f.pGraph) f.pGraph->Release();
    IEnumPins *pEnumPins = NULL;
    pFilter->EnumPins(&pEnumPins);
    IPin *pPin = NULL;
    while(pEnumPins->Next(1, &pPin, NULL) == S_OK){
      PIN_DIRECTION d;
      pPin->QueryDirection(&d); // 0: in, 1: out
      PIN_INFO p;
      pPin->QueryPinInfo(&p);
      std::cout << fmt(" [%d=%d:%s]", d, p.dir, toMBS(p.achName).c_str());
      pPin->Release();
    }
    pEnumPins->Release();
    std::cout << std::endl;
dspConfig(pFilter);
    pFilter->Release();
  }
  return 0;
}

std::map<int, Device> DeviceEnumerator::getVideoDevicesMap() {
	return getDevicesMap(CLSID_VideoInputDeviceCategory);
}

std::map<int, Device> DeviceEnumerator::getAudioDevicesMap() {
	return getDevicesMap(CLSID_AudioInputDeviceCategory);
}

// Returns a map of id and devices that can be used
std::map<int, Device> DeviceEnumerator::getDevicesMap(const GUID deviceClass)
{
	std::map<int, Device> deviceMap;

	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		return deviceMap; // Empty deviceMap as an error
	}

	// Create the System Device Enumerator
	ICreateDevEnum *pDevEnum;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

	// If succeeded, create an enumerator for the category
	IEnumMoniker *pEnum = NULL;
	if (SUCCEEDED(hr)) {
		hr = pDevEnum->CreateClassEnumerator(deviceClass, &pEnum, 0);
		if (hr == S_FALSE) {
			hr = VFW_E_NOT_FOUND;
		}
		pDevEnum->Release();
	}

	// Now we check if the enumerator creation succeeded
	int deviceId = -1;
	if (SUCCEEDED(hr)) {
		// Fill the map with id and friendly device name
		IMoniker *pMoniker = NULL;
		while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {

dspNameAndPins(pMoniker);

			IPropertyBag *pPropBag;
			HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
			if (FAILED(hr)) {
				pMoniker->Release();
				continue;
			}

			// Create variant to hold data
			VARIANT var;
			VariantInit(&var);

			std::string deviceName;
			std::string devicePath;

			// Read FriendlyName or Description
			hr = pPropBag->Read(L"Description", &var, 0); // Read description
			if (FAILED(hr)) {
				// If description fails, try with the friendly name
				hr = pPropBag->Read(L"FriendlyName", &var, 0);
			}
			// If still fails, continue with next device
			if (FAILED(hr)) {
std::cerr << fmt("Failed: Description or FirendlyName\n");
				VariantClear(&var);
				continue;
			}
			// Convert to string
			else {
				deviceName = ConvertBSTRToMBS(var.bstrVal);
			}

			VariantClear(&var); // We clean the variable in order to read the next value
std::cout << fmt("DeviceName: %s\n", deviceName.c_str());

								// We try to read the DevicePath
			hr = pPropBag->Read(L"DevicePath", &var, 0);
			if (FAILED(hr)) {
std::cerr << fmt("Failed: DevicePath\n");
				VariantClear(&var);
				continue; // If it fails we continue with next device
			}
			else {
				devicePath = ConvertBSTRToMBS(var.bstrVal);
			}
std::cout << fmt("DevicePath: %s\n", devicePath.c_str());

			// We populate the map
			deviceId++;
			Device currentDevice;
			currentDevice.id = deviceId;
			currentDevice.deviceName = deviceName;
			currentDevice.devicePath = devicePath;
			deviceMap[deviceId] = currentDevice;

		}
		pEnum->Release();
	}
	CoUninitialize();
	return deviceMap;
}

/*
This two methods were taken from
https://stackoverflow.com/questions/6284524/bstr-to-stdstring-stdwstring-and-vice-versa
*/

std::string DeviceEnumerator::ConvertBSTRToMBS(BSTR bstr)
{
	int wslen = ::SysStringLen(bstr);
	return ConvertWCSToMBS((wchar_t*)bstr, wslen);
}

std::string DeviceEnumerator::ConvertWCSToMBS(const wchar_t* pstr, long wslen)
{
	int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

	std::string dblstr(len, '\0');
	len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */,
		pstr, wslen /* not necessary NULL-terminated */,
		&dblstr[0], len,
		NULL, NULL /* no default char */);

	return dblstr;
}
