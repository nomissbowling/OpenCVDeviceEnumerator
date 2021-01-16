#include <iomanip>
#include <iostream>

#include "DeviceEnumerator.h"

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

{
  IBaseFilter *pFilter = NULL;
  HRESULT hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
  if(SUCCEEDED(hr)){
    FILTER_INFO f;
    pFilter->QueryFilterInfo(&f);
    std::string s = ConvertWCSToMBS(f.achName, -1);
    std::cout << "Filter: " << std::hex << std::setw(16) << std::setfill('0') << (unsigned long long)pFilter << std::endl;
    std::cout << "Filter[" << s.c_str() << "]";
    if(f.pGraph) f.pGraph->Release();
    IEnumPins *pEnumPins = NULL;
    pFilter->EnumPins(&pEnumPins);
    IPin *pPin = NULL;
    while(pEnumPins->Next(1, &pPin, NULL) == S_OK){
      PIN_DIRECTION d;
      pPin->QueryDirection(&d); // 0: in, 1: out
      PIN_INFO p;
      pPin->QueryPinInfo(&p);
      std::string s = ConvertWCSToMBS(p.achName, -1);
      std::cout << " [" << d << "=" << p.dir << ":" << s.c_str() << "]";
      pPin->Release();
    }
    pEnumPins->Release();
    std::cout << std::endl;
    pFilter->Release();
  }
}

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
std::cerr << "Failed: Description or FirendlyName" << std::endl;
				VariantClear(&var);
				continue;
			}
			// Convert to string
			else {
				deviceName = ConvertBSTRToMBS(var.bstrVal);
			}

			VariantClear(&var); // We clean the variable in order to read the next value
std::cout << "FriendlyName: " << deviceName.c_str() << std::endl;

								// We try to read the DevicePath
			hr = pPropBag->Read(L"DevicePath", &var, 0);
			if (FAILED(hr)) {
std::cerr << "Failed: DevicePath" << std::endl;
				VariantClear(&var);
				continue; // If it fails we continue with next device
			}
			else {
				devicePath = ConvertBSTRToMBS(var.bstrVal);
			}
std::cout << "DevicePath: " << devicePath.c_str() << std::endl;

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
