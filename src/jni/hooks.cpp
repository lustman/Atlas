#include "hooks.hpp"
#include "calls.hpp"

id_parts_t id_parts( JNIEnv* env, jclass clazz, const char* name, const char* sig ) {
	return { g_writer.object_name( env, clazz ), g_writer.class_name( clazz ), name ? name : "", sig ? sig : "" };
}

void log_method( JNIEnv* env, const char* api, jclass clazz, const char* name, const char* sig, jmethodID result ) {
	auto part = id_parts( env, clazz, name, sig );
	if ( result )
		g_writer.set_method( result, part.m_java, part.m_nm, part.m_sg );
	g_writer.log_id( result, "jmethodID", std::format( "env->{}({}, {}, {})", api, part.m_cls, g_writer.literal( part.m_nm ), g_writer.literal( part.m_sg ) ), std::format( "{} {}{}", part.m_java, part.m_nm, part.m_sg ) );
}

void log_field( JNIEnv* env, const char* api, jclass clazz, const char* name, const char* sig, jfieldID result ) {
	auto part = id_parts( env, clazz, name, sig );
	auto is_static = std::string_view{ api ? api : "" }.contains( "Static" );
	auto expr = std::format( "env->{}({}, {}, {})", api, part.m_cls, g_writer.literal( part.m_nm ), g_writer.literal( part.m_sg ) );
	auto comment = std::format( "{} {} {}", part.m_java, part.m_nm, part.m_sg );
	if ( result )
		g_writer.log_field( result, is_static, part.m_java, part.m_nm, part.m_sg, expr, comment, true );
	else
		g_writer.log_id( result, "jfieldID", expr, comment );
}

c_swap o_GetVersion = {};
jint hk_GetVersion( JNIEnv* env ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetVersion.invoke< &JNINativeInterface_::GetVersion >( env );

	if ( !muted( ) )
		g_writer.log_var( "jint", "env->GetVersion()", std::format( "{:#x}", static_cast< uint32_t >( result ) ) );

	return result;
}

c_swap o_DefineClass = {};
jclass hk_DefineClass( JNIEnv* env, const char* name, jobject loader, const jbyte* buf, jsize len ) {
	c_caller scope( _ReturnAddress( ) );

	auto count = !muted( ) && buf && len > 0 ? static_cast< size_t >( len ) : size_t{};
	auto blob = g_writer.base64( buf, count );
	auto* result = o_DefineClass.invoke< &JNINativeInterface_::DefineClass >( env, name, loader, buf, len );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jclass", std::format( "env->DefineClass({}, {}, (const jbyte*)b64_decode(\"{}\"), {})", g_writer.literal( name ? name : "" ), g_writer.object_name( env, loader ), blob, len ), name ? name : "", ref_kind_t::local );

	return result;
}

c_swap o_FindClass = {};
jclass hk_FindClass( JNIEnv* env, const char* name ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_FindClass.invoke< &JNINativeInterface_::FindClass >( env, name );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jclass", std::format( "env->FindClass({})", g_writer.literal( name ? name : "" ) ), name ? name : "", ref_kind_t::local );

	return result;
}

c_swap o_FromReflectedMethod = {};
jmethodID hk_FromReflectedMethod( JNIEnv* env, jobject method ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_FromReflectedMethod.invoke< &JNINativeInterface_::FromReflectedMethod >( env, method );

	if ( !muted( ) )
		g_writer.log_id( result, "jmethodID", std::format( "env->FromReflectedMethod({})", g_writer.object_name( env, method ) ), "" );

	return result;
}

c_swap o_FromReflectedField = {};
jfieldID hk_FromReflectedField( JNIEnv* env, jobject field ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_FromReflectedField.invoke< &JNINativeInterface_::FromReflectedField >( env, field );

	if ( !muted( ) )
		g_writer.log_id( result, "jfieldID", std::format( "env->FromReflectedField({})", g_writer.object_name( env, field ) ), "" );

	return result;
}

c_swap o_ToReflectedMethod = {};
jobject hk_ToReflectedMethod( JNIEnv* env, jclass cls, jmethodID methodID, jboolean isStatic ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_ToReflectedMethod.invoke< &JNINativeInterface_::ToReflectedMethod >( env, cls, methodID, isStatic );

	if ( !muted( ) ) {
		g_writer.method_name( env, nullptr, cls, methodID, isStatic == JNI_TRUE );
		auto type = g_writer.object_name( env, cls );
		auto method = g_writer.method_name( methodID );
		g_writer.log_obj( env, result, "jobject", std::format( "env->ToReflectedMethod({}, {}, {})", type, method, g_writer.bool_str( isStatic ) ), "", ref_kind_t::local );
	}

	return result;
}

c_swap o_GetSuperclass = {};
jclass hk_GetSuperclass( JNIEnv* env, jclass sub ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetSuperclass.invoke< &JNINativeInterface_::GetSuperclass >( env, sub );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jclass", std::format( "env->GetSuperclass({})", g_writer.object_name( env, sub ) ), "", ref_kind_t::local );

	return result;
}

c_swap o_IsAssignableFrom = {};
jboolean hk_IsAssignableFrom( JNIEnv* env, jclass sub, jclass sup ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_IsAssignableFrom.invoke< &JNINativeInterface_::IsAssignableFrom >( env, sub, sup );

	if ( !muted( ) )
		g_writer.log_var( "jboolean", std::format( "env->IsAssignableFrom({}, {})", g_writer.object_name( env, sub ), g_writer.object_name( env, sup ) ), g_writer.bool_str( result ) );

	return result;
}

c_swap o_ToReflectedField = {};
jobject hk_ToReflectedField( JNIEnv* env, jclass cls, jfieldID fieldID, jboolean isStatic ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_ToReflectedField.invoke< &JNINativeInterface_::ToReflectedField >( env, cls, fieldID, isStatic );

	if ( !muted( ) ) {
		auto field = g_writer.field_name( env, cls, fieldID, isStatic == JNI_TRUE, true );
		auto type = g_writer.object_name( env, cls );
		g_writer.log_obj( env, result, "jobject", std::format( "env->ToReflectedField({}, {}, {})", type, field, g_writer.bool_str( isStatic ) ), "", ref_kind_t::local );
	}

	return result;
}

c_swap o_Throw = {};
jint hk_Throw( JNIEnv* env, jthrowable obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_Throw.invoke< &JNINativeInterface_::Throw >( env, obj );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->Throw({})", g_writer.object_name( env, obj ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_ThrowNew = {};
jint hk_ThrowNew( JNIEnv* env, jclass clazz, const char* msg ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_ThrowNew.invoke< &JNINativeInterface_::ThrowNew >( env, clazz, msg );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->ThrowNew({}, {})", g_writer.object_name( env, clazz ), g_writer.literal( msg ? msg : "" ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_ExceptionOccurred = {};
jthrowable hk_ExceptionOccurred( JNIEnv* env ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_ExceptionOccurred.invoke< &JNINativeInterface_::ExceptionOccurred >( env );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jthrowable", "env->ExceptionOccurred()", "", ref_kind_t::local );

	return result;
}

c_swap o_ExceptionDescribe = {};
void hk_ExceptionDescribe( JNIEnv* env ) {
	c_caller scope( _ReturnAddress( ) );

	o_ExceptionDescribe.invoke< &JNINativeInterface_::ExceptionDescribe >( env );

	if ( !muted( ) )
		g_writer.log( "env->ExceptionDescribe()", "" );
}

c_swap o_ExceptionClear = {};
void hk_ExceptionClear( JNIEnv* env ) {
	c_caller scope( _ReturnAddress( ) );

	o_ExceptionClear.invoke< &JNINativeInterface_::ExceptionClear >( env );

	if ( !muted( ) )
		g_writer.log( "env->ExceptionClear()", "" );
}

c_swap o_FatalError = {};
void hk_FatalError( JNIEnv* env, const char* msg ) {
	c_caller scope( _ReturnAddress( ) );

	if ( !muted( ) )
		g_writer.log( std::format( "env->FatalError({})", g_writer.literal( msg ? msg : "" ) ), "" );
	o_FatalError.invoke< &JNINativeInterface_::FatalError >( env, msg );
}

c_swap o_PushLocalFrame = {};
jint hk_PushLocalFrame( JNIEnv* env, jint capacity ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_PushLocalFrame.invoke< &JNINativeInterface_::PushLocalFrame >( env, capacity );

	if ( !muted( ) ) {
		if ( result == JNI_OK )
			g_writer.push_frame( );
		g_writer.log_var( "jint", std::format( "env->PushLocalFrame({})", capacity ), std::format( "{}", result ) );
	}

	return result;
}

c_swap o_PopLocalFrame = {};
jobject hk_PopLocalFrame( JNIEnv* env, jobject result ) {
	c_caller scope( _ReturnAddress( ) );

	auto kept = !muted( ) ? g_writer.object_name( env, result ) : std::string{};
	auto* ret = o_PopLocalFrame.invoke< &JNINativeInterface_::PopLocalFrame >( env, result );

	if ( !muted( ) ) {
		g_writer.pop_frame( ret, result );
		g_writer.log( std::format( "env->PopLocalFrame({})", kept ), ret ? "kept" : "NULL" );
	}

	return ret;
}

c_swap o_NewGlobalRef = {};
jobject hk_NewGlobalRef( JNIEnv* env, jobject lobj ) {
	c_caller scope( _ReturnAddress( ) );

	auto src = !muted( ) ? g_writer.object_name( env, lobj ) : std::string{};
	auto* result = o_NewGlobalRef.invoke< &JNINativeInterface_::NewGlobalRef >( env, lobj );

	if ( !muted( ) ) {
		g_writer.save_text( g_writer.stored_str( lobj ) );
		g_writer.log_obj( env, result, "jobject", std::format( "env->NewGlobalRef({})", src ), src, ref_kind_t::global );
	}

	return result;
}

c_swap o_DeleteGlobalRef = {};
void hk_DeleteGlobalRef( JNIEnv* env, jobject gref ) {
	c_caller scope( _ReturnAddress( ) );

	auto name = !muted( ) ? g_writer.object_name( env, gref ) : std::string{};
	o_DeleteGlobalRef.invoke< &JNINativeInterface_::DeleteGlobalRef >( env, gref );

	if ( !muted( ) ) {
		g_writer.drop_global( gref );
		g_writer.log( std::format( "env->DeleteGlobalRef({})", name ), "" );
	}
}

c_swap o_DeleteLocalRef = {};
void hk_DeleteLocalRef( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto name = !muted( ) ? g_writer.object_name( env, obj ) : std::string{};
	o_DeleteLocalRef.invoke< &JNINativeInterface_::DeleteLocalRef >( env, obj );

	if ( !muted( ) ) {
		g_writer.drop_local( obj );
		g_writer.log( std::format( "env->DeleteLocalRef({})", name ), "" );
	}
}

c_swap o_IsSameObject = {};
jboolean hk_IsSameObject( JNIEnv* env, jobject obj1, jobject obj2 ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_IsSameObject.invoke< &JNINativeInterface_::IsSameObject >( env, obj1, obj2 );

	if ( !muted( ) ) {
		auto a = g_writer.object_name( env, obj1 );
		auto b = g_writer.object_name( env, obj2 );
		if ( result )
			g_writer.same_ref( obj2, obj1 );
		g_writer.log_var( "jboolean", std::format( "env->IsSameObject({}, {})", a, b ), g_writer.bool_str( result ) );
	}

	return result;
}

c_swap o_NewLocalRef = {};
jobject hk_NewLocalRef( JNIEnv* env, jobject ref ) {
	c_caller scope( _ReturnAddress( ) );

	auto src = !muted( ) ? g_writer.object_name( env, ref ) : std::string{};
	auto* result = o_NewLocalRef.invoke< &JNINativeInterface_::NewLocalRef >( env, ref );

	if ( !muted( ) ) {
		g_writer.save_text( g_writer.stored_str( ref ) );
		g_writer.log_obj( env, result, "jobject", std::format( "env->NewLocalRef({})", src ), src, ref_kind_t::local );
	}

	return result;
}

c_swap o_EnsureLocalCapacity = {};
jint hk_EnsureLocalCapacity( JNIEnv* env, jint capacity ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_EnsureLocalCapacity.invoke< &JNINativeInterface_::EnsureLocalCapacity >( env, capacity );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->EnsureLocalCapacity({})", capacity ), std::format( "{}", result ) );

	return result;
}

c_swap o_AllocObject = {};
jobject hk_AllocObject( JNIEnv* env, jclass clazz ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_AllocObject.invoke< &JNINativeInterface_::AllocObject >( env, clazz );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jobject", std::format( "env->AllocObject({})", g_writer.object_name( env, clazz ) ), g_writer.class_name( clazz ), ref_kind_t::local );

	return result;
}

c_swap o_GetObjectClass = {};
jclass hk_GetObjectClass( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetObjectClass.invoke< &JNINativeInterface_::GetObjectClass >( env, obj );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jclass", std::format( "env->GetObjectClass({})", g_writer.object_name( env, obj ) ), "", ref_kind_t::local );

	return result;
}

c_swap o_IsInstanceOf = {};
jboolean hk_IsInstanceOf( JNIEnv* env, jobject obj, jclass clazz ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_IsInstanceOf.invoke< &JNINativeInterface_::IsInstanceOf >( env, obj, clazz );

	if ( !muted( ) )
		g_writer.log_var( "jboolean", std::format( "env->IsInstanceOf({}, {})", g_writer.object_name( env, obj ), g_writer.object_name( env, clazz ) ), g_writer.bool_str( result ) );

	return result;
}

c_swap o_GetMethodID = {};
jmethodID hk_GetMethodID( JNIEnv* env, jclass clazz, const char* name, const char* sig ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetMethodID.invoke< &JNINativeInterface_::GetMethodID >( env, clazz, name, sig );

	if ( !muted( ) )
		log_method( env, "GetMethodID", clazz, name, sig, result );

	return result;
}

c_swap o_GetFieldID = {};
jfieldID hk_GetFieldID( JNIEnv* env, jclass clazz, const char* name, const char* sig ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetFieldID.invoke< &JNINativeInterface_::GetFieldID >( env, clazz, name, sig );

	if ( !muted( ) )
		log_field( env, "GetFieldID", clazz, name, sig, result );

	return result;
}

c_swap o_GetStaticMethodID = {};
jmethodID hk_GetStaticMethodID( JNIEnv* env, jclass clazz, const char* name, const char* sig ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetStaticMethodID.invoke< &JNINativeInterface_::GetStaticMethodID >( env, clazz, name, sig );

	if ( !muted( ) )
		log_method( env, "GetStaticMethodID", clazz, name, sig, result );

	return result;
}

c_swap o_GetStaticFieldID = {};
jfieldID hk_GetStaticFieldID( JNIEnv* env, jclass clazz, const char* name, const char* sig ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetStaticFieldID.invoke< &JNINativeInterface_::GetStaticFieldID >( env, clazz, name, sig );

	if ( !muted( ) )
		log_field( env, "GetStaticFieldID", clazz, name, sig, result );

	return result;
}

c_swap o_NewString = {};
jstring hk_NewString( JNIEnv* env, const jchar* unicode, jsize len ) {
	c_caller scope( _ReturnAddress( ) );

	auto count = !muted( ) && unicode && len > 0 ? static_cast< size_t >( len ) : size_t{};
	auto blob = g_writer.base64( unicode, count * sizeof( jchar ) );
	auto* result = o_NewString.invoke< &JNINativeInterface_::NewString >( env, unicode, len );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jstring", std::format( "env->NewString((const jchar*)b64_decode(\"{}\"), {})", blob, len ), "", ref_kind_t::local );

	return result;
}

c_swap o_GetStringChars = {};
const jchar* hk_GetStringChars( JNIEnv* env, jstring str, jboolean* isCopy ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetStringChars.invoke< &JNINativeInterface_::GetStringChars >( env, str, isCopy );
	auto n = result ? g_writer.string_len( env, str ) : jsize{};

	if ( !muted( ) ) {
		auto count = result && n > 0 ? static_cast< size_t >( n ) : size_t{};
		auto blob = g_writer.base64( result, count * sizeof( jchar ) );
		auto copy = isCopy ? g_writer.bool_str( *isCopy ) : std::string{ "NULL" };
		g_writer.log_ptr( "const jchar*", std::format( "env->GetStringChars({}, NULL)", g_writer.object_name( env, str ) ), std::format( "isCopy={} {}", copy, blob ) );
	}

	return result;
}

c_swap o_NewStringUTF = {};
jstring hk_NewStringUTF( JNIEnv* env, const char* utf ) {
	c_caller scope( _ReturnAddress( ) );

	auto text = utf ? std::string{ utf } : std::string{};
	auto* result = o_NewStringUTF.invoke< &JNINativeInterface_::NewStringUTF >( env, utf );

	if ( !muted( ) ) {
		g_writer.save_text( text );
		g_writer.log_obj( env, result, "jstring", std::format( "env->NewStringUTF({})", g_writer.literal( text ) ), g_writer.show_string( text ), ref_kind_t::local );
	}

	return result;
}

c_swap o_GetStringUTFChars = {};
const char* hk_GetStringUTFChars( JNIEnv* env, jstring str, jboolean* isCopy ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetStringUTFChars.invoke< &JNINativeInterface_::GetStringUTFChars >( env, str, isCopy );

	if ( !muted( ) ) {
		auto text = result ? std::string{ result } : std::string{};
		auto copy = isCopy ? g_writer.bool_str( *isCopy ) : std::string{ "NULL" };
		g_writer.log_ptr( "const char*", std::format( "env->GetStringUTFChars({}, NULL)", g_writer.object_name( env, str ) ), std::format( "isCopy={} {}", copy, g_writer.show_string( text ) ) );
	}

	return result;
}

c_swap o_GetArrayLength = {};
jsize hk_GetArrayLength( JNIEnv* env, jarray array ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetArrayLength.invoke< &JNINativeInterface_::GetArrayLength >( env, array );

	if ( !muted( ) )
		g_writer.log_var( "jsize", std::format( "env->GetArrayLength({})", g_writer.object_name( env, array ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_NewObjectArray = {};
jobjectArray hk_NewObjectArray( JNIEnv* env, jsize len, jclass clazz, jobject init ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_NewObjectArray.invoke< &JNINativeInterface_::NewObjectArray >( env, len, clazz, init );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jobjectArray", std::format( "env->NewObjectArray({}, {}, {})", len, g_writer.object_name( env, clazz ), g_writer.object_name( env, init ) ), std::format( "len {}", len ), ref_kind_t::local );

	return result;
}

c_swap o_GetObjectArrayElement = {};
jobject hk_GetObjectArrayElement( JNIEnv* env, jobjectArray array, jsize index ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetObjectArrayElement.invoke< &JNINativeInterface_::GetObjectArrayElement >( env, array, index );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jobject", std::format( "env->GetObjectArrayElement({}, {})", g_writer.object_name( env, array ), index ), "", ref_kind_t::local );

	return result;
}

c_swap o_SetObjectArrayElement = {};
void hk_SetObjectArrayElement( JNIEnv* env, jobjectArray array, jsize index, jobject val ) {
	c_caller scope( _ReturnAddress( ) );

	o_SetObjectArrayElement.invoke< &JNINativeInterface_::SetObjectArrayElement >( env, array, index, val );

	if ( !muted( ) )
		g_writer.log( std::format( "env->SetObjectArrayElement({}, {}, {})", g_writer.object_name( env, array ), index, g_writer.object_name( env, val ) ), "" );
}

c_swap o_RegisterNatives = {};
jint hk_RegisterNatives( JNIEnv* env, jclass clazz, const JNINativeMethod* methods, jint nMethods ) {
	c_caller scope( _ReturnAddress( ) );

	std::string note = {};
	if ( !muted( ) && methods && nMethods > 0 ) {
		for ( jint i = 0; i < nMethods; ++i ) {
			if ( i )
				note += ", ";
			note += std::format( "{} {} {:p}", methods[ i ].name ? methods[ i ].name : "?", methods[ i ].signature ? methods[ i ].signature : "?", methods[ i ].fnPtr );
		}
	}
	auto result = o_RegisterNatives.invoke< &JNINativeInterface_::RegisterNatives >( env, clazz, methods, nMethods );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->RegisterNatives({}, methods, {})", g_writer.object_name( env, clazz ), nMethods ), note );

	return result;
}

c_swap o_UnregisterNatives = {};
jint hk_UnregisterNatives( JNIEnv* env, jclass clazz ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_UnregisterNatives.invoke< &JNINativeInterface_::UnregisterNatives >( env, clazz );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->UnregisterNatives({})", g_writer.object_name( env, clazz ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_MonitorEnter = {};
jint hk_MonitorEnter( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_MonitorEnter.invoke< &JNINativeInterface_::MonitorEnter >( env, obj );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->MonitorEnter({})", g_writer.object_name( env, obj ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_MonitorExit = {};
jint hk_MonitorExit( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_MonitorExit.invoke< &JNINativeInterface_::MonitorExit >( env, obj );

	if ( !muted( ) )
		g_writer.log_var( "jint", std::format( "env->MonitorExit({})", g_writer.object_name( env, obj ) ), std::format( "{}", result ) );

	return result;
}

c_swap o_GetJavaVM = {};
jint hk_GetJavaVM( JNIEnv* env, JavaVM** vm ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetJavaVM.invoke< &JNINativeInterface_::GetJavaVM >( env, vm );

	if ( !muted( ) )
		g_writer.log_var( "jint", "env->GetJavaVM(&vm)", std::format( "{} {:p}", result, vm && *vm ? static_cast< void* >( *vm ) : nullptr ) );

	return result;
}

c_swap o_GetStringRegion = {};
void hk_GetStringRegion( JNIEnv* env, jstring str, jsize start, jsize len, jchar* buf ) {
	c_caller scope( _ReturnAddress( ) );

	o_GetStringRegion.invoke< &JNINativeInterface_::GetStringRegion >( env, str, start, len, buf );

	if ( !muted( ) ) {
		auto count = buf && len > 0 ? static_cast< size_t >( len ) : size_t{};
		auto blob = g_writer.base64( buf, count * sizeof( jchar ) );
		g_writer.log( std::format( "env->GetStringRegion({}, {}, {}, buf)", g_writer.object_name( env, str ), start, len ), blob );
	}
}

c_swap o_GetStringUTFRegion = {};
void hk_GetStringUTFRegion( JNIEnv* env, jstring str, jsize start, jsize len, char* buf ) {
	c_caller scope( _ReturnAddress( ) );

	o_GetStringUTFRegion.invoke< &JNINativeInterface_::GetStringUTFRegion >( env, str, start, len, buf );

	if ( !muted( ) ) {
		auto text = buf && len > 0 ? std::string{ buf, buf + ( len > 0 ? len : 0 ) } : std::string{};
		g_writer.log( std::format( "env->GetStringUTFRegion({}, {}, {}, buf)", g_writer.object_name( env, str ), start, len ), g_writer.show_string( text ) );
	}
}

c_swap o_GetPrimitiveArrayCritical = {};
void* hk_GetPrimitiveArrayCritical( JNIEnv* env, jarray array, jboolean* isCopy ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetPrimitiveArrayCritical.invoke< &JNINativeInterface_::GetPrimitiveArrayCritical >( env, array, isCopy );

	if ( !muted( ) ) {
		auto copy = isCopy ? g_writer.bool_str( *isCopy ) : std::string{ "NULL" };
		g_writer.log_ptr( "void*", std::format( "env->GetPrimitiveArrayCritical({}, NULL)", g_writer.object_name( env, array ) ), std::format( "isCopy={} {:p}", copy, result ) );
	}

	return result;
}

c_swap o_ReleasePrimitiveArrayCritical = {};
void hk_ReleasePrimitiveArrayCritical( JNIEnv* env, jarray array, void* carray, jint mode ) {
	c_caller scope( _ReturnAddress( ) );

	o_ReleasePrimitiveArrayCritical.invoke< &JNINativeInterface_::ReleasePrimitiveArrayCritical >( env, array, carray, mode );

	if ( !muted( ) )
		g_writer.log( std::format( "env->ReleasePrimitiveArrayCritical({}, /*carray*/ NULL, {})", g_writer.object_name( env, array ), g_writer.mode_str( mode ) ), "" );
}

c_swap o_GetStringCritical = {};
const jchar* hk_GetStringCritical( JNIEnv* env, jstring str, jboolean* isCopy ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetStringCritical.invoke< &JNINativeInterface_::GetStringCritical >( env, str, isCopy );

	if ( !muted( ) ) {
		auto copy = isCopy ? g_writer.bool_str( *isCopy ) : std::string{ "NULL" };
		g_writer.log_ptr( "const jchar*", std::format( "env->GetStringCritical({}, NULL)", g_writer.object_name( env, str ) ), std::format( "isCopy={} {:p}", copy, static_cast< const void* >( result ) ) );
	}

	return result;
}

c_swap o_ReleaseStringCritical = {};
void hk_ReleaseStringCritical( JNIEnv* env, jstring str, const jchar* cstring ) {
	c_caller scope( _ReturnAddress( ) );

	o_ReleaseStringCritical.invoke< &JNINativeInterface_::ReleaseStringCritical >( env, str, cstring );

	if ( !muted( ) )
		g_writer.log( std::format( "env->ReleaseStringCritical({}, /*cstring*/ NULL)", g_writer.object_name( env, str ) ), "" );
}

c_swap o_NewWeakGlobalRef = {};
jweak hk_NewWeakGlobalRef( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto src = !muted( ) ? g_writer.object_name( env, obj ) : std::string{};
	auto* result = o_NewWeakGlobalRef.invoke< &JNINativeInterface_::NewWeakGlobalRef >( env, obj );

	if ( !muted( ) ) {
		g_writer.save_text( g_writer.stored_str( obj ) );
		g_writer.log_obj( env, result, "jweak", std::format( "env->NewWeakGlobalRef({})", src ), src, ref_kind_t::weak );
	}

	return result;
}

c_swap o_DeleteWeakGlobalRef = {};
void hk_DeleteWeakGlobalRef( JNIEnv* env, jweak ref ) {
	c_caller scope( _ReturnAddress( ) );

	auto name = !muted( ) ? g_writer.object_name( env, ref ) : std::string{};
	o_DeleteWeakGlobalRef.invoke< &JNINativeInterface_::DeleteWeakGlobalRef >( env, ref );

	if ( !muted( ) ) {
		g_writer.drop_global( ref );
		g_writer.log( std::format( "env->DeleteWeakGlobalRef({})", name ), "" );
	}
}

c_swap o_ExceptionCheck = {};
jboolean hk_ExceptionCheck( JNIEnv* env ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_ExceptionCheck.invoke< &JNINativeInterface_::ExceptionCheck >( env );

	if ( !muted( ) )
		g_writer.log_var( "jboolean", "env->ExceptionCheck()", g_writer.bool_str( result ) );

	return result;
}

c_swap o_NewDirectByteBuffer = {};
jobject hk_NewDirectByteBuffer( JNIEnv* env, void* address, jlong capacity ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_NewDirectByteBuffer.invoke< &JNINativeInterface_::NewDirectByteBuffer >( env, address, capacity );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jobject", std::format( "env->NewDirectByteBuffer((void*){:p}, {}LL)", address, capacity ), std::format( "cap {}", capacity ), ref_kind_t::local );

	return result;
}

c_swap o_GetDirectBufferAddress = {};
void* hk_GetDirectBufferAddress( JNIEnv* env, jobject buf ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetDirectBufferAddress.invoke< &JNINativeInterface_::GetDirectBufferAddress >( env, buf );

	if ( !muted( ) )
		g_writer.log_ptr( "void*", std::format( "env->GetDirectBufferAddress({})", g_writer.object_name( env, buf ) ), std::format( "{:p}", result ) );

	return result;
}

c_swap o_GetDirectBufferCapacity = {};
jlong hk_GetDirectBufferCapacity( JNIEnv* env, jobject buf ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetDirectBufferCapacity.invoke< &JNINativeInterface_::GetDirectBufferCapacity >( env, buf );

	if ( !muted( ) )
		g_writer.log_var( "jlong", std::format( "env->GetDirectBufferCapacity({})", g_writer.object_name( env, buf ) ), std::format( "{}LL", result ) );

	return result;
}

c_swap o_GetObjectRefType = {};
jobjectRefType hk_GetObjectRefType( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetObjectRefType.invoke< &JNINativeInterface_::GetObjectRefType >( env, obj );

	if ( !muted( ) )
		g_writer.log_var( "jobjectRefType", std::format( "env->GetObjectRefType({})", g_writer.object_name( env, obj ) ), g_writer.ref_type( result ) );

	return result;
}

c_swap o_GetModule = {};
jobject hk_GetModule( JNIEnv* env, jclass clazz ) {
	c_caller scope( _ReturnAddress( ) );

	auto* result = o_GetModule.invoke< &JNINativeInterface_::GetModule >( env, clazz );

	if ( !muted( ) )
		g_writer.log_obj( env, result, "jobject", std::format( "env->GetModule({})", g_writer.object_name( env, clazz ) ), "", ref_kind_t::local );

	return result;
}

c_swap o_IsVirtualThread = {};
jboolean hk_IsVirtualThread( JNIEnv* env, jobject obj ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_IsVirtualThread.invoke< &JNINativeInterface_::IsVirtualThread >( env, obj );

	if ( !muted( ) )
		g_writer.log_var( "jboolean", std::format( "env->IsVirtualThread({})", g_writer.object_name( env, obj ) ), g_writer.bool_str( result ) );

	return result;
}

c_swap o_GetStringUTFLengthAsLong = {};
jlong hk_GetStringUTFLengthAsLong( JNIEnv* env, jstring str ) {
	c_caller scope( _ReturnAddress( ) );

	auto result = o_GetStringUTFLengthAsLong.invoke< &JNINativeInterface_::GetStringUTFLengthAsLong >( env, str );

	if ( !muted( ) )
		g_writer.log_var( "jlong", std::format( "env->GetStringUTFLengthAsLong({})", g_writer.object_name( env, str ) ), std::format( "{}LL", result ) );

	return result;
}

template< fn_name_t Name, auto Member >
struct string_len_t {
	static constexpr auto fn = Member;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static jsize hk( JNIEnv* env, jstring str ) {
		c_caller scope( _ReturnAddress( ) );

		auto result = hook( ).template invoke< Member >( env, str );
		if ( !muted( ) )
			g_writer.log_var( "jsize", std::format( "env->{}({})", Name.view( ), g_writer.object_name( env, str ) ), std::format( "{}", result ) );

		return result;
	}
};

template< fn_name_t Name, typename Ch, auto Member >
struct release_chars_t {
	static constexpr auto fn = Member;

	static c_swap& hook( ) {
		static c_swap value = {};
		return value;
	}

	static void hk( JNIEnv* env, jstring str, const Ch* chars ) {
		c_caller scope( _ReturnAddress( ) );

		hook( ).template invoke< Member >( env, str, chars );
		if ( !muted( ) )
			g_writer.log( std::format( "env->{}({}, /*chars*/ NULL)", Name.view( ), g_writer.object_name( env, str ) ), "" );
	}
};

template< typename Fn >
int hook( JNINativeInterface_* table, Fn JNINativeInterface_::* member, c_swap& swap, Fn detour ) {
	auto* slot = reinterpret_cast< void** >( &( table->*member ) );
	if ( !slot || !detour || !*slot )
		return 0;

	if ( !swap.create( slot, reinterpret_cast< void* >( detour ) ) )
		return 0;

	if ( !swap.enable( ) )
		return 0;

	return 1;
}


int install_jni( JNIEnv* env ) {
	if ( !env || !env->functions )
		return 0;

	static std::mutex lock = {};
	std::lock_guard guard( lock );
	auto* table = const_cast< JNINativeInterface_* >( env->functions );
	static std::vector< void* > installed = {};
	if ( std::ranges::contains( installed, table ) )
		return 0;

	installed.push_back( table );
	g_writer.save_raw( table );
	auto version = table->GetVersion ? table->GetVersion( env ) : JNI_VERSION_1_8;
	auto major = version >= 0x00090000 ? ( version >> 16 ) : 1;

	auto hooked = 0;

	hooked += hook( table, &JNINativeInterface_::GetVersion, o_GetVersion, &hk_GetVersion );
	hooked += hook( table, &JNINativeInterface_::DefineClass, o_DefineClass, &hk_DefineClass );
	hooked += hook( table, &JNINativeInterface_::FindClass, o_FindClass, &hk_FindClass );
	hooked += hook( table, &JNINativeInterface_::FromReflectedMethod, o_FromReflectedMethod, &hk_FromReflectedMethod );
	hooked += hook( table, &JNINativeInterface_::FromReflectedField, o_FromReflectedField, &hk_FromReflectedField );
	hooked += hook( table, &JNINativeInterface_::ToReflectedMethod, o_ToReflectedMethod, &hk_ToReflectedMethod );
	hooked += hook( table, &JNINativeInterface_::GetSuperclass, o_GetSuperclass, &hk_GetSuperclass );
	hooked += hook( table, &JNINativeInterface_::IsAssignableFrom, o_IsAssignableFrom, &hk_IsAssignableFrom );
	hooked += hook( table, &JNINativeInterface_::ToReflectedField, o_ToReflectedField, &hk_ToReflectedField );
	hooked += hook( table, &JNINativeInterface_::Throw, o_Throw, &hk_Throw );
	hooked += hook( table, &JNINativeInterface_::ThrowNew, o_ThrowNew, &hk_ThrowNew );
	hooked += hook( table, &JNINativeInterface_::ExceptionOccurred, o_ExceptionOccurred, &hk_ExceptionOccurred );
	hooked += hook( table, &JNINativeInterface_::ExceptionDescribe, o_ExceptionDescribe, &hk_ExceptionDescribe );
	hooked += hook( table, &JNINativeInterface_::ExceptionClear, o_ExceptionClear, &hk_ExceptionClear );
	hooked += hook( table, &JNINativeInterface_::FatalError, o_FatalError, &hk_FatalError );
	hooked += hook( table, &JNINativeInterface_::PushLocalFrame, o_PushLocalFrame, &hk_PushLocalFrame );
	hooked += hook( table, &JNINativeInterface_::PopLocalFrame, o_PopLocalFrame, &hk_PopLocalFrame );
	hooked += hook( table, &JNINativeInterface_::NewGlobalRef, o_NewGlobalRef, &hk_NewGlobalRef );
	hooked += hook( table, &JNINativeInterface_::DeleteGlobalRef, o_DeleteGlobalRef, &hk_DeleteGlobalRef );
	hooked += hook( table, &JNINativeInterface_::DeleteLocalRef, o_DeleteLocalRef, &hk_DeleteLocalRef );
	hooked += hook( table, &JNINativeInterface_::IsSameObject, o_IsSameObject, &hk_IsSameObject );
	hooked += hook( table, &JNINativeInterface_::NewLocalRef, o_NewLocalRef, &hk_NewLocalRef );
	hooked += hook( table, &JNINativeInterface_::EnsureLocalCapacity, o_EnsureLocalCapacity, &hk_EnsureLocalCapacity );
	hooked += hook( table, &JNINativeInterface_::AllocObject, o_AllocObject, &hk_AllocObject );
	hooked += hook( table, &JNINativeInterface_::GetObjectClass, o_GetObjectClass, &hk_GetObjectClass );
	hooked += hook( table, &JNINativeInterface_::IsInstanceOf, o_IsInstanceOf, &hk_IsInstanceOf );
	hooked += hook( table, &JNINativeInterface_::GetMethodID, o_GetMethodID, &hk_GetMethodID );
	hooked += hook( table, &JNINativeInterface_::GetFieldID, o_GetFieldID, &hk_GetFieldID );
	hooked += hook( table, &JNINativeInterface_::GetStaticMethodID, o_GetStaticMethodID, &hk_GetStaticMethodID );
	hooked += hook( table, &JNINativeInterface_::GetStaticFieldID, o_GetStaticFieldID, &hk_GetStaticFieldID );
	hooked += hook( table, &JNINativeInterface_::NewString, o_NewString, &hk_NewString );

	hooked += hook( table, &JNINativeInterface_::GetStringChars, o_GetStringChars, &hk_GetStringChars );

	hooked += hook( table, &JNINativeInterface_::NewStringUTF, o_NewStringUTF, &hk_NewStringUTF );

	hooked += hook( table, &JNINativeInterface_::GetStringUTFChars, o_GetStringUTFChars, &hk_GetStringUTFChars );

	hooked += hook( table, &JNINativeInterface_::GetArrayLength, o_GetArrayLength, &hk_GetArrayLength );
	hooked += hook( table, &JNINativeInterface_::NewObjectArray, o_NewObjectArray, &hk_NewObjectArray );
	hooked += hook( table, &JNINativeInterface_::GetObjectArrayElement, o_GetObjectArrayElement, &hk_GetObjectArrayElement );
	hooked += hook( table, &JNINativeInterface_::SetObjectArrayElement, o_SetObjectArrayElement, &hk_SetObjectArrayElement );
	hooked += hook( table, &JNINativeInterface_::RegisterNatives, o_RegisterNatives, &hk_RegisterNatives );
	hooked += hook( table, &JNINativeInterface_::UnregisterNatives, o_UnregisterNatives, &hk_UnregisterNatives );
	hooked += hook( table, &JNINativeInterface_::MonitorEnter, o_MonitorEnter, &hk_MonitorEnter );
	hooked += hook( table, &JNINativeInterface_::MonitorExit, o_MonitorExit, &hk_MonitorExit );
	hooked += hook( table, &JNINativeInterface_::GetJavaVM, o_GetJavaVM, &hk_GetJavaVM );
	hooked += hook( table, &JNINativeInterface_::GetStringRegion, o_GetStringRegion, &hk_GetStringRegion );
	hooked += hook( table, &JNINativeInterface_::GetStringUTFRegion, o_GetStringUTFRegion, &hk_GetStringUTFRegion );
	hooked += hook( table, &JNINativeInterface_::GetPrimitiveArrayCritical, o_GetPrimitiveArrayCritical, &hk_GetPrimitiveArrayCritical );
	hooked += hook( table, &JNINativeInterface_::ReleasePrimitiveArrayCritical, o_ReleasePrimitiveArrayCritical, &hk_ReleasePrimitiveArrayCritical );
	hooked += hook( table, &JNINativeInterface_::GetStringCritical, o_GetStringCritical, &hk_GetStringCritical );
	hooked += hook( table, &JNINativeInterface_::ReleaseStringCritical, o_ReleaseStringCritical, &hk_ReleaseStringCritical );
	hooked += hook( table, &JNINativeInterface_::NewWeakGlobalRef, o_NewWeakGlobalRef, &hk_NewWeakGlobalRef );
	hooked += hook( table, &JNINativeInterface_::DeleteWeakGlobalRef, o_DeleteWeakGlobalRef, &hk_DeleteWeakGlobalRef );
	hooked += hook( table, &JNINativeInterface_::ExceptionCheck, o_ExceptionCheck, &hk_ExceptionCheck );
	hooked += hook( table, &JNINativeInterface_::NewDirectByteBuffer, o_NewDirectByteBuffer, &hk_NewDirectByteBuffer );
	hooked += hook( table, &JNINativeInterface_::GetDirectBufferAddress, o_GetDirectBufferAddress, &hk_GetDirectBufferAddress );
	hooked += hook( table, &JNINativeInterface_::GetDirectBufferCapacity, o_GetDirectBufferCapacity, &hk_GetDirectBufferCapacity );

	hooked += hooks<
		string_len_t< "GetStringLength", &JNINativeInterface_::GetStringLength >,
		string_len_t< "GetStringUTFLength", &JNINativeInterface_::GetStringUTFLength >,
		release_chars_t< "ReleaseStringChars", jchar, &JNINativeInterface_::ReleaseStringChars >,
		release_chars_t< "ReleaseStringUTFChars", char, &JNINativeInterface_::ReleaseStringUTFChars >
	>( table );

	hooked += hook_calls <
		call_t< "NewObject", call_mode_t::class_call, ret_mode_t::object, jobject, &JNINativeInterface_::NewObject, &JNINativeInterface_::NewObjectV, &JNINativeInterface_::NewObjectA >,
		call_t< "CallObjectMethod", call_mode_t::instance, ret_mode_t::object, jobject, &JNINativeInterface_::CallObjectMethod, &JNINativeInterface_::CallObjectMethodV, &JNINativeInterface_::CallObjectMethodA >,
		call_t< "CallBooleanMethod", call_mode_t::instance, ret_mode_t::value, jboolean, &JNINativeInterface_::CallBooleanMethod, &JNINativeInterface_::CallBooleanMethodV, &JNINativeInterface_::CallBooleanMethodA, value_text< jboolean > >,
		call_t< "CallByteMethod", call_mode_t::instance, ret_mode_t::value, jbyte, &JNINativeInterface_::CallByteMethod, &JNINativeInterface_::CallByteMethodV, &JNINativeInterface_::CallByteMethodA, value_text< jbyte > >,
		call_t< "CallCharMethod", call_mode_t::instance, ret_mode_t::value, jchar, &JNINativeInterface_::CallCharMethod, &JNINativeInterface_::CallCharMethodV, &JNINativeInterface_::CallCharMethodA, value_text< jchar > >,
		call_t< "CallShortMethod", call_mode_t::instance, ret_mode_t::value, jshort, &JNINativeInterface_::CallShortMethod, &JNINativeInterface_::CallShortMethodV, &JNINativeInterface_::CallShortMethodA, value_text< jshort > >,
		call_t< "CallIntMethod", call_mode_t::instance, ret_mode_t::value, jint, &JNINativeInterface_::CallIntMethod, &JNINativeInterface_::CallIntMethodV, &JNINativeInterface_::CallIntMethodA >,
		call_t< "CallLongMethod", call_mode_t::instance, ret_mode_t::value, jlong, &JNINativeInterface_::CallLongMethod, &JNINativeInterface_::CallLongMethodV, &JNINativeInterface_::CallLongMethodA, value_text< jlong > >,
		call_t< "CallFloatMethod", call_mode_t::instance, ret_mode_t::value, jfloat, &JNINativeInterface_::CallFloatMethod, &JNINativeInterface_::CallFloatMethodV, &JNINativeInterface_::CallFloatMethodA, value_text< jfloat > >,
		call_t< "CallDoubleMethod", call_mode_t::instance, ret_mode_t::value, jdouble, &JNINativeInterface_::CallDoubleMethod, &JNINativeInterface_::CallDoubleMethodV, &JNINativeInterface_::CallDoubleMethodA, value_text< jdouble > >,
		call_t< "CallVoidMethod", call_mode_t::instance, ret_mode_t::void_ret, void, &JNINativeInterface_::CallVoidMethod, &JNINativeInterface_::CallVoidMethodV, &JNINativeInterface_::CallVoidMethodA >,
		call_t< "CallNonvirtualObjectMethod", call_mode_t::nonvirtual, ret_mode_t::object, jobject, &JNINativeInterface_::CallNonvirtualObjectMethod, &JNINativeInterface_::CallNonvirtualObjectMethodV, &JNINativeInterface_::CallNonvirtualObjectMethodA >,
		call_t< "CallNonvirtualBooleanMethod", call_mode_t::nonvirtual, ret_mode_t::value, jboolean, &JNINativeInterface_::CallNonvirtualBooleanMethod, &JNINativeInterface_::CallNonvirtualBooleanMethodV, &JNINativeInterface_::CallNonvirtualBooleanMethodA, value_text< jboolean > >,
		call_t< "CallNonvirtualByteMethod", call_mode_t::nonvirtual, ret_mode_t::value, jbyte, &JNINativeInterface_::CallNonvirtualByteMethod, &JNINativeInterface_::CallNonvirtualByteMethodV, &JNINativeInterface_::CallNonvirtualByteMethodA, value_text< jbyte > >,
		call_t< "CallNonvirtualCharMethod", call_mode_t::nonvirtual, ret_mode_t::value, jchar, &JNINativeInterface_::CallNonvirtualCharMethod, &JNINativeInterface_::CallNonvirtualCharMethodV, &JNINativeInterface_::CallNonvirtualCharMethodA, value_text< jchar > >,
		call_t< "CallNonvirtualShortMethod", call_mode_t::nonvirtual, ret_mode_t::value, jshort, &JNINativeInterface_::CallNonvirtualShortMethod, &JNINativeInterface_::CallNonvirtualShortMethodV, &JNINativeInterface_::CallNonvirtualShortMethodA, value_text< jshort > >,
		call_t< "CallNonvirtualIntMethod", call_mode_t::nonvirtual, ret_mode_t::value, jint, &JNINativeInterface_::CallNonvirtualIntMethod, &JNINativeInterface_::CallNonvirtualIntMethodV, &JNINativeInterface_::CallNonvirtualIntMethodA >,
		call_t< "CallNonvirtualLongMethod", call_mode_t::nonvirtual, ret_mode_t::value, jlong, &JNINativeInterface_::CallNonvirtualLongMethod, &JNINativeInterface_::CallNonvirtualLongMethodV, &JNINativeInterface_::CallNonvirtualLongMethodA, value_text< jlong > >,
		call_t< "CallNonvirtualFloatMethod", call_mode_t::nonvirtual, ret_mode_t::value, jfloat, &JNINativeInterface_::CallNonvirtualFloatMethod, &JNINativeInterface_::CallNonvirtualFloatMethodV, &JNINativeInterface_::CallNonvirtualFloatMethodA, value_text< jfloat > >,
		call_t< "CallNonvirtualDoubleMethod", call_mode_t::nonvirtual, ret_mode_t::value, jdouble, &JNINativeInterface_::CallNonvirtualDoubleMethod, &JNINativeInterface_::CallNonvirtualDoubleMethodV, &JNINativeInterface_::CallNonvirtualDoubleMethodA, value_text< jdouble > >,
		call_t< "CallNonvirtualVoidMethod", call_mode_t::nonvirtual, ret_mode_t::void_ret, void, &JNINativeInterface_::CallNonvirtualVoidMethod, &JNINativeInterface_::CallNonvirtualVoidMethodV, &JNINativeInterface_::CallNonvirtualVoidMethodA >,
		call_t< "CallStaticObjectMethod", call_mode_t::class_call, ret_mode_t::object, jobject, &JNINativeInterface_::CallStaticObjectMethod, &JNINativeInterface_::CallStaticObjectMethodV, &JNINativeInterface_::CallStaticObjectMethodA >,
		call_t< "CallStaticBooleanMethod", call_mode_t::class_call, ret_mode_t::value, jboolean, &JNINativeInterface_::CallStaticBooleanMethod, &JNINativeInterface_::CallStaticBooleanMethodV, &JNINativeInterface_::CallStaticBooleanMethodA, value_text< jboolean > >,
		call_t< "CallStaticByteMethod", call_mode_t::class_call, ret_mode_t::value, jbyte, &JNINativeInterface_::CallStaticByteMethod, &JNINativeInterface_::CallStaticByteMethodV, &JNINativeInterface_::CallStaticByteMethodA, value_text< jbyte > >,
		call_t< "CallStaticCharMethod", call_mode_t::class_call, ret_mode_t::value, jchar, &JNINativeInterface_::CallStaticCharMethod, &JNINativeInterface_::CallStaticCharMethodV, &JNINativeInterface_::CallStaticCharMethodA, value_text< jchar > >,
		call_t< "CallStaticShortMethod", call_mode_t::class_call, ret_mode_t::value, jshort, &JNINativeInterface_::CallStaticShortMethod, &JNINativeInterface_::CallStaticShortMethodV, &JNINativeInterface_::CallStaticShortMethodA, value_text< jshort > >,
		call_t< "CallStaticIntMethod", call_mode_t::class_call, ret_mode_t::value, jint, &JNINativeInterface_::CallStaticIntMethod, &JNINativeInterface_::CallStaticIntMethodV, &JNINativeInterface_::CallStaticIntMethodA >,
		call_t< "CallStaticLongMethod", call_mode_t::class_call, ret_mode_t::value, jlong, &JNINativeInterface_::CallStaticLongMethod, &JNINativeInterface_::CallStaticLongMethodV, &JNINativeInterface_::CallStaticLongMethodA, value_text< jlong > >,
		call_t< "CallStaticFloatMethod", call_mode_t::class_call, ret_mode_t::value, jfloat, &JNINativeInterface_::CallStaticFloatMethod, &JNINativeInterface_::CallStaticFloatMethodV, &JNINativeInterface_::CallStaticFloatMethodA, value_text< jfloat > >,
		call_t< "CallStaticDoubleMethod", call_mode_t::class_call, ret_mode_t::value, jdouble, &JNINativeInterface_::CallStaticDoubleMethod, &JNINativeInterface_::CallStaticDoubleMethodV, &JNINativeInterface_::CallStaticDoubleMethodA, value_text< jdouble > >,
		call_t< "CallStaticVoidMethod", call_mode_t::class_call, ret_mode_t::void_ret, void, &JNINativeInterface_::CallStaticVoidMethod, &JNINativeInterface_::CallStaticVoidMethodV, &JNINativeInterface_::CallStaticVoidMethodA >
	> ( table );

	hooked += hooks <
		get_field_t< "GetObjectField", jobject, jobject, &JNINativeInterface_::GetObjectField, true >,
		get_field_t< "GetBooleanField", jobject, jboolean, &JNINativeInterface_::GetBooleanField, false, value_text< jboolean > >,
		get_field_t< "GetByteField", jobject, jbyte, &JNINativeInterface_::GetByteField, false, value_text< jbyte > >,
		get_field_t< "GetCharField", jobject, jchar, &JNINativeInterface_::GetCharField, false, value_text< jchar > >,
		get_field_t< "GetShortField", jobject, jshort, &JNINativeInterface_::GetShortField, false, value_text< jshort > >,
		get_field_t< "GetIntField", jobject, jint, &JNINativeInterface_::GetIntField, false >,
		get_field_t< "GetLongField", jobject, jlong, &JNINativeInterface_::GetLongField, false, value_text< jlong > >,
		get_field_t< "GetFloatField", jobject, jfloat, &JNINativeInterface_::GetFloatField, false, value_text< jfloat > >,
		get_field_t< "GetDoubleField", jobject, jdouble, &JNINativeInterface_::GetDoubleField, false, value_text< jdouble > >,
		set_field_t< "SetObjectField", jobject, jobject, &JNINativeInterface_::SetObjectField, true >,
		set_field_t< "SetBooleanField", jobject, jboolean, &JNINativeInterface_::SetBooleanField, false, value_text< jboolean > >,
		set_field_t< "SetByteField", jobject, jbyte, &JNINativeInterface_::SetByteField, false, value_text< jbyte > >,
		set_field_t< "SetCharField", jobject, jchar, &JNINativeInterface_::SetCharField, false, value_text< jchar > >,
		set_field_t< "SetShortField", jobject, jshort, &JNINativeInterface_::SetShortField, false, value_text< jshort > >,
		set_field_t< "SetIntField", jobject, jint, &JNINativeInterface_::SetIntField, false >,
		set_field_t< "SetLongField", jobject, jlong, &JNINativeInterface_::SetLongField, false, value_text< jlong > >,
		set_field_t< "SetFloatField", jobject, jfloat, &JNINativeInterface_::SetFloatField, false, value_text< jfloat > >,
		set_field_t< "SetDoubleField", jobject, jdouble, &JNINativeInterface_::SetDoubleField, false, value_text< jdouble > >,
		get_field_t< "GetStaticObjectField", jclass, jobject, &JNINativeInterface_::GetStaticObjectField, true >,
		get_field_t< "GetStaticBooleanField", jclass, jboolean, &JNINativeInterface_::GetStaticBooleanField, false, value_text< jboolean > >,
		get_field_t< "GetStaticByteField", jclass, jbyte, &JNINativeInterface_::GetStaticByteField, false, value_text< jbyte > >,
		get_field_t< "GetStaticCharField", jclass, jchar, &JNINativeInterface_::GetStaticCharField, false, value_text< jchar > >,
		get_field_t< "GetStaticShortField", jclass, jshort, &JNINativeInterface_::GetStaticShortField, false, value_text< jshort > >,
		get_field_t< "GetStaticIntField", jclass, jint, &JNINativeInterface_::GetStaticIntField, false >,
		get_field_t< "GetStaticLongField", jclass, jlong, &JNINativeInterface_::GetStaticLongField, false, value_text< jlong > >,
		get_field_t< "GetStaticFloatField", jclass, jfloat, &JNINativeInterface_::GetStaticFloatField, false, value_text< jfloat > >,
		get_field_t< "GetStaticDoubleField", jclass, jdouble, &JNINativeInterface_::GetStaticDoubleField, false, value_text< jdouble > >,
		set_field_t< "SetStaticObjectField", jclass, jobject, &JNINativeInterface_::SetStaticObjectField, true >,
		set_field_t< "SetStaticBooleanField", jclass, jboolean, &JNINativeInterface_::SetStaticBooleanField, false, value_text< jboolean > >,
		set_field_t< "SetStaticByteField", jclass, jbyte, &JNINativeInterface_::SetStaticByteField, false, value_text< jbyte > >,
		set_field_t< "SetStaticCharField", jclass, jchar, &JNINativeInterface_::SetStaticCharField, false, value_text< jchar > >,
		set_field_t< "SetStaticShortField", jclass, jshort, &JNINativeInterface_::SetStaticShortField, false, value_text< jshort > >,
		set_field_t< "SetStaticIntField", jclass, jint, &JNINativeInterface_::SetStaticIntField, false >,
		set_field_t< "SetStaticLongField", jclass, jlong, &JNINativeInterface_::SetStaticLongField, false, value_text< jlong > >,
		set_field_t< "SetStaticFloatField", jclass, jfloat, &JNINativeInterface_::SetStaticFloatField, false, value_text< jfloat > >,
		set_field_t< "SetStaticDoubleField", jclass, jdouble, &JNINativeInterface_::SetStaticDoubleField, false, value_text< jdouble > >,
		new_array_t< "NewBooleanArray", jbooleanArray, &JNINativeInterface_::NewBooleanArray >,
		new_array_t< "NewByteArray", jbyteArray, &JNINativeInterface_::NewByteArray >,
		new_array_t< "NewCharArray", jcharArray, &JNINativeInterface_::NewCharArray >,
		new_array_t< "NewShortArray", jshortArray, &JNINativeInterface_::NewShortArray >,
		new_array_t< "NewIntArray", jintArray, &JNINativeInterface_::NewIntArray >,
		new_array_t< "NewLongArray", jlongArray, &JNINativeInterface_::NewLongArray >,
		new_array_t< "NewFloatArray", jfloatArray, &JNINativeInterface_::NewFloatArray >,
		new_array_t< "NewDoubleArray", jdoubleArray, &JNINativeInterface_::NewDoubleArray >,
		get_elems_t< "GetBooleanArrayElements", jbooleanArray, jboolean, &JNINativeInterface_::GetBooleanArrayElements >,
		get_elems_t< "GetByteArrayElements", jbyteArray, jbyte, &JNINativeInterface_::GetByteArrayElements >,
		get_elems_t< "GetCharArrayElements", jcharArray, jchar, &JNINativeInterface_::GetCharArrayElements >,
		get_elems_t< "GetShortArrayElements", jshortArray, jshort, &JNINativeInterface_::GetShortArrayElements >,
		get_elems_t< "GetIntArrayElements", jintArray, jint, &JNINativeInterface_::GetIntArrayElements >,
		get_elems_t< "GetLongArrayElements", jlongArray, jlong, &JNINativeInterface_::GetLongArrayElements >,
		get_elems_t< "GetFloatArrayElements", jfloatArray, jfloat, &JNINativeInterface_::GetFloatArrayElements >,
		get_elems_t< "GetDoubleArrayElements", jdoubleArray, jdouble, &JNINativeInterface_::GetDoubleArrayElements >,
		release_elems_t< "ReleaseBooleanArrayElements", jbooleanArray, jboolean, &JNINativeInterface_::ReleaseBooleanArrayElements >,
		release_elems_t< "ReleaseByteArrayElements", jbyteArray, jbyte, &JNINativeInterface_::ReleaseByteArrayElements >,
		release_elems_t< "ReleaseCharArrayElements", jcharArray, jchar, &JNINativeInterface_::ReleaseCharArrayElements >,
		release_elems_t< "ReleaseShortArrayElements", jshortArray, jshort, &JNINativeInterface_::ReleaseShortArrayElements >,
		release_elems_t< "ReleaseIntArrayElements", jintArray, jint, &JNINativeInterface_::ReleaseIntArrayElements >,
		release_elems_t< "ReleaseLongArrayElements", jlongArray, jlong, &JNINativeInterface_::ReleaseLongArrayElements >,
		release_elems_t< "ReleaseFloatArrayElements", jfloatArray, jfloat, &JNINativeInterface_::ReleaseFloatArrayElements >,
		release_elems_t< "ReleaseDoubleArrayElements", jdoubleArray, jdouble, &JNINativeInterface_::ReleaseDoubleArrayElements >,
		get_region_t< "GetBooleanArrayRegion", jbooleanArray, jboolean, &JNINativeInterface_::GetBooleanArrayRegion >,
		get_region_t< "GetByteArrayRegion", jbyteArray, jbyte, &JNINativeInterface_::GetByteArrayRegion >,
		get_region_t< "GetCharArrayRegion", jcharArray, jchar, &JNINativeInterface_::GetCharArrayRegion >,
		get_region_t< "GetShortArrayRegion", jshortArray, jshort, &JNINativeInterface_::GetShortArrayRegion >,
		get_region_t< "GetIntArrayRegion", jintArray, jint, &JNINativeInterface_::GetIntArrayRegion >,
		get_region_t< "GetLongArrayRegion", jlongArray, jlong, &JNINativeInterface_::GetLongArrayRegion >,
		get_region_t< "GetFloatArrayRegion", jfloatArray, jfloat, &JNINativeInterface_::GetFloatArrayRegion >,
		get_region_t< "GetDoubleArrayRegion", jdoubleArray, jdouble, &JNINativeInterface_::GetDoubleArrayRegion >,
		set_region_t< "SetBooleanArrayRegion", jbooleanArray, jboolean, &JNINativeInterface_::SetBooleanArrayRegion >,
		set_region_t< "SetByteArrayRegion", jbyteArray, jbyte, &JNINativeInterface_::SetByteArrayRegion >,
		set_region_t< "SetCharArrayRegion", jcharArray, jchar, &JNINativeInterface_::SetCharArrayRegion >,
		set_region_t< "SetShortArrayRegion", jshortArray, jshort, &JNINativeInterface_::SetShortArrayRegion >,
		set_region_t< "SetIntArrayRegion", jintArray, jint, &JNINativeInterface_::SetIntArrayRegion >,
		set_region_t< "SetLongArrayRegion", jlongArray, jlong, &JNINativeInterface_::SetLongArrayRegion >,
		set_region_t< "SetFloatArrayRegion", jfloatArray, jfloat, &JNINativeInterface_::SetFloatArrayRegion >,
		set_region_t< "SetDoubleArrayRegion", jdoubleArray, jdouble, &JNINativeInterface_::SetDoubleArrayRegion >
	> ( table );

	if ( version >= JNI_VERSION_1_6 )
		hooked += hook( table, &JNINativeInterface_::GetObjectRefType, o_GetObjectRefType, &hk_GetObjectRefType );
	if ( major >= 9 )
		hooked += hook( table, &JNINativeInterface_::GetModule, o_GetModule, &hk_GetModule );
	if ( major >= 19 )
		hooked += hook( table, &JNINativeInterface_::IsVirtualThread, o_IsVirtualThread, &hk_IsVirtualThread );
	if ( major >= 24 )
		hooked += hook( table, &JNINativeInterface_::GetStringUTFLengthAsLong, o_GetStringUTFLengthAsLong, &hk_GetStringUTFLengthAsLong );

	return hooked;
}
