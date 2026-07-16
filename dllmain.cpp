#include "Hooks.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <d3dx9.h>
#include "ini.h"
#include "Events.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "Utils.h"
#include "ThreadWork.hpp"
#include "licenses.h"
#include "Logger.hpp"

#define VERSION "1.0"

/*
* TODO:
* [ ] - Add a clutch system for shifting
* [ ] - Proper gearbox support
* [ ] - Full wheel support(although game already has it, it does not work well for pedals without "combined" mode)
* [ ] - Compatability with Widescreen fix, and if possible, force our code to be the first one or last to execute(depends on code sections)
*/

#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#define MAKE_CALL(address, type, ...) ((type)(address))(__VA_ARGS__)
#define MAKE_VCALL(index, type, C, ...) ((type)(*(void***)(C))[index])(C, __VA_ARGS__)

#define SIZEOF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

// #define DO_DEBUG

std::vector<std::pair<Utils::String, float>> g_LatestLog;

bool g_bDebugMode = false;

void LogDebug(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	Logger::Get()->log("DEBUG", fmt, va);

	if (g_bDebugMode)
	{
		Utils::String str;
		str.formatV(fmt, va);

		g_LatestLog.push_back({ Utils::Format("[%-8s] %s", "DEBUG", str.c_str()), 5.f });
	}

	va_end(va);
}

void LogInfo(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	Logger::Get()->log("INFO", fmt, va);

	if (g_bDebugMode)
	{
		Utils::String str;
		str.formatV(fmt, va);

		g_LatestLog.push_back({ Utils::Format("[%-8s] %s", "INFO", str.c_str()), 5.f });
	}

	va_end(va);
}

void LogWarning(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	Logger::Get()->log("WARNING", fmt, va);

	if (g_bDebugMode)
	{
		Utils::String str;
		str.formatV(fmt, va);

		g_LatestLog.push_back({ Utils::Format("[%-8s] %s", "WARNING", str.c_str()), 5.f });
	}

	va_end(va);
}

void LogError(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	Logger::Get()->log("ERROR", fmt, va);
	
	if (g_bDebugMode)
	{
		Utils::String str;
		str.formatV(fmt, va);

		g_LatestLog.push_back({ Utils::Format("[%-8s] %s", "ERROR", str.c_str()), 5.f });
	}

	va_end(va);
}

void LogCritical(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	Logger::Get()->log("CRITICAL", fmt, va);

	if (g_bDebugMode)
	{
		Utils::String str;
		str.formatV(fmt, va);
		g_LatestLog.push_back({ Utils::Format("[%-8s] %s", "CRITICAL", str.c_str()), 5.f });
	}

	va_end(va);
}

LPDIRECTINPUTDEVICE8A* g_pInputDevices = (LPDIRECTINPUTDEVICE8A*)0xB1F57C;
LPDIRECTINPUT8A& g_pInput = *(LPDIRECTINPUT8A*)0xB1F5CC; // we'll use them for reference

IDirect3D9*& g_pD3D = *(IDirect3D9**)0xAB0AB8;
IDirect3DDevice9*& g_pDevice = *(IDirect3DDevice9**)0xAB0ABC;
HWND& g_hWnd = *(HWND*)0xAB0AD8;

class cConnectedDevice
{
	char* m_pProductName;
	LPDIRECTINPUTDEVICE8A m_pDevice;
	int m_nDeviceType;
public:
	cConnectedDevice(LPDIRECTINPUTDEVICE8A pDevice, int nDeviceType, const char* pProductName)
	{
		m_pDevice = pDevice;
		m_nDeviceType = nDeviceType;

		size_t nLen = strlen(pProductName);
		m_pProductName = new char[nLen + 1];
		strcpy_s(m_pProductName, nLen + 1, pProductName);
	}

	~cConnectedDevice()
	{
		delete[] m_pProductName;
	}

	void release()
	{
		if (m_pDevice)
		{
			m_pDevice->Release();
			m_pDevice = nullptr;
		}
	}

	LPDIRECTINPUTDEVICE8A getDevice() const { return m_pDevice; }
	const char* getProductName() const { return m_pProductName; }
	int getDeviceType() const { return m_nDeviceType; }
};

std::vector<cConnectedDevice*> g_ConnectedDevices;

BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
	if (!lpddi)
		return DIENUM_STOP;

	LPDIRECTINPUTDEVICE8A pDevice = nullptr;
	if (FAILED(g_pInput->CreateDevice(lpddi->guidInstance, &pDevice, nullptr)))
	{
		LogError("Failed to create device for %s", lpddi->tszProductName);
		return DIENUM_CONTINUE;
	}

	cConnectedDevice* pConnectedDevice = new cConnectedDevice(pDevice, lpddi->dwDevType, lpddi->tszProductName);

	g_ConnectedDevices.push_back(pConnectedDevice);

	return DIENUM_CONTINUE;
}

int* dword_B1F584 = (int*)0xB1F584;

enum GearMap // Map from the game itself
{
	GEAR_REVERSE = 0,
	GEAR_NEUTRAL = 1,
	GEAR_FIRST = 2,
	GEAR_SECOND = 3,
	GEAR_THIRD = 4,
	GEAR_FOURTH = 5,
	GEAR_FIFTH = 6,
	GEAR_SIXTH = 7,

	GEAR_MAX
};

enum ShifterPreference
{
	SHIFTER_MANUAL = 0,
	SHIFTER_SEQUENTIAL = 1,
	SHIFTER_AUTOMATIC = 2, // let's make a real automatic from this one, but only D/N/R

	SHIFTER_DO_NOT_USE = -1
};

const char* toString(enum GearPreference gearPreference)
{
	switch (gearPreference)
	{
		case SHIFTER_MANUAL:
			return "Manual";
		case SHIFTER_SEQUENTIAL:
			return "Sequential";
		case SHIFTER_AUTOMATIC:
			return "Automatic";
		case SHIFTER_DO_NOT_USE:
			return "None";
		default:
			return "Unknown";
	}

	return "Unknown";
}

const char* toString(enum GearMap gear)
{
	switch (gear)
	{
		case GEAR_REVERSE:
			return "Reverse";
		case GEAR_NEUTRAL:
			return "Neutral";
		case GEAR_FIRST:
			return "First";
		case GEAR_SECOND:
			return "Second";
		case GEAR_THIRD:
			return "Third";
		case GEAR_FOURTH:
			return "Fourth";
		case GEAR_FIFTH:
			return "Fifth";
		case GEAR_SIXTH:
			return "Sixth";
		default:
			return "Unknown";
	}

	return "Unknown"; // which hopefully won't happen
}

int g_ShifterPreference = SHIFTER_MANUAL;

int g_KeyBoundMap[GEAR_MAX] = { 0, 0, 0, 0, 0, 0, 0, 0 };

bool g_bRefreshRememberedDevice = false;
char* g_pRememberedShifterProdName = nullptr;

LPDIRECTINPUTDEVICE8A GetShifterDevice()
{
	if (!g_pRememberedShifterProdName) return nullptr;
	else // lazy initializing our last device
	{
		static LPDIRECTINPUTDEVICE8A pLastDevice = nullptr;
		static int initFlag = 0;

		if (g_bRefreshRememberedDevice)
		{
			initFlag &= ~1; // reset the initialized flag
			g_bRefreshRememberedDevice = false;
		}

		if (initFlag & 1)
			return pLastDevice;

		cConnectedDevice* pDevice = nullptr;

		for (cConnectedDevice* dev : g_ConnectedDevices)
		{
			if (strcmp(dev->getProductName(), g_pRememberedShifterProdName) == 0)
				pDevice = dev;
		}

		if (pDevice && !(initFlag & 1))
		{
			pLastDevice = pDevice->getDevice();
			pLastDevice->SetDataFormat(&c_dfDIJoystick2);
			pLastDevice->SetCooperativeLevel(g_hWnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
			initFlag |= 1;
		}
		else if (!pDevice)
		{
			static int msgCount = 0;

			if (msgCount == 0)
				LogError("Device %s was not found. Try reconnecting it and restarting the game. Or select another device!", g_pRememberedShifterProdName);

			msgCount = (msgCount + 1) % 600; // 10 seconds at 60fps, so we don't spam the log too much
		}
	}
	return nullptr;
}

SafeHook::MidAsmHook ShifterBehavior(0x428E65, [](SafeHook::CTX& ctx)
	{
		unsigned int _this = ctx.edi.i32;
		if (g_ShifterPreference == SHIFTER_DO_NOT_USE)
			return;

		LPDIRECTINPUTDEVICE8A pShifterDevice = GetShifterDevice();
		if (pShifterDevice)
		{
			DIJOYSTATE2 shifterState;
			pShifterDevice->Acquire();

			pShifterDevice->Poll();
			static int gear = GEAR_NEUTRAL; // Neutral gear
			if (HRESULT hr = pShifterDevice->GetDeviceState(sizeof(shifterState), &shifterState); hr == DI_OK)
			{
			pShifterDeviceGetDeviceStateSuccess:
				bool anyPressed = false;
				switch (g_ShifterPreference)
				{
					case SHIFTER_MANUAL:
					{
						for (int i = 0; i < GEAR_MAX; i++)
						{
							if (g_KeyBoundMap[i] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[i]] & 0x80))
							{
								gear = i;
								anyPressed = true;
								break;
							}
						}
						if (!anyPressed)
						{
							gear = GEAR_NEUTRAL; // Default to neutral if no button is pressed
						}
						break;
					}
					case SHIFTER_SEQUENTIAL: // we'll use 3 and 4 gears for our sequential shifter
					{
						static int lastState = 0;
						if (g_KeyBoundMap[GEAR_THIRD] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[GEAR_THIRD]] & 0x80))
						{
							if (!(lastState & 1))
							{
								if (gear < GEAR_SIXTH)
									gear++;

								lastState |= 1;
							}
						}
						else if (g_KeyBoundMap[GEAR_FOURTH] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[GEAR_FOURTH]] & 0x80))
						{
							if (!(lastState & 2))
							{
								if (gear > GEAR_REVERSE)
									gear--;

								lastState |= 2;
							}
						}
						else
						{
							lastState = 0; // reset last state if no button is pressed
						}
						break;
					}
					case SHIFTER_AUTOMATIC:
					{
						static int gearState = GEAR_NEUTRAL;
						unsigned int something = _this - 0x248;
						float maybeRPMRange = *(float*)(_this + 0x3C);

						if (g_KeyBoundMap[GEAR_REVERSE] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[GEAR_REVERSE]] & 0x80))
							gearState = GEAR_REVERSE;
						else if (g_KeyBoundMap[GEAR_NEUTRAL] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[GEAR_NEUTRAL]] & 0x80))
							gearState = GEAR_NEUTRAL;
						else if (g_KeyBoundMap[GEAR_FIRST] != -1 && (shifterState.rgbButtons[g_KeyBoundMap[GEAR_FIRST]] & 0x80))
							gearState = GEAR_FIRST;

						switch (gearState)
						{
							case GEAR_FIRST: // e.g. Drive mode
								MAKE_CALL(0x4193C0, void(__thiscall*)(unsigned int, float, float), _this - 0x294, maybeRPMRange, 1.0f);
								break;
							case GEAR_NEUTRAL:
								MAKE_CALL(0x419290, void(__thiscall*)(unsigned int, int), _this - 0x294, GEAR_NEUTRAL);
								break;
							case GEAR_REVERSE:
								MAKE_CALL(0x419290, void(__thiscall*)(unsigned int, int), _this - 0x294, GEAR_REVERSE);
								break;
							default:
								break;
						}

						MAKE_CALL(0x4191D0, void(__thiscall*)(unsigned int, float, float), _this - 0x294, maybeRPMRange, (float)(gearState - 1));
					}
					default:
						break;
				}
			}
			else if (hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST)
			{
				pShifterDevice->Acquire();
				pShifterDevice->Poll();

				if (HRESULT hr = pShifterDevice->GetDeviceState(sizeof(shifterState), &shifterState); hr == DI_OK)
					goto pShifterDeviceGetDeviceStateSuccess;
				else if (hr == DIERR_UNPLUGGED)
				{
					static int msgCount = 0;
					if (msgCount == 0)
						LogError("Device has been lost. Try replugging it.");

					msgCount = (msgCount + 1) % 600; // every 10 seconds
				}
			}

			if (g_ShifterPreference != SHIFTER_AUTOMATIC)
				MAKE_CALL(0x419290, void(__thiscall*)(unsigned int, int), _this - 0x294, gear); // AIVehicleHuman::SetGear
		}
	});

float g_WelcomeTimer = 5.f;
bool g_bMenuOpen = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC g_oWndProc = nullptr;

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) > 0L)
		return 1L;

	return CallWindowProc(g_oWndProc, hWnd, msg, wParam, lParam);
}

namespace Input
{
	unsigned char g_KeyState[256] = { 0 };

	class cKeyboard
	{
		unsigned int m_on[8];
		unsigned int m_trig[8];
		unsigned int m_rel[8];
		unsigned int m_old[8];

		unsigned int m_TimeSinceLastTouched = 0;
	public:
		cKeyboard()
		{
			memset(m_on, 0, sizeof(m_on));
			memset(m_trig, 0, sizeof(m_trig));
			memset(m_rel, 0, sizeof(m_rel));
			memset(m_old, 0, sizeof(m_old));
		}

		void Update()
		{
			bool touched = false;
			for (int i = 0; i < 8; i++)
			{
				m_old[i] = m_on[i];
				m_on[i] = 0;
				m_trig[i] = 0;
				m_rel[i] = 0;
			}

			for (int i = 0; i < sizeof(g_KeyState); i++)
			{
				if (g_KeyState[i] & 0x80)
				{
					m_on[i >> 5] |= (1 << (i & 0x1F));
					touched = true;
				}
				else m_on[i >> 5] &= ~(1 << (i & 0x1F));
			}

			for (int i = 0; i < 8; i++)
			{
				m_trig[i] = m_on[i] & ~m_old[i];
				m_rel[i] = ~m_on[i] & m_old[i];
			}

			if (touched)
				m_TimeSinceLastTouched = 0;
			else
				m_TimeSinceLastTouched++;
		}

		bool trig(unsigned int key) const
		{
			return (m_trig[key >> 5] & (1 << (key & 0x1F))) != 0;
		}

		bool rel(unsigned int key) const
		{
			return (m_rel[key >> 5] & (1 << (key & 0x1F))) != 0;
		}

		bool on(unsigned int key) const
		{
			return (m_on[key >> 5] & (1 << (key & 0x1F))) != 0;
		}
	};
}

Input::cKeyboard g_Keyboard;

bool g_bResetResources = false;

struct Link
{
	std::string m_Name;
	std::string m_URL;
};

std::vector<Link> g_ContactLinks;
std::vector<Link> g_DonationLinks;

void SaveConfig()
{
	IniReader ini("NFSCShifterSupport.ini");

	ini.WriteString("Settings", "ShifterPreference", toString((GearPreference)g_ShifterPreference));
	for (int i = 0; i < GEAR_MAX; i++)
	{
		const char* gearName = toString((GearMap)i);
		ini.WriteInt("ShifterButtons", gearName, g_KeyBoundMap[i]);
	}
	ini.WriteString("Settings", "ShifterProductName", g_pRememberedShifterProdName ? g_pRememberedShifterProdName : "");
}

void LoadConfig()
{
	IniReader ini("NFSCShifterSupport.ini");

	const char* shifterPreference = ini.ReadString("Settings", "ShifterPreference", "Manual").c_str();

	if (_stricmp(shifterPreference, "manual") == 0)
		g_ShifterPreference = SHIFTER_MANUAL;
	else if (_stricmp(shifterPreference, "automatic") == 0)
		g_ShifterPreference = SHIFTER_AUTOMATIC;
	else if (_stricmp(shifterPreference, "sequential") == 0)
		g_ShifterPreference = SHIFTER_SEQUENTIAL;
	else if (_stricmp(shifterPreference, "none") == 0)
		g_ShifterPreference = SHIFTER_DO_NOT_USE;
	else
		g_ShifterPreference = SHIFTER_MANUAL;

	for (int i = 0; i < GEAR_MAX; i++)
	{
		const char* gearName = toString((GearMap)i);
		g_KeyBoundMap[i] = ini.ReadInt("ShifterButtons", gearName, -1);
	}

	std::string rememberedShifterProdName = ini.ReadString("Settings", "ShifterProductName", "");
	if (!rememberedShifterProdName.empty())
	{
		g_pRememberedShifterProdName = new char[rememberedShifterProdName.length() + 1];
		strcpy_s(g_pRememberedShifterProdName, rememberedShifterProdName.length() + 1, rememberedShifterProdName.c_str());
	}
}

void DrawMenu()
{
	ImGui::Begin("Shifter Mod", nullptr, ImGuiWindowFlags_NoCollapse);
	if (ImGui::Button("Save Config"))
	{
		SaveConfig();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Config"))
	{
		LoadConfig();
	}
	ImGui::Separator();
	if (ImGui::BeginTabBar("ShifterModTabs"))
	{
		if (ImGui::BeginTabItem("Settings"))
		{
			ImGui::Text("Shifter Preference:");
			if (ImGui::RadioButton("Manual", g_ShifterPreference == SHIFTER_MANUAL))
				g_ShifterPreference = SHIFTER_MANUAL;
			if (ImGui::RadioButton("Sequential", g_ShifterPreference == SHIFTER_SEQUENTIAL))
				g_ShifterPreference = SHIFTER_SEQUENTIAL;
			if (ImGui::RadioButton("Automatic(broken)", g_ShifterPreference == SHIFTER_AUTOMATIC))
				g_ShifterPreference = SHIFTER_AUTOMATIC;
			if (ImGui::RadioButton("Do Not Use", g_ShifterPreference == SHIFTER_DO_NOT_USE))
				g_ShifterPreference = SHIFTER_DO_NOT_USE;
			ImGui::Separator();
			ImGui::Text("Key Bindings:");
			switch (g_ShifterPreference)
			{
				case SHIFTER_MANUAL:
				{
					for (int i = 0; i < GEAR_MAX; i++)
					{
						if (i == GEAR_NEUTRAL)
							continue;

						ImGui::PushID(i);

						char label[128] = { 0 };
						sprintf_s(label, "Gear %s", toString((GearMap)i));
						sprintf_s(label, "%s: Button %d", label, g_KeyBoundMap[i]);
						ImGui::Text("%s", label);
						ImGui::SameLine();
						if (ImGui::Button("Rebind"))
							ImGui::OpenPopup("RebindPopup");

						if (ImGui::BeginPopup("RebindPopup"))
						{
							ImGui::Text("Press a button on your shifter to bind it to Gear %s", toString((GearMap)i));
							LPDIRECTINPUTDEVICE8A pShifterDevice = GetShifterDevice();
							if (pShifterDevice)
							{
								if (HRESULT hr = pShifterDevice->Acquire(); hr != DI_OK)
									pShifterDevice->Acquire();

								pShifterDevice->Poll();
								DIJOYSTATE2 shifterState{ 0 };
								if (HRESULT hr = pShifterDevice->GetDeviceState(sizeof(DIJOYSTATE2), &shifterState); hr == DI_OK)
								{
									for (int j = 0; j < 128; j++)
									{
										if (shifterState.rgbButtons[j] & 0x80)
										{
											g_KeyBoundMap[i] = j;
											ImGui::CloseCurrentPopup();
											break;
										}
									}
								}
								else
								{
									ImGui::Text("Failed to get shifter state. Error: 0x%08X", hr);
								}
							}
							else
							{
								ImGui::TextDisabled("No shifter device detected. Please select and select your shifter.");
							}
							ImGui::EndPopup();
						}

						ImGui::PopID();
					}
					break;
				}
				case SHIFTER_SEQUENTIAL:
				{
					for (int i = GEAR_THIRD; i <= GEAR_FOURTH; i++)
					{
						ImGui::PushID(i);
						char label[128] = { 0 };

						sprintf_s(label, "Gear %s", i == GEAR_THIRD ? "Up" : "Down");

						ImGui::Text("%s: Button %d", label, g_KeyBoundMap[i]);
						ImGui::SameLine();
						if (ImGui::Button("Rebind"))
							ImGui::OpenPopup("RebindPopup");

						if (ImGui::BeginPopup("RebindPopup"))
						{
							ImGui::Text("Press a button on your shifter to bind it to Gear %s", i == GEAR_THIRD ? "Up" : "Down");
							LPDIRECTINPUTDEVICE8A pShifterDevice = GetShifterDevice();
							if (pShifterDevice)
							{
								if (HRESULT hr = pShifterDevice->Acquire(); hr != DI_OK)
									pShifterDevice->Acquire();
								pShifterDevice->Poll();
								DIJOYSTATE2 shifterState{ 0 };
								if (HRESULT hr = pShifterDevice->GetDeviceState(sizeof(DIJOYSTATE2), &shifterState); hr == DI_OK)
								{
									for (int j = 0; j < 128; j++)
									{
										if (shifterState.rgbButtons[j] & 0x80)
										{
											g_KeyBoundMap[i] = j;
											ImGui::CloseCurrentPopup();
											break;
										}
									}
								}
								else
								{
									ImGui::Text("Failed to get shifter state. Error: 0x%08X", hr);
								}
							}
							else
							{
								ImGui::TextDisabled("No shifter device detected. Please select and select your shifter.");
							}
							ImGui::EndPopup();
						}

						ImGui::PopID();
					}
					break;
				}
				case SHIFTER_AUTOMATIC:
				{
					for (int i = GEAR_REVERSE; i <= GEAR_FIRST; i++)
					{
						/*
							if (i == GEAR_NEUTRAL)
								continue;

							even if there are automatic shifters, they definitely should have neutral gear
						*/

						ImGui::PushID(i);
						
						char label[128] = { 0 };
						if (i == GEAR_REVERSE)
							sprintf_s(label, "Gear Reverse");
						else if (i == GEAR_NEUTRAL)
							sprintf_s(label, "Gear Neutral");
						else
							sprintf_s(label, "Gear Drive");

						ImGui::Text("%s: Button %d", label, g_KeyBoundMap[i]);
						ImGui::SameLine();
						if (ImGui::Button("Rebind"))
							ImGui::OpenPopup("RebindPopup");

						if (ImGui::BeginPopup("RebindPopup"))
						{
							ImGui::Text("Press a button on your shifter to bind it to %s", label);
							LPDIRECTINPUTDEVICE8A pShifterDevice = GetShifterDevice();
							if (pShifterDevice)
							{
								if (HRESULT hr = pShifterDevice->Acquire(); hr != DI_OK)
									pShifterDevice->Acquire();
								pShifterDevice->Poll();
								DIJOYSTATE2 shifterState{ 0 };
								if (HRESULT hr = pShifterDevice->GetDeviceState(sizeof(DIJOYSTATE2), &shifterState); hr == DI_OK)
								{
									for (int j = 0; j < 128; j++)
									{
										if (shifterState.rgbButtons[j] & 0x80)
										{
											g_KeyBoundMap[i] = j;
											ImGui::CloseCurrentPopup();
											break;
										}
									}
								}
								else
								{
									ImGui::Text("Failed to get shifter state. Error: 0x%08X", hr);
								}
							}
							else
							{
								ImGui::TextDisabled("No shifter device detected. Please select and select your shifter.");
							}

							ImGui::EndPopup();
						}

						ImGui::PopID();
					}
					break;
				}
				case SHIFTER_DO_NOT_USE:
				{
					ImGui::Text("Shifter is disabled. No key bindings available.");
					break;
				}
			}
			ImGui::Separator();
			ImGui::Text("Device Selection:");
			if (ImGui::BeginChild("##DeviceList", ImVec2(0, 200), ImGuiChildFlags_AutoResizeX))
			{
				if (ImGui::Button("Refresh Device List"))
				{
					for (cConnectedDevice* device : g_ConnectedDevices)
					{
						device->release();

						delete device;
					}

					g_ConnectedDevices.clear();
					g_bRefreshRememberedDevice = true;
					g_pInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, nullptr, DIEDFL_ATTACHEDONLY);
				}

				for (cConnectedDevice* dev : g_ConnectedDevices)
				{
					bool valid = true;
					{
						LPDIRECTINPUTDEVICE8A pDevice = dev->getDevice();

						if (pDevice->Acquire() == DIERR_NOTACQUIRED)
						{
							pDevice->Acquire();
						}

						DIJOYSTATE2 dummy{ 0 };
						if (HRESULT hr = pDevice->GetDeviceState(sizeof(dummy), &dummy); hr == DIERR_UNPLUGGED || hr == DIERR_INPUTLOST)
							valid = false;
					}

					ImGui::BeginDisabled(!valid);

					if (ImGui::Selectable(dev->getProductName(), g_pRememberedShifterProdName && strcmp(g_pRememberedShifterProdName, dev->getProductName()) == 0))
					{
						if (g_pRememberedShifterProdName)
							delete[] g_pRememberedShifterProdName;

						g_pRememberedShifterProdName = new char[strlen(dev->getProductName()) + 1];
						strcpy_s(g_pRememberedShifterProdName, strlen(dev->getProductName()) + 1, dev->getProductName());
						g_bRefreshRememberedDevice = true;
					}

					ImGui::EndDisabled();
				}
				ImGui::EndChild();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("About"))
		{
			ImGui::Text("NFSC Shifter Support " VERSION);
			ImGui::Text("Developed by: Frouk");
			ImGui::TextLinkOpenURL("Project is open source and is available on GitHub.(press this text to open repository)", "https://github.com/Frouk3/NFSCShifterSupport");

			ImGui::SeparatorText("Social/Contact");

			int sameLineCounter = 0;
			if (g_ContactLinks.empty())
			{
				ImGui::TextDisabled("Not available.");
			}
			else
			{
				for (const auto& link : g_ContactLinks)
				{
					if (ImGui::Button(link.m_Name.c_str()))
						ThreadWork::AddThread(new cThread([](cThread* thread, LPVOID param)
														  {
															  const char* url = (const char*)param;

															  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
														  }, (void*)link.m_URL.c_str()));

					if (sameLineCounter < (int)(g_ContactLinks.size() - 1))
					{
						ImGui::SameLine();
						sameLineCounter++;
					}
					else
					{
						sameLineCounter = 0;
					}
				}
			}

			ImGui::SeparatorText("Donations");
			sameLineCounter = 0;
			if (g_DonationLinks.empty())
			{
				ImGui::TextDisabled("Not available.");
			}
			else
			{
				for (const auto& link : g_DonationLinks)
				{
					if (ImGui::Button(link.m_Name.c_str()))
						ThreadWork::AddThread(new cThread([](cThread* thread, LPVOID param)
														  {
															  const char* url = (const char*)param;

															  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
														  }, (void*)link.m_URL.c_str()));

					if (sameLineCounter < (int)(g_DonationLinks.size() - 1))
					{
						ImGui::SameLine();
						sameLineCounter++;
					}
					else
					{
						sameLineCounter = 0;
					}
				}
			}

			ImGui::SeparatorText("License");

			for (std::pair<const char*, const char*> license : { std::pair{"ImGui" , g_ImGuiLicense}, {"HDE x86", g_HdeLicense86} , {"HDE x64", g_HdeLicense64} })
			{
				ImGui::PushID(license.first);

				if (ImGui::CollapsingHeader(license.first))
					ImGui::TextWrapped("%s", license.second);

				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

int ShowMouseCursorImGui(bool show)
{
	static int reqCnt = 0;
	if (show)
		reqCnt++;
	else
		reqCnt--;

	if (reqCnt < 0)
		reqCnt = 0;

	ImGui::GetIO().MouseDrawCursor = reqCnt > 0;
	return reqCnt;
}

void Render()
{
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX9_NewFrame();
	ImGui::NewFrame();

	if (g_WelcomeTimer > 0.f)
	{
		g_WelcomeTimer -= ImGui::GetIO().DeltaTime;
		if (g_WelcomeTimer < 0.f || g_bMenuOpen)
			g_WelcomeTimer = 0.f;
	}

	if (g_WelcomeTimer > 0.f)
		ImGui::GetForegroundDrawList()->AddText(ImVec2(15, 15), -1, "NFSC Shifter Mod\nPress Alt + F9 to open the menu.");

	if (g_bDebugMode)
	{
		ImGui::GetForegroundDrawList()->AddText(ImVec2(15, 45), -1, "Debug Mode Enabled");
		float yOffset = 60.f;
		for (auto& message : g_LatestLog)
		{
			if (message.second > 0.f)
			{
				ImGui::GetForegroundDrawList()->AddText(ImVec2(15, yOffset), -1, message.first.c_str());
				message.second -= ImGui::GetIO().DeltaTime;
				yOffset += 15.f;
				if (message.second < 0.f)
					message.second = 0.f;
			}
		}

		for (int i = g_LatestLog.size() - 1; i >= 0; i--) // removing from the end
		{
			if (g_LatestLog[i].second <= 0.f)
				g_LatestLog.erase(g_LatestLog.begin() + i);
		}
	}

	if (g_bMenuOpen)
		DrawMenu();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Shutdown()
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG)g_oWndProc);
}

CREATE_HOOK(false, 0x73A380, void*, __cdecl, Render_init)
{
	void* result = original();

	if (g_pDevice)
	{
		Events::OnInputDeviceCreate += [](void*)
			{
				if (g_pInput)
				{
					g_pInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, nullptr, DIEDFL_ATTACHEDONLY);
				}
			};

		Events::OnDeviceResetEvent.before += []() // unfortunatelly, does not work
			{
				ImGui_ImplDX9_InvalidateDeviceObjects();
				g_bResetResources = true;
			};

		g_oWndProc = (WNDPROC)SetWindowLongPtrA(g_hWnd, GWLP_WNDPROC, (LONG)hkWndProc);
		ImGui::CreateContext();

		ImGui_ImplWin32_Init(g_hWnd);
		ImGui_ImplDX9_Init(g_pDevice);

		// no after because for some unknown fucking reason it crashes, even in hooked Reset method

		Events::DrawingEvent += []()
			{
				if (g_bResetResources)
				{
					ImGui_ImplDX9_CreateDeviceObjects();
					g_bResetResources = false;
				}
				Render();
			};

		Events::MainExitEvent += []()
			{
				Shutdown();
				SaveConfig();

				for (cConnectedDevice* dev : g_ConnectedDevices)
				{
					if (dev)
					{
						dev->getDevice()->Unacquire();
						dev->release();

						delete dev;
					}
				}

				g_bRefreshRememberedDevice = true; // just in case

				g_ConnectedDevices.clear();
			};

		Events::MainUpdateEvent += []()
			{
				for (int i = 0; i < 256; i++)
					Input::g_KeyState[i] = GetKeyState(i) & 0x80;

				g_Keyboard.Update();

				if (g_Keyboard.on(VK_MENU) && g_Keyboard.trig(VK_F9))
				{
					g_bMenuOpen ^= true;
					ShowMouseCursorImGui(g_bMenuOpen);
				}

				ThreadWork::UpdateThreads();
			};
	}
	return result;
}

class NFSCShifterSupport
{
public:
	NFSCShifterSupport()
	{
		Events::OnInitializationEvent += []()
			{
				wchar_t szPath[MAX_PATH] = { 0 };
				GetModuleFileNameW(GetModuleHandleA(nullptr), szPath, MAX_PATH);
				if (wchar_t* pLastSlash = wcsrchr(szPath, L'\\'))
					*pLastSlash = 0; // removing executable from path

				wcscat_s(szPath, L"\\NFSCShifterSupport.log");

				Logger::Get()->open(szPath);

				if (wchar_t* dot = wcsrchr(szPath, L'.'))
					*dot = 0;

				wcscat_s(szPath, L".debug");

				struct _stat64i32 fileInfo;

				if (_wstat(szPath, &fileInfo) == 0)
					g_bDebugMode = true;

				LoadConfig();

				ThreadWork::AddThread([](cThread* pThread, LPVOID)
					{
						static CRITICAL_SECTION iniLock;

						InitializeCriticalSection(&iniLock);

						EnterCriticalSection(&iniLock);

						std::vector<char> buffer = Utils::FetchURL("https://raw.githubusercontent.com/Frouk3/mod_about_links/main/links");

						if (!buffer.empty())
						{
							IniReader ini;
							ini.ParseData(buffer.data());

							auto ParseSections = [&ini](const char* section, std::vector<Link>& storage)
								{
									IniReader::IniSection* sect = ini.get(section);
									if (sect)
									{
										int i = 0;
										while (true)
										{
											Utils::String root = Utils::Format("Link[%d]", i);
											IniReader::IniSection::IniKey* name = sect->get((root + ".Name").c_str());
											IniReader::IniSection::IniKey* url = sect->get((root + ".URL").c_str());
											if (!name || !url)
												break;

											Link link;
											link.m_Name = name->getValue();
											link.m_URL = url->getValue();

											storage.push_back(link);
											i++;
										}
									}
								};

							ParseSections("Social", g_ContactLinks);
							ParseSections("Donations", g_DonationLinks);
						}

						LeaveCriticalSection(&iniLock);

						DeleteCriticalSection(&iniLock);
					}, nullptr);
			};

		Events::MainExitEvent += []()
			{
				Logger::Get()->close();
			};
	}
} Plugin;