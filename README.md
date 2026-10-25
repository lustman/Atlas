# Atlas

Atlas is a tool for tracing JNI calls. It is suitable for J2C applications and for applications built with pure JNI.

It runs only on Windows.

## Build

```
msbuild Atlas.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild Atlas.vcxproj /p:Configuration=Release-Injectable /p:Platform=x64
```

## Editions

There are two builds.

**Atlas-OnLoad.dll** is loaded by the JVM. Patch a class's `<clinit>`.

```
ldc "C:\\path\\to\\Atlas-OnLoad.dll"
invokestatic java/lang/System.load (Ljava/lang/String;)V
```

**Atlas-Injectable.dll** is for a process that is already running, or for a JAR you cannot edit. Use LLA or MMap.

## Log

Traces are written to `C:\Atlas\<process>_<pid>\`.

Example traces:

```c
jstring jstring_1207 = env->NewStringUTF("atlas_test_228"); /// atlas_test_228

jobject jobject_305 = env->CallObjectMethodV(jstring_1207, jmethodID_7); /// java/lang/String intern()Ljava/lang/String; atlas_test_228

jboolean jboolean_299 = env->ExceptionCheck(); /// JNI_FALSE

jobject jobject_306 = env->NewGlobalRef(jobject_305); /// jobject_305 same as jstring_1207

env->DeleteLocalRef(jstring_1207);

env->DeleteLocalRef(jobject_305);

jstring jstring_1208 = env->NewStringUTF("Access granted."); /// Access granted.

jobject jobject_307 = env->CallObjectMethodV(jstring_1208, jmethodID_7); /// java/lang/String intern()Ljava/lang/String; Access granted.

jboolean jboolean_300 = env->ExceptionCheck(); /// JNI_FALSE

jobject jobject_308 = env->NewGlobalRef(jobject_307); /// jobject_307 same as jstring_1208

env->DeleteLocalRef(jstring_1208);

env->DeleteLocalRef(jobject_307);

jstring jstring_1209 = env->NewStringUTF("Access denied."); /// Access denied.

jobject jobject_309 = env->CallObjectMethodV(jstring_1209, jmethodID_7); /// java/lang/String intern()Ljava/lang/String; Access denied.

jboolean jboolean_301 = env->ExceptionCheck(); /// JNI_FALSE

jobject jobject_310 = env->NewGlobalRef(jobject_309); /// jobject_309 same as jstring_1209

env->DeleteLocalRef(jstring_1209);

env->DeleteLocalRef(jobject_309);
```

```c
jmethodID jmethodID_22 = env->GetMethodID(jweak_4, "equals", "(Ljava/lang/Object;)Z"); ///  equals(Ljava/lang/Object;)Z

jboolean jboolean_440 = env->ExceptionCheck(); /// JNI_FALSE

jboolean jboolean_441 = env->CallBooleanMethodV(jobject_377, jmethodID_22, jobject_306); ///  equals(Ljava/lang/Object;)Z JNI_FALSE 1337lox1337
```

## License

Project code: [LICENSE](LICENSE).

JNI headers: [vendor/jni/LICENSE](vendor/jni/LICENSE).