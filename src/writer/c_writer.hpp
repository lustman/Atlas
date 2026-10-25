#pragma once

#include "common.hpp"

enum class ref_kind_t : uint8_t { local, global, weak };

struct ref_t {
	std::string m_name = {};
	std::string m_class_name = {};
	std::string m_text = {};
	ref_kind_t m_kind = ref_kind_t::local;
};

struct id_t {
	std::string m_name = {};
	std::string m_class_name = {};
	std::string m_member = {};
	std::string m_sig = {};
};

struct text_hash_t {
	using is_transparent = void;

	size_t operator( )( std::string_view text ) const noexcept {
		size_t hash = 14695981039346656037ull;
		for ( auto ch : text ) {
			hash ^= static_cast< unsigned char >( ch );
			hash *= 1099511628211ull;
		}

		return hash;
	}
};

struct text_eq_t {
	using is_transparent = void;

	bool operator( )( std::string_view a, std::string_view b ) const noexcept { return a == b; }
};

bool muted( );
bool set_mute( bool value );

class c_mute {
public:
	c_mute( ) { m_prev = set_mute( true ); }
	~c_mute( ) { set_mute( m_prev ); }
	c_mute( const c_mute& ) = delete;
	c_mute& operator=( const c_mute& ) = delete;

private:
	bool m_prev = {};
};

class c_writer {
	friend bool muted( );
	friend bool set_mute( bool value );
	friend void* set_caller( void* from );

public:
	bool open( );
	void flush( );
	void save_raw( const JNINativeInterface_* table );
	const std::string& path( ) const { return m_path; }

	std::string object_name( JNIEnv* env, jobject value );
	std::string method_name( jmethodID id );
	std::string field_name( jfieldID id );
	std::string field_name( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class = false );
	void method_name( JNIEnv* env, jobject obj, jclass clazz, jmethodID id, bool is_static );
	std::string field_text( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class = false );
	std::string read_string( JNIEnv* env, jobject obj, jfieldID id, bool is_static, jobject value, bool is_class = false );
	void log_field( jfieldID id, bool is_static, std::string_view java, std::string_view member, std::string_view sig, std::string_view expr, std::string_view comment, bool call );
	std::string literal( std::string_view text );
	std::string base64( const void* data, size_t size );
	std::string bool_str( jboolean value );
	std::string mode_str( jint mode );
	std::string ref_type( jobjectRefType kind );
	std::string show_string( std::string_view text );
	std::string env_text( std::string_view name, std::string_view head, std::string_view extra );
	std::string join( std::string_view a, std::string_view b );
	std::string end_text( jmethodID id, std::string_view value, jobject self = nullptr );
	std::string read_string( JNIEnv* env, jmethodID id, jobject value );
	std::string read_string( JNIEnv* env, jfieldID id, jobject value );
	std::string stored_str( jobject value );
	void save_text( std::string_view text );
	std::string method_text( jmethodID id );
	std::string field_text( jfieldID id );
	std::string va_args( JNIEnv* env, jmethodID id, va_list args );
	std::string jvalue_args( JNIEnv* env, jmethodID id, const jvalue* args );
	std::string class_name( jclass cls );
	jsize array_len( JNIEnv* env, jarray array );
	jsize string_len( JNIEnv* env, jstring str );

	void log( std::string_view expr, std::string_view comment );
	void log_var( std::string_view type, std::string_view expr, std::string_view comment );
	void log_ptr( std::string_view type, std::string_view expr, std::string_view comment );
	void log_obj( JNIEnv* env, jobject ptr, std::string_view type, std::string_view expr, std::string_view comment, ref_kind_t kind );
	void log_id( void* ptr, std::string_view type, std::string_view expr, std::string_view comment );
	void set_method( jmethodID id, std::string_view java, std::string_view name, std::string_view sig );
	void set_field( jfieldID id, std::string_view java, std::string_view name, std::string_view sig );
	void drop_local( jobject obj );
	void drop_global( jobject obj );
	void push_frame( );
	void pop_frame( jobject ret, jobject src );
	void same_ref( jobject dst, jobject src );

private:
	struct jni_raw_t {
		jboolean( *m_exception_check )( JNIEnv* ) = {};
		void ( *m_exception_clear )( JNIEnv* ) = {};
		const char* ( *m_get_string_utf_chars )( JNIEnv*, jstring, jboolean* ) = {};
		void ( *m_release_string_utf_chars )( JNIEnv*, jstring, const char* ) = {};
		jsize( *m_get_string_length )( JNIEnv*, jstring ) = {};
		jsize( *m_get_array_length )( JNIEnv*, jarray ) = {};
		jclass( JNICALL* m_find_class )( JNIEnv*, const char* ) = {};
		jclass( JNICALL* m_get_object_class )( JNIEnv*, jobject ) = {};
		jboolean( JNICALL* m_is_instance_of )( JNIEnv*, jobject, jclass ) = {};
		jmethodID( JNICALL* m_get_method_id )( JNIEnv*, jclass, const char*, const char* ) = {};
		jobject( JNICALL* m_to_reflected_method )( JNIEnv*, jclass, jmethodID, jboolean ) = {};
		jobject( JNICALL* m_to_reflected_field )( JNIEnv*, jclass, jfieldID, jboolean ) = {};
		jobject( JNICALL* m_call_object_a )( JNIEnv*, jobject, jmethodID, const jvalue* ) = {};
		jboolean( JNICALL* m_call_boolean_a )( JNIEnv*, jobject, jmethodID, const jvalue* ) = {};
		void ( JNICALL* m_delete_local )( JNIEnv*, jobject ) = {};
		jobject( JNICALL* m_new_global )( JNIEnv*, jobject ) = {};
		jint( JNICALL* m_push_frame )( JNIEnv*, jint ) = {};
		jobject( JNICALL* m_pop_frame )( JNIEnv*, jobject ) = {};
		jobject( JNICALL* m_get_array_element )( JNIEnv*, jobjectArray, jsize ) = {};
		jobjectRefType( JNICALL* m_ref_type )( JNIEnv*, jobject ) = {};
	};

	struct java_t {
		bool m_ok = {};
		bool m_failed = {};
		jclass m_class = {};
		jclass m_method = {};
		jclass m_constructor = {};
		jclass m_field = {};
		jmethodID m_class_name = {};
		jmethodID m_is_primitive = {};
		jmethodID m_method_name = {};
		jmethodID m_method_types = {};
		jmethodID m_method_return = {};
		jmethodID m_method_class = {};
		jmethodID m_constructor_types = {};
		jmethodID m_constructor_class = {};
		jmethodID m_field_name = {};
		jmethodID m_field_type = {};
		jmethodID m_field_class = {};
	};

	struct object_info_t {
		std::string m_type = {};
		std::string m_comment = {};
		std::string m_class_name = {};
		std::string m_text = {};
	};

	struct param_t {
		char m_tag = {};
		bool m_array = {};
	};

	struct log_t {
		void* m_file = {};
		std::string m_pending = {};
	};

	struct frame_t {
		std::unordered_map< void*, ref_t > m_locals = {};
	};

	struct thread_t {
		bool m_mute = {};
		void* m_caller = {};
		std::vector< frame_t > m_frames = {};
		std::string m_saved = {};
	};

	struct module_cache_t {
		uint32_t m_gen = {};
		uintptr_t m_start = {};
		uintptr_t m_end = {};
		std::string m_name = {};
	};

	thread_t& cur_thread( );
	bool valid_ref( const void* value, bool align = true );
	bool pending( JNIEnv* env );
	void clear( JNIEnv* env );
	object_info_t object_info( jobject value );

	struct jni_frame_t {
		c_writer* m_writer = {};
		JNIEnv* m_env = {};
		bool m_ok = {};

		jni_frame_t( c_writer* writer, JNIEnv* env ) : m_writer( writer ), m_env( env ) {
			if ( writer && writer->m_raw.m_push_frame && writer->m_raw.m_push_frame( env, 64 ) == JNI_OK )
				m_ok = true;
		}

		~jni_frame_t( ) {
			if ( !m_ok || !m_writer || !m_writer->m_raw.m_pop_frame )
				return;

			if ( m_writer->pending( m_env ) )
				m_writer->clear( m_env );
			m_writer->m_raw.m_pop_frame( m_env, nullptr );
		}

		jni_frame_t( const jni_frame_t& ) = delete;
		jni_frame_t& operator=( const jni_frame_t& ) = delete;
	};

	jclass new_global( JNIEnv* env, jclass cls );
	jmethodID find_method( JNIEnv* env, jclass cls, const char* name, const char* sig );
	bool ensure_java( JNIEnv* env );
	std::string read_string( JNIEnv* env, jobject obj, jmethodID id );
	jobject read_ref( JNIEnv* env, jobject obj, jmethodID id );
	jboolean read_bool( JNIEnv* env, jobject obj, jmethodID id );
	std::string read_java( JNIEnv* env, jclass cls );
	std::string read_sig( JNIEnv* env, jclass cls );
	std::string object_type( std::string_view java );
	std::string name_class( JNIEnv* env, jclass cls, bool save );
	void save_ref( JNIEnv* env, jobject obj, std::string name, std::string java );
	id_t find_field( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class );
	std::string read_utf( JNIEnv* env, jstring value );
	std::string read_string( JNIEnv* env, jobject value, std::string_view sig, bool method );
	ref_t* find_ref( void* ptr );
	std::string find_name( void* ptr );
	std::string find_local( void* ptr );
	bool copy_ref( void* ptr, ref_t& out );
	std::string find_java( jclass cls );
	void ensure_frame( );
	void store_local( void* ptr, ref_t&& ref );
	void store_global( void* ptr, ref_t&& ref );
	std::string name_id( std::unordered_map< void*, id_t >& ids, void* ptr, std::string_view type );
	void set_id( std::unordered_map< void*, id_t >& ids, void* ptr, std::string_view java, std::string_view name, std::string_view sig );
	std::string id_text( std::unordered_map< void*, id_t >& ids, void* ptr, bool field );
	void add_arg( std::vector< std::string >& parts, JNIEnv* env, param_t param, jvalue value, bool promoted );
	std::string next_name( std::string_view type );
	std::string seq_name( std::string_view type );
	std::string old_name( std::string_view text, std::string_view name );
	std::string normalize( std::string_view name );
	std::string sanitize( std::string_view text );
	std::string noted( std::string_view expr, std::string_view comment );
	std::string var_line( std::string_view type, std::string_view name, std::string_view expr, std::string_view comment );
	std::string call_line( std::string_view expr, std::string_view comment );
	template< typename T, typename Fn >
	jsize read_len( JNIEnv* env, T value, Fn fn );
	std::vector< param_t > parse_sig( std::string_view sig );
	std::string signature( jmethodID id );
	void line( std::string_view text );
	void write_unlocked( std::string_view text );
	void flush_log( log_t& log );
	bool write_span( void* file, const void* data, size_t size );
	std::string process_name( );
	std::string module_of( void* from );
	std::string cached_module( uintptr_t address );
	void refresh_modules( );
	std::string region_name( void* from );
	log_t* log_for( void* from );

	struct module_range_t {
		uintptr_t m_start = {};
		uintptr_t m_end = {};
		std::string m_name = {};
	};

	jni_raw_t m_raw = {};
	java_t m_java = {};
	std::recursive_mutex m_lock = {};
	std::recursive_mutex m_jni = {};
	std::unordered_map< uint32_t, thread_t > m_threads = {};
	module_cache_t m_cache = {};
	std::unordered_map< void*, ref_t > m_globals = {};
	std::unordered_map< void*, id_t > m_ids = {};
	std::unordered_map< void*, id_t > m_fields = {};
	std::unordered_map< std::string, id_t, text_hash_t, text_eq_t > m_members = {};
	std::unordered_map< std::string, std::string, text_hash_t, text_eq_t > m_classes = {};
	std::unordered_map< std::string, uint32_t, text_hash_t, text_eq_t > m_seq = {};
	std::unordered_map< std::string, std::string, text_hash_t, text_eq_t > m_texts = {};
	std::vector< module_range_t > m_ranges = {};
	uint32_t m_modules_gen = {};
	std::unordered_map< std::string, log_t > m_logs = {};
	std::string m_dir = {};
	std::string m_path = {};
	bool m_saved = {};
};

extern c_writer g_writer;
