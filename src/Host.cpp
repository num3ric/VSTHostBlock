#include "Host.h"
//#include "windowsx.h"
namespace vst
	{

	VSTHost::~VSTHost(void)
		{

		}

	VSTHost::VSTHost(const Format &format)
		: Node(format)
		{
		if (getChannelMode() != ChannelMode::SPECIFIED)
			{
			setChannelMode(ChannelMode::SPECIFIED);
			setNumChannels(2);
			}
		setHWND((HWND)ci::app::getWindow()->getNative());
		}

	static VstIntPtr VSTCALLBACK HostCallback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);



	bool VSTHost::checkPlatform()
		{
#if VST_64BIT_PLATFORM
		console() << "*** This is a 64 Bit Build! ***" << endl;
#else
		//console() << "*** This is a 32 Bit Build! ***" << endl;
#endif

		int sizeOfVstIntPtr = sizeof (VstIntPtr);
		int sizeOfVstInt32 = sizeof (VstInt32);
		int sizeOfPointer = sizeof (void*);
		int sizeOfAEffect = sizeof (AEffect);

		

		return sizeOfVstIntPtr == sizeOfPointer;
		}

	int VSTHost::start(const char* fileName)
		{
		if (!checkPlatform())
			{
			console() << "Platform verification failed! Please check your Compiler Settings!" << endl;
			return -1;
			}

		//console() << "HOST> Load library..." << endl;

		if (!loader.loadLibrary(fileName))
			{
			console() << "Failed to load VST Plugin library!" << endl;
			return -1;
			}

		mainEntry = loader.getMainEntry();
		if (!mainEntry)
			{
			console() << "VST Plugin main entry not found!" << endl;
			return -1;
			}

		//console() << "HOST> Create effect..." + endl;
		effect = mainEntry(HostCallback);
		if (!effect)
			{
			console() << "Failed to create effect instance!" << endl;
			return -1;
			}

		//console() + "HOST> Init sequence..." + endl;
		effect->dispatcher(effect, effOpen, 0, 0, 0, 0);

		//effect->dispatcher(effect, effEditOpen, 0, 0, editorHwnd, 0);
		effect->dispatcher(effect, effSetSampleRate, 0, 0, 0, getSampleRate());
		effect->dispatcher(effect, effSetBlockSize, 0, getFramesPerBlock(), 0, 0);
		effect->dispatcher(effect, effMainsChanged, 0, 1, 0, 0);
		setFlags();
		//ShowWindow(editorHwnd, true);
		return 0;
		}

	int VSTHost::load(DataSourceRef filename)
		{
		start(filename->getFilePath().generic_string().c_str());
		return 1;

		}

	//-------------------------------------------------------------------------------------------------------
	std::string VSTHost::checkEffectProperties()
		{
		std::string properties;
		properties += " -- Vst Properties -- \n";
		properties += "Plugin name: " + toString(effectName) + "\nVendor: " + toString(vendorString) + "\nProduct:" + toString(productString);

		properties += "Has editor: " + toString(fHasEditor) + "\nIs synth: " + toString(fSynth) + "\nCan do replacing: " + toString(fCanReplacing);
		effect->dispatcher(effect, effGetEffectName, 0, 0, effectName, 0);
		effect->dispatcher(effect, effGetVendorString, 0, 0, vendorString, 0);
		effect->dispatcher(effect, effGetProductString, 0, 0, productString, 0);


		properties += "numPrograms: " + toString(effect->numPrograms) + "\nNum params: " + toString(effect->numParams) + "\nNumInputs: " + toString(effect->numInputs)
			+ "\nNumOutputs: " + toString(effect->numOutputs);


		// Iterate programs...
		for (VstInt32 progIndex = 0; progIndex < effect->numPrograms; progIndex++)
			{
			char progName[256] = { 0 };
			if (!effect->dispatcher(effect, effGetProgramNameIndexed, progIndex, 0, progName, 0))
				{
				effect->dispatcher(effect, effSetProgram, 0, progIndex, 0, 0); // Note: old program not restored here!
				effect->dispatcher(effect, effGetProgramName, 0, 0, progName, 0);
				}
			properties += "\nProgram " + toString(progIndex) + ":: " + toString(progName);
			}

		// Iterate parameters...
		for (VstInt32 paramIndex = 0; paramIndex < effect->numParams; paramIndex++)
			{
			char paramName[256] = { 0 };
			char paramLabel[256] = { 0 };
			char paramDisplay[256] = { 0 };

			effect->dispatcher(effect, effGetParamName, paramIndex, 0, paramName, 0);
			effect->dispatcher(effect, effGetParamLabel, paramIndex, 0, paramLabel, 0);
			effect->dispatcher(effect, effGetParamDisplay, paramIndex, 0, paramDisplay, 0);
			float value = effect->getParameter(effect, paramIndex);

			properties += "\nParam " + toString(paramIndex) + " : " + toString(paramName) + ": " + toString(paramDisplay) + " | " + toString(paramLabel) + " value: " + toString(value);
			}

		// Can-do nonsense...
		static const char* canDos[] =
			{
			"receiveVstEvents",
			"receiveVstMidiEvent",
			"midiProgramNames"
			};

		for (VstInt32 canDoIndex = 0; canDoIndex < sizeof (canDos) / sizeof (canDos[0]); canDoIndex++)
			{
			console() << "Can do %s... " << canDos[canDoIndex] << endl;
			VstInt32 result = (VstInt32)effect->dispatcher(effect, effCanDo, 0, 0, (void*)canDos[canDoIndex], 0);
			switch (result)
				{
				case 0: console() << "don't know" << endl; break;
				case 1: console() << "yes" << endl; break;
				case -1: console() << "definitely not!" << endl; break;
				default: console() << "?????" << endl;
				}
			}
		return properties;
		}



	VstIntPtr VSTCALLBACK HostCallback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
		{
		VstIntPtr result = 0;

		// Filter idle calls...
		bool filtered = false;
		if (opcode == audioMasterIdle)
			{
			static bool wasIdle = false;
			if (wasIdle)
				filtered = true;
			else
				{
				console() << "(Future idle calls will not be displayed!)" << endl;
				wasIdle = true;
				}
			}

		if (!filtered)
			//console() <<"Main Host PLUG> HostCallback (opcode %d)\n index = %d, value = %p, ptr = %p, opt = %f\n", opcode, index, FromVstPtr<void> (value), ptr, opt);

			switch (opcode)
			{
				case audioMasterVersion:
					result = kVstVersion;
					break;
			}

		return result;
		}

	void VSTHost::setSampleRate(int samprate)
		{
		m_samprate = samprate;
		// suspend efect then change sr and resume
		effect->dispatcher(effect, effMainsChanged, 0, 0, 0, 0);
		effect->dispatcher(effect, effSetSampleRate, 0, 0, 0, samprate);
		effect->dispatcher(effect, effMainsChanged, 0, 1, 0, 0);
		}
	void VSTHost::setBlockSize(int blocksize)
		{
		m_blocksize = blocksize;
		// suspend efect then change blocksize and resume
		effect->dispatcher(effect, effMainsChanged, 0, 0, 0, 0);
		effect->dispatcher(effect, effSetBlockSize, 0, blocksize, 0, 0);
		effect->dispatcher(effect, effMainsChanged, 0, 1, 0, 0);
		}


	bool VSTHost::setWindowIdle()
		{

		effect->dispatcher(effect, effEditIdle, 0, 0, 0, 0);

		return true;

		}

	LONG WINAPI PluginWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
		// get effect pointer
		vst::VSTHost* pHost = (vst::VSTHost*)GetWindowLong(hwnd, GWL_USERDATA);

		if (pHost)
			{
			switch (message)
				{

				case WM_TIMER:
					pHost->effect->dispatcher(pHost->effect, effEditIdle, 0, 0, NULL, 0.0f);
					return 0;

					// close plugin window
				case WM_MOVING:
					pHost->effect->dispatcher(pHost->effect, effEditIdle, 0, 0, NULL, 0.0f);
					return 0;

				case WM_CLOSE:

					pHost->effect->dispatcher(pHost->effect, effEditClose, 0, 0, NULL, 0.0f);

					return DefWindowProc(hwnd, message, wParam, lParam);
				}
			}

		return DefWindowProc(hwnd, message, wParam, lParam);
		}
	void VSTHost::openEditor(){


		wchar_t* wString = new wchar_t[256];
		MultiByteToWideChar(CP_ACP, 0, effectName, -1, wString, 256);

		HINSTANCE hInstance = GetModuleHandle(NULL);

		WNDCLASS windowClass;
		windowClass.style = 0;
		windowClass.lpfnWndProc = PluginWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = hInstance;
		windowClass.hIcon = 0;
		windowClass.hCursor = 0;
		windowClass.hbrBackground = 0;
		windowClass.lpszMenuName = 0;
		windowClass.lpszClassName = L"Cinder VST";
		RegisterClass(&windowClass);

		editorHwnd = CreateWindowEx(WS_EX_TOOLWINDOW, L"Cinder VST", wString, WS_VISIBLE | WS_SYSMENU | WS_CAPTION, 64, 64, 64, 16, hwnd, NULL, hInstance, NULL);
		SetWindowLong(editorHwnd, GWL_USERDATA, (long)this);

		ERect * er;
		effect->dispatcher(effect, effEditOpen, 0, 0, editorHwnd, 0.0f);
		effect->dispatcher(effect, effEditGetRect, 0, 0, &er, 0.0f);
		SetWindowPos(editorHwnd, hwnd, 0, 0, er->right + 6, er->bottom + 22, SWP_NOMOVE | SWP_NOZORDER);
		////effect->dispatcher(effect, effEditTop, 0, 0, NULL, 0.0f);
		ShowWindow(editorHwnd, SW_SHOWNORMAL);

		SetTimer(editorHwnd, 1, 16, NULL);

		}

	vst::VstHostRef VstHost()
		{
		throw std::logic_error("The method or operation is not implemented.");
		}

	void VSTHost::process(ci::audio2::Buffer *buffer){
		 //TODO: Find better solution for pointers (smart_ptr?) 

		float **inputs = 0;
		float **outputs = 0;

		VstInt32 numInputs = effect->numInputs;
		VstInt32 numOutputs = effect->numOutputs;
		int blockSize = buffer->getNumFrames();
		if (numInputs > 0)
			{
			inputs = new float *[numInputs];

			for (VstInt32 i = 0; i < numInputs; i++)
				{
				inputs[i] = new float[blockSize];
				}
			}
		if (numOutputs > 0)
			{
			outputs = new float *[numOutputs];
			for (VstInt32 i = 0; i < numOutputs; i++)
				{
				outputs[i] = new float[blockSize];
				}
			}


		effect->dispatcher(effect, effSetSampleRate, 0, 0, NULL,
						   (float)getSampleRate());
		effect->dispatcher(effect, effSetBlockSize, 0, blockSize * 2, NULL, 0.0);


		//float ** data = new float *[buffer->getNumChannels()];
		float ** data = (float **)buffer->getData();
		if (!fSynth)
			{

			for (size_t i = 0; i < blockSize; i++)
				{
				inputs[0][i] = buffer->getChannel(0)[i];
				inputs[1][i] = buffer->getChannel(1)[i];
				}
			}

		effect->processReplacing(effect, inputs, outputs, blockSize);


		for (size_t i = 0; i < blockSize; i++)
			{
			buffer->getChannel(0)[i] = outputs[0][i];
			buffer->getChannel(1)[i] = outputs[1][i];
			}


		for (int i = 0; i < numInputs; i++)
			delete inputs[i];
		for (int i = 0; i < numOutputs; i++)
			delete outputs[i];
		delete[]inputs;
		delete[]outputs;
		}


	void VSTHost::sendMidiEvent(int status, int8_t midi1, int8_t midi2)
		{
		VstMidiEvent midiEvent;
		midiEvent.type = kVstMidiType;
		midiEvent.byteSize = sizeof(midiEvent);
		midiEvent.deltaFrames = 0;
		midiEvent.flags = 0;
		midiEvent.noteOffset = 0;
		midiEvent.noteLength = 0;
		midiEvent.midiData[0] = status; // 0x90; //note on, note off is 0x80
		midiEvent.midiData[1] = midi1; // 64; //note
		midiEvent.midiData[2] = midi2; //100; //velocity
		midiEvent.midiData[3] = 0;
		midiEvent.detune = 0;
		midiEvent.noteOffVelocity = 0;
		midiEvent.reserved1 = 0;
		midiEvent.reserved2 = 0;

		VstEvents *mEventList;
		mEventList = (VstEvents *)new char[sizeof(mEventList->numEvents) + sizeof(mEventList->reserved) + sizeof(mEventList->events[0]) * 512]; //kMaxEventsPerSlice = 512
		mEventList->numEvents = 1;
		mEventList->reserved = 0;
		mEventList->events[0] = (VstEvent*)&midiEvent;

		effect->dispatcher(effect, effProcessEvents, 0, 0, mEventList, 0.0f);

		}



	void VSTHost::setFlags()
		{
		fSynth = (effect->flags & effFlagsIsSynth); /*? fSynth = true : fSynth = false;*/
		fHasEditor = (effect->flags & effFlagsHasEditor); /*? fHasEditor = true : fHasEditor = false;*/
		fCanReplacing = (effect->flags & effFlagsCanReplacing); /*? fCanReplacing = true : fCanReplacing = false;*/
		effect->dispatcher(effect, effGetEffectName, 0, 0, effectName, 0);
		console() << effectName << "\n" <<

			"Has editor: " << fHasEditor << " Is synth: " << fSynth << " Can do replacing: " << fCanReplacing << endl;
		}

	void VSTHost::setParameter(const int &parameterIndex, const float &parameter)
{
		if (effect->dispatcher(effect, effCanBeAutomated, parameterIndex, NULL, NULL, 0.0f))
			{
			effect->setParameter(effect, parameterIndex, parameter);
			}
		else
			{
			console() << "THIS CANNOT BE AUTOMATED" << endl;
			}
		
		}

	}

