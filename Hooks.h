#pragma once
#include "SafeHook/SafeHook.h"

namespace Hooks {}

#define CREATE_HOOK(startDisabled, addr, ret, callDecl, name, ...) \
	namespace Hooks					\
	{								\
		struct sHooked_##name		\
		{							\
			static inline ret callDecl hooked(__VA_ARGS__);			\
			static inline ret(callDecl *original)(__VA_ARGS__) = nullptr;						\
			SafeHook::Hook hook;	\
			void Enable() { hook.Enable(); } void Disable() { hook.Disable(); } 			\
			sHooked_##name() { new (&hook) SafeHook::Hook((void*)(addr), hooked, !startDisabled, (void**)&original); } 	\
			~sHooked_##name() = default;		\
		};							\
	}								\
	static inline Hooks::sHooked_##name Hooked_##name;								\
	ret callDecl Hooks::sHooked_##name::hooked(__VA_ARGS__)							

#define CREATE_THISCALL(startDisabled, addr, ret, name, thisType, ...) \
	namespace Hooks\
	{\
		struct sHooked_##name\
		{\
			static inline ret __fastcall hooked(thisType pThis, void*, __VA_ARGS__);\
			static inline ret(__thiscall *original)(thisType pThis, __VA_ARGS__) = nullptr;\
			SafeHook::Hook hook;\
			void Enable() { hook.Enable();} void Disable() { hook.Disable(); }\
			sHooked_##name() { new(&hook) SafeHook::Hook((void*)(addr), hooked, !startDisabled, (void**)&original); }	\
			~sHooked_##name() = default;	\
		};								\
	}														\
	static inline Hooks::sHooked_##name Hooked_##name;	\
	ret	__fastcall Hooks::sHooked_##name::hooked(thisType pThis, void *, __VA_ARGS__) 