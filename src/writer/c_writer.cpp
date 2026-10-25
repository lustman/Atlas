#include "c_writer.hpp"
#include "ldr.hpp"

bool muted( ) {
	std::lock_guard lock( g_writer.m_lock );

	return g_writer.cur_thread( ).m_mute;
}

bool set_mute( bool value ) {
	std::lock_guard lock( g_writer.m_lock );
	auto& slot = g_writer.cur_thread( );
	auto prev = slot.m_mute;
	slot.m_mute = value;

	return prev;
}

void* set_caller( void* from ) {
	std::lock_guard lock( g_writer.m_lock );
	auto& slot = g_writer.cur_thread( );
	auto prev = slot.m_caller;
	slot.m_caller = from;

	return prev;
}

c_writer::thread_t& c_writer::cur_thread( ) {
	return this->m_threads[ ::GetCurrentThreadId( ) ];
}

std::string join_args( const std::vector< std::string >& parts ) {
	std::string out = {};

	for ( size_t i = 0; i < parts.size( ); ++i ) {
		if ( i )
			out += ", ";
		out += parts[ i ];
	}

	return out;
}

c_writer g_writer = {};

bool c_writer::open( ) {
	std::lock_guard lock( this->m_lock );

	if ( !this->m_dir.empty( ) )
		return true;

	auto name = this->process_name( );
	auto dir = std::format( "C:\\Atlas\\{}_{}", name, ::GetCurrentProcessId( ) );

	if ( !::CreateDirectoryA( "C:\\Atlas", nullptr ) && ::GetLastError( ) != ERROR_ALREADY_EXISTS )
		return false;

	if ( !::CreateDirectoryA( dir.c_str( ), nullptr ) && ::GetLastError( ) != ERROR_ALREADY_EXISTS )
		return false;

	this->m_dir = dir;
	this->m_path = dir;

	return true;
}

void c_writer::save_raw( const JNINativeInterface_* table ) {
	if ( !table || this->m_saved )
		return;

	this->m_raw.m_exception_check = table->ExceptionCheck;
	this->m_raw.m_exception_clear = table->ExceptionClear;
	this->m_raw.m_get_string_utf_chars = table->GetStringUTFChars;
	this->m_raw.m_release_string_utf_chars = table->ReleaseStringUTFChars;
	this->m_raw.m_get_string_length = table->GetStringLength;
	this->m_raw.m_get_array_length = table->GetArrayLength;
	this->m_raw.m_find_class = table->FindClass;
	this->m_raw.m_get_object_class = table->GetObjectClass;
	this->m_raw.m_is_instance_of = table->IsInstanceOf;
	this->m_raw.m_get_method_id = table->GetMethodID;
	this->m_raw.m_to_reflected_method = table->ToReflectedMethod;
	this->m_raw.m_to_reflected_field = table->ToReflectedField;
	this->m_raw.m_call_object_a = table->CallObjectMethodA;
	this->m_raw.m_call_boolean_a = table->CallBooleanMethodA;
	this->m_raw.m_delete_local = table->DeleteLocalRef;
	this->m_raw.m_new_global = table->NewGlobalRef;
	this->m_raw.m_push_frame = table->PushLocalFrame;
	this->m_raw.m_pop_frame = table->PopLocalFrame;
	this->m_raw.m_get_array_element = table->GetObjectArrayElement;
	this->m_raw.m_ref_type = table->GetObjectRefType;
	this->m_saved = true;
}

std::string c_writer::object_name( JNIEnv* env, jobject value ) {
	if ( !value )
		return "NULL";

	auto* ptr = reinterpret_cast< void* >( value );
	if ( auto name = this->find_name( ptr ); !name.empty( ) )
		return name;

	auto java = std::string{};
	if ( !muted( ) && env && this->valid_ref( value, false ) && !this->pending( env ) && this->ensure_java( env ) ) {
		c_mute mute = {};
		auto ok = this->m_raw.m_is_instance_of && this->m_java.m_class && this->m_raw.m_is_instance_of( env, value, this->m_java.m_class ) == JNI_TRUE;
		if ( this->pending( env ) ) {
			this->clear( env );
			ok = false;
		}
		if ( ok ) {
			auto name = this->name_class( env, static_cast< jclass >( value ), true );
			if ( name != "NULL" )
				return name;
		}

		if ( this->m_raw.m_get_object_class ) {
			auto* cls = this->m_raw.m_get_object_class( env, value );
			if ( !this->pending( env ) && cls )
				java = this->read_java( env, cls );
			if ( cls && this->m_raw.m_delete_local )
				this->m_raw.m_delete_local( env, cls );
			if ( this->pending( env ) )
				this->clear( env );
		}
	}

	auto info = this->object_info( value );
	if ( !java.empty( ) ) {
		info.m_class_name = java;
		info.m_comment = std::format( "{} {:p}", java, ptr );
		info.m_type = this->object_type( java );
	}

	std::lock_guard lock( this->m_lock );

	if ( auto name = this->find_local( ptr ); !name.empty( ) )
		return name;

	if ( auto found = this->m_globals.find( ptr ); found != this->m_globals.end( ) )
		return found->second.m_name;

	auto used = info.m_type.empty( ) ? std::string{ "jobject" } : info.m_type;
	auto name = this->seq_name( used );
	ref_t ref = {};

	ref.m_name = name;
	ref.m_class_name = std::move( info.m_class_name );
	ref.m_text = std::move( info.m_text );
	ref.m_kind = ref_kind_t::local;
	this->ensure_frame( );
	this->cur_thread( ).m_frames.back( ).m_locals.insert_or_assign( ptr, std::move( ref ) );

	auto note = info.m_comment.empty( ) ? std::string{ "external" } : std::format( "external {}", info.m_comment );
	this->write_unlocked( this->var_line( used, name, "NULL", note ) );

	return name;
}

std::string c_writer::method_name( jmethodID id ) {
	return this->name_id( this->m_ids, reinterpret_cast< void* >( id ), "jmethodID" );
}

std::string c_writer::field_name( jfieldID id ) {
	return this->name_id( this->m_fields, reinterpret_cast< void* >( id ), "jfieldID" );
}

std::string c_writer::literal( std::string_view text ) {
	auto plain = true;
	for ( auto ch : text ) {
		auto c = static_cast< unsigned char >( ch );
		if ( c < 32 || c >= 127 ) {
			plain = false;
			break;
		}
	}
	if ( !plain )
		return std::format( "b64_decode(\"{}\")", this->base64( text.data( ), text.size( ) ) );

	std::string out = { "\"" };
	for ( auto ch : text ) {
		if ( ch == '\\' || ch == '"' )
			out.push_back( '\\' );

		out.push_back( ch );
	}
	out.push_back( '"' );

	return out;
}

std::string c_writer::base64( const void* data, size_t size ) {
	if ( !data || !size )
		return {};

	static constexpr char table[ ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	auto* bytes = static_cast< const uint8_t* >( data );
	std::string out = {};
	out.reserve( ( size + 2 ) / 3 * 4 );
	for ( size_t i = 0; i < size; i += 3 ) {
		uint32_t n = static_cast< uint32_t >( bytes[ i ] ) << 16;
		if ( i + 1 < size )
			n |= static_cast< uint32_t >( bytes[ i + 1 ] ) << 8;
		if ( i + 2 < size )
			n |= static_cast< uint32_t >( bytes[ i + 2 ] );
		out.push_back( table[ ( n >> 18 ) & 63 ] );
		out.push_back( table[ ( n >> 12 ) & 63 ] );
		out.push_back( i + 1 < size ? table[ ( n >> 6 ) & 63 ] : '=' );
		out.push_back( i + 2 < size ? table[ n & 63 ] : '=' );
	}

	return out;
}

std::string c_writer::bool_str( jboolean value ) { return value ? "JNI_TRUE" : "JNI_FALSE"; }

std::string c_writer::mode_str( jint mode ) {
	if ( mode == 0 )
		return "0";

	if ( mode == JNI_COMMIT )
		return "JNI_COMMIT";

	if ( mode == JNI_ABORT )
		return "JNI_ABORT";

	return std::format( "{}", mode );
}

std::string c_writer::ref_type( jobjectRefType kind ) {
	switch ( kind ) {
	case JNIInvalidRefType:
		return "JNIInvalidRefType";
	case JNILocalRefType:
		return "JNILocalRefType";
	case JNIGlobalRefType:
		return "JNIGlobalRefType";
	case JNIWeakGlobalRefType:
		return "JNIWeakGlobalRefType";
	default:
		return std::format( "{}", static_cast< int32_t >( kind ) );
	}
}

std::string c_writer::show_string( std::string_view text ) {
	auto plain = true;
	for ( auto ch : text ) {
		auto c = static_cast< unsigned char >( ch );
		if ( c < 32 || c >= 127 ) {
			plain = false;
			break;
		}
	}
	if ( plain )
		return this->sanitize( text );

	return std::format( "b64:{}", this->base64( text.data( ), text.size( ) ) );
}

std::string c_writer::env_text( std::string_view name, std::string_view head, std::string_view extra ) {
	if ( extra.empty( ) )
		return std::format( "env->{}({})", name, head );

	return std::format( "env->{}({}, {})", name, head, extra );
}

std::string c_writer::join( std::string_view a, std::string_view b ) {
	if ( a.empty( ) )
		return std::string{ b };

	if ( b.empty( ) )
		return std::string{ a };

	return std::format( "{} {}", a, b );
}



std::string c_writer::end_text( JNIEnv* env, jmethodID id, std::string_view value, jobject self ) {
	std::string out = {};
	auto add = [ & ]( std::string_view part ) {
		if ( part.empty( ) )
			return;

		if ( !out.empty( ) )
			out.push_back( ' ' );

		out.append( part );
	};

	add( this->method_text( id ) );
	add( value );

	if ( self ) {
		auto owned = this->stored_str( self );
		if ( !owned.empty( ) )
			owned = this->show_string( owned );
		if ( !owned.empty( ) && owned != value )
			add( owned );
	}


	return out;
}

std::string c_writer::stored_str( jobject value ) {
	if ( !value )
		return {};

	ref_t ref = {};
	if ( !this->copy_ref( reinterpret_cast< void* >( value ), ref ) )
		return {};

	return ref.m_text;
}

void c_writer::save_text( std::string_view text ) {
	std::lock_guard lock( this->m_lock );
	this->cur_thread( ).m_saved = std::string{ text };
}

std::string c_writer::read_string( JNIEnv* env, jmethodID id, jobject value ) {
	auto sig = id ? this->signature( id ) : std::string{};

	return this->read_string( env, value, sig, true );
}

std::string c_writer::read_string( JNIEnv* env, jfieldID id, jobject value ) {
	auto sig = std::string{};
	if ( id ) {
		std::lock_guard lock( this->m_lock );
		auto found = this->m_fields.find( reinterpret_cast< void* >( id ) );
		if ( found != this->m_fields.end( ) )
			sig = found->second.m_sig;
	}

	return this->read_string( env, value, sig, false );
}

std::string c_writer::method_text( jmethodID id ) {
	return this->id_text( this->m_ids, reinterpret_cast< void* >( id ), false );
}

std::string c_writer::field_text( jfieldID id ) {
	return this->id_text( this->m_fields, reinterpret_cast< void* >( id ), true );
}

std::string value_text( char tag, jvalue value, bool promoted ) {
	switch ( tag ) {
	case 'Z':
		return ( promoted ? value.i != 0 : value.z != 0 ) ? "JNI_TRUE" : "JNI_FALSE";
	case 'B':
		return std::format( "{}", static_cast< int32_t >( promoted ? static_cast< jbyte >( value.i ) : value.b ) );
	case 'C':
		return std::format( "{}", static_cast< uint32_t >( promoted ? static_cast< jchar >( value.i ) : value.c ) );
	case 'S':
		return std::format( "{}", static_cast< int32_t >( promoted ? static_cast< jshort >( value.i ) : value.s ) );
	case 'I':
		return std::format( "{}", value.i );
	case 'J':
		return std::format( "{}LL", value.j );
	case 'F':
		return std::format( "{:g}", promoted ? value.d : static_cast< double >( value.f ) );
	case 'D':
		return std::format( "{:g}", value.d );
	default:
		return "/*?*/";
	}
}

jvalue read_va_arg( va_list args, char tag, bool object ) {
	jvalue value = {};
	if ( object )
		value.l = va_arg( args, jobject );
	else if ( tag == 'J' )
		value.j = va_arg( args, jlong );
	else if ( tag == 'F' || tag == 'D' )
		value.d = va_arg( args, jdouble );
	else
		value.i = va_arg( args, jint );

	return value;
}

std::string c_writer::va_args( JNIEnv* env, jmethodID id, va_list args ) {
	auto sig = this->signature( id );
	if ( sig.empty( ) )
		return "/* unknown args */";

	auto params = this->parse_sig( sig );
	if ( params.empty( ) )
		return {};

	std::vector< std::string > parts = {};
	parts.reserve( params.size( ) );
	for ( auto& param : params ) {
		auto object = param.m_array || param.m_tag == 'L';
		this->add_arg( parts, env, param, read_va_arg( args, param.m_tag, object ), true );
	}

	return join_args( parts );
}

std::string c_writer::jvalue_args( JNIEnv* env, jmethodID id, const jvalue* args ) {
	auto sig = this->signature( id );
	if ( sig.empty( ) )
		return "/* unknown args */";

	auto params = this->parse_sig( sig );
	if ( params.empty( ) )
		return {};

	if ( !args )
		return "/* null args */";

	std::vector< std::string > parts = {};
	parts.reserve( params.size( ) );
	for ( size_t i = 0; i < params.size( ); ++i )
		this->add_arg( parts, env, params[ i ], args[ i ], false );

	return join_args( parts );
}

bool c_writer::valid_ref( const void* value, bool align ) {
	auto address = reinterpret_cast< uintptr_t >( value );
	if ( align && ( address & 7 ) != 0 )
		return false;

	return address >= 0x10000 && address <= 0x00007FFFFFFFFFFFull;
}

jclass c_writer::new_global( JNIEnv* env, jclass cls ) {
	if ( !cls || this->pending( env ) )
		return nullptr;

	auto* global = static_cast< jclass >( this->m_raw.m_new_global( env, cls ) );
	this->m_raw.m_delete_local( env, cls );
	if ( this->pending( env ) ) {
		this->clear( env );
		return nullptr;
	}

	return global;
}

jmethodID c_writer::find_method( JNIEnv* env, jclass cls, const char* name, const char* sig ) {
	if ( !cls )
		return nullptr;

	auto id = this->m_raw.m_get_method_id( env, cls, name, sig );
	if ( this->pending( env ) ) {
		this->clear( env );
		return nullptr;
	}

	return id;
}

bool c_writer::ensure_java( JNIEnv* env ) {
	{
		std::lock_guard lock( this->m_lock );
		if ( this->m_java.m_ok )
			return true;
		if ( this->m_java.m_failed )
			return false;
	}
	if ( !env || this->pending( env ) || !this->m_raw.m_find_class || !this->m_raw.m_get_method_id || !this->m_raw.m_new_global || !this->m_raw.m_delete_local )
		return false;

	c_mute mute = {};
	std::lock_guard jni( this->m_jni );
	{
		std::lock_guard lock( this->m_lock );
		if ( this->m_java.m_ok )
			return true;
		if ( this->m_java.m_failed )
			return false;
	}

	auto* lang = this->new_global( env, this->m_raw.m_find_class( env, "java/lang/Class" ) );
	auto* method = this->new_global( env, this->m_raw.m_find_class( env, "java/lang/reflect/Method" ) );
	auto* ctor = this->new_global( env, this->m_raw.m_find_class( env, "java/lang/reflect/Constructor" ) );
	auto* field = this->new_global( env, this->m_raw.m_find_class( env, "java/lang/reflect/Field" ) );
	if ( !lang || !method || !ctor || !field ) {
		if ( this->pending( env ) )
			this->clear( env );
		std::lock_guard lock( this->m_lock );
		this->m_java.m_failed = true;
		return false;
	}

	java_t got = {};
	got.m_class = lang;
	got.m_method = method;
	got.m_constructor = ctor;
	got.m_field = field;
	got.m_class_name = this->find_method( env, lang, "getName", "()Ljava/lang/String;" );
	got.m_is_primitive = this->find_method( env, lang, "isPrimitive", "()Z" );
	got.m_method_name = this->find_method( env, method, "getName", "()Ljava/lang/String;" );
	got.m_method_types = this->find_method( env, method, "getParameterTypes", "()[Ljava/lang/Class;" );
	got.m_method_return = this->find_method( env, method, "getReturnType", "()Ljava/lang/Class;" );
	got.m_method_class = this->find_method( env, method, "getDeclaringClass", "()Ljava/lang/Class;" );
	got.m_constructor_types = this->find_method( env, ctor, "getParameterTypes", "()[Ljava/lang/Class;" );
	got.m_constructor_class = this->find_method( env, ctor, "getDeclaringClass", "()Ljava/lang/Class;" );
	got.m_field_name = this->find_method( env, field, "getName", "()Ljava/lang/String;" );
	got.m_field_type = this->find_method( env, field, "getType", "()Ljava/lang/Class;" );
	got.m_field_class = this->find_method( env, field, "getDeclaringClass", "()Ljava/lang/Class;" );
	if ( !got.m_class_name || !got.m_is_primitive || !got.m_method_name || !got.m_method_types || !got.m_method_return || !got.m_method_class || !got.m_constructor_types || !got.m_constructor_class || !got.m_field_name || !got.m_field_type || !got.m_field_class ) {
		if ( this->pending( env ) )
			this->clear( env );
		std::lock_guard lock( this->m_lock );
		this->m_java.m_failed = true;
		return false;
	}

	got.m_ok = true;
	std::lock_guard lock( this->m_lock );
	this->m_java = got;
	return true;
}

std::string c_writer::read_string( JNIEnv* env, jobject obj, jmethodID id ) {
	if ( !env || !obj || !id || !this->m_raw.m_call_object_a )
		return {};

	auto* value = static_cast< jstring >( this->m_raw.m_call_object_a( env, obj, id, nullptr ) );
	if ( !value || this->pending( env ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		if ( value && this->m_raw.m_delete_local )
			this->m_raw.m_delete_local( env, value );
		return {};
	}

	auto text = this->read_utf( env, value );
	if ( this->m_raw.m_delete_local )
		this->m_raw.m_delete_local( env, value );
	return text;
}

jobject c_writer::read_ref( JNIEnv* env, jobject obj, jmethodID id ) {
	if ( !env || !obj || !id || !this->m_raw.m_call_object_a )
		return nullptr;

	auto* value = this->m_raw.m_call_object_a( env, obj, id, nullptr );
	if ( this->pending( env ) ) {
		this->clear( env );
		if ( value && this->m_raw.m_delete_local )
			this->m_raw.m_delete_local( env, value );
		return nullptr;
	}

	return value;
}

jboolean c_writer::read_bool( JNIEnv* env, jobject obj, jmethodID id ) {
	if ( !env || !obj || !id || !this->m_raw.m_call_boolean_a )
		return JNI_FALSE;

	auto value = this->m_raw.m_call_boolean_a( env, obj, id, nullptr );
	if ( this->pending( env ) ) {
		this->clear( env );
		return JNI_FALSE;
	}

	return value;
}

std::string c_writer::read_java( JNIEnv* env, jclass cls ) {
	if ( !cls || !this->m_java.m_class_name )
		return {};

	auto text = this->read_string( env, cls, this->m_java.m_class_name );
	if ( text.empty( ) || text == "boolean" || text == "byte" || text == "char" || text == "short" || text == "int" || text == "long" || text == "float" || text == "double" || text == "void" )
		return {};

	for ( auto& ch : text )
		if ( ch == '.' )
			ch = '/';

	return text;
}

std::string c_writer::read_sig( JNIEnv* env, jclass cls ) {
	if ( !cls )
		return {};

	if ( this->read_bool( env, cls, this->m_java.m_is_primitive ) == JNI_TRUE ) {
		auto name = this->read_string( env, cls, this->m_java.m_class_name );
		if ( name == "boolean" )
			return "Z";
		if ( name == "byte" )
			return "B";
		if ( name == "char" )
			return "C";
		if ( name == "short" )
			return "S";
		if ( name == "int" )
			return "I";
		if ( name == "long" )
			return "J";
		if ( name == "float" )
			return "F";
		if ( name == "double" )
			return "D";
		if ( name == "void" )
			return "V";
		return {};
	}

	auto name = this->read_java( env, cls );
	if ( name.empty( ) )
		return {};
	if ( name.front( ) == '[' )
		return name;

	return std::format( "L{};", name );
}

std::string c_writer::object_type( std::string_view java ) {
	if ( java == "java/lang/String" )
		return "jstring";
	if ( java == "[Z" )
		return "jbooleanArray";
	if ( java == "[B" )
		return "jbyteArray";
	if ( java == "[C" )
		return "jcharArray";
	if ( java == "[S" )
		return "jshortArray";
	if ( java == "[I" )
		return "jintArray";
	if ( java == "[J" )
		return "jlongArray";
	if ( java == "[F" )
		return "jfloatArray";
	if ( java == "[D" )
		return "jdoubleArray";
	if ( !java.empty( ) && java.front( ) == '[' )
		return "jobjectArray";

	return "jobject";
}

void c_writer::save_ref( JNIEnv* env, jobject obj, std::string name, std::string java ) {
	if ( !obj || name.empty( ) )
		return;
	if ( this->find_name( reinterpret_cast< void* >( obj ) ) == name )
		return;

	ref_t ref = {};
	ref.m_name = std::move( name );
	ref.m_class_name = std::move( java );
	ref.m_kind = ref_kind_t::local;
	if ( env && this->m_raw.m_ref_type ) {
		auto kind = this->m_raw.m_ref_type( env, obj );
		if ( kind == JNIGlobalRefType )
			ref.m_kind = ref_kind_t::global;
		else if ( kind == JNIWeakGlobalRefType )
			ref.m_kind = ref_kind_t::weak;
	}
	if ( ref.m_kind == ref_kind_t::local )
		this->store_local( reinterpret_cast< void* >( obj ), std::move( ref ) );
	else
		this->store_global( reinterpret_cast< void* >( obj ), std::move( ref ) );
}

std::string c_writer::name_class( JNIEnv* env, jclass cls, bool save ) {
	if ( !cls )
		return "NULL";

	auto* ptr = reinterpret_cast< void* >( cls );
	if ( auto name = this->find_name( ptr ); !name.empty( ) )
		return name;

	auto java = this->read_java( env, cls );
	if ( java.empty( ) ) {
		if ( !save || muted( ) )
			return "NULL";
		return this->object_name( env, cls );
	}

	auto known = std::string{};
	{
		std::lock_guard lock( this->m_lock );
		if ( auto found = this->m_classes.find( java ); found != this->m_classes.end( ) )
			known = found->second;
	}
	if ( !known.empty( ) ) {
		if ( save )
			this->save_ref( env, cls, known, java );
		return known;
	}

	if ( !save ) {
		auto name = this->next_name( "jclass" );
		this->line( this->var_line( "jclass", name, std::format( "env->FindClass({})", this->literal( java ) ), std::format( "{} {:p}", java, ptr ) ) );
		std::lock_guard lock( this->m_lock );
		this->m_classes.emplace( java, name );
		return name;
	}

	auto kind = ref_kind_t::local;
	if ( this->m_raw.m_ref_type ) {
		auto got = this->m_raw.m_ref_type( env, cls );
		if ( got == JNIGlobalRefType )
			kind = ref_kind_t::global;
		else if ( got == JNIWeakGlobalRefType )
			kind = ref_kind_t::weak;
	}
	this->log_obj( env, cls, "jclass", std::format( "env->FindClass({})", this->literal( java ) ), java, kind );
	auto name = this->find_name( ptr );
	if ( name.empty( ) )
		return "NULL";

	std::lock_guard lock( this->m_lock );
	this->m_classes.emplace( java, name );
	return name;
}

void c_writer::log_field( jfieldID id, bool is_static, std::string_view java, std::string_view member, std::string_view sig, std::string_view expr, std::string_view comment, bool call ) {
	if ( !id )
		return;

	auto* ptr = reinterpret_cast< void* >( id );
	if ( is_static ) {
		this->set_field( id, java, member, sig );
		{
			std::lock_guard lock( this->m_lock );
			auto found = this->m_fields.find( ptr );
			if ( found != this->m_fields.end( ) && !found->second.m_name.empty( ) ) {
				if ( call )
					this->write_unlocked( this->call_line( expr, this->join( comment, found->second.m_name ) ) );
				return;
			}
		}
		this->log_id( ptr, "jfieldID", expr, comment );
		return;
	}

	auto key = std::format( "{}\n{:p}", java, ptr );
	std::lock_guard lock( this->m_lock );
	auto& slot = this->m_members[ key ];
	if ( !slot.m_name.empty( ) ) {
		if ( call )
			this->write_unlocked( this->call_line( expr, this->join( comment, slot.m_name ) ) );
		return;
	}

	slot.m_class_name = std::string{ java };
	slot.m_member = std::string{ member };
	slot.m_sig = std::string{ sig };
	slot.m_name = this->seq_name( "jfieldID" );
	this->write_unlocked( this->var_line( "jfieldID", slot.m_name, expr, comment ) );
}

std::string c_writer::field_name( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class ) {
	auto got = this->find_field( env, obj, id, is_static, is_class );
	if ( !got.m_name.empty( ) )
		return got.m_name;

	return this->field_name( id );
}

std::string c_writer::field_text( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class ) {
	auto got = this->find_field( env, obj, id, is_static, is_class );
	if ( got.m_name.empty( ) )
		return this->field_text( id );

	return std::format( "{} {} {}", got.m_class_name, got.m_member, got.m_sig );
}

std::string c_writer::read_string( JNIEnv* env, jobject obj, jfieldID id, bool is_static, jobject value, bool is_class ) {
	auto got = this->find_field( env, obj, id, is_static, is_class );
	if ( got.m_sig.empty( ) )
		return this->read_string( env, id, value );

	return this->read_string( env, value, got.m_sig, false );
}

id_t c_writer::find_field( JNIEnv* env, jobject obj, jfieldID id, bool is_static, bool is_class ) {
	if ( !id )
		return {};

	auto* ptr = reinterpret_cast< void* >( id );
	if ( is_static ) {
		std::lock_guard lock( this->m_lock );
		if ( auto found = this->m_fields.find( ptr ); found != this->m_fields.end( ) && !found->second.m_name.empty( ) && !found->second.m_sig.empty( ) )
			return found->second;
	}

	if ( !env || !obj || !this->valid_ref( obj, false ) || this->pending( env ) || muted( ) || !this->ensure_java( env ) || !this->m_raw.m_to_reflected_field )
		return {};

	c_mute mute = {};
	std::lock_guard jni( this->m_jni );
	jni_frame_t frame( this, env );
	if ( !frame.m_ok )
		return {};

	jclass cls = {};
	jclass drop = {};
	if ( is_static || is_class )
		cls = static_cast< jclass >( obj );
	else if ( this->m_raw.m_get_object_class ) {
		cls = this->m_raw.m_get_object_class( env, obj );
		drop = cls;
	}
	if ( !cls || this->pending( env ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		return {};
	}
	auto prev = this->read_java( env, cls );
	if ( !is_static && !prev.empty( ) ) {
		std::lock_guard lock( this->m_lock );
		if ( auto found = this->m_members.find( std::format( "{}\n{:p}", prev, ptr ) ); found != this->m_members.end( ) && !found->second.m_name.empty( ) && !found->second.m_sig.empty( ) )
			return found->second;
	}

	auto address = reinterpret_cast< uintptr_t >( id );
	if ( is_static ) {
		if ( !this->valid_ref( id, false ) )
			return {};
	} else if ( ( address & 2 ) == 0 ) {
		return {};
	}

	auto* reflected = this->m_raw.m_to_reflected_field( env, cls, id, is_static ? JNI_TRUE : JNI_FALSE );
	if ( !reflected || this->pending( env ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		return {};
	}

	auto nm = this->read_string( env, reflected, this->m_java.m_field_name );
	auto* type = static_cast< jclass >( this->read_ref( env, reflected, this->m_java.m_field_type ) );
	auto sg = this->read_sig( env, type );
	auto* decl = static_cast< jclass >( this->read_ref( env, reflected, this->m_java.m_field_class ) );
	auto java = decl ? this->read_java( env, decl ) : prev;
	if ( nm.empty( ) || sg.empty( ) || java.empty( ) )
		return {};

	auto* got = decl ? decl : cls;
	auto save = false;
	if ( is_static && ( prev.empty( ) || prev == java ) ) {
		got = cls;
		save = true;
	}
	auto name = this->name_class( env, got, save );
	if ( name == "NULL" )
		return {};

	auto api = is_static ? "GetStaticFieldID" : "GetFieldID";
	auto expr = std::format( "env->{}({}, {}, {})", api, name, this->literal( nm ), this->literal( sg ) );
	auto text = std::format( "{} {} {}", java, nm, sg );
	this->log_field( id, is_static, java, nm, sg, expr, text, false );

	id_t out = {};
	{
		std::lock_guard lock( this->m_lock );
		if ( is_static ) {
			if ( auto found = this->m_fields.find( ptr ); found != this->m_fields.end( ) )
				out = found->second;
		} else {
			auto key = std::format( "{}\n{:p}", java, ptr );
			if ( auto found = this->m_members.find( key ); found != this->m_members.end( ) ) {
				out = found->second;
				auto prev_key = std::format( "{}\n{:p}", prev.empty( ) ? java : prev, ptr );
				if ( prev_key != key )
					this->m_members.emplace( std::move( prev_key ), out );
			}
		}
	}
	if ( drop && this->m_raw.m_delete_local )
		this->m_raw.m_delete_local( env, drop );
	return out;
}

void c_writer::method_name( JNIEnv* env, jobject obj, jclass clazz, jmethodID id, bool is_static ) {
	if ( !env || !id || !this->valid_ref( id, false ) )
		return;

	auto* ptr = reinterpret_cast< void* >( id );
	auto known = false;
	{
		std::lock_guard lock( this->m_lock );
		if ( auto found = this->m_ids.find( ptr ); found != this->m_ids.end( ) ) {
			if ( !found->second.m_name.empty( ) && !found->second.m_sig.empty( ) )
				return;
			known = !found->second.m_name.empty( );
		}
	}
	if ( this->pending( env ) || muted( ) || !this->ensure_java( env ) || !this->m_raw.m_to_reflected_method )
		return;

	c_mute mute = {};
	std::lock_guard jni( this->m_jni );
	{
		std::lock_guard lock( this->m_lock );
		if ( auto found = this->m_ids.find( ptr ); found != this->m_ids.end( ) && !found->second.m_name.empty( ) && !found->second.m_sig.empty( ) )
			return;
	}

	jni_frame_t frame( this, env );
	if ( !frame.m_ok )
		return;

	auto* cls = clazz;
	if ( !cls && obj && !is_static && this->m_raw.m_get_object_class )
		cls = this->m_raw.m_get_object_class( env, obj );
	if ( !cls || this->pending( env ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		return;
	}

	auto* reflected = this->m_raw.m_to_reflected_method( env, cls, id, is_static ? JNI_TRUE : JNI_FALSE );
	if ( !reflected || this->pending( env ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		return;
	}

	auto init = this->m_raw.m_is_instance_of && this->m_raw.m_is_instance_of( env, reflected, this->m_java.m_constructor ) == JNI_TRUE;
	if ( this->pending( env ) ) {
		this->clear( env );
		return;
	}

	auto nm = init ? std::string{ "<init>" } : this->read_string( env, reflected, this->m_java.m_method_name );
	auto* params = static_cast< jobjectArray >( this->read_ref( env, reflected, init ? this->m_java.m_constructor_types : this->m_java.m_method_types ) );
	if ( !params || !this->m_raw.m_get_array_length || !this->valid_ref( params, false ) ) {
		if ( this->pending( env ) )
			this->clear( env );
		return;
	}
	auto sg = std::string{ "(" };
	auto count = this->m_raw.m_get_array_length( env, params );
	if ( this->pending( env ) ) {
		this->clear( env );
		return;
	}
	auto ok = true;
	for ( jsize i = 0; i < count; ++i ) {
		auto* item = static_cast< jclass >( this->m_raw.m_get_array_element ? this->m_raw.m_get_array_element( env, params, i ) : nullptr );
		auto part = this->read_sig( env, item );
		if ( item && this->m_raw.m_delete_local )
			this->m_raw.m_delete_local( env, item );
		if ( part.empty( ) ) {
			ok = false;
			break;
		}
		sg += part;
	}
	sg.push_back( ')' );
	if ( init )
		sg.push_back( 'V' );
	else {
		auto* ret = static_cast< jclass >( this->read_ref( env, reflected, this->m_java.m_method_return ) );
		auto part = this->read_sig( env, ret );
		if ( part.empty( ) )
			ok = false;
		else
			sg += part;
	}
	auto* decl = static_cast< jclass >( this->read_ref( env, reflected, init ? this->m_java.m_constructor_class : this->m_java.m_method_class ) );
	auto java = decl ? this->read_java( env, decl ) : this->read_java( env, cls );
	if ( !ok || nm.empty( ) || java.empty( ) )
		return;

	auto prev = this->read_java( env, cls );
	auto* got = decl ? decl : cls;
	auto save = false;
	if ( clazz && ( prev.empty( ) || prev == java ) ) {
		got = clazz;
		save = true;
	}
	auto name = this->name_class( env, got, save );
	if ( name == "NULL" )
		return;

	this->set_method( id, java, nm, sg );
	if ( known )
		return;

	auto api = is_static ? "GetStaticMethodID" : "GetMethodID";
	this->log_id( ptr, "jmethodID", std::format( "env->{}({}, {}, {})", api, name, this->literal( nm ), this->literal( sg ) ), std::format( "{} {}{}", java, nm, sg ) );
}

jsize c_writer::array_len( JNIEnv* env, jarray array ) {
	if ( !env || !this->valid_ref( array ) || !this->m_raw.m_get_array_length )
		return 0;

	c_mute mute = {};
	if ( this->pending( env ) )
		return 0;

	auto len = this->m_raw.m_get_array_length( env, array );
	if ( this->pending( env ) ) {
		this->clear( env );

		return 0;
	}

	return len;
}

jsize c_writer::string_len( JNIEnv* env, jstring str ) {
	if ( !env || !this->valid_ref( str ) || !this->m_raw.m_get_string_length )
		return 0;

	c_mute mute = {};
	if ( this->pending( env ) )
		return 0;

	auto len = this->m_raw.m_get_string_length( env, str );
	if ( this->pending( env ) ) {
		this->clear( env );

		return 0;
	}

	return len;
}

void c_writer::log( std::string_view expr, std::string_view comment ) { this->line( this->call_line( expr, comment ) ); }

void c_writer::log_var( std::string_view type, std::string_view expr, std::string_view comment ) {
	auto name = this->next_name( type );
	this->line( this->var_line( type, name, expr, comment ) );
}

void c_writer::log_ptr( std::string_view type, std::string_view expr, std::string_view comment ) {
	auto name = this->next_name( "native" );
	this->line( this->var_line( type, name, expr, comment ) );
}

void c_writer::log_obj( JNIEnv* env, jobject ptr, std::string_view type, std::string_view expr, std::string_view comment, ref_kind_t kind ) {
	if ( !ptr ) {
		{
			std::lock_guard lock( this->m_lock );
			this->cur_thread( ).m_saved = {};
		}
		this->log_var( type, expr, comment.empty( ) ? std::string_view{ "NULL" } : comment );

		return;
	}
	auto info = this->object_info( ptr );
	auto used = std::string{ type };
	if ( ( type == "jobject" || type == "jarray" ) && !info.m_type.empty( ) && info.m_type != "jobject" )
		used = info.m_type;
	auto name = this->next_name( used );
	ref_t ref = {};
	ref.m_name = name;
	ref.m_kind = kind;
	if ( used == "jclass" && !comment.empty( ) && comment.find( ' ' ) == std::string_view::npos )
		ref.m_class_name = this->normalize( comment );
	else
		ref.m_class_name = std::move( info.m_class_name );
	if ( ref.m_class_name.empty( ) && used == "jclass" )
		ref.m_class_name = this->class_name( env, static_cast< jclass >( ptr ) );
	auto java = ref.m_class_name;
	{
		std::lock_guard lock( this->m_lock );
		auto& saved = this->cur_thread( ).m_saved;
		if ( info.m_text.empty( ) && !saved.empty( ) )
			info.m_text = std::move( saved );
		saved = {};
	}
	ref.m_text = info.m_text;
	if ( kind == ref_kind_t::local )
		this->store_local( reinterpret_cast< void* >( ptr ), std::move( ref ) );
	else
		this->store_global( reinterpret_cast< void* >( ptr ), std::move( ref ) );
	auto note = std::string{ comment };
	if ( info.m_text.empty( ) && !info.m_comment.empty( ) && info.m_comment != comment )
		note = this->join( note, info.m_comment );
	if ( used == "jclass" && !java.empty( ) && note.find( java ) == std::string::npos )
		note = this->join( note, java );
	if ( !info.m_text.empty( ) ) {
		auto shown = this->show_string( info.m_text );
		auto prev = this->old_name( info.m_text, name );
		auto seen = !prev.empty( ) && prev != name && note.find( prev ) == std::string::npos && note.find( shown ) == std::string::npos;
		if ( seen )
			note = this->join( note, std::format( "same as {}", prev ) );
		else if ( !shown.empty( ) && note.find( shown ) == std::string::npos )
			note = this->join( note, shown );
	}
	this->line( this->var_line( used, name, expr, note ) );
}

void c_writer::log_id( void* ptr, std::string_view type, std::string_view expr, std::string_view comment ) {
	if ( !ptr ) {
		this->log_var( type, expr, comment.empty( ) ? std::string_view{ "NULL" } : comment );

		return;
	}
	std::lock_guard lock( this->m_lock );
	auto& map = type == "jfieldID" ? this->m_fields : this->m_ids;
	auto& slot = map[ ptr ];
	if ( !slot.m_name.empty( ) ) {
		this->write_unlocked( this->call_line( expr, this->join( comment, slot.m_name ) ) );

		return;
	}
	slot.m_name = this->seq_name( type );
	this->write_unlocked( this->var_line( type, slot.m_name, expr, comment ) );
}

void c_writer::set_method( jmethodID id, std::string_view java, std::string_view name, std::string_view sig ) {
	this->set_id( this->m_ids, reinterpret_cast< void* >( id ), java, name, sig );
}

void c_writer::set_field( jfieldID id, std::string_view java, std::string_view name, std::string_view sig ) {
	this->set_id( this->m_fields, reinterpret_cast< void* >( id ), java, name, sig );
}

void c_writer::drop_local( jobject obj ) {
	if ( !obj )
		return;

	auto* ptr = reinterpret_cast< void* >( obj );
	std::lock_guard lock( this->m_lock );
	for ( auto& frame : this->cur_thread( ).m_frames )
		frame.m_locals.erase( ptr );
}

void c_writer::drop_global( jobject obj ) {
	if ( !obj )
		return;

	std::lock_guard lock( this->m_lock );
	this->m_globals.erase( reinterpret_cast< void* >( obj ) );
}

void c_writer::push_frame( ) {
	std::lock_guard lock( this->m_lock );
	this->cur_thread( ).m_frames.emplace_back( );
}

void c_writer::pop_frame( jobject ret, jobject src ) {
	ref_t ref = {};
	std::lock_guard lock( this->m_lock );
	auto have = src && this->copy_ref( reinterpret_cast< void* >( src ), ref ) && !ref.m_name.empty( ) && ref.m_name != "NULL";
	auto& frames = this->cur_thread( ).m_frames;
	if ( !frames.empty( ) )
		frames.pop_back( );

	this->ensure_frame( );
	if ( !ret || !have )
		return;

	ref.m_kind = ref_kind_t::local;
	frames.back( ).m_locals.insert_or_assign( reinterpret_cast< void* >( ret ), std::move( ref ) );
}

void c_writer::same_ref( jobject dst, jobject src ) {
	if ( !dst || !src )
		return;

	ref_t ref = {};
	if ( !this->copy_ref( reinterpret_cast< void* >( src ), ref ) || ref.m_name.empty( ) )
		return;

	auto* ptr = reinterpret_cast< void* >( dst );
	if ( ref.m_kind == ref_kind_t::local )
		this->store_local( ptr, std::move( ref ) );
	else
		this->store_global( ptr, std::move( ref ) );
}

bool c_writer::pending( JNIEnv* env ) {
	if ( !env || !this->m_raw.m_exception_check )
		return false;

	return this->m_raw.m_exception_check( env ) == JNI_TRUE;
}

void c_writer::clear( JNIEnv* env ) {
	if ( !env || !this->m_raw.m_exception_clear )
		return;

	this->m_raw.m_exception_clear( env );
}

c_writer::object_info_t c_writer::object_info( jobject value ) {
	object_info_t info = {};
	info.m_type = "jobject";
	if ( !value )
		return info;

	if ( auto known = this->find_java( static_cast< jclass >( value ) ); !known.empty( ) ) {
		info.m_type = "jclass";
		info.m_class_name = known;
		info.m_comment = known;

		return info;
	}

	info.m_comment = std::format( "{:p}", static_cast< void* >( value ) );

	return info;
}

std::string c_writer::class_name( JNIEnv*, jclass cls ) {
	return this->find_java( cls );
}

std::string c_writer::read_utf( JNIEnv* env, jstring value ) {
	if ( !this->valid_ref( value ) || !this->m_raw.m_get_string_utf_chars )
		return {};

	auto* chars = this->m_raw.m_get_string_utf_chars( env, value, nullptr );
	if ( this->pending( env ) ) {
		this->clear( env );

		return {};
	}
	std::string text = chars ? chars : "";
	if ( chars && this->m_raw.m_release_string_utf_chars )
		this->m_raw.m_release_string_utf_chars( env, value, chars );

	return text;
}

std::string c_writer::read_string( JNIEnv* env, jobject value, std::string_view sig, bool method ) {
	{
		std::lock_guard lock( this->m_lock );
		this->cur_thread( ).m_saved = {};
	}
	if ( !env || !this->valid_ref( value ) || sig.empty( ) )
		return {};

	auto ret = sig;
	if ( method ) {
		auto close = sig.rfind( ')' );
		if ( close == std::string_view::npos )
			return {};

		ret = sig.substr( close + 1 );
	}
	if ( ret != "Ljava/lang/String;" )
		return {};

	c_mute guard = {};
	if ( this->pending( env ) )
		return {};

	auto text = this->read_utf( env, static_cast< jstring >( value ) );
	if ( this->pending( env ) ) {
		this->clear( env );

		return {};
	}

	{
		std::lock_guard lock( this->m_lock );
		this->cur_thread( ).m_saved = text;
	}

	return this->show_string( text );
}

std::string c_writer::find_name( void* ptr ) {
	if ( auto local = this->find_local( ptr ); !local.empty( ) )
		return local;

	std::lock_guard lock( this->m_lock );
	if ( auto found = this->m_globals.find( ptr ); found != this->m_globals.end( ) )
		return found->second.m_name;

	return {};
}

ref_t* c_writer::find_ref( void* ptr ) {
	auto& frames = this->cur_thread( ).m_frames;
	for ( auto it = frames.rbegin( ); it != frames.rend( ); ++it ) {
		if ( auto found = it->m_locals.find( ptr ); found != it->m_locals.end( ) )
			return &found->second;
	}

	return nullptr;
}

std::string c_writer::find_local( void* ptr ) {
	std::lock_guard lock( this->m_lock );
	if ( auto* slot = this->find_ref( ptr ) )
		return slot->m_name;

	return {};
}

bool c_writer::copy_ref( void* ptr, ref_t& out ) {
	std::lock_guard lock( this->m_lock );
	if ( auto* slot = this->find_ref( ptr ) ) {
		out = *slot;

		return true;
	}

	if ( auto found = this->m_globals.find( ptr ); found != this->m_globals.end( ) ) {
		out = found->second;

		return true;
	}

	return false;
}

std::string c_writer::find_java( jclass cls ) {
	if ( !cls )
		return {};

	std::lock_guard lock( this->m_lock );
	auto* ptr = reinterpret_cast< void* >( cls );
	if ( auto* slot = this->find_ref( ptr ); slot && !slot->m_class_name.empty( ) )
		return slot->m_class_name;

	if ( auto found = this->m_globals.find( ptr ); found != this->m_globals.end( ) && !found->second.m_class_name.empty( ) )
		return found->second.m_class_name;

	return {};
}

std::string c_writer::name_id( std::unordered_map< void*, id_t >& ids, void* ptr, std::string_view type ) {
	if ( !ptr )
		return "NULL";

	std::lock_guard lock( this->m_lock );
	auto& slot = ids[ ptr ];
	if ( !slot.m_name.empty( ) )
		return slot.m_name;

	slot.m_name = this->seq_name( type );
	this->write_unlocked( this->var_line( type, slot.m_name, "NULL", std::format( "external {:p}", ptr ) ) );

	return slot.m_name;
}

void c_writer::set_id( std::unordered_map< void*, id_t >& ids, void* ptr, std::string_view java, std::string_view name, std::string_view sig ) {
	if ( !ptr )
		return;

	std::lock_guard lock( this->m_lock );
	auto& slot = ids[ ptr ];
	if ( slot.m_class_name.empty( ) && !java.empty( ) )
		slot.m_class_name = java;
	if ( slot.m_member.empty( ) && !name.empty( ) )
		slot.m_member = name;
	if ( slot.m_sig.empty( ) && !sig.empty( ) )
		slot.m_sig = sig;
}

std::string c_writer::id_text( std::unordered_map< void*, id_t >& ids, void* ptr, bool field ) {
	if ( !ptr )
		return {};

	std::lock_guard lock( this->m_lock );
	auto found = ids.find( ptr );
	if ( found == ids.end( ) )
		return {};

	auto& slot = found->second;
	if ( slot.m_member.empty( ) && slot.m_sig.empty( ) )
		return {};

	if ( field )
		return std::format( "{} {} {}", slot.m_class_name, slot.m_member, slot.m_sig );

	return std::format( "{} {}{}", slot.m_class_name, slot.m_member, slot.m_sig );
}

void c_writer::add_arg( std::vector< std::string >& parts, JNIEnv* env, param_t param, jvalue value, bool promoted ) {
	if ( param.m_array || param.m_tag == 'L' ) {
		parts.push_back( this->object_name( env, value.l ) );

		return;
	}

	parts.push_back( value_text( param.m_tag, value, promoted ) );
}

void c_writer::ensure_frame( ) {
	auto& frames = this->cur_thread( ).m_frames;
	if ( frames.empty( ) )
		frames.emplace_back( );
}

void c_writer::store_local( void* ptr, ref_t&& ref ) {
	std::lock_guard lock( this->m_lock );
	this->ensure_frame( );
	this->cur_thread( ).m_frames.back( ).m_locals.insert_or_assign( ptr, std::move( ref ) );
}

void c_writer::store_global( void* ptr, ref_t&& ref ) {
	std::lock_guard lock( this->m_lock );
	this->m_globals.insert_or_assign( ptr, std::move( ref ) );
}

std::string c_writer::next_name( std::string_view type ) {
	std::lock_guard lock( this->m_lock );

	return this->seq_name( type );
}

std::string c_writer::seq_name( std::string_view type ) {
	auto found = this->m_seq.find( type );
	if ( found == this->m_seq.end( ) )
		found = this->m_seq.emplace( std::string{ type }, 0 ).first;

	++found->second;

	return std::format( "{}_{}", type, found->second );
}

std::string c_writer::old_name( std::string_view text, std::string_view name ) {
	if ( text.empty( ) || text.size( ) > 4096 )
		return {};

	std::lock_guard lock( this->m_lock );
	if ( auto found = this->m_texts.find( text ); found != this->m_texts.end( ) )
		return found->second;

	this->m_texts.emplace( std::string{ text }, std::string{ name } );

	return {};
}

std::string c_writer::normalize( std::string_view name ) {
	std::string out = { name.begin( ), name.end( ) };
	for ( auto& ch : out )
		if ( ch == '.' )
			ch = '/';

	return out;
}

std::string c_writer::sanitize( std::string_view text ) {
	std::string out = {};
	out.reserve( text.size( ) );
	for ( auto ch : text ) {
		auto c = static_cast< unsigned char >( ch );
		if ( c < 32 || c >= 127 )
			out.push_back( '?' );
		else
			out.push_back( ch );
	}

	return out;
}

std::string c_writer::var_line( std::string_view type, std::string_view name, std::string_view expr, std::string_view comment ) {
	auto note = this->sanitize( comment );
	if ( note.empty( ) )
		return std::format( "{} {} = {};", type, name, expr );

	return std::format( "{} {} = {}; /// {}", type, name, expr, note );
}

std::string c_writer::call_line( std::string_view expr, std::string_view comment ) {
	auto note = this->sanitize( comment );
	if ( note.empty( ) )
		return std::format( "{};", expr );

	return std::format( "{}; /// {}", expr, note );
}

std::vector< c_writer::param_t > c_writer::parse_sig( std::string_view sig ) {
	std::vector< param_t > out = {};
	auto open = sig.find( '(' );
	auto close = sig.find( ')' );
	if ( open == std::string_view::npos || close == std::string_view::npos || close < open )
		return out;

	auto params = sig.substr( open + 1, close - open - 1 );
	for ( size_t i = 0; i < params.size( ); ) {
		param_t param = {};
		while ( i < params.size( ) && params[ i ] == '[' ) {
			param.m_array = true;
			++i;
		}
		if ( i >= params.size( ) )
			break;

		auto ch = params[ i ];
		if ( ch == 'L' ) {
			auto semi = params.find( ';', i );
			if ( semi == std::string_view::npos )
				break;

			param.m_tag = 'L';
			i = semi + 1;
		} else {
			param.m_tag = ch;
			++i;
		}
		out.push_back( param );
	}

	return out;
}

std::string c_writer::signature( jmethodID id ) {
	if ( !id )
		return {};

	std::lock_guard lock( this->m_lock );
	auto found = this->m_ids.find( reinterpret_cast< void* >( id ) );
	if ( found == this->m_ids.end( ) )
		return {};

	return found->second.m_sig;
}

void c_writer::line( std::string_view text ) {
	std::lock_guard lock( this->m_lock );
	this->write_unlocked( text );
}

void c_writer::flush_log( log_t& log ) {
	if ( !log.m_file || log.m_pending.empty( ) )
		return;

	this->write_span( log.m_file, log.m_pending.data( ), log.m_pending.size( ) );
	log.m_pending.clear( );
}

void c_writer::flush( ) {
	std::unique_lock lock( this->m_lock, std::try_to_lock );
	if ( !lock.owns_lock( ) )
		return;

	for ( auto& entry : this->m_logs ) {
		this->flush_log( entry.second );
		if ( entry.second.m_file )
			::FlushFileBuffers( entry.second.m_file );
	}
}

bool c_writer::write_span( void* file, const void* data, size_t size ) {
	auto* cursor = static_cast< const char* >( data );
	auto left = size;
	while ( left ) {
		auto chunk = static_cast< unsigned long >( left > 0x100000 ? 0x100000 : left );
		unsigned long wrote = {};
		if ( !::WriteFile( file, cursor, chunk, &wrote, nullptr ) || !wrote )
			return false;

		cursor += wrote;
		left -= wrote;
	}

	return true;
}

void c_writer::write_unlocked( std::string_view text ) {
	auto* log = this->log_for( this->cur_thread( ).m_caller );
	if ( !log )
		return;

	if ( !text.empty( ) )
		log->m_pending.append( text );
	log->m_pending.append( "\n\n" );
	this->flush_log( *log );
}

std::string c_writer::cached_module( uintptr_t address ) {
	if ( this->m_cache.m_gen == this->m_modules_gen && address >= this->m_cache.m_start && address < this->m_cache.m_end && !this->m_cache.m_name.empty( ) )
		return this->m_cache.m_name;

	for ( auto& range : this->m_ranges ) {
		if ( address < range.m_start || address >= range.m_end )
			continue;

		this->m_cache.m_gen = this->m_modules_gen;
		this->m_cache.m_start = range.m_start;
		this->m_cache.m_end = range.m_end;
		this->m_cache.m_name = range.m_name;

		return range.m_name;
	}

	return {};
}

void c_writer::refresh_modules( ) {
	auto* root = peb( );
	if ( !root || !root->m_ldr )
		return;

	auto* head = &root->m_ldr->m_in_load_order;
	for ( auto* link = head->m_flink; link && link != head; link = link->m_flink ) {
		auto* entry = reinterpret_cast< ldr_entry_t* >( link );
		auto start = reinterpret_cast< uintptr_t >( entry->m_dll_base );
		auto end = start + entry->m_size_of_image;
		if ( !start || end <= start )
			continue;

		auto known = false;
		for ( auto& range : this->m_ranges ) {
			if ( range.m_start == start ) {
				known = true;
				break;
			}
		}
		if ( known )
			continue;

		auto name = std::string{};
		auto count = entry->m_base_name.m_length / sizeof( uint16_t );
		auto* wide = entry->m_base_name.m_buffer;
		name.reserve( count );
		for ( uint16_t i = 0; wide && i < count; ++i ) {
			auto ch = wide[ i ];
			auto bad = ch < 32 || ch >= 127 || ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|';
			name.push_back( bad ? '_' : static_cast< char >( ch ) );
		}
		if ( name.empty( ) )
			name = "module";

		this->m_ranges.erase( std::remove_if( this->m_ranges.begin( ), this->m_ranges.end( ), [ & ]( module_range_t& range ) {
			auto overlaps = start < range.m_end && end > range.m_start;
			return overlaps && range.m_name.rfind( "region_", 0 ) == 0;
		} ), this->m_ranges.end( ) );

		module_range_t range = {};
		range.m_start = start;
		range.m_end = end;
		range.m_name = std::move( name );
		this->m_ranges.push_back( std::move( range ) );
		++this->m_modules_gen;
	}
}

std::string c_writer::region_name( void* from ) {
	MEMORY_BASIC_INFORMATION info = {};
	if ( !::VirtualQuery( from, &info, sizeof( info ) ) || !info.AllocationBase )
		return "unknown";

	auto start = reinterpret_cast< uintptr_t >( info.AllocationBase );
	auto cursor = start;
	auto end = start;
	while ( ::VirtualQuery( reinterpret_cast< void* >( cursor ), &info, sizeof( info ) ) ) {
		if ( reinterpret_cast< uintptr_t >( info.AllocationBase ) != start )
			break;

		auto next = reinterpret_cast< uintptr_t >( info.BaseAddress ) + info.RegionSize;
		if ( next <= cursor )
			break;

		end = next;
		cursor = next;
	}

	if ( auto name = this->cached_module( start ); !name.empty( ) )
		return name;

	auto name = std::format( "region_{:x}", start );
	module_range_t range = {};
	range.m_start = start;
	range.m_end = end > start ? end : start + 1;
	range.m_name = name;
	this->m_ranges.push_back( std::move( range ) );
	++this->m_modules_gen;

	return name;
}

std::string c_writer::module_of( void* from ) {
	if ( !from )
		return "unknown";

	auto address = reinterpret_cast< uintptr_t >( from );
	if ( auto name = this->cached_module( address ); !name.empty( ) )
		return name;

	this->refresh_modules( );
	if ( auto name = this->cached_module( address ); !name.empty( ) )
		return name;

	return this->region_name( from );
}

c_writer::log_t* c_writer::log_for( void* from ) {
	if ( this->m_dir.empty( ) )
		return {};

	auto name = this->module_of( from );
	if ( auto found = this->m_logs.find( name ); found != this->m_logs.end( ) )
		return &found->second;

	auto path = this->m_dir + "\\" + name + ".c";
	auto handle = ::CreateFileA( path.c_str( ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( handle == INVALID_HANDLE_VALUE )
		return {};

	auto placed = this->m_logs.emplace( std::move( name ), log_t{ handle, {} } );

	return &placed.first->second;
}

std::string c_writer::process_name( ) {
	auto* entry = main_module( );
	auto* wide = entry ? entry->m_base_name.m_buffer : nullptr;
	auto count = wide ? entry->m_base_name.m_length / sizeof( uint16_t ) : uint16_t{};

	std::string name = {};
	name.reserve( count );

	for ( uint16_t i = 0; i < count; ++i ) {
		auto ch = wide[ i ];
		name.push_back( ch < 128 ? static_cast< char >( ch ) : '_' );
	}

	auto dot = name.find_last_of( '.' );
	if ( dot != std::string::npos )
		name.resize( dot );

	for ( auto& ch : name ) {
		auto ok = ( ch >= '0' && ch <= '9' ) || ( ch >= 'A' && ch <= 'Z' ) || ( ch >= 'a' && ch <= 'z' ) || ch == '-' || ch == '_';
		if ( !ok )
			ch = '_';
	}

	if ( name.empty( ) )
		name = "process";
	if ( name.size( ) > 64 )
		name.resize( 64 );

	return name;
}
