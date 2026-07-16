#pragma once

#include "injector/injector.hpp"
#include "injector/hooking.hpp"
#include "stdint.h"
#include <vector>
#include <functional>
#include "Hooks.h"

class Events
{
public:
	enum class CallingConvention { Cdecl, Stdcall, Fastcall, Thiscall };
	enum class FunctionAddPriority { AddBefore, AddAfter };

	template <FunctionAddPriority prior, uintptr_t injectionPoint, CallingConvention C, typename... Args>
	class IEventBase
	{
	public:
		class Key
		{
		private:
			std::vector<std::function<void(Args...)>> m_vector;
		public:

			std::vector<std::function<void(Args...)>>& getVector() { return m_vector; }

			void add(std::function<void(Args...)> cb) { m_vector.push_back(std::move(cb)); }
			void remove(std::function<void(Args...)> cb) { for (auto it = m_vector.begin(); it != m_vector.end(); ++it) { if (*it == cb) { m_vector.erase(it); break; } } }

			Key& operator+=(std::function<void(Args...)> cb) { add(std::move(cb)); return *this; }
			Key& operator-=(std::function<void(Args...)> cb) { remove(std::move(cb)); return *this; }

			void operator()(Args... args) { for (auto& hook : m_vector) hook(args...); }
		};
	public:
		Key before;
		Key after;

		IEventBase& operator+=(std::function<void(Args...)> cb) { (prior == FunctionAddPriority::AddBefore ? before : after) += cb; return *this; }
	};

	template <FunctionAddPriority prior, uintptr_t injectionPoint, CallingConvention C, typename... Args>
	class IEvent;

	template <FunctionAddPriority prior, uintptr_t injectionPoint, typename... Args>
	class IEvent<prior, injectionPoint, CallingConvention::Cdecl, Args...> : public IEventBase<prior, injectionPoint, CallingConvention::Cdecl, Args...>
	{
	private:
		using hook = injector::function_hooker<injectionPoint, void* (Args...)>;
	public:
		IEvent()
		{
			if (!injectionPoint) // can be handled via .before() and .after() without hooking if the injection point is 0, so we can just skip hooking in that case
				return;

			injector::make_static_hook<hook>([this](std::function<void* (Args...)> orig, Args... args)
				{
					for (auto& hook : this->before.getVector())
						hook(args...);

					void* ret = orig(args...);

					for (auto& hook : this->after.getVector())
						hook(args...);

					return ret;
				});
		}
	};

	template <FunctionAddPriority prior, uintptr_t injectionPoint, typename... Args>
	class IEvent<prior, injectionPoint, CallingConvention::Thiscall, Args...> : public IEventBase<prior, injectionPoint, CallingConvention::Thiscall, Args...>
	{
	private:
		using hook = injector::function_hooker_thiscall<injectionPoint, void* (Args...)>;
	public:
		IEvent()
		{
			if (!injectionPoint)
				return;

			injector::make_static_hook<hook>([this](std::function<void* (Args...)> orig, Args... args)
				{
					for (auto& hook : this->before.getVector())
						hook(args...);

					void* ret = orig(args...);

					for (auto& hook : this->after.getVector())
						hook(args...);

					return ret;
				});
		}
	};

	template <FunctionAddPriority prior, uintptr_t injectionPoint, typename... Args>
	class IEvent<prior, injectionPoint, CallingConvention::Stdcall, Args...> : public IEventBase<prior, injectionPoint, CallingConvention::Stdcall, Args...>
	{
	private:
		using hook = injector::function_hooker_stdcall<injectionPoint, void* (Args...)>;
	public:
		IEvent()
		{
			if (!injectionPoint)
				return;

			injector::make_static_hook<hook>([this](std::function<void* (Args...)> orig, Args... args)
				{
					for (auto& hook : this->before.getVector())
						hook(args...);

					void* ret = orig(args...);

					for (auto& hook : this->after.getVector())
						hook(args...);

					return ret;
				});
		}
	};

	template <FunctionAddPriority prior, uintptr_t injectionPoint, typename... Args>
	class IEvent<prior, injectionPoint, CallingConvention::Fastcall, Args...> : public IEventBase<prior, injectionPoint, CallingConvention::Fastcall, Args...>
	{
	private:
		using hook = injector::function_hooker_fastcall<injectionPoint, void* (Args...)>;
	public:
		IEvent()
		{
			if (!injectionPoint)
				return;

			injector::make_static_hook<hook>([this](std::function<void* (Args...)> orig, Args... args)
				{
					for (auto& hook : this->before.getVector())
						hook(args...);

					void* ret = orig(args...);

					for (auto& hook : this->after.getVector())
						hook(args...);

					return ret;
				});
		}
	};
	template <uintptr_t beforeInjectionPoint, uintptr_t afterInjectionPoint, CallingConvention C, typename... Args>
	class IDualEvent;

	template <uintptr_t beforeInjectionPoint, uintptr_t afterInjectionPoint, typename... Args>
	class IDualEvent<beforeInjectionPoint, afterInjectionPoint, CallingConvention::Cdecl, Args...>
	{
	private:
		using Key = typename IEventBase<FunctionAddPriority::AddBefore, beforeInjectionPoint, CallingConvention::Cdecl, Args...>::Key;
		using before_hook = injector::function_hooker<beforeInjectionPoint, void* (Args...)>;
		using after_hook = injector::function_hooker<afterInjectionPoint, void* (Args...)>;
	public:
		Key before;
		Key after;

		IDualEvent()
		{
			if (!beforeInjectionPoint && !afterInjectionPoint) // can be handled via .before() and .after() without hooking if both injection points are 0, so we can just skip hooking in that case
				return;

			if (beforeInjectionPoint)
				injector::make_static_hook<before_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						for (auto& hook : before.getVector())
							hook(args...);

						return orig(args...);
					});

			if (afterInjectionPoint)
				injector::make_static_hook<after_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						void* ret = orig(args...);

						for (auto& hook : after.getVector())
							hook(args...);

						return ret;
					});
		}

		// IDualEvent& operator+=(std::function<void(Args...)> cb) { before += std::move(cb); return *this; }
	};

	template <uintptr_t beforeInjectionPoint, uintptr_t afterInjectionPoint, typename... Args>
	class IDualEvent<beforeInjectionPoint, afterInjectionPoint, CallingConvention::Thiscall, Args...>
	{
	private:
		using Key = typename IEventBase<FunctionAddPriority::AddBefore, beforeInjectionPoint, CallingConvention::Thiscall, Args...>::Key;
		using before_hook = injector::function_hooker_thiscall<beforeInjectionPoint, void* (Args...)>;
		using after_hook = injector::function_hooker_thiscall<afterInjectionPoint, void* (Args...)>;
	public:
		Key before;
		Key after;

		IDualEvent()
		{
			if (!beforeInjectionPoint && !afterInjectionPoint)
				return;

			if (beforeInjectionPoint)
				injector::make_static_hook<before_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						for (auto& hook : before.getVector())
							hook(args...);

						return orig(args...);
					});

			if (afterInjectionPoint)
				injector::make_static_hook<after_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						void* ret = orig(args...);

						for (auto& hook : after.getVector())
							hook(args...);

						return ret;
					});
		}

		// IDualEvent& operator+=(std::function<void(Args...)> cb) { before += std::move(cb); return *this; }
	};

	template <uintptr_t beforeInjectionPoint, uintptr_t afterInjectionPoint, typename... Args>
	class IDualEvent<beforeInjectionPoint, afterInjectionPoint, CallingConvention::Stdcall, Args...>
	{
	private:
		using Key = typename IEventBase<FunctionAddPriority::AddBefore, beforeInjectionPoint, CallingConvention::Stdcall, Args...>::Key;
		using before_hook = injector::function_hooker_stdcall<beforeInjectionPoint, void* (Args...)>;
		using after_hook = injector::function_hooker_stdcall<afterInjectionPoint, void* (Args...)>;
	public:
		Key before;
		Key after;

		IDualEvent()
		{
			if (!beforeInjectionPoint && !afterInjectionPoint)
				return;

			if (beforeInjectionPoint)
				injector::make_static_hook<before_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						for (auto& hook : before.getVector())
							hook(args...);

						return orig(args...);
					});

			if (afterInjectionPoint)
				injector::make_static_hook<after_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						void* ret = orig(args...);

						for (auto& hook : after.getVector())
							hook(args...);

						return ret;
					});
		}

		// IDualEvent& operator+=(std::function<void(Args...)> cb) { before += std::move(cb); return *this; }
	};

	template <uintptr_t beforeInjectionPoint, uintptr_t afterInjectionPoint, typename... Args>
	class IDualEvent<beforeInjectionPoint, afterInjectionPoint, CallingConvention::Fastcall, Args...>
	{
	private:
		using Key = typename IEventBase<FunctionAddPriority::AddBefore, beforeInjectionPoint, CallingConvention::Fastcall, Args...>::Key;
		using before_hook = injector::function_hooker_fastcall<beforeInjectionPoint, void* (Args...)>;
		using after_hook = injector::function_hooker_fastcall<afterInjectionPoint, void* (Args...)>;
	public:
		Key before;
		Key after;

		IDualEvent()
		{
			if (!beforeInjectionPoint && !afterInjectionPoint)
				return;

			if (beforeInjectionPoint)
				injector::make_static_hook<before_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						for (auto& hook : before.getVector())
							hook(args...);

						return orig(args...);
					});

			if (afterInjectionPoint)
				injector::make_static_hook<after_hook>([this](std::function<void* (Args...)> orig, Args... args)
					{
						void* ret = orig(args...);

						for (auto& hook : after.getVector())
							hook(args...);

						return ret;
					});
		}

		// IDualEvent& operator+=(std::function<void(Args...)> cb) { before += std::move(cb); return *this; }
	};
public:

	static inline IEvent<FunctionAddPriority::AddBefore, 0x6B8F52, CallingConvention::Cdecl> MainExitEvent;
	static inline IEvent<FunctionAddPriority::AddAfter, 0x712187, CallingConvention::Cdecl> GraphicsSettingsSaveEvent;
	static inline IEvent<FunctionAddPriority::AddAfter, 0x6B795C, CallingConvention::Cdecl> MainUpdateEvent;
	static inline IEvent<FunctionAddPriority::AddAfter, 0x7313DF, CallingConvention::Cdecl> DrawingEvent;
	static inline IDualEvent<0x72B37D, 0x72B592, CallingConvention::Cdecl> OnDeviceResetEvent;
	static inline IEvent<FunctionAddPriority::AddAfter, 0x73A380, CallingConvention::Cdecl> OnInitializationEvent;
	static inline IEvent<FunctionAddPriority::AddAfter, 0x73610E, CallingConvention::Thiscall, void*> OnInputDeviceCreate;
	// static inline IEvent<0, 0x731439, Caves::AfterDrawingEventHook> AfterDrawingEvent;
};