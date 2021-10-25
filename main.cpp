//Must be compiled in mode Debug x86
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

uintptr_t dwLocalPlayer;
uintptr_t dwEntityList;
uintptr_t dwGlowObjectManager;
uintptr_t dwViewMatrix;
uintptr_t dwForceJump;
uintptr_t dwSmokeCount;
//defaults
#define m_iGlowIndex 0x10488
#define m_iTeamNum 0xF4
#define m_iHealth 0x100
#define m_bDormant 0xED
#define m_iCrosshairId 0x11838
#define m_bSpotted 0x93D
#define m_dwBoneMatrix 0x26A8
#define m_vecOrigin 0x138
#define m_flFlashDuration 0x10470
#define m_ArmorValue 0x117CC
#define m_fFlags 0x104

const int SCREEN_WIDTH = GetSystemMetrics(SM_CXSCREEN);
const int xhairx = SCREEN_WIDTH / 2;
const int SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
const int xhairy = SCREEN_HEIGHT / 2;

uintptr_t moduleBase;
DWORD procId;
DWORD bytesRead;
HWND hwnd;
HANDLE hProcess;
HANDLE hAimassist;
int closest;

uintptr_t GetModuleBaseAddress(const char* modName, DWORD procId) {

	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);

	if (hsnap != INVALID_HANDLE_VALUE) {

		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);

		if (Module32First(hsnap, &modEntry)) {

			do {
				if (!strcmp(modEntry.szModule, modName)) {
					CloseHandle(hsnap);
					return (uintptr_t)modEntry.modBaseAddr;
				}
			}
			while (Module32Next(hsnap, &modEntry));
		}
	}
}

MODULEENTRY32 GetModule(const char* modName, DWORD procId) {
	
	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	
	if (hsnap != INVALID_HANDLE_VALUE) {
		
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		
		if (Module32First(hsnap, &modEntry)) {
			
			do {
				if (!strcmp(modEntry.szModule, modName)) {
					CloseHandle(hsnap);
					return modEntry;
				}
			}
			while (Module32Next(hsnap, &modEntry));
		}
	}
	MODULEENTRY32 module = { -1 };
	return module;
}

uintptr_t FindPattern(MODULEENTRY32 module, uint8_t* arr, const char* pattern, int offset, int extra) {
	
	uintptr_t scan = 0x0;
	const char* pat = pattern;
	uintptr_t firstMatch = 0;
	
	for (uintptr_t pCur = (uintptr_t)arr; pCur < (uintptr_t)arr + module.modBaseSize; ++pCur) {
		if (!*pat) { scan = firstMatch; break; }
		
		if (*(uint8_t*)pat == '\?' || *(uint8_t*)pCur == ((((pat[0] & (~0x20)) >= 'A' && (pat[0] & (~0x20)) <= 'F') ? ((pat[0] & (~0x20)) - 'A' + 0xa) : ((pat[0] >= '0' && pat[0] <= '9') ? pat[0] - '0' : 0)) << 4 | (((pat[1] & (~0x20)) >= 'A' && (pat[1] & (~0x20)) <= 'F') ? ((pat[1] & (~0x20)) - 'A' + 0xa) : ((pat[1] >= '0' && pat[1] <= '9') ? pat[1] - '0' : 0)))) {
			if (!firstMatch) firstMatch = pCur;

			if (!pat[2]) { scan = firstMatch; break; }

			if (*(WORD*)pat == 16191 /*?*/ || *(uint8_t*)pat != '\?') pat += 3;
			else pat += 2;
		}
		else { pat = pattern; firstMatch = 0; }
	}
	if (!scan) return 0x0;
	
	uint32_t read;
	ReadProcessMemory(hProcess, (void*)(scan - (uintptr_t)arr + (uintptr_t)module.modBaseAddr + offset), &read, sizeof(read), NULL);
	
	return read + extra;
}

template<typename T> T RPM(SIZE_T address) {
	T buffer;
	ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), 0);
	return buffer;
}

template<typename T> void WPM(SIZE_T address, T buffer) {
	WriteProcessMemory(hProcess, (LPVOID)address, &buffer, sizeof(buffer), 0);
}

struct glowStructEnemy {
	float red = 1.f;
	float green = 0.f;
	float blue = 0.f;
	float alpha = 0.7f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnoccluded = false;
	BYTE fullBloom = false;
} glowEnm;

struct glowStructLocal {
	float red = 0.5f;
	float green = 0.5f;
	float blue = 1.f;
	float alpha = 0.5f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnoccluded = false;
	BYTE fullBloom = false;
} glowLocal;

uintptr_t getLocalPlayer() {
	return RPM<uintptr_t>(moduleBase + dwLocalPlayer);
}

uintptr_t getPlayer(int index) {
	return RPM<uintptr_t>(moduleBase + dwEntityList + index * 0x10);
}

int getTeam(uintptr_t player) {
	return RPM<int>(player + m_iTeamNum);
}

int getCrosshairID(uintptr_t player) {
	return RPM<int>(player + m_iCrosshairId);
}

class Vector3 {
public:
	float x, y, z;
	Vector3() : x(0.f), y(0.f), z(0.f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

int getPlayerHealth(uintptr_t player) {
	return RPM<int>(player + m_iHealth);
}

int getPlayerArmor(uintptr_t player) {
	return RPM<int>(player + m_ArmorValue);
}

Vector3 PlayerLocation(uintptr_t player) { //stores XYZ coordinates in a Vector3
	return RPM<Vector3>(player + m_vecOrigin);
}

bool DormantCheck(uintptr_t player) {
	return RPM<int>(player + m_bDormant);
}

Vector3 get_head(uintptr_t player) {
	
	struct boneMatrix_t {
		byte pad3[12];
		float x;
		
		byte pad1[12];
		float y;
		
		byte pad2[12];
		float z;
	};
	
	uintptr_t boneBase = RPM<uintptr_t>(player + m_dwBoneMatrix);
	boneMatrix_t boneMatrix = RPM<boneMatrix_t>(boneBase + (sizeof(boneMatrix) * 7/*8 is the boneid for head, 7 is the neck*/));
	
	return Vector3(boneMatrix.x, boneMatrix.y, boneMatrix.z);
}

struct view_matrix_t {
	float matrix[16];
} vm;

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) { //this turns 3D coordinates (ex: XYZ) int 2D coordinates (ex: XY)
	
	struct Vector3 out;
	
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	out.x = SCREEN_WIDTH * .5f;
	out.y = SCREEN_HEIGHT * .5f;

	out.x += 0.5f * _x * SCREEN_WIDTH + 0.5f;
	out.y -= 0.5f * _y * SCREEN_HEIGHT + 0.5f;

	return out;
}

float pythag(int x1, int y1, int x2, int y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

int FindClosestEnemy() {
	
	float Finish;
	int ClosestEntity = 1;
	Vector3 Calc = { 0, 0, 0 };
	float Closest = FLT_MAX;
	int localTeam = getTeam(getLocalPlayer());

	for (short int i = 1; i <= 32; i++) { //loops through all the entitys
		DWORD Entity = getPlayer(i);
		
		int EnmTeam = getTeam(Entity);
		if (EnmTeam == localTeam)
			continue;

		int EnmHealth = getPlayerHealth(Entity);
		if (EnmHealth < 1 || EnmHealth > 100)
			continue;

		int Dormant = DormantCheck(Entity);
		if (Dormant)
			continue;
		
		Vector3 headBone = WorldToScreen(get_head(Entity), vm);
		
		Finish = pythag(headBone.x, headBone.y, xhairx, xhairy);
		
		if (Finish < Closest) {
			Closest = Finish;
			ClosestEntity = i;
		}
	}
	return ClosestEntity;
}

void FindClosestEnemyThread() {
	while (true) {
		closest = FindClosestEnemy();
	}
}

int main() {

	SetConsoleTitle(TEXT("GTBOT by zhivko..."));

	std::cout << "This product is a pre-release and is for testing purposes!\nUnauthorized access and distribution are prohibited!\nIf you continue you agree with the terms!\n\n";
	system("pause");
	system("cls");
	
	//code
	hwnd = FindWindowA(0, "Counter-Strike: Global Offensive");
	if (hwnd == NULL) { std::cout << "Could not find CS:GO window!\nExiting..."; Sleep(2000); exit(0); } std::cout << "Initializing...\n";
	GetWindowThreadProcessId(hwnd, &procId);
	moduleBase = GetModuleBaseAddress("client.dll", procId);
	MODULEENTRY32 client = GetModule("client.dll", procId);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, procId);
	auto bytes = new uint8_t[client.modBaseSize];
	ReadProcessMemory(hProcess, client.modBaseAddr, bytes, client.modBaseSize, &bytesRead); if (bytesRead != client.modBaseSize) throw;
	hAimassist = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)FindClosestEnemyThread, 0, 0x00000004, 0);
	uintptr_t jumpBuffer;
	
	dwLocalPlayer = FindPattern(client, bytes, "8D 34 85 ? ? ? ? 89 15 ? ? ? ? 8B 41 08 8B 48 04 83 F9 FF", 0x3, 0x4);//0x3 - offset, 0x4 - extra
	dwLocalPlayer = dwLocalPlayer - (uintptr_t)client.modBaseAddr;
	dwEntityList = FindPattern(client, bytes, "BB ? ? ? ? 83 FF 01 0F 8C ? ? ? ? 3B F8", 0x1, 0x0);
	dwEntityList = dwEntityList - (uintptr_t)client.modBaseAddr;
	dwGlowObjectManager = FindPattern(client, bytes, "A1 ? ? ? ? A8 01 75 4B", 0x1, 0x4);
	dwGlowObjectManager = dwGlowObjectManager - (uintptr_t)client.modBaseAddr;
	dwViewMatrix = FindPattern(client, bytes, "0F 10 05 ? ? ? ? 8D 85 ? ? ? ? B9", 0x3, 0xB0);
	dwViewMatrix = dwViewMatrix - (uintptr_t)client.modBaseAddr;
	dwForceJump = FindPattern(client, bytes, "8B 0D ? ? ? ? 8B D6 8B C1 83 CA 02", 0x2, 0x0);
	dwForceJump = dwForceJump - (uintptr_t)client.modBaseAddr;
	dwSmokeCount = FindPattern(client, bytes, "A3 ? ? ? ? 57 8B CB", 0x1, 0x0);
	delete[] bytes;

	bool aimassist = false, flash = false;
	
	std::cout << "Done!"; system("cls");

	while (WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT) {

		uintptr_t dwGlowManager = RPM<uintptr_t>(moduleBase + dwGlowObjectManager);
		int LocalTeam = RPM<int>(getLocalPlayer() + m_iTeamNum);
		int CrosshairID = getCrosshairID(getLocalPlayer());
		int CrosshairTeam = getTeam(getPlayer(CrosshairID - 1));
		int TLocalTeam = getTeam(getLocalPlayer());
		vm = RPM<view_matrix_t>(moduleBase + dwViewMatrix);
		Vector3 closestw2shead = WorldToScreen(get_head(getPlayer(closest)), vm);
		int flags = RPM<int>(getLocalPlayer() + m_fFlags);
		
		if (GetAsyncKeyState(0x01) & 1 && aimassist) {
			if (closestw2shead.z >= 0.001f/*onscreen check*/) { SetCursorPos(closestw2shead.x, closestw2shead.y); }
		}

		if (GetAsyncKeyState(0x12)) {
			if (CrosshairID > 0 && CrosshairID <= 32 && TLocalTeam != CrosshairTeam) {
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				Sleep(200);
			}
		}

		for (short int i = 1; i <= 32; i++) {
			uintptr_t dwEntity = RPM<uintptr_t>(moduleBase + dwEntityList + i * 0x10);
			int iGlowIndx = RPM<int>(dwEntity + m_iGlowIndex);

			int EnmHealth = RPM<int>(dwEntity + m_iHealth);
			if (EnmHealth < 1 || EnmHealth > 100)
				continue;
			
			int Dormant = RPM<int>(dwEntity + m_bDormant);
			if (Dormant)
				continue;

			int EntityTeam = RPM<int>(dwEntity + m_iTeamNum);

			if (LocalTeam == EntityTeam) {
				WPM<glowStructLocal>(dwGlowManager + (iGlowIndx * 0x38) + 0x8, glowLocal);
			}
			else if (LocalTeam != EntityTeam) {
				WPM<glowStructEnemy>(dwGlowManager + (iGlowIndx * 0x38) + 0x8, glowEnm);
			}
		}

		for (short int i = 1; i <= 32; i++) {
			DWORD dwCurrentEntity = RPM<DWORD>(moduleBase + dwEntityList + i * 0x10);
			
			if (dwCurrentEntity) {
				WPM<bool>(dwCurrentEntity + m_bSpotted, true);
			}
		}

		if (flash) {
			WPM<float>(getLocalPlayer() + m_flFlashDuration, 0.f);
			WPM<short>(dwSmokeCount, 0);
		}

		if (GetAsyncKeyState(0x2D) && !aimassist) {
			ResumeThread(hAimassist);
			aimassist = true;
			std::cout << "AIM ASSIST IS ENABLED\nTURN OFF \"RAW INPUT\" IN GAME SETTINGS\n";
		}

		if (GetAsyncKeyState(0x2E) && aimassist) {
			SuspendThread(hAimassist);
			aimassist = false;
			system("cls");
		}

		if (GetAsyncKeyState(0x21) && !flash) {
			flash = true;
			std::cout << "PREVENT FLASHING/SMOKE IS ENABLED\n";
		}

		if (GetAsyncKeyState(0x22) && flash) {
			flash = false;
			system("cls");
		}

		if (GetAsyncKeyState(0x24) & 1) {
			system("cls");

			std::cout << "Enemy:\n";
			for (short int i = 1; i <= 32; i++) {
				int EntityTeam = RPM<int>(RPM<uintptr_t>(moduleBase + dwEntityList + i * 0x10) + m_iTeamNum);
				short int health = getPlayerHealth(getPlayer(i));
				short int armor = getPlayerArmor(getPlayer(i));

				if (health >= 1 && health <= 100 && LocalTeam != EntityTeam) {
					std::cout << health << " - " << armor << std::endl;
				}
			}
		}

		if (GetAsyncKeyState(VK_SPACE) & 0x8000 && aimassist) {
			if (flags & 1) { jumpBuffer = 5; } else { jumpBuffer = 4; }
			WPM<uintptr_t>(moduleBase + dwForceJump, jumpBuffer);
		}
	}
}
