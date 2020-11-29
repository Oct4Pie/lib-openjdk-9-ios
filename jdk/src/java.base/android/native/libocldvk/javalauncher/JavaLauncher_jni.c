/*
 * Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

// Implement the Dalvik JavaLauncher native methods.
// These JNI methods call into the Oracle JVM JavaLauncher library to
// create and interact with the Oracle JVM via Oracle JVM JNI
//
// This library transforms Dalvik JNI objects to C objects in order
// call into the Oracle JVM JavaLauncher library.

#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include <dlfcn.h>

#include <sys/param.h>
#include <unistd.h>

// Dalvik jni.h
#include <jni.h>
#include <pthread.h>

// Oracle Java JVM bridge
#include "javalauncher_api.h"

#include "DalvikProxySelector.h"

#include "JavaLauncher_jni.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JAVALAUNCHERCALLBACK_CLASSNAME \
        "com/oracle/dalvik/javalauncher/JavaLauncherCallback"
#define JAVALAUNCHERCALLBACK_METHOD "callback"
#define JAVALAUNCHERCALLBACK_METHOD_SIG "(Ljava/lang/String;I)V"

#define LOGGER __android_log_print

#define HAVEEXCEPTION_OR_NULL(err, obj, tag) \
    HAVEEXCEPTION(err, tag); \
    do { \
    if (obj == NULL) { \
        LOGGER(3, "JVM", err); \
        goto tag; \
    } \
    } while(0)

#define HAVEEXCEPTION(err, tag) \
    do { \
    jthrowable jexception = (*env)->ExceptionOccurred(env); \
    if (jexception) { \
        (*env)->ExceptionDescribe(env); \
        (*env)->ExceptionClear(env); \
        LOGGER(3, "JVMEXCEP", err); \
        goto tag; \
    } \
    } while(0)

static char **getStringArray(JNIEnv *env, jobjectArray args, int *len)
{
    char **cargs = NULL;
    int args_len = 0;
    *len = 0;
    if (args == NULL) {
        return NULL;
    }
    args_len = (*env)->GetArrayLength(env, args);
    HAVEEXCEPTION("JavaLauncher_jni::getStringArray: Cannot get args array length.", _gsa_fail);

    cargs = (char**) calloc(args_len, sizeof(char*));
    if (cargs == NULL) {
        LOGGER(3, "JVM", "JavaLauncher_jni::getStringArray: "
            "calloc failed.");
        return NULL;
    }
    // Keep track of partial allocations
    int howmany = 0;
    int i;
    const char *ansiString = NULL;
    for (i = 0; i < args_len; i++) {
        jstring stringElement = (jstring)
            (*env)->GetObjectArrayElement(env, args, i);
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getStringArray: "
           "GetObjectArrayElement failed.", stringElement, _gsa_fail);

        jsize len = (*env)->GetStringUTFLength(env, stringElement);
        HAVEEXCEPTION("JavaLauncher_jni::getStringArray: "
           "GetStringUTFLength failed.",  _gsa_fail);

        ansiString = (*env)->GetStringUTFChars(env, stringElement, NULL);
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getStringArray: "
           "GetStringUTFChars failed.",
           ansiString, _gsa_fail);

        cargs[i] = strndup(ansiString, len);
        if (cargs[i] == NULL) {
#ifdef DEBUG
            LOGGER(3, "JVM",
                "JavaLauncher_jni::getStringArray: "
                "strndup failed for arg '%s'.", ansiString);
#endif
            (*env)->ReleaseStringUTFChars(env, stringElement,
                (const char*)ansiString);
            HAVEEXCEPTION("JavaLauncher_jni::getStringArray: "
                "ReleaseStringUTFChars failed.", _gsa_fail);
        }
        ++howmany;
        (*env)->ReleaseStringUTFChars(env, stringElement,
            (const char*)ansiString);
        HAVEEXCEPTION("JavaLauncher_jni::getStringArray: "
            "ReleaseStringUTFChars failed ", _gsa_fail);
    }
    goto _gsa_done;

_gsa_fail:
    // Free up partial allocation
    if (howmany != 0) {
        int i;
        for (i = 0; i < howmany; ++i) {
            free(cargs[i]);
        }
    }
    free(cargs);
    cargs = NULL;

_gsa_done:
    *len = args_len;
    return cargs;
}


// Keep these around
static jclass jlc_class = NULL;
static jmethodID jlc_callbackID = NULL;

static pthread_mutex_t _exit_mutex;
static pthread_cond_t  _exit_cond;
static int dalvik_exit_code;
static jboolean java_exited;
static jboolean java_exit_thread_ready = JNI_FALSE;

static int
invoke_java_launcher_callback(JNIEnv *env, jstring jmsg, jint errorcode,
        jobject callback)
{
    int result = JL_INVOKECALLBACKFAILED;

    jlc_class = (*env)->FindClass(env, JAVALAUNCHERCALLBACK_CLASSNAME);
    HAVEEXCEPTION_OR_NULL(
        "JavaLauncher_jni::invoke_java_launcher_callback: "
        "FindClass failed." JAVALAUNCHERCALLBACK_CLASSNAME,
        jlc_class, _ijlc_fail);

    jmethodID jlc_callbackID = (*env)->GetMethodID(env, jlc_class,
        JAVALAUNCHERCALLBACK_METHOD, JAVALAUNCHERCALLBACK_METHOD_SIG);
    HAVEEXCEPTION_OR_NULL(
        "JavaLauncher_jni::invoke_java_launcher_callback: "
        "GetMethodID failed " JAVALAUNCHERCALLBACK_METHOD,
        jlc_callbackID, _ijlc_fail);

    (*env)->CallVoidMethod(env, callback, jlc_callbackID, jmsg, errorcode);
    HAVEEXCEPTION("JavaLauncher_jni::invoke_java_launcher_callback: "
            "CallVoidMethod  failed " JAVALAUNCHERCALLBACK_METHOD, _ijlc_fail);

_ijlc_fail:
    return result;

}

static int perform_error_callback(JNIEnv *env, char *msg, int errorcode,
        jobject callback)
{
    int result = JL_INVOKECALLBACKFAILED;
    if (callback == NULL) {
        LOGGER(3, "JVM",
            "JavaLauncher_jni::perform_error_callback: "
            "No callback, received error '%s', errorcode %d", msg, errorcode);
        return JL_OK;
    }
    jstring jmsg = (*env)->NewStringUTF(env, msg);
    HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::perform_error_callback: "
        "NewStringUTF failed ", jmsg, _perrc_fail);
    result = invoke_java_launcher_callback(env, jmsg, (jint)errorcode,
        callback);
_perrc_fail:
    return result;
}

#define THROWABLE_CLASSNAME "java/lang/Throwable"

static jclass throwable_class = NULL;
static jmethodID throwable_toStringID = NULL;

static int perform_exception_callback(JNIEnv *env, char *msg, int errorcode,
    jthrowable jexception, jobject callback)
{
    int result = JL_INVOKECALLBACKFAILED;
    const char *exception_msg = NULL;

    if (throwable_class == NULL) {
        throwable_class = (*env)->FindClass(env, THROWABLE_CLASSNAME);
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::perform_exception_callback: "
            "FindClass failed for " THROWABLE_CLASSNAME ".\n",
            throwable_class, _pec_fail);
        throwable_class = (*env)->NewGlobalRef(env, throwable_class);
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::perform_exception_callback: "
            "NewGlobalRef failed for " THROWABLE_CLASSNAME,
            throwable_class, _pec_fail);
    }
    if (throwable_toStringID == NULL) {
        throwable_toStringID = (*env)->GetMethodID(env, throwable_class,
            "toString", "()Ljava/lang/String;");
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::perform_exception_callback: "
            "GetMethodID failed for Throwable.toString().\n",
            throwable_toStringID, _pec_fail);
        throwable_toStringID = (jmethodID)(*env)->NewGlobalRef(env,
            (jobject)jlc_callbackID);
        HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::perform_exception_callback: "
            "NewGlobalRef failed for Throwable.toString().\n",
            throwable_toStringID, _pec_fail);
    }
    jstring jexception_msg = (*env)->CallObjectMethod(env, jexception,
        throwable_toStringID);
    // Let jexception_msg be NULL, and use msg if not NULL.
    HAVEEXCEPTION("JavaLauncher_jni::perform_exception_callback: "
        "CallObjectMethod failed for Throwable.toString().\n", _pec_fail);

    jstring emsg = NULL;
    if (msg != NULL) {
        emsg = (*env)->NewStringUTF(env, msg);
        // Let's not care if the emsg is NULL, we just don't try and use it
        // if there is no exception
        HAVEEXCEPTION("JavaLauncher_jni::perform_exception_callback: "
            "NewStringUTF failed for msg.\n",
            _pec_fail);
    }
    if (callback == NULL) {
        LOGGER(3, "JVM", "JavaLauncher_jni::perform_error_callback: "
            "No callback, received exception '%s'\n"
            "error msg '%s', errorcode %d",
            (exception_msg != NULL ? exception_msg : "NULL"),
            (emsg != NULL ? msg : "NULL"), errorcode);
        return JL_OK;
    } else {
        if (exception_msg != NULL) {
            result = invoke_java_launcher_callback(env, jexception_msg,
                    (jint)errorcode, callback);
        } else if (emsg != NULL) {
            result = invoke_java_launcher_callback(env, emsg, (jint)errorcode,
                callback);
        } else {
            jstring emsg = (*env)->NewStringUTF(env, "Unknown error");
            // Let's not care if the emsg is NULL, we just don't try and use it
            // if there is no exception
            HAVEEXCEPTION("JavaLauncher_jni::perform_exception_callback: "
                "NewStringUTF failed for 'Unknown error'.\n",
                _pec_fail);
            result = invoke_java_launcher_callback(env, emsg, (jint)errorcode,
                callback);
        }
    }
_pec_fail:
    return result;
}

typedef struct {
    JNIEnv *env;
    jobject callback;
} appdata_t;

static void dalvik_javaLauncherCallback(char *msg, int errorcode,
        void *appdata)
{
    // The dalvik java JavaLauncherCallback object
    appdata_t *envandcb = (appdata_t*)appdata;
    if (envandcb == NULL || envandcb->env == NULL ||
            envandcb->callback == NULL) {
        LOGGER(3, "JVM",
            "JavaLauncher_jni::dalvik_javaLauncherCallback: "
            "No Java callback, received error: '%s'.", msg);
        return;
    }
    JNIEnv *env = envandcb->env;
    jstring jmsg = (*env)->NewStringUTF(env, msg);
    int result = invoke_java_launcher_callback(envandcb->env,
        jmsg, errorcode, envandcb->callback);
}

static char *getString(JNIEnv *env, jstring str)
{
    jboolean copy;

    if (str == NULL) {
#ifdef DEBUG
        LOGGER(3, "JVM", "JavaLauncher_jni::getString: NULL str argument.");
#endif
        return NULL;
    }
    jsize len = (*env)->GetStringUTFLength(env, str);
    HAVEEXCEPTION("JavaLauncher_jni::getString: "
           "GetStringUTFLength failed.",  _gs_fail);

    const char *tmpstr = (*env)->GetStringUTFChars(env, str, &copy);
    HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getString: "
           "GetStringUTFChars failed.", tmpstr, _gs_fail);

    char *cstr = strndup(tmpstr, len);
    if (cstr == NULL) {
#ifdef DEBUG
        LOGGER(3, "JVM",
            "JavaLauncher_jni::getString: strndup of jstring '%s' failed.",
            tmpstr);
#endif
    }
    (*env)->ReleaseStringUTFChars(env, str, (const char*)tmpstr);
    HAVEEXCEPTION("JavaLauncher_jni::getStringField: "
        "ReleaseStringUTFChars failed.", _gs_fail);
_gs_fail:
    return cstr;
}

static char *getStringField(JNIEnv *env, const char *classname,
        const char *field)
{
    jclass fldclass = (*env)->FindClass(env, classname);
    HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getStringField: "
        "FindClass failed for ", fldclass, _gsf_fail);

    jfieldID fldid = (*env)->GetStaticFieldID(env, fldclass, field,
        "Ljava/lang/String");
    HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getStringField: "
        "GetStaticFieldID failed for ", fldid, _gsf_fail);

    jstring fldstr = (*env)->GetStaticObjectField(env, fldclass, fldid);
    HAVEEXCEPTION_OR_NULL("JavaLauncher_jni::getStringField: "
        "GetStaticField failed for", fldstr, _gsf_fail);

_gsf_fail:
    return getString(env, fldstr);
}

static char *getPath(const char *prefix, const char *path)
{
    int len = strlen(prefix) + strlen(path) + 2; // 2 for '/' + '\0';
    char *fullpath = (char *)calloc(1, len);
    if (fullpath == NULL) {
        LOGGER(3, "JVM", "JavaLauncher_jni:getPath: "
            "calloc failed for path %s/%s.", prefix, path);
        return NULL;
    }
    snprintf(fullpath, len, "%s/%s", prefix, path);
    return fullpath;
}

/*
 * Pointer to the needed JNI invocation API, initialized by loadJavaVM.
 */
typedef jint (JNICALL *CreateJavaVM_t)(JavaVM **pvm, void **env, void *args);

static CreateJavaVM_t jl_JNI_CreateJavaVM;
static char *libjvmpath = NULL;
static void *libjvm = NULL;
static char *javaHome = NULL;
static char *javaLib = NULL;


static int loadJavaVM()
{
    if (libjvmpath == NULL) {
        libjvmpath = getPath(javaLib, "libjvm.so");
        if (libjvmpath == NULL) {
            LOGGER(3, "JVM", "JavaLauncher_jni::loadJavaVM: "
            "getPath failed building full path for libjvm.so.");
            return JL_CANTLOADLIBJVM;
        }
    }

    if (libjvm != NULL) {
        libjvm = dlopen(libjvmpath, RTLD_NOW + RTLD_GLOBAL);
        if (libjvm == NULL) {
            LOGGER(3, "JVM", "JavaLauncher_jni::loadJavaVM: "
                "dlopen failed to open %s (dlerror %s).",
                libjvmpath, dlerror());
            return JL_CANTLOADLIBJVM;
        }
    }

    if (jl_JNI_CreateJavaVM != NULL) {
        jl_JNI_CreateJavaVM = (CreateJavaVM_t)dlsym(libjvm, "JNI_CreateJavaVM");
        if (jl_JNI_CreateJavaVM == NULL) {
            LOGGER(3, "JVM", "JavaLauncher_jni::loadJavaVM: "
            "dlsym failed to get JNI_CreateJavaVM (dlerror %s).", dlerror());
            return JL_CANTLOADLIBJVM;
        }
    }
    return JL_OK;
}

/*
 * Class:     com_oracle_dalvik_JavaLauncher
 * Method:    _createJavaVM
 * Signature: ([Ljava/lang/String;Lcom/oracle/dalvik/javalauncher/JavaLauncherCallback;)I
 *
 * o Initialize some global variables from Field members javaHome
 *   and javaLib. Load libjvm and the SE java launcher library.
 */
JNIEXPORT jint JNICALL Java_com_oracle_dalvik_javalauncher_JavaLauncher__1createJavaVM
  (JNIEnv *env, jobject thisobj, jobjectArray javaArgs, jobject callback)
{
    int java_args_len = 0;
    char **java_args = getStringArray(env, javaArgs, &java_args_len);

    appdata_t appdata;
    appdata.env = env;
    appdata.callback = callback;
    int result = jl_createJavaVM(java_args, java_args_len,
        (JavaLauncherCallback)dalvik_javaLauncherCallback, (void*)&appdata);

    return result;
}

  /*
   * This stub is called from Oracle Java (java.cpp) when System.exit() is
   * called. It notifies the Dalvik thread which completes the System.exit()
   * in the context of the Dalvik thread
   */
JNIEXPORT void JNICALL
jl_system_exit(int code) {
  LOGGER(3,"JVM", "JavaLauncher_jni::jl_system_exit %d", code);
  if (java_exit_thread_ready) {
    pthread_mutex_lock(&_exit_mutex);
    dalvik_exit_code = code;
    java_exited = JNI_TRUE;
    pthread_cond_signal(&_exit_cond);
    pthread_mutex_unlock(&_exit_mutex);
    // Wait a bit for Dalvik thread to wake up and kill the process.
    // If it takes more than 20 sec. we'll just hammer the process by calling
    // exit()
    sleep(20);
  }
  // Exiting before we are fully initialized, just kill the process.
  exit(code);
}

JNIEXPORT jint JNICALL Java_com_oracle_dalvik_javalauncher_JavaLauncher__1exitJava
  (JNIEnv *env, jobject thisobj)
{
  /*
   * This is the Dalvik exit thread that waits for a notify from Oracle Java
   * thread above. It returns exit code back to Dalvik Java call which calls
   * Dalvik System.exit(code) which unwinds process gracefully
   */
  pthread_mutex_lock(&_exit_mutex);
  java_exit_thread_ready = JNI_TRUE;
  while (!java_exited) {
    pthread_cond_wait(&_exit_cond, &_exit_mutex);
  }
  pthread_mutex_unlock(&_exit_mutex);
  return dalvik_exit_code;
}

/*
 * Class:     com_oracle_dalvik_JavaLauncher
 * Method:    _callJava
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Lcom/oracle/dalvik/javalauncher/JavaLauncherCallback;)I
 */
JNIEXPORT jint JNICALL Java_com_oracle_dalvik_javalauncher_JavaLauncher__1callJava
  (JNIEnv *env, jobject thisobj, jstring jmainClass, jstring jmainMethod,
  jstring jsignature, jobjectArray jappArgs, jobject callback)
{
    appdata_t appdata;
    appdata.env = env;
    appdata.callback = callback;
    char *mainClass = getString(env, jmainClass);
    char *mainMethod = getString(env, jmainMethod);
    char *signature = getString(env, jsignature);
    int app_args_len = 0;
    char **appArgs = getStringArray(env, jappArgs, &app_args_len);

    return jl_callJava(mainClass, mainMethod, signature, appArgs, app_args_len,
        (JavaLauncherCallback)dalvik_javaLauncherCallback, (void*)&appdata);
}

/*
 * Class:     com_oracle_dalvik_JavaLauncher
 * Method:    _destroyJavaVM
 * Signature: (Lcom/oracle/dalvik/JavaLauncherCallback;)I
 */
JNIEXPORT jint JNICALL Java_com_oracle_dalvik_javalauncher_JavaLauncher__1destroyJavaVM
  (JNIEnv *env, jobject thisobj, jobject callback)
{
    appdata_t appdata;
    appdata.env = env;
    appdata.callback = callback;

    return jl_destroyJavaVM((JavaLauncherCallback)dalvik_javaLauncherCallback,
        &appdata);
}

JNIEnv *dalvikJNIEnvPtr = NULL;
JavaVM *dalvikJavaVMPtr = NULL;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    // Assume the Oracle JavaLauncher library is linked in.
    // The sources could be part of the Dalvik libjavalauncher.so
    //
    // Still probably need java.home to pass to the
    // Oracle JavaLauncher JNI side in order to load libjvm.so

    dalvikJavaVMPtr = vm;
    JNIEnv* env = NULL;
    (*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4);
    if (env == NULL) {
        LOGGER(3,"JVM", "JavaLauncher_jni::JNI_OnLoad: "
            "Cannot initDalvikProxySelectorData()");
    }
    dalvikJNIEnvPtr = env;

    initDalvikProxySelectorData(env);
#ifdef DEBUG
    LOGGER(3,"JVM", "JavaLauncher_jni::JNI_OnLoad: "
        "initDalvikProxySelectorData called.");
#endif
    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    // TODO: unload libjvm.so
}

JNIEXPORT jint Java_com_oracle_dalvik_javalauncher_JavaLauncher__1initialize(
        JNIEnv *env, jobject thisobj, jobject jstr_javaHome)
{
    LOGGER(3, "JVM", "JavaLauncher_jni::initialize: ");

    if (jstr_javaHome == NULL) {
#ifdef DEBUG
        LOGGER(3, "JVM", "JavaLauncher_jni::initialize: "
            "javaHome argument is NULL");
#endif
        return JL_FAIL;
    }
    if (javaHome == NULL) {
        javaHome = getString(env, jstr_javaHome);
        if (javaHome == NULL) {
#ifdef DEBUG
            LOGGER(3, "JVM", "JavaLauncher_jni::initialize: "
                "getString failed for 'javaHome'.");
#endif
            return JL_FAIL;
        }
    }
    if (javaLib == NULL) {
        // Need to figure out whether client is never minimal.
        javaLib = getPath(javaHome, "lib");
        if (javaLib == NULL) {
#ifdef DEBUG
            LOGGER(3, "JVM", "JavaLauncher_jni::initialize: "
                "getPath failed building full path for 'javaLib'.");
#endif
            return JL_FAIL;
        }
    }
    // Call into javalauncher_api
    int result = jl_initialize(javaLib);
    if (result == JL_OK) {
      pthread_mutex_init(&_exit_mutex, NULL);
      pthread_cond_init(&_exit_cond, NULL);
    } else {
        LOGGER(3,"JVM", "JavaLauncher_jni::initialize: "
            "Cannot initialize javalauncher_api, errocode %d.", result);
    }
    return result;
}

#ifdef __cplusplus
}
#endif
