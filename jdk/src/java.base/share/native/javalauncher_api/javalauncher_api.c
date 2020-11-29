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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>

#include <pthread.h>

#ifndef STATIC_BUILD
#include <dlfcn.h>
#endif

#include "jni.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include "javalauncher_api.h"

static FILE *java_error_out = NULL;
#define ERROROUT (java_error_out == NULL ? stderr : java_error_out)

#define MSGBUFSIZE 1024

#ifdef __APPLE__
#include <TargetConditionals.h>
#else
#define TARGET_OS_IPHONE 0
#endif

#if ! TARGET_OS_IPHONE
#define JL_LIBJVM "libjvm.so"
#else
/* In case it is not a STATIC_BUILD */
#define JL_LIBJVM "libjvm.dylib"
#endif

#ifdef DEBUG

#ifndef __ANDROID__
static void jl_log(int prio, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(ERROROUT, fmt, args);
    va_end(args);
}
#else
#define jl_log __android_log_print
#endif /* __ANDROID__ */

#else

#define jl_log(prio, tag, fmt, ...)

#endif /* DEBUG */

#define HAVE_EXCEPTION(e, m) \
    do {\
    if ((*e)->ExceptionOccurred(e) != NULL) { \
        jl_log(3, "JL", m);\
        (*e)->ExceptionDescribe(e);\
        (*e)->ExceptionClear(e);\
        return;\
    } \
    } while (0)

#define EMPTYSTR(s) ((s) == NULL || *(s) == '\0')

static JavaVM *jvm = NULL;
static pthread_rwlock_t jvm_lock = PTHREAD_RWLOCK_INITIALIZER;

static JavaVM* get_jvm() {
    JavaVM *_jvm = NULL;
    pthread_rwlock_rdlock(&jvm_lock);
    _jvm = jvm;
    pthread_rwlock_unlock(&jvm_lock);
    return _jvm;
}
static void set_jvm(JavaVM *_jvm) {
    pthread_rwlock_wrlock(&jvm_lock);
    jvm = _jvm;
    pthread_rwlock_unlock(&jvm_lock);
}

static int jvm_destroyed = 0;
static pthread_rwlock_t jvm_destroyed_lock = PTHREAD_RWLOCK_INITIALIZER;
static int get_jvm_destroyed() {
    int _jvm_destroyed = 0;
    pthread_rwlock_rdlock(&jvm_destroyed_lock);
    _jvm_destroyed = jvm_destroyed;
    pthread_rwlock_unlock(&jvm_destroyed_lock);
    return _jvm_destroyed;
}

static JavaVMInitArgs *vmInitArgs = NULL;
static JavaVMOption *jvmOptions = NULL;

#define SIGNATURE0 "([Ljava/lang/String;)V"
#define SIGNATURE1 "()V"
#define DEFAULT_SIGNATURE SIGNATURE0

size_t sigcount = 2;
static char *signatures[] = {
        SIGNATURE0,
        SIGNATURE1
};

// global object references, must be NULL'd
// when the vm is destroyed
static jobject _context_class_loader;
static jobject _java_lang_string;
static jobject _java_lang_thread;
static jmethodID _java_thread_current_thread;
static jmethodID _java_thread_set_context_classloader;

static jobject get_context_classloader(JNIEnv *env);
static jobject get_java_lang_string(JNIEnv *env);
static jmethodID get_set_context_classloader_method_id(JNIEnv *env);
static jmethodID get_current_thread_method_id(JNIEnv *env);
static jobject get_java_lang_thread(JNIEnv *env);
static int init_context_classloader_javaclasses_and_methodids(JNIEnv *env);
static void cleanup_java_references();
static int set_current_thread_context_classloader(JNIEnv *env);

static int is_supported_signature(char *signature)
{
    size_t i;
    for (i = 0; i < sigcount; ++i) {
        if (strncmp(signature, signatures[i], strlen(signatures[i])) == 0) {
            return 1;
        }
    }
    return 0;
}

static void perform_error_callback(char *msg, int error_code,
        JavaLauncherCallback callback, void *app_data)
{
    if (callback != NULL) {
        callback(msg, error_code, app_data);
    }
    jl_log(3, "JL", "javalauncher_api::perform_error_callback: "
        "%s - error %d\n", msg, error_code);
}

static void perform_exception_callback(JNIEnv *env, char *msg, int error_code,
    jthrowable jexception, JavaLauncherCallback callback, void *app_data)
{
    const char *exception_msg = NULL;

    jclass throwable_class = (*env)->FindClass(env, "java/lang/Throwable");
    HAVE_EXCEPTION(env,
        "perform_callback: FindClass java/lang/Throwable failed.\n");
    jmethodID throwable_toString = (*env)->GetMethodID(env, throwable_class,
        "toString", "()Ljava/lang/String;");
    HAVE_EXCEPTION(env,
        "perform_callback: GetMethodID for toString failed.\n");
    jstring jexception_msg = (*env)->CallObjectMethod(env, jexception,
        throwable_toString);
    HAVE_EXCEPTION(env,
        "perform_callback: CallObjectMethod toString failed.\n");
    if (jexception_msg != NULL) {
        exception_msg = (*env)->GetStringUTFChars(env, jexception_msg, 0);
        HAVE_EXCEPTION(env, "perform_callback: GetStringUTFChars failed.\n");
    }
    if (callback != NULL) {
        if (exception_msg != NULL) {
            callback(exception_msg, error_code, app_data);
        } else if (msg != NULL) {
            callback(msg, error_code, app_data);
        } else {
            callback("perform_exception_callback: Unknown error.", error_code,
                app_data);
            jl_log(3, "JL", "javalauncher_api::perform_exception_callback: "
                "Called with no message.");
        }
    }
#ifdef DEBUG
    if (msg != NULL) {
        jl_log(3, "JL", msg);
    }
    if (exception_msg != NULL) {
        jl_log(3, "JL", exception_msg);
    }
#endif
    if (exception_msg != NULL) {
        (*env)->ReleaseStringUTFChars(env, jexception_msg, exception_msg);
    }
    return;
}

static void jl_freejvm()
{
    set_jvm(NULL);
    free(jvmOptions);
    jvmOptions = NULL;
    free(vmInitArgs);
    vmInitArgs = NULL;
}

/* The prototype for JNI_CreateJavaVM needed for platforms that use
 * shared libraries
 */
typedef jint JNICALL (*JNI_CreateJavaVM_t)
(JavaVM **pvm, void **env, void *args);

#ifndef STATIC_BUILD
static JNI_CreateJavaVM_t jl_JNI_CreateJavaVM = NULL;
static void *libjvm = NULL;
static int initialized = 0;
#else
static JNI_CreateJavaVM_t jl_JNI_CreateJavaVM = JNI_CreateJavaVM;
static int initialized = 1;
#endif

#ifndef STATIC_BUILD
// Listed in search order.
static char *jvmtypes[] = {
    "minimal", "client", "server", NULL
};

static int loadJavaVM(const char *java_lib)
{
    char libjvmpath[PATH_MAX];
    int i;
    for (i = 0; jvmtypes[i] != NULL; ++i) {
        int len = strlen(java_lib) + strlen(jvmtypes[i]) +
                strlen(JL_LIBJVM) + 3; // 3 for 2 '/' + '\0';
        if (len >= PATH_MAX) {
            jl_log(3, "JL", "javalauncher_api::loadJavaVM: "
                "Cannot load %s/%s/%s, path too long.", java_lib,
                jvmtypes[i], JL_LIBJVM);
            return JL_CANTLOADLIBJVM;
        }
        snprintf(libjvmpath, len, "%s/%s/%s", java_lib, jvmtypes[i], JL_LIBJVM);
        libjvm = dlopen(libjvmpath, RTLD_NOW + RTLD_GLOBAL);
        if (libjvm != NULL) {
            break;
        } else {
            jl_log(3, "JL", "javalauncher_api::loadJavaVM: "
                   "dlopen failed for %s (dlerr r %s).", libjvmpath, dlerror());
        }
    }
    if (libjvm == NULL) {
        jl_log(3, "JL", "javalauncher_api::loadJavaVM: "
            "dlopen failed to open %s (dlerror %s).", libjvmpath, dlerror());
        return JL_CANTLOADLIBJVM;
    } else {
        return JL_OK;
    }
}
#endif


int jl_initialize(const char *java_lib)
{
#ifdef STATIC_BUILD
    initialized = 1;
    return JL_OK;
#else
    int result = JL_OK;
    if (initialized) {
        return JL_OK;
    }
    result = loadJavaVM(java_lib);
    if (result != JL_OK) {
        return result;
    }

    jl_JNI_CreateJavaVM = (JNI_CreateJavaVM_t)dlsym(libjvm, "JNI_CreateJavaVM");
    if (jl_JNI_CreateJavaVM == NULL) {
        jl_log(3, "JL", "javalauncher_api::jl_initialize: "
            "dlsym failed for JNI_CreateJavaVM (dlerror %s).", dlerror());
        return JL_CANTLOADLIBJVM;
    }
    initialized = 1;

    return JL_OK;
#endif
}

// jl_createJavaVM and jl_destroyJavaVM should only be called
// on the same thread, otherwise JavaVM->DestroyJavaVM does not return.
// So there should be no contention between these two functions.
int jl_createJavaVM(char **jvm_args, int jvm_args_len,
        JavaLauncherCallback callback, void *app_data)
{
    JavaVM *_jvm = NULL;
    JNIEnv *env = NULL;
    jint result = JL_OK;
    char msgbuf[MSGBUFSIZE];

    java_error_out = ERROROUT;

#ifndef STATIC_BUILD
    if (!initialized) {
        perform_error_callback(
            "jl_createJavaVM: jl_initialize "
            "has not been called, cannot create the Java VM.",
            JL_NOTINITIALIZED, callback, app_data);
        return JL_NOTINITIALIZED;
    }
#endif

    if (jvm_destroyed) {
        perform_error_callback(
            "jl_createJavaVM: jl_destroyJavaVM "
            "has been called, cannot re-create the Java VM.",
            JL_JVMISDESTROYED, callback, app_data);
        return JL_JVMISDESTROYED;
    }

    if (jvm != NULL) {
        jl_log(3, "JL", "javalauncher_api::jl_createJavaVM: "
            "The Java VM has already been created.\n");
        return JL_OK;
    }

    if (jvm_args_len != 0 && (jvm_args == NULL || jvm_args[0] == NULL)) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_createJavaVM: "
                 "jvm_args_len is %d but there are no arguments set\n",
                 jvm_args_len);
        perform_error_callback(msgbuf, JL_BADARGUMENTS, callback, app_data);
        return JL_BADARGUMENTS;
    }

    if (jvm_args_len != 0) {
        jvmOptions = (JavaVMOption*)calloc(jvm_args_len,
            sizeof(JavaVMOption));
        if (jvmOptions == NULL) {
            perform_error_callback("jl_createJavaVM: "
                "Cannot create JavaVMOptions.\n",
                JL_CANNOTCREATEJVMOPTIONS, callback, app_data);
            return JL_CANNOTCREATEJVMOPTIONS;
        }
        int i;
        for (i = 0; i < jvm_args_len ; ++i) {
            jvmOptions[i].optionString = jvm_args[i];
        }
    }

    vmInitArgs = (JavaVMInitArgs*)calloc(1, sizeof(JavaVMInitArgs));
    if (vmInitArgs == NULL) {
        result = JL_CANNOTCREATEVMINITARGS;
        perform_error_callback("jl_createJavaVM: Cannot create JavaVMInitArgs.",
            result, callback, app_data);
        free(jvmOptions);
        return JL_CANNOTCREATEVMINITARGS;
    }
    vmInitArgs->version = JNI_VERSION_1_8;
    vmInitArgs->options = jvmOptions;
    vmInitArgs->nOptions = jvm_args_len;

    /* Create the Java VM */
    result = jl_JNI_CreateJavaVM(&_jvm, (void**)&env, (void*)vmInitArgs);
    if (result == JNI_OK) {
        // cache frequently used java class references and method id's
        // and get the context classloader
        result = init_context_classloader_javaclasses_and_methodids(env);
        if (result < 0) {
            jthrowable jexception = (*env)->ExceptionOccurred(env);
            if (jexception) {
                (*env)->ExceptionDescribe(env);
                (*env)->ExceptionClear(env);
            }
            (*_jvm)->DetachCurrentThread(_jvm);
            (*_jvm)->DestroyJavaVM(_jvm);
            jvm_destroyed = 1;
            cleanup_java_references();
            snprintf(msgbuf, MSGBUFSIZE, "jl_createJavaVM: "
                     "Cannot create Java VM. Could not initialize context "
                     "classloader, java classes or method ids.\n");
        }

    } else {
        snprintf(msgbuf, MSGBUFSIZE, "jl_createJavaVM: "
                 "Cannot create Java VM. JNI_CreateJavaVM returned error %d.\n",
                 result);
    }
    if (result < 0) {
        perform_error_callback(msgbuf, JL_CANNOTCREATEJVM, callback, app_data);
        free(jvmOptions);
        free(vmInitArgs);
        jvmOptions = NULL;
        vmInitArgs = NULL;
        jl_log(3, "JL", "javalauncher_api::jl_createJavaVM: "
               "VM not created.");
        return JL_CANNOTCREATEJVM;
    }

    jl_log(3, "JL", "javalauncher_api::jl_createJavaVM: VM created.");

    set_jvm(_jvm);

    return JL_OK;
}

// Use get_jvm() to check for the lock on jvm and
// get_jvm_destroyed to check for the lock on jvm_destroyed.

int jl_callJava(char *javaclass, char *method, char *signature,
                char **app_args, int app_args_len,
                JavaLauncherCallback callback,
                void *app_data)
{
    JavaVM *_jvm = NULL;
    JNIEnv *env = NULL;
    jclass cls = NULL;
    jmethodID mid = NULL;
    jobjectArray jobjarrayargs = NULL;

    jthrowable jexception = NULL;
    jint result = 0;
    char *errmsg = NULL;
    char msgbuf[MSGBUFSIZE];

    if (get_jvm_destroyed()) {
        perform_error_callback("jl_callJava: "
            "jl_destroyJavaVM has been called, cannot execute java code.\n",
            JL_JVMISDESTROYED, callback, app_data);
        return JL_JVMISDESTROYED;
    }
    _jvm = get_jvm();
    if (_jvm == NULL) {
        perform_error_callback("jl_callJava: "
            "jl_createJavaVM has not been called.\n",
            JL_CREATEJAVAVMNOTCALLED, callback, app_data);
        return JL_CREATEJAVAVMNOTCALLED;
    }

    if (EMPTYSTR(javaclass) || EMPTYSTR(method)) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
                 "javaclass '%s' or method '%s' argument is not set.",
                 EMPTYSTR(javaclass) ? "" : javaclass,
                 EMPTYSTR(method) ? "" : method);
        perform_error_callback(msgbuf, JL_BADARGUMENTS, callback, app_data);
        return JL_BADARGUMENTS;
    }

    if (app_args_len != 0 && (app_args == NULL || app_args[0] == NULL)) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
                 "app_args_len is %d but there are no arguments set\n",
                 app_args_len);
        perform_error_callback(msgbuf, JL_BADARGUMENTS, callback, app_data);
        return JL_BADARGUMENTS;
    }
    // Use a default signature of "([Ljava/lang/String;)V"
    if (EMPTYSTR(signature)) {
        signature = DEFAULT_SIGNATURE;
    } else
    if (!is_supported_signature(signature)) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
                 "Unsupported signature '%s'.\n", signature);
        perform_error_callback(msgbuf, JL_BADSIGNATURE, callback, app_data);
        return JL_BADSIGNATURE;
    }

    // When compiling using the NDK this is the cast that is needed for "&env".
    // (const struct JNINativeInterface ***)
    result = (*_jvm)->AttachCurrentThread(_jvm, (void **)&env, NULL);
    if (result != JNI_OK) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
                 "Cannot attach current thread error = %d\n", result);
        errmsg = msgbuf;
        result = JL_CANNOTATTACHCURRTHREAD;
        // Send the error here, since env is probably null.
        perform_error_callback(errmsg, result, callback, app_data);
        return result;
    }
    // local ref capacity
    // app_args_len + mainclass + mainmethod + stringclass + object array +
    // jexception ? so lets say app_args_len + 20 for the misc refs, and the
    // calls to get the current thread
    result = (*env)->PushLocalFrame(env, app_args_len + 20);
    if (result != 0) {
        snprintf(msgbuf, MSGBUFSIZE,
                "jl_callJava: Cannot push local frame for %d "
                 "local references, error = %d .",
                 app_args_len + 10, result);
        errmsg = msgbuf;
        result = JL_CANNOTPUSHLOCALFRAME;

    }
    if (set_current_thread_context_classloader(env)) {
        errmsg = "jl_callJava: "
                 "Cannot set current thread's context classloader.";
        result = JL_CANTSETCONTEXTCLASSLOADER;
        goto jl_calljava_return;
    }

    // Prepare to run the application's java code
    cls = (*env)->FindClass(env, javaclass);
    if (cls == NULL) {
        result = JL_CANNOTFINDCLASS;
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
            "Cannot find class '%s'\n", javaclass);
        errmsg = msgbuf;
        goto jl_calljava_return;
    }

    mid = (*env)->GetStaticMethodID(env, cls, method, signature);
    if (mid == NULL) {
        result = JL_METHODDOESNOTEXIST;
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: Method '%s' does not exist.\n", method);
        errmsg = msgbuf;
        goto jl_calljava_return;
    }

    // Only one of two signatures with or without an array of strings
    // Always send a non null jobjarrayargs
    jobjarrayargs = (*env)->NewObjectArray(env, app_args_len,
                                           get_java_lang_string(env), NULL);
    if (jobjarrayargs == NULL) {
        result = JL_CANNOTCREATEOBJARRAY;
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
             "Cannot create object array for %d elements",
             app_args_len);
        errmsg = msgbuf;
        goto jl_calljava_return;
    }
    int i;
    for (i = 0; i < app_args_len; ++i) {
        jstring jarg = (*env)->NewStringUTF(env, app_args[i]);
        if (jarg == NULL) {
            result = JL_CANNOTCREATESTRUTF;
            snprintf(msgbuf, MSGBUFSIZE,
             "jl_callJava: Cannot create java string for '%s'\n", app_args[i]);
            errmsg = msgbuf;
            goto jl_calljava_return;
        }
        (*env)->SetObjectArrayElement(env, jobjarrayargs, i, jarg);
        if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
            result = JL_CANNOTSETARRAYELEMENT;
            snprintf(msgbuf, MSGBUFSIZE,
                "jl_callJava: Cannot set array element %d to '%s'\n",
                i, app_args[i]);
            errmsg = msgbuf;
            goto jl_calljava_return;
        }
    }

    // Call the application's java code
    (*env)->CallStaticVoidMethod(env, cls, mid, jobjarrayargs);
jl_calljava_return:
    jexception = (*env)->ExceptionOccurred(env);
    if (jexception) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        perform_exception_callback(env, errmsg, result, jexception, callback,
            app_data);
    } else if (result != 0) {
        perform_error_callback(errmsg, result, callback, app_data);
    }

    (*env)->PopLocalFrame(env, NULL);

    result = (*_jvm)->DetachCurrentThread(_jvm);
    if (result != JNI_OK) {
        snprintf(msgbuf, MSGBUFSIZE, "jl_callJava: "
             "Cannot detach current thread error = %d\n", result);
        perform_error_callback(errmsg, result, callback, app_data);
        result = JL_CANNOTDETACHCURRTHREAD;
    }
    return result;
}

int jl_destroyJavaVM(JavaLauncherCallback callback, void *app_data)
{
    jint detachres = 0;
    jint result = JNI_OK;

    if (jvm_destroyed) {
        jl_log(3, "JL", "javalauncher_api::jl_destroyJavaVM: "
            "has already been called.\n");
        return JL_JVMISDESTROYED;
    }

    if (jvm == NULL) {
        jl_log(3, "JL", "javalauncher_api::jl_destroyJavaVM: "
            "jl_createJavaVM has not been called.\n");
        return JL_CREATEJAVAVMNOTCALLED;
    }
    // lock jvm_destroyed
    pthread_rwlock_wrlock(&jvm_destroyed_lock);
    jvm_destroyed = 1;
    pthread_rwlock_unlock(&jvm_destroyed_lock);

    detachres = (*jvm)->DetachCurrentThread(jvm);
    if (detachres != JNI_OK) {
        char msgbuf[256];
        snprintf(msgbuf, 256,
            "jl_destroyJavaVM: Cannot detach current thread, error = %d.\n",
                 detachres);
        perform_error_callback(msgbuf, JL_CANNOTDETACHCURRTHREAD, callback, app_data);
    }
    result = (*jvm)->DestroyJavaVM(jvm);
    cleanup_java_references();
    jl_freejvm();

    return result;
}

// Return a global reference for the
// "jdk.internal.loader.ClassLoaders.appClassLoader()"
// classloader that is initialized during the system classloader
// initialization, or NULL if the call fails. Log any error messages
// from jni calls.
// Caller must check and handle exceptions if NULL is returned.
static jobject get_context_classloader(JNIEnv *env)
{
    if (_context_class_loader == NULL) {
        jclass lclass = (*env)->FindClass(env, "jdk/internal/loader/ClassLoaders");
        if (lclass == NULL) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Cannot find class jdk.internal.loader.ClassLoaders");
            return NULL;
        }
        jmethodID getlauncher = (*env)->GetStaticMethodID(env, lclass,
                                                          "appClassLoader",
                                                       "()Ljava/lang/ClassLoader;");
        if (getlauncher == NULL) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Cannot get ClassLoaders.appClassLoader() method id");
            return NULL;
        }
        jobject launcher = (*env)->CallStaticObjectMethod(env, lclass,
                                                          getlauncher);
        if (launcher == NULL || (*env)->ExceptionCheck(env) == JNI_TRUE) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Call to ClassLoaders.appClassLoader() failed.");
            return NULL;
        }
        /*
        jmethodID getloader = (*env)->GetMethodID(env, lclass,
                                                  "getClassLoader",
                                                  "()Ljava/lang/ClassLoader;");
        if (getloader == NULL) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Cannot get ClassLoaders.appClassLoader.getClassLoader method id.");
            return NULL;
        }
        jobject loader = (*env)->CallObjectMethod(env, launcher, getloader);
        if (loader == NULL || (*env)->ExceptionCheck(env) == JNI_TRUE) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Call to ClassLoader.appClassLoader.getClassLoader failed.");
            return NULL;
        }
        _context_class_loader = (*env)->NewGlobalRef(env, loader);
        */
        _context_class_loader = (*env)->NewGlobalRef(env, launcher);
        if (_context_class_loader == NULL) {
            jl_log(3, "JL", "get_context_classloader: %s\n",
                   "Cannot get global reference to the context classloader.");
            return NULL;
        }
    }
    return _context_class_loader;
}

// Call Thread.setContextClassLoader() and return 0 on success, -1 on failure.
// Caller must check and handle exceptions if -1 is returned.
static int set_current_thread_context_classloader(JNIEnv *env)
{
    jobject thr = (*env)->CallStaticObjectMethod(env, get_java_lang_thread(env),
                                             get_current_thread_method_id(env));
    if (thr == NULL || (*env)->ExceptionCheck(env) == JNI_TRUE) {
        jl_log(3, "JL", "set_current_thread_context_classloader: "
               "Call to Thread.currentThread() failed.\n");
        return -1;
    }

    (*env)->CallVoidMethod(env, thr, get_set_context_classloader_method_id(env),
                           get_context_classloader(env));
    if ((*env)->ExceptionCheck(env) == JNI_TRUE) {
        jl_log(3, "JL", "set_current_thread_context_classloader: "
               "Call to Thread.setContextClassLoader() failed.\n");
        return -1;
    }
    return 0;
}

// Initialize the context classloader, java class
// references and method ids that are used by jl_callJava and return
// 0 on success, -1 on failure.
// The caller must check and handle exceptions if -1 is returned.
static int init_context_classloader_javaclasses_and_methodids(JNIEnv *env)
{
    if (get_java_lang_string(env) == NULL ||
            get_java_lang_thread(env) == NULL ||
            get_current_thread_method_id(env) == NULL ||
            get_set_context_classloader_method_id(env) == NULL ||
            get_context_classloader(env) == NULL) {
        return -1;
    } else {
        return 0;
    }
}

static void cleanup_java_references()
{
    // No need to delete the global reference, since the
    // vm has been destroyed.
    _context_class_loader = NULL;
    _java_lang_string = NULL;
    _java_lang_thread = NULL;

    _java_thread_current_thread = NULL;
    _java_thread_set_context_classloader = NULL;
}

// Return the java/lang/String class object or NULL.
// Caller must check and handle exceptions if NULL is returned.
static jobject get_java_lang_string(JNIEnv *env)
{
    if (_java_lang_string == NULL) {
        jclass aclass = (*env)->FindClass(env, "java/lang/String");
        if (aclass == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                   "Cannot find class 'java/lang/String'\n");
            return NULL;
        }
        _java_lang_string = (*env)->NewGlobalRef(env, aclass);
        if (_java_lang_string == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                "Cannot get global reference for class 'java/lang/String'.\n");
            return NULL;
        }
    }
    return _java_lang_string;
}

// Return the java/lang/Thread class object or NULL.
// Caller must check and handle exceptions if NULL is returned.
static jobject get_java_lang_thread(JNIEnv *env)
{
    if (_java_lang_thread == NULL) {
        jclass aclass = (*env)->FindClass(env, "java/lang/Thread");
        if (aclass == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                   "Cannot find class 'java/lang/Thread'\n");
            return NULL;
        }
        _java_lang_thread = (*env)->NewGlobalRef(env, aclass);
        if (_java_lang_thread == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                "Cannot get global reference for class 'java/lang/Thread'.\n");
            return NULL;
        }
    }
    return _java_lang_thread;
}

// Return Thread.currentThread() method id or NULL.
// Caller must check and handle exceptions if NULL is returned.
static jmethodID get_current_thread_method_id(JNIEnv *env)
{
    if (_java_thread_current_thread == NULL) {
        _java_thread_current_thread =
            (*env)->GetStaticMethodID(env, get_java_lang_thread(env),
                                      "currentThread",
                                      "()Ljava/lang/Thread;");
        if (_java_thread_current_thread == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                   "Cannot get Thread.currentThread() method id.");
            return NULL;
        }
    }
    return _java_thread_current_thread;
}

// Return the Thread.setContextClassLoader() method id or NULL
// Caller must check and handle exceptions if NULL is returned.
static jmethodID get_set_context_classloader_method_id(JNIEnv *env)
{
    if (_java_thread_set_context_classloader == NULL) {
        _java_thread_set_context_classloader =
            (*env)->GetMethodID(env, get_java_lang_thread(env),
                                "setContextClassLoader",
                                "(Ljava/lang/ClassLoader;)V");
        if (_java_thread_set_context_classloader == NULL) {
            jl_log(3, "JL", "init_javaclasses_and_methodids:"
                   "Cannot get Thread.setContextClassLoader() method id.");
            return NULL;
        }
    }
    return _java_thread_set_context_classloader;
}


