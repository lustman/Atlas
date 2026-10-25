#include "entry.hpp"
#include "hooks.hpp"

#include <shellapi.h>

notify_t msg = {};

void copy_chars( char* dst, size_t cap, std::string_view src ) {
	auto count = src.size( );
	if ( count >= cap )
		count = cap - 1;

	std::copy_n( src.begin( ), count, dst );
	dst[ count ] = 0;
}

bool bind_env( JavaVM* vm, JNIEnv** out, bool* attached ) {
	JNIEnv* env = {};
	auto status = vm->GetEnv( reinterpret_cast< void** >( &env ), JNI_VERSION_1_8 );
	auto did = status == JNI_EDETACHED;
	if ( did )
		status = vm->AttachCurrentThread( reinterpret_cast< void** >( &env ), nullptr );
	if ( status != JNI_OK || !env )
		return false;

	*out = env;
	if ( attached )
		*attached = did;

	return true;
}

unsigned long __stdcall notify_thread( void* ) {
	auto* window = ::CreateWindowExA( 0, "STATIC", "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr );
	if ( !window )
		return 0;

	NOTIFYICONDATAA data = {};
	data.cbSize = sizeof( data );
	data.hWnd = window;
	data.uID = 1;
	data.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
	data.hIcon = ::LoadIcon( nullptr, IDI_INFORMATION );
	data.dwInfoFlags = NIIF_INFO;
	data.uTimeout = 2000;
	copy_chars( data.szInfoTitle, sizeof( data.szInfoTitle ), msg.m_title );
	copy_chars( data.szTip, sizeof( data.szTip ), msg.m_title );
	copy_chars( data.szInfo, sizeof( data.szInfo ), msg.m_text );

	if ( !::Shell_NotifyIconA( NIM_ADD, &data ) ) {
		::DestroyWindow( window );

		return 0;
	}

	auto end = ::GetTickCount64( ) + 2000;
	while ( ::GetTickCount64( ) < end ) {
		::MsgWaitForMultipleObjects( 0, nullptr, 0, 50, QS_ALLINPUT );

		MSG msg = {};
		for ( auto i = 0; i < 8 && ::PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ); ++i ) {
			if ( msg.message == WM_QUIT )
				continue;

			::TranslateMessage( &msg );
			::DispatchMessageA( &msg );
		}
	}

	::Shell_NotifyIconA( NIM_DELETE, &data );
	::DestroyWindow( window );

	return 0;
}

void notify( std::string_view title, std::string_view text ) {
	copy_chars( msg.m_title, sizeof( msg.m_title ), title );
	copy_chars( msg.m_text, sizeof( msg.m_text ), text );

	auto* thread = ::CreateThread( nullptr, 0, notify_thread, nullptr, 0, nullptr );
	if ( thread )
		::CloseHandle( thread );
}

int hook_env( JNIEnv* env ) {
	if ( !g_writer.open( ) ) {
		notify( "Couldn't open log for writing.", "" );

		return 0;
	}

	auto hook_count = install_jni( env );
	notify( std::format( "Hooked {} slots", hook_count ), g_writer.path( ) );

	return hook_count;
}

#ifdef INJECTABLE

unsigned long inject_thread( void* ) {
	while ( 1 ) {
		JavaVM* vm = {};
		jsize count = {};
		JNIEnv* env = {};

		auto* jvm_mod = ::GetModuleHandleA( "jvm.dll" );
		if ( !jvm_mod ) {
			::Sleep( 50 );
			continue;
		}

		auto get_vm_fn = reinterpret_cast< jint( * )( JavaVM**, jsize, jsize* ) >( ::GetProcAddress( jvm_mod, "JNI_GetCreatedJavaVMs" ) );
		if ( !get_vm_fn ) {
			::Sleep( 50 );
			continue;
		}

		if ( get_vm_fn( &vm, 1, &count ) != JNI_OK || !vm || count < 1 ) {
			::Sleep( 50 );
			continue;
		}

		bool attached = {};
		if ( !bind_env( vm, &env, &attached ) ) {
			::Sleep( 50 );
			continue;
		}

		hook_env( env );
		if ( attached )
			vm->DetachCurrentThread( );

		return 0;
	}
}

int initialize( ) {
	void* thread = ::CreateThread( nullptr, 0, inject_thread, nullptr, 0, nullptr );
	if ( thread )
		return ::CloseHandle( thread );

	return 0;
}

#else

extern "C" __declspec( dllexport ) jint JNI_OnLoad( JavaVM* vm, void* ) {
	JNIEnv* env = {};
	if ( !bind_env( vm, &env, nullptr ) )
		return JNI_ERR;

	hook_env( env );

	return JNI_VERSION_1_8;
}

#endif

int DllMain( void* mod, uint32_t reason, void* ) {
	if ( reason == DLL_PROCESS_DETACH ) {
		g_writer.flush( );

		return 1;
	}

	if ( reason != DLL_PROCESS_ATTACH )
		return 1;

#ifdef INJECTABLE
	return initialize( );
#else
	return 1;
#endif
}
