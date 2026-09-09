// mingw port of IV-SDK Hooks.h: the upstream file uses MSVC __declspec(naked)
// + __asm blocks; here each trampoline is a GCC naked function with AT&T asm.
// Run()/returnAddress/callAddress/thisParam get explicit assembler names so the
// basic-asm bodies can reference them. Event API is unchanged. The original
// file is kept next to this one as Hooks.h.upstream.
namespace plugin
{
	// pushad; call Run; popad; jmp returnAddress
#define IVSDK_JMP_HOOK(tag) \
	void Run() __asm__("ivsdk_" #tag "_run"); \
	uintptr_t returnAddress __asm__("ivsdk_" #tag "_ret"); \
	__attribute__((naked)) void MainHook() \
	{ \
		__asm__ volatile("pushal\n\tcall ivsdk_" #tag "_run\n\tpopal\n\tjmp *ivsdk_" #tag "_ret"); \
	}
	// mov [thisParam], ecx; call callAddress; pushad; call Run; popad; ret
#define IVSDK_THISCALL_HOOK(tag) \
	void Run() __asm__("ivsdk_" #tag "_run"); \
	uintptr_t callAddress __asm__("ivsdk_" #tag "_call"); \
	__attribute__((naked)) void MainHook() \
	{ \
		__asm__ volatile("movl %ecx, ivsdk_" #tag "_this\n\tcall *ivsdk_" #tag "_call\n\tpushal\n\tcall ivsdk_" #tag "_run\n\tpopal\n\tret"); \
	}

	namespace processScriptsEvent
	{
		uint8_t threadDummy[256];
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(processScripts)

		void Run()
		{
			auto bak = CTheScripts::m_pCurrentThread;
			CTheScripts::m_pCurrentThread = (uint32_t)threadDummy;

			for (auto& f : funcPtrs)
			{
				f();
			}

			CTheScripts::m_pCurrentThread = bak;
		}
		// CGame::Process
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace gameLoadPriorityEvent
	{
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(gameLoadPriority)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f();
			}
		}
		// before the first LoadLevel call, use for files that need to overwrite base game files
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace gameLoadEvent
	{
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(gameLoad)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f();
			}
		}
		// after the last LoadLevel call, use for addon files that don't interfere with game files
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace ingameStartupEvent
	{
		uint8_t threadDummy[256];
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(ingameStartup)

		void Run()
		{
			auto bak = CTheScripts::m_pCurrentThread;
			CTheScripts::m_pCurrentThread = (uint32_t)threadDummy;

			for (auto& f : funcPtrs)
			{
				f();
			}

			CTheScripts::m_pCurrentThread = bak;
		}
		// after the game has loaded, natives can be used here
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace mountDeviceEvent
	{
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(mountDevice)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f();
			}
		}
		// after the game mounts its devices (common:/ etc)
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace drawingEvent
	{
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(drawing)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f();
			}
		}
		// 2D drawing (CSprite2d / CFont) works here
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace processCameraEvent
	{
		std::list<void(*)()> funcPtrs;
		IVSDK_JMP_HOOK(processCamera)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f();
			}
		}
		// after CCamera::Process
		void Add(void(*funcPtr)())
		{
			funcPtrs.emplace_back(funcPtr);
		}
	};

	namespace processAutomobileEvent
	{
		CVehicle* thisParam __asm__("ivsdk_processAutomobile_this");
		std::list<void(*)(CVehicle*)> funcPtrs;
		IVSDK_THISCALL_HOOK(processAutomobile)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f(thisParam);
			}
		}
		// after CAutomobile::Process, overriding steer & pedals works here
		void Add(void(*funcPtr)(CVehicle*))
		{
			funcPtrs.emplace_back(funcPtr);
		}
	}

	namespace processPadEvent
	{
		CPad* thisParam __asm__("ivsdk_processPad_this");
		std::list<void(*)(CPad*)> funcPtrs;
		IVSDK_THISCALL_HOOK(processPad)

		void Run()
		{
			for (auto& f : funcPtrs)
			{
				f(thisParam);
			}
		}
		// set all pad controls here, called once per frame for each pad
		void Add(void(*funcPtr)(CPad*))
		{
			funcPtrs.emplace_back(funcPtr);
		}
	}

#undef IVSDK_JMP_HOOK
#undef IVSDK_THISCALL_HOOK

	namespace Overrides
	{
		void GetTexture(CSprite2d(__stdcall* funcPtr)(char*))
		{
			injector::MakeJMP(AddressSetter::Get(0x21DA10, 0xD300), funcPtr);
		}
	}
};
