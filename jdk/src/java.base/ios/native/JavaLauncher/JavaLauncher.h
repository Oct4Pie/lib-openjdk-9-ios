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

#import <Foundation/Foundation.h>
#import "JavaArgs.h"
#include "javalauncher_api.h"

/*!
 * \mainpage
 * <p>
 * The JavaLauncher.framework defines two classes JavaArgs and
 * JavaLauncher and a C function api.
 * </p>
 * <p>
 * The JavaArgs class encapsulates configuration and runtime properties
 * used to configure the Java virtual machine and optionally define
 * the method and class to run in the VM.
 * </p>
 * <p>
 * The JavaLauncher class implements methods to create a Java VM,
 * invoke static void methods, and destroy the instance of the VM.
 * </p>
 * <p>
 * The C function API defines 2 methods jl_createJavaVM(), jl_callJava(),
 * and jl_destroyJavaVM() that allow an application to
 * create a Java Virtual Machine, execute static void methods, and
 * destroy the VM instance.
 * </p>
 * <p>
 * The framework provides the three header files JavaArgs.h,
 * JavaLauncher.h, and javalauncher_api.h.
 * </p>
 * <dl class="data"><dt><b>Copyright:</b></dt><dd>
 *  Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * </dd></dl>
 * \date 2013-09-05
 */

/*!
 * \class JavaLauncher
 * \headerfile JavaLauncher.h
 * \brief
 * The JavaLauncher class implements methods to create a Java VM,
 * invoke static void methods, and destroy the instance of the VM.
 * \details
 * The JavaLauncher class implements methods to create a Java VM,
 * invoke static void methods, and destroy the instance of the VM.
 * Once the VM is destroyed no
 * other calls into the VM are possible. The VM cannot be restarted.
 * <p>
 * <b>Calling the createJava, callJava, and
 * and destroyJavaVM methods.</b>
 * </p>
 * <p>
 * These methods are helpers designed to be called using
 * performSelectorInBackground:selector:withObject passing a JavaArgs id
 * as the object. These methods are simple wrappers that call the low
 * level C api as described in javalauncher_api.h.
 * </p>
 * <p>
 * The JavaArgs instance may be initialized by default from a
 * java.util.Properties formatted file called JavaLauncherArgs.properties
 * or from an application defined file, or initialized during runtime.
 * The JavaLauncher.createJavaVM method calls JavaArgs.getCallJavaArgs
 * to obtain a CreateJavaArgs structure and calls jl_createJavaVM with the
 * jvmArgs and JavaArgs.callback members and then frees the
 * CreateJavaArgs structure. Unlike callJava
 * createJavaVM may be called from the main thread.
 * An example of calling createJavaVM
 * <p>
 * <pre><code>
 * // Provide a callback method to receive error notifications.
 * ...
 * static void javaLauncherCallback(String msg, int errorcode, void *appData) {
 *      ....do something when notified an error has occurred...
 * }
 * // The createJavaVM  method does not have to be called in a separate
 * // thread, for example it can be called during application initialization.
 * // java_args will be initialized from the default JavaLauncherArgs.properties
 * // file.
 * ... during initialization
 * JavaArgs java_args = [[JavaArgs alloc] init];
 * java_args.callback = javaLauncherCallback;
 * [JavaLauncher createJavaVM: java_args];
 * ...
 * </code></pre>
 * </p>
 * <p>
 * Once the Java VM has been created, java code can be executed as necessary
 * by calling JavaLauncher.callJava(). This method should be
 * called in a separate thread since it does not return until the
 * java program ends. It is designed to be called by
 * performSelectorInBackground:selector:withObject passing a JavaArgs id.
 * </p>
 * <p><pre><code>
 * ... in some event A ... *
 * // In a simple case the java_args is a global variable containing
 * // the method and arguments to execute.
 * [JavaLauncher performSelectorInBackground:\@callJava withObject:eventA_java_args];
 *
 * ... in some event B ... *
 * [JavaLauncher performSelectorInBackground:\@callJava withObject:eventB_java_args];
 * ...
 * </code></pre>
 * </p>
 * <p>
 * destroyJavaVM can be called in the application's termination code.
 * </p>
 * <p><pre><code>
 * ... terminate....
 * [JavaLauncher destroyJavaVM :destroyVM_java_args];
 * ...
 * </code></pre>
 * <p>
 * Methods that accept a JavaLauncherCallback argument will invoke
 * the callback on error or if a Java exception bubbles up from the Java
 * program. If a Java exception occurs the String
 * passed as the first callback argument will be the
 * result of the Throwable.toString() method. If an error occurs
 * that is not an exception, it is a general message related to the cause of the
 * error. The error code will be one of the javalauncher_api.h error macro
 * constants.
 * </p>
 * <p>
 * When a Java exception occurs, the ExceptionDescribe and
 * ExceptionClear JNI methods are called before the callback
 * is invoked.
 * </p>
 */

@interface JavaLauncher : NSObject

/*!
 * \details This method is a convenience method that calls
 * jl_createJavaVM. See javalauncher_api.h for more information.
 * \param java_args A JavaArgs instance defining parameters for
 * jl_createJavaVM.
 * \return JL_OK on success, or one of the javalauncher_api.h error macro constants.
 */
+ (int)createJavaVM :(id)java_args;

/*!
 * \details This method is a convenience method that calls
 * jl_callJava. See javalauncher_api.h for more information.
 * \param java_args A JavaArgs instance defining parameters for
 * jl_callJava.
 * \return JL_OK on success, or one of the javalauncher_api.h error macro constants.
 */
+ (int)callJava :(id)java_args;

/*!
 * \details This method is a convenience method that calls
 * jl_destroyJavaVM. See javalauncher_api.h for more information.
 * \param java_args A JavaArgs instance defining parameters for
 * jl_destroyJavaVM.
 * \return JL_OK on success, or one of the javalauncher_api.h error macro constants.
 */
+ (int)destroyJavaVM :(id)java_args;

@end
