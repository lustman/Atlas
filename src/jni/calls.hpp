#pragma once

#include "c_swap.hpp"
#include "hooks.hpp"
#include "ldr.hpp"

template< size_t N >
struct fn_name_t {
	char m_data[ N ] = {};

	constexpr fn_name_t( const char ( &text )[ N ] ) {
		for ( size_t i = 0; i < N; ++i )
			m_data[ i ] = text[ i ];
	}

	constexpr std::string_view view( ) const { return { m_data, N - 1 }; }
};

template< typename T >
const char* type_name( ) {
	if constexpr ( std::is_same_v< T, jboolean > )
		return "jboolean";
	else if constexpr ( std::is_same_v< T, jbyte > )
		return "jbyte";
	else if constexpr ( std::is_same_v< T, jchar > )
		return "jchar";
	else if constexpr ( std::is_same_v< T, jshort > )
		return "jshort";
	else if constexpr ( std::is_same_v< T, jint > )
		return "jint";
	else if constexpr ( std::is_same_v< T, jlong > )
		return "jlong";
	else if constexpr ( std::is_same_v< T, jfloat > )
		return "jfloat";
	else if constexpr ( std::is_same_v< T, jdouble > )
		return "jdouble";
	else if constexpr ( std::is_same_v< T, jbooleanArray > )
		return "jbooleanArray";
	else if constexpr ( std::is_same_v< T, jbyteArray > )
		return "jbyteArray";
	else if constexpr ( std::is_same_v< T, jcharArray > )
		return "jcharArray";
	else if constexpr ( std::is_same_v< T, jshortArray > )
		return "jshortArray";
	else if constexpr ( std::is_same_v< T, jintArray > )
		return "jintArray";
	else if constexpr ( std::is_same_v< T, jlongArray > )
		return "jlongArray";
	else if constexpr ( std::is_same_v< T, jfloatArray > )
		return "jfloatArray";
	else if constexpr ( std::is_same_v< T, jdoubleArray > )
		return "jdoubleArray";
	else if constexpr ( std::is_same_v< T, jobjectArray > )
		return "jobjectArray";
	else
		return "jobject";
}

template< typename T >
std::string value_text( T value ) {
	if constexpr ( std::is_same_v< T, jboolean > )
		return g_writer.bool_str( value );
	else if constexpr ( std::is_same_v< T, jbyte > )
		return std::format( "{}", static_cast< int32_t >( value ) );
	else if constexpr ( std::is_same_v< T, jchar > )
		return std::format( "{}", static_cast< uint32_t >( value ) );
	else if constexpr ( std::is_same_v< T, jshort > )
		return std::format( "{}", static_cast< int32_t >( value ) );
	else if constexpr ( std::is_same_v< T, jint > )
		return std::format( "{}", value );
	else if constexpr ( std::is_same_v< T, jlong > )
		return std::format( "{}LL", value );
	else if constexpr ( std::is_same_v< T, jfloat > )
		return std::format( "{:g}", static_cast< double >( value ) );
	else if constexpr ( std::is_same_v< T, jdouble > )
		return std::format( "{:g}", value );
	else
		return {};
}

std::string read_va( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, va_list args, bool is_static ) {
	if ( muted( ) )
		return {};

	g_writer.method_name( env, obj, clazz, methodID, is_static );

	va_list copy = {};
	va_copy( copy, args );
	auto decoded = g_writer.va_args( env, methodID, copy );
	va_end( copy );

	return decoded;
}

std::string read_jvalues( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, const jvalue* args, bool is_static ) {
	if ( muted( ) )
		return {};

	g_writer.method_name( env, obj, clazz, methodID, is_static );

	return g_writer.jvalue_args( env, methodID, args );
}

enum class call_mode_t : uint8_t { instance, nonvirtual, class_call };
enum class ret_mode_t : uint8_t { value, object, void_ret };

template< typename Fn >
int hook( JNINativeInterface_* table, Fn JNINativeInterface_::* member, c_swap& swap, Fn detour );

template< fn_name_t Name, call_mode_t Mode, ret_mode_t Kind, typename Ret, auto Fn, auto FnV, auto FnA, auto Note = value_text< jint > >
struct call_t {
	static constexpr auto fn = Fn;
	static constexpr auto fn_v = FnV;
	static constexpr auto fn_a = FnA;
	static constexpr bool static_call = Name.view( ).starts_with( "CallStatic" );

	static c_swap& plain( ) {
		static c_swap value = {};
		return value;
	}

	static c_swap& v( ) {
		static c_swap value = {};
		return value;
	}

	static c_swap& a( ) {
		static c_swap value = {};
		return value;
	}

	static std::string arg_text( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID ) {
		auto method = g_writer.method_name( methodID );
		if constexpr ( Mode == call_mode_t::instance )
			return std::format( "{}, {}", g_writer.object_name( env, obj ), method );
		else if constexpr ( Mode == call_mode_t::nonvirtual ) {
			auto recv = g_writer.object_name( env, obj );
			auto type = g_writer.object_name( env, clazz );
			return std::format( "{}, {}, {}", recv, type, method );
		} else
			return std::format( "{}, {}", g_writer.object_name( env, clazz ), method );
	}

	static jobject recv_obj( jobject obj ) {
		if constexpr ( Mode == call_mode_t::class_call )
			return nullptr;
		else
			return obj;
	}

	template< typename T >
	static void log_result( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, std::string_view api, std::string_view decoded, T result ) {
		if ( muted( ) )
			return;

		auto expr = g_writer.env_text( api, arg_text( env, obj, clazz, methodID ), decoded );
		if constexpr ( Kind == ret_mode_t::object )
			g_writer.log_obj( env, result, "jobject", expr, g_writer.end_text( env, methodID, g_writer.read_string( env, methodID, result ), recv_obj( obj ) ), ref_kind_t::local );
		else if constexpr ( Kind == ret_mode_t::value )
			g_writer.log_var( type_name< Ret >( ), expr, g_writer.end_text( env, methodID, Note( result ), recv_obj( obj ) ) );
	}

	static void log_noret( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, std::string_view api, std::string_view decoded ) {
		if ( muted( ) )
			return;

		g_writer.log( g_writer.env_text( api, arg_text( env, obj, clazz, methodID ), decoded ), g_writer.end_text( env, methodID, "", recv_obj( obj ) ) );
	}

	static Ret hk( JNIEnv* env, jobject obj, jmethodID methodID, ... ) requires ( Mode == call_mode_t::instance && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, obj, nullptr, methodID, args, false );
		auto result = v( ).invoke< FnV >( env, obj, methodID, args );
		va_end( args );

		log_result( env, obj, nullptr, methodID, Name.view( ), decoded, result );

		return result;
	}

	static void hk( JNIEnv* env, jobject obj, jmethodID methodID, ... ) requires ( Mode == call_mode_t::instance && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, obj, nullptr, methodID, args, false );
		v( ).invoke< FnV >( env, obj, methodID, args );
		va_end( args );

		log_noret( env, obj, nullptr, methodID, Name.view( ), decoded );
	}

	static Ret hk_v( JNIEnv* env, jobject obj, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::instance && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, obj, nullptr, methodID, args, false );
		auto result = v( ).invoke< FnV >( env, obj, methodID, args );

		log_result( env, obj, nullptr, methodID, std::format( "{}V", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_v( JNIEnv* env, jobject obj, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::instance && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, obj, nullptr, methodID, args, false );
		v( ).invoke< FnV >( env, obj, methodID, args );

		log_noret( env, obj, nullptr, methodID, std::format( "{}V", Name.view( ) ), decoded );
	}

	static Ret hk_a( JNIEnv* env, jobject obj, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::instance && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, obj, nullptr, methodID, args, false );
		auto result = a( ).invoke< FnA >( env, obj, methodID, args );

		log_result( env, obj, nullptr, methodID, std::format( "{}A", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_a( JNIEnv* env, jobject obj, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::instance && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, obj, nullptr, methodID, args, false );
		a( ).invoke< FnA >( env, obj, methodID, args );

		log_noret( env, obj, nullptr, methodID, std::format( "{}A", Name.view( ) ), decoded );
	}

	static Ret hk( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, ... ) requires ( Mode == call_mode_t::nonvirtual && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, obj, clazz, methodID, args, false );
		auto result = v( ).invoke< FnV >( env, obj, clazz, methodID, args );
		va_end( args );

		log_result( env, obj, clazz, methodID, Name.view( ), decoded, result );

		return result;
	}

	static void hk( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, ... ) requires ( Mode == call_mode_t::nonvirtual && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, obj, clazz, methodID, args, false );
		v( ).invoke< FnV >( env, obj, clazz, methodID, args );
		va_end( args );

		log_noret( env, obj, clazz, methodID, Name.view( ), decoded );
	}

	static Ret hk_v( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::nonvirtual && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, obj, clazz, methodID, args, false );
		auto result = v( ).invoke< FnV >( env, obj, clazz, methodID, args );

		log_result( env, obj, clazz, methodID, std::format( "{}V", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_v( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::nonvirtual && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, obj, clazz, methodID, args, false );
		v( ).invoke< FnV >( env, obj, clazz, methodID, args );

		log_noret( env, obj, clazz, methodID, std::format( "{}V", Name.view( ) ), decoded );
	}

	static Ret hk_a( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::nonvirtual && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, obj, clazz, methodID, args, false );
		auto result = a( ).invoke< FnA >( env, obj, clazz, methodID, args );

		log_result( env, obj, clazz, methodID, std::format( "{}A", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_a( JNIEnv* env, jobject obj, jclass clazz, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::nonvirtual && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, obj, clazz, methodID, args, false );
		a( ).invoke< FnA >( env, obj, clazz, methodID, args );

		log_noret( env, obj, clazz, methodID, std::format( "{}A", Name.view( ) ), decoded );
	}

	static Ret hk( JNIEnv* env, jclass clazz, jmethodID methodID, ... ) requires ( Mode == call_mode_t::class_call && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, nullptr, clazz, methodID, args, static_call );
		auto result = v( ).invoke< FnV >( env, clazz, methodID, args );
		va_end( args );

		log_result( env, nullptr, clazz, methodID, Name.view( ), decoded, result );

		return result;
	}

	static void hk( JNIEnv* env, jclass clazz, jmethodID methodID, ... ) requires ( Mode == call_mode_t::class_call && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		va_list args = {};
		va_start( args, methodID );
		auto decoded = read_va( env, nullptr, clazz, methodID, args, static_call );
		v( ).invoke< FnV >( env, clazz, methodID, args );
		va_end( args );

		log_noret( env, nullptr, clazz, methodID, Name.view( ), decoded );
	}

	static Ret hk_v( JNIEnv* env, jclass clazz, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::class_call && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, nullptr, clazz, methodID, args, static_call );
		auto result = v( ).invoke< FnV >( env, clazz, methodID, args );

		log_result( env, nullptr, clazz, methodID, std::format( "{}V", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_v( JNIEnv* env, jclass clazz, jmethodID methodID, va_list args ) requires ( Mode == call_mode_t::class_call && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_va( env, nullptr, clazz, methodID, args, static_call );
		v( ).invoke< FnV >( env, clazz, methodID, args );

		log_noret( env, nullptr, clazz, methodID, std::format( "{}V", Name.view( ) ), decoded );
	}

	static Ret hk_a( JNIEnv* env, jclass clazz, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::class_call && Kind != ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, nullptr, clazz, methodID, args, static_call );
		auto result = a( ).invoke< FnA >( env, clazz, methodID, args );

		log_result( env, nullptr, clazz, methodID, std::format( "{}A", Name.view( ) ), decoded, result );

		return result;
	}

	static void hk_a( JNIEnv* env, jclass clazz, jmethodID methodID, const jvalue* args ) requires ( Mode == call_mode_t::class_call && Kind == ret_mode_t::void_ret ) {
		c_caller scope( _ReturnAddress( ) );

		auto decoded = read_jvalues( env, nullptr, clazz, methodID, args, static_call );
		a( ).invoke< FnA >( env, clazz, methodID, args );

		log_noret( env, nullptr, clazz, methodID, std::format( "{}A", Name.view( ) ), decoded );
	}
};

template< fn_name_t Name, typename Owner, typename Ret, auto Fn, bool Ref, auto Note = value_text< jint > >
struct get_field_t {
	static constexpr auto fn = Fn;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static Ret hk( JNIEnv* env, Owner obj, jfieldID fieldID ) {
		c_caller scope( _ReturnAddress( ) );

		constexpr auto static_field = std::is_same_v< Owner, jclass >;
		auto field = std::string{};
		auto recv = std::string{};
		auto text = std::string{};
		if ( !muted( ) ) {
			field = g_writer.field_name( env, obj, fieldID, static_field );
			recv = g_writer.object_name( env, obj );
			text = g_writer.field_text( env, obj, fieldID, static_field );
		}

		auto result = hook( ).invoke< Fn >( env, obj, fieldID );

		if ( muted( ) )
			return result;

		auto expr = std::format( "env->{}({}, {})", Name.view( ), recv, field );
		if constexpr ( Ref )
			g_writer.log_obj( env, result, "jobject", expr, g_writer.join( text, g_writer.read_string( env, obj, fieldID, static_field, result ) ), ref_kind_t::local );
		else
			g_writer.log_var( type_name< Ret >( ), expr, g_writer.join( text, Note( result ) ) );

		return result;
	}
};

template< fn_name_t Name, typename Owner, typename T, auto Fn, bool Ref, auto Text = value_text< jint > >
struct set_field_t {
	static constexpr auto fn = Fn;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static void hk( JNIEnv* env, Owner obj, jfieldID fieldID, T val ) {
		c_caller scope( _ReturnAddress( ) );

		constexpr auto static_field = std::is_same_v< Owner, jclass >;
		auto field = std::string{};
		auto recv = std::string{};
		auto shown = std::string{};
		auto text = std::string{};
		if ( !muted( ) ) {
			field = g_writer.field_name( env, obj, fieldID, static_field );
			recv = g_writer.object_name( env, obj );
			if constexpr ( Ref )
				shown = g_writer.object_name( env, val );
			else
				shown = Text( val );
			text = g_writer.field_text( env, obj, fieldID, static_field );
		}

		hook( ).invoke< Fn >( env, obj, fieldID, val );

		if ( muted( ) )
			return;

		g_writer.log( std::format( "env->{}({}, {}, {})", Name.view( ), recv, field, shown ), text );
	}
};

template< fn_name_t Name, typename Ret, auto Fn >
struct new_array_t {
	static constexpr auto fn = Fn;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static Ret hk( JNIEnv* env, jsize len ) {
		c_caller scope( _ReturnAddress( ) );

		auto* result = hook( ).invoke< Fn >( env, len );

		if ( !muted( ) )
			g_writer.log_obj( env, result, type_name< Ret >( ), std::format( "env->{}({})", Name.view( ), len ), std::format( "len {}", len ), ref_kind_t::local );

		return result;
	}
};

template< fn_name_t Name, typename Arr, typename Elem, auto Fn >
struct get_elems_t {
	static constexpr auto fn = Fn;

	using ptr_t = Elem*;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static ptr_t hk( JNIEnv* env, Arr array, jboolean* isCopy ) {
		c_caller scope( _ReturnAddress( ) );

		auto* result = hook( ).invoke< Fn >( env, array, isCopy );
		auto len = !muted( ) ? g_writer.array_len( env, array ) : jsize{};

		if ( !muted( ) ) {
			auto count = result && len > 0 ? static_cast< size_t >( len ) : size_t{};
			auto blob = g_writer.base64( result, count * sizeof( Elem ) );
			auto copy = isCopy ? g_writer.bool_str( *isCopy ) : std::string{ "NULL" };
			auto note = std::format( "isCopy={} len={} {}", copy, len, blob );
			g_writer.log_ptr( std::format( "{}*", type_name< Elem >( ) ), std::format( "env->{}({}, NULL)", Name.view( ), g_writer.object_name( env, array ) ), note );
		}

		return result;
	}
};

template< fn_name_t Name, typename Arr, typename Elem, auto Fn >
struct release_elems_t {
	static constexpr auto fn = Fn;

	using ptr_t = Elem*;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static void hk( JNIEnv* env, Arr array, ptr_t elems, jint mode ) {
		c_caller scope( _ReturnAddress( ) );

		hook( ).invoke< Fn >( env, array, elems, mode );

		if ( !muted( ) )
			g_writer.log( std::format( "env->{}({}, /*elems*/ NULL, {})", Name.view( ), g_writer.object_name( env, array ), g_writer.mode_str( mode ) ), "" );
	}
};

template< fn_name_t Name, typename Arr, typename Elem, auto Fn >
struct get_region_t {
	static constexpr auto fn = Fn;

	using ptr_t = Elem*;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static void hk( JNIEnv* env, Arr array, jsize start, jsize len, ptr_t buf ) {
		c_caller scope( _ReturnAddress( ) );

		hook( ).invoke< Fn >( env, array, start, len, buf );

		if ( !muted( ) ) {
			auto count = len > 0 && buf ? static_cast< size_t >( len ) : size_t{};
			auto blob = g_writer.base64( buf, count * sizeof( Elem ) );
			g_writer.log( std::format( "env->{}({}, {}, {}, buf)", Name.view( ), g_writer.object_name( env, array ), start, len ), blob );
		}
	}
};

template< fn_name_t Name, typename Arr, typename Elem, auto Fn >
struct set_region_t {
	static constexpr auto fn = Fn;

	using ptr_t = const Elem*;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static void hk( JNIEnv* env, Arr array, jsize start, jsize len, ptr_t buf ) {
		c_caller scope( _ReturnAddress( ) );

		auto count = !muted( ) && len > 0 && buf ? static_cast< size_t >( len ) : size_t{};
		auto blob = g_writer.base64( buf, count * sizeof( Elem ) );
		hook( ).invoke< Fn >( env, array, start, len, buf );

		if ( !muted( ) )
			g_writer.log( std::format( "env->{}({}, {}, {}, (const {}*)b64_decode(\"{}\"))", Name.view( ), g_writer.object_name( env, array ), start, len, type_name< Elem >( ), blob ), std::format( "{} bytes", len ) );
	}
};

template< typename Set >
int hook_call( JNINativeInterface_* table ) {
	auto hooked = 0;

	hooked += hook( table, Set::fn, Set::plain( ), &Set::hk );
	hooked += hook( table, Set::fn_v, Set::v( ), &Set::hk_v );
	hooked += hook( table, Set::fn_a, Set::a( ), &Set::hk_a );

	return hooked;
}

template< typename... Sets >
int hook_calls( JNINativeInterface_* table ) {
	return ( hook_call< Sets >( table ) + ... + 0 );
}

template< typename Set >
int add_hook( JNINativeInterface_* table ) {
	return hook( table, Set::fn, Set::hook( ), &Set::hk );
}

template< typename... Sets >
int hooks( JNINativeInterface_* table ) {
	return ( add_hook< Sets >( table ) + ... + 0 );
}
