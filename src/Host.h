#include "cinder/audio/Node.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/dsp/Converter.h"
#include "cinder/Thread.h"
#include "cinder/Utilities.h"


#include "cinder/app/App.h"
#include <iostream>
#include "aeffectx.h"
#ifdef WIN32
#include <Windows.h>
#endif

using namespace ci;
using namespace ci::app;
using namespace std;
namespace vst {

	typedef AEffect* (*PluginEntryProc) (audioMasterCallback audioMaster);

	typedef std::shared_ptr<class VSTHost> VstHostRef;

	struct PluginLoader
		{
		void* module;

		PluginLoader()
			: module(0) {}

		~PluginLoader() {
			if (module) {
#if WIN32
				FreeLibrary((HMODULE)module);
#elif TARGET_API_MAC_CARBON
				CFBundleUnloadExecutable ((CFBundleRef)module);
				CFRelease((CFBundleRef)module);
#endif
				}
			}

		bool loadLibrary(const char* fileName) {
#if WIN32
			module = LoadLibraryA(fileName);
#elif TARGET_API_MAC_CARBON
			CFStringRef fileNameString = CFStringCreateWithCString (NULL, fileName, kCFStringEncodingUTF8);
			if (fileNameString == 0)
				return false;
			CFURLRef url = CFURLCreateWithFileSystemPath (NULL, fileNameString, kCFURLPOSIXPathStyle, false);
			CFRelease (fileNameString);
			if (url == 0)
				return false;
			module = CFBundleCreate (NULL, url);
			CFRelease (url);
			if (module && CFBundleLoadExecutable ((CFBundleRef)module) == false)
				return false;
#endif
			return module != 0;
			console() << "Loadding" << endl;
			}

		PluginEntryProc getMainEntry() {
			PluginEntryProc mainProc = 0;
#if WIN32
			mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "VSTPluginMain");
			if (!mainProc)
				mainProc = (PluginEntryProc)GetProcAddress((HMODULE)module, "main");
#elif TARGET_API_MAC_CARBON
			mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName ((CFBundleRef)module, CFSTR("VSTPluginMain"));
			if (!mainProc)
				mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName((CFBundleRef)module, CFSTR("main_macho"));
#endif
			return mainProc;
			}
		//-------------------------------------------------------------------------------------------------------
		};

	class VSTHost : public ci::audio::Node
		{
		public:
			
			VSTHost(const Format &format = Format());
			virtual ~VSTHost(void);

		public:         // Public interface functions
			int VSTHost::start(const char* fileName);

			int VSTHost::load(DataSourceRef filename);
			int VSTHost::getNumPrograms() { return effect->numPrograms; };
			int VSTHost::getNumParams() { return effect->numParams; };
			int VSTHost::getNumInputs() { return effect->numInputs; };
			int VSTHost::getNumOutputs() { return effect->numOutputs; };
			void VSTHost::setSampleRate(int samprate);
			void VSTHost::setBlockSize(int blocksize);

			AEffect* VSTHost::getEffectPointer() { return effect; };

			void setParameter(const int &parameterIndex, const float &parameter);
			AEffect* effect;
			std::string checkEffectProperties();

			bool VSTHost::setWindowIdle();
			HWND winInitChildOLD(int width, int height, string name);
			void setHWND(HWND mainWindow) { hwnd = mainWindow; }
			void openEditor();
			void process(ci::audio::Buffer *buffer);
			void sendMidiEvent(int status, int8_t midi1, int8_t midi2);


		protected:    

			void checkEffectProcessing(AEffect* effect);
			bool VSTHost::checkPlatform();

		private:     

			PluginLoader loader;
			PluginEntryProc mainEntry;
			ci::audio::BufferInterleaved mBufferInterleaved;
			char id_name[75];
			char effectName[256];
			char vendorString[256];
			char productString[256];
			void getPluginCategory();
			float m_samprate;
			VstInt32 m_blocksize;
			int height;
			int width;
			HWND hwnd;
			HWND editorHwnd;

			int fHasEditor, fCanReplacing, fSynth;
			void *m_window;         // HWND (Win32) or WindowRef (Carbon)
			void setFlags();
		};
	
	}