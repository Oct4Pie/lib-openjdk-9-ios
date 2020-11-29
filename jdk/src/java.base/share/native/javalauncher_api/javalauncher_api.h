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

#ifndef JavaLauncher_JavaLauncher_h
#define JavaLauncher_JavaLauncher_h

/*!
 * \headerfile javalauncher_api.h
 * \typedef void (*JavaLauncherCallback)(const char *exception_msg, int error_code, void *app_data);
 * \brief The prototype for the application implemented callback function.
 * \param exception_msg The result of calling the exception's toString()
 * method if
 * a JNI exception occurred or to an error message related to error_code.
 * \param error_code an error code identifying the failure.
 * \param app_data application defined data.
 * \details
 * The callback is invoked if a JNI exception or error occurs or an exception
 * bubbles up from the java program. It is called with the exception message
 * or error message, along with the application's "app_data" pointer.
 * If a JNI exception occurs JNIEnv ExceptionDescribe and ExceptionClear
 * are called before the invoking the callback.
 */
typedef void (*JavaLauncherCallback)(const char *exception_msg, int error_code,
        void *app_data);

/*!
 * \headerfile javalauncher_api.h <JavaLauncher/javalauncher_api.h>
 * JavaLauncher API return codes.
 *
 * The javalauncher_api methods return these error codes in response to
 * errors and exceptions. The value is returned from method calls and
 * passed to JavaLauncherCallback when invoked.
 */
/*!
 * Return code for successful method calls.
 */
#define JL_OK 0
/*!
 * Return code for general failed method calls.
 */
#define JL_FAIL -1
/*!
 * Returned if the jl_createJavaVM method has not been called.
 */
#define JL_CREATEJAVAVMNOTCALLED -1000
/*!
 * Returned if JNI_CreateJavaVM fails.
 */
#define JL_CANNOTCREATEJVM -1001
/*!
 * Returned if a JavaVMOption} struct cannot be allocated.
 */
#define JL_CANNOTCREATEJVMOPTIONS -1002
/*!
 * Returned if the FindClass JNI method fails.
 */
#define JL_CANNOTFINDCLASS -1003
/*!
 * Returned if a method argument is not set.
 */
#define JL_METHODNOTSET -1004
/*!
 * Returned if the GetStaticMethodID JNI method fails.
 */
#define JL_METHODDOESNOTEXIST -1005
/*!
 * Returned if the GetStringUTFChars JNI method fails.
 */
#define JL_CANNOTGETUTFCHARS -1006
/*!
 * Returned if the SetObjectArrayElement JNI method fails.
 */
#define JL_CANNOTSETARRAYELEMENT -1007
/*!
 * Returned if the VM AttachCurrentThread method fails.
 */
#define JL_CANNOTATTACHCURRTHREAD -1008
/*!
 * Returned if a JNIEnv pointer cannot be obtained.
 */
#define JL_CANNOTGETJNIENV -1009
/*!
 * Returned if the VM DetachCurrentThread method fails.
 */
#define JL_CANNOTDETACHCURRTHREAD -1010
/*!
 * Returned if a JavaVMInitArgs struct cannot be allocated.
 */
#define JL_CANNOTCREATEVMINITARGS -1011
/*!
 * Returned if the ReleaseObjectArray JNI method fails.
 */
#define JL_CANNOTRELEASEOBJECTARRAY -1012
/*!
 * Returned if the PushLocalFrame JNI method fails.
 */
#define JL_CANNOTPUSHLOCALFRAME -1013
/*!
 * Returned if the CreateObjectArray JNI method fails.
 */
#define JL_CANNOTCREATEOBJARRAY -1014
/*!
 * Returned if the NewStringUTF JNI method fails.
 */
#define JL_CANNOTCREATESTRUTF -1015
/*!
 * Returned if the VM DestroyJavaVM method fails.
 */
#define JL_JVMISDESTROYED -1016
/*!
 * Returned if an argument to an api is not correct
 */
#define JL_BADARGUMENTS -1017
/*!
 * Returned if the method signature is not supported
 */
#define JL_BADSIGNATURE -1018
/*!
 * Returned if the vm dynamic library cannot be opened
 */
#define JL_CANTLOADLIBJVM -1019
/*!
 * Returned if the callback could not invoked
 */
#define JL_INVOKECALLBACKFAILED -1020
/*!
 * Returned if jl_initialize has not been called
 */
#define JL_NOTINITIALIZED -1021
/*!
 * Returned if the context class loader cannot be set on the current thread
 */
#define JL_CANTSETCONTEXTCLASSLOADER -1022


/*! \private Must be called when libjvm is a dynamic library
 * \brief Loads the Java Virtual Machine shared library For non-statically
 * linked applications
 * \details This function must be called when the application is dynamically
 * linked. It loads the Java Virtual Machine shared library.
 * \param javalibhome The absolute path to the directory containing the
 * libjvm shared library.
 * \result Returns JL_OK on success or one of the javalauncher_api error codes
 * on failure.
 */
int jl_initialize(const char* javalibhome);

/*!
 * \brief Create a Java Virtual Machine.
 * \details
 * jl_createJavaVM creates a Java Virtual Machine by calling
 * JNI_CreateJavaVM with
 * configuration properties and options. It may be called in the main thread and
 * must be called and return before calling jl_callJava.
 * \param vmargs An array of Java properties and options. Each element is
 * passed as a JavaVMOption. The elements are of the form
 * "-Djava.class.path=.:/classes" or "-Xms32m". Options on the form
 * "-cp .:/classes" are not supported.
 * \param vmargs_len The number of elements in vmargs.
 * \param callback An implementation of JavaLauncherCallback or NULL. If
 * "callback" is not NULL, it will be called if an error
 * or exception occurs while attempting to create the VM,
 * with an error msg, the failure error code and app_data.
 * If a JNI exception occurs JNIEnv->ExceptionDescribe and
 * JNIEnv->ExceptionClear are called before invoking the callback.
 * \param app_data Application defined data.
 * \result Return JL_OK if the call to JNI_CreateJavaVM was
 * successful or else one of the error codes defined in javalauncher_api.h
 */
extern int jl_createJavaVM(char **vmargs, int vmargs_len, JavaLauncherCallback callback, void *app_data);

/*!
 * \brief Invoke static void methods on a named class.
 * \details jl_callJava must be called in its own thread, since it does not
 * return until the java method ends.
 * \param javaclass The Java class path.
 * \param method The static void method to invoke on "javaclass"
 * \param signature The method's signature. Must be "([Ljava/lang/String;)V" or
 * "()V"
 * \param app_args The method arguments. Each element is one of the white space
 * separated arguments as they would appear on the java command line. If an
 * argument is of the form "count=3" then this option would occupy one
 * element. Arguments of the form "-verboselevel 5" occupy two adjacent
 * elements.
 * \param app_args_len The number of elements in app_args.
 * \param callback An implementation of JavaLauncherCallback or NULL. If
 * "callback" is not NULL, it will be called if an error
 * or exception occurs while attempting to create the VM,
 * with an error msg, the failure error code and app_data.
 * If a JNI exception occurs JNIEnv->ExceptionDescribe and
 * JNIEnv->ExceptionClear are called before invoking the callback.
 * \param app_data Application defined data.
 * \result Return JL_OK if the call to JNI_CreateJavaVM was
 * successful or else one of the error codes defined in javalauncher_api.h
 */

extern int jl_callJava(char *javaclass, char *method, char *signature, char **app_args, int app_args_len, JavaLauncherCallback callback, void *app_data);

/*!
 * \brief Destroy the Java Virtual Machine created with jl_createJavaVM.
 * \details jl_destroyJavaVM destroys the VM created by jl_createJavaVM
 * by calling JavaVM JNI function DestroyJavaVM.
 * Once this function is called no other
 * javalauncher_api functions can be called. Once the Java Virtual Machine
 * is destroyed it cannot be restarted.
 * \param callback An implementation of JavaLauncherCallback or NULL. If
 * "callback" is not NULL, it will be called if an error
 * or exception occurs while attempting to create the VM,
 * with an error msg, the failure error code and app_data.
 * If a JNI exception occurs JNIEnv->ExceptionDescribe and
 * JNIEnv->ExceptionClear are called before invoking the callback.
 * \param app_data Application defined data.
 * \result Return JL_OK if the call to JNI_CreateJavaVM was
 * successful or else one of the error codes defined in javalauncher_api.h
 */
extern int jl_destroyJavaVM(JavaLauncherCallback callback, void *app_data);

#endif
