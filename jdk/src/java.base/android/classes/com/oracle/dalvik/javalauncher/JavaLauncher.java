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

package com.oracle.dalvik.javalauncher;

import java.util.Properties;

/**
 * <p>
 * The JavaLauncher class implements methods to create a Java VM,
 * invoke static void methods, and
 * destroy the instance of the VM. Once the VM is destroyed no
 * other calls into the VM are possible. The VM cannot be restarted.
 * </p>
 * <b>Calling the {@code createJava}, {@code callJava}, and
 * and {@code destroyJavaVM} methods.</b>
 * <p>
 * The arguments for configuring the Java VM, the class and method to
 * run and its arguments can be obtained from a {@code java.util.Properties}
 * file. The {@link com.oracle.dalvik.javalauncher.JavaArgs} class may
 * read properties from a {@code java.util.ResourceBundle} called
 * {@code JavaLauncherArgs} created by the application
 * developer. The arguments can also be derived by the running application
 * and passed to the methods.
 * </p>
 * <p>The {@code createJavaVM} method requires three arguments
 * <ul>
 * <li> {@code String javaHome} - the absolute path to the root directory
 * of the jre. This path is used to load the Java VM shared library. The
 * standard jre structure is expected under {@code javaHome}
 * </li>
 * <li>{@code String[] jvmArgs} - the options and properties used to
 * configure the VM. These values become the {@code JavaVMOptions} used
 * in the call to {@code JNI_CreateJavaVM}.  The array values are VM
 * options and properties. A VM property is specified with the equal
 * sign and value (with no intervening white space) as in
 * {@code -Djava.class.path=.:%@DD/lib/app.jar} where {@code %@DD}
 * is replaced with {@code ApplicationInfo.dataDir}. An option that requires
 * a value is specified similarly as in
 * {@code -agentlib:jdwp=server=y,transport=dt_socket,address=10.0.1.184:35000,suspend=n}.
 * Options that do not require a value are specified as usual as in
 * {@code -Xms32m}.
 * </li>
 * <li>{@code callback} - an instance of
 * {@link com.oracle.dalvik.javalauncher.JavaLauncherCallback} or null.
 * {@code callback} is invoked if an exception or error
 * occurs during the creation and startup of the Java VM, or if an
 * exception bubbles up from the Java program.
 * </li>
 * </ul>
 * <p><blockquote><pre>
 * // The createJavaVM  method does not have to be called in a separate
 * // thread, for example called during application initialization.
 * int result = JavaLauncher.createJavaVM(javaHome, jvmArgs, callback);
 * if (result != JavaLauncher.JL_OK) {
 *    ...error handling...
 * }
 * </pre></blockquote>
 * <p>
 * If {@code createJavaVM} is successful, {@code JavaLauncher.JL_OK}
 * is returned or one of the {@code JavaLauncher} error code constants
 * if it fails.
 * </p>
 * <p>
 * Once the Java VM has been created, java code can be executed as necessary
 * by calling {@code JavaLauncher.callJava()}. This method should be
 * called in a separate thread since it does not return until the
 * java program ends. The arguments are
 * <ul>
 * <li> {@code mainClass} - {@code mainMethod} is invoked on this
 * class.
 * </li>
 * <li> {@code mainMethod} - a static void method invoked on
 * {@code mainClass}
 * </li>
 * <li> {@code signature} - the signature of {@code mainMethod}.
 * Only {@code ([Ljava/lang/String;)V} and {@code ()V} are
 * supported.
 * </li>
 * <li> {@code appArgs} - the arguments passed to {@code mainMethod}
 * </li>
 * <li> {@code callback} - application callback
 * {@link com.oracle.dalvik.javalauncher.JavaLauncherCallback}
 * </li>
 * </ul>
 * <p> {@code JavaLauncher.callJava()} returns {@code JL_OK}
 * on success or a {@code JavaLauncher} error code constant on
 * failure.
 * </p>
 * <p><blockquote><pre>
 * ... in some event A ... *
 * // JavaLauncher.callJava must be called in a new thread.
 * new Thread(new Runnable() {
 *    public void run() {
 *        int result = JavaLauncher.callJava(mainClass, mainMethod,
 *              signature, appArgs, null);
 *        if (result != JavaLauncher.JL_OK) {
 *            ...error handling...
 *        }
 *    }}).start();
 * ...
 *
 * ... in some event B ... *
 * new Thread(new Runnable() {
 *    public void run() {
 *        int result = JavaLauncher.callJava(mainClass, mainMethod,
 *              signature, appArgs, null);
 *        if (result != JavaLauncher.JL_OK) {
 *            ...error handling...
 *        }
 *    }}).start();
 * ...
 * </pre></blockquote>
 * <p>
 * {@code destroyJavaVM} should be called in the application's termination
 * code. It will return if there is an error or if called previously.
 * </p>
 * <p><blockquote><pre>
 * ... terminate....
 *     int result = JavaLauncher.destroyJavaVM(callback);
 *     if (result != JavaLauncher.JL_OK) {
 *            // report event destroy failed.
 *     }
 * ...
 * </pre></blockquote>
 * <p>
 * Methods that accept a {@code JavaLauncherCallback} argument will invoke
 * the callback on error or if a Java exception bubbles up from the Java
 * program. If a Java exception occurs the {@code String}
 * passed as the first argument to the callback will be the
 * result of the {@code Throwable.toString()} method. If an error occurs
 * that is not an exception, it is a general message related to the cause of the
 * error. The error code will be one of the {@code JavaLauncher.JL_*} error
 * constants.
 * </p>
 * <p>
 * When a Java exception occurs, the {@code ExceptionDescribe} and
 * {@code ExceptionClear} JNI methods are called before the callback
 * is invoked.
 * </p>
 */
public class JavaLauncher {

    // MUST BE KEPT IN SYNC WITH
    // jdk/src/closed/share/native/javalauncher/javalauncher_api.c

    /** Return code for successful method calls */
    public static int JL_OK = 0;
    /** Return code for general failed method calls */
    public static int  JL_FAIL = -1;
    /** Returned if the {@code createJavaVM} method has not been called. */
    public static int JL_CREATEJAVAVMNOTCALLED = -1000;
    /** Returned if {@code JNI_CreateJavaVM} fails. */
    public static int JL_CANNOTCREATEJVM = -1001;
    /** Returned if a {@code JavaVMOption} struct cannot be allocated. */
    public static int JL_CANNOTCREATEJVMOPTIONS = -1002;
    /** Returned if the {@code FindClass} JNI method fails. */
    public static int JL_CANNOTFINDCLASS = -1003;
    /** Returned if a method argument is not set. */
    public static int JL_METHODNOTSET = -1004;
    /** Returned if the {@code GetStaticMethodID} JNI method fails. */
    public static int JL_METHODDOESNOTEXIST = -1005;
    /** Returned if the {@code GetStringUTFChars} JNI method fails. */
    public static int JL_CANNOTGETUTFCHARS = -1006;
    /** Returned if the {@code SetObjectArrayElement}JNI method fails. */
    public static int JL_CANNOTSETARRAYELEMENT = -1007;
    /** Returned if the VM {@code AttachCurrentThread} method fails. */
    public static int JL_CANNOTATTACHCURRTHREAD = -1008;
    /** Returned if a {@code JNIEnv} pointer cannot be obtained. */
    public static int JL_CANNOTGETJNIENV = -1009;
    /** Returned if the VM {@code DetachCurrentThread} method fails. */
    public static int JL_CANNOTDETACHCURRTHREAD = -1010;
    /** Returned if a {@code JavaVMInitArgs} struct cannot be allocated. */
    public static int JL_CANNOTCREATEVMINITARGS = -1011;
    /** Returned if the {@code ReleaseObjectArray} JNI method fails. */
    public static int JL_CANNOTRELEASEOBJECTARRAY = -1012;
    /** Returned if the {@code PushLocalFrame} JNI method fails. */
    public static int JL_CANNOTPUSHLOCALFRAME = -1013;
    /** Returned if the {@code CreateObjectArray} JNI method fails. */
    public static int JL_CANNOTCREATEOBJARRAY = -1014;
    /** Returned if the {@code NewStringUTF} JNI method fails. */
    public static int JL_CANNOTCREATESTRUTF = -1015;
    /** Returned if the VM {@code DestroyJavaVM} mathod fails. */
    public static int JL_JVMISDESTROYED = -1016;
    /** Returned if an argument to an api is not correct */
    public static int  JL_BADARGUMENTS = -1017;
    /** Returned if the method signature is not supported */
    public static int  JL_BADSIGNATURE = -1018;
    /** Returned if the vm dymanic library cannot be opened */
    public static int  JL_CANTLOADLIBJVM = -1019;
    /** Returned if the callback could not invoked */
    public static int JL_INVOKECALLBACKFAILED = -1020;
    /** Returned if jl_initialize has not been called */
    public static int JL_NOTINITIALIZED = -1021;
    /** Returned if the context class loader cannot be set on the current
     * thread */
    public static int JL_CANTSETCONTEXTCLASSLOADER = -1022;

    /** Returned if the {@code createJavaVM} method has already been
     * called. */
    public static int JL_CREATEJAVAVMHASBEENCALLED = -2000;
    /** Returned if a JavaLauncher instance cannot be obtained. */
    public static int JL_NOINSTANCE = -2001;

    /** Log level indicating that the JavaLauncher interfaces cannot
     * continue. */
    private static int FATAL = 1;
    /** Log level indicating that the JavaLauncher interfaces behaviour may
     * be unable to function as expected. */
    private static int WARNING = 2;
    /** Log level indicating an informational message. */
    private static int INFO = 3;
    /** Log level indicating a debug message. */
    private static int DEBUG = 4;

    /**
     * Set to true when destroyJavaVM has been called.
     */
    private static Boolean isVmDestroyed = Boolean.FALSE;

    /**
     * Set to true when the createJavaVM has been called successfully.
     */
    private static boolean isVmCreated;

    /**
     * A default JavaLauncherCallback.
     */
    private static JavaLauncherCallback jlc;

    /**
     * Set to {@code true} when the {@code createJavaVM}
     * is called and returns successfully */
    private static boolean initialized;

    /**
     * Be verbose with messages
     */
    private static boolean VERBOSE = true;

    // prevent the default constructor from showing up in javadoc.
    private JavaLauncher() {
    }

    /**
     * Load the VM shared library and {@code JavaLauncher} native libraries.
     * The location of the VM's shared library is derived from
     * {@code javaHome}.
     * If initialization fails {@code IllegaArgumentException} is thrown.
     * @param javaHome the absolute path to the jre directory.
     */
    private static void initialize(String javaHome) {
        System.loadLibrary("ocldvk");
        int result = _initialize(javaHome);
        if (result != JL_OK) {
            log(FATAL, "JavaLauncher(): _initialize failed with errorcode " +
                result);
            throw new IllegalArgumentException(
                "Cannot initialize Java runtime.");
        }
        try {
            // Create a default callback for logging.
            jlc = new JavaLauncherCallback() {
                public void callback(String msg, int errorcode) {
                    if (com.oracle.dalvik.javalauncher.JavaLauncher.VERBOSE) {
                        JavaLauncher.log(DEBUG,
                            "JavaLauncherCallback: " +
                            msg + " (errorcode " + errorcode + ")");
                    }
                }
            };
        } catch (Exception e) {
            log(FATAL, "JavaLauncher(): received exception.", e);
            throw new IllegalArgumentException(
                "Cannot initialize Java runtime.");
        }
        initialized = true;
    }
    // Synchronize on isVmDestroyed
    // Assume JavaLauncher has not been initialized, i.e.
    // createJavaVM has not been called.
    // If either callJava or destroyJavaVM are called
    // in parallel, one will block on isVmDestroyed, if the
    // VM has been destroyed then call returns immediately
    // If not, then since createJavaVM has not been called,
    // isVmCreated will be false and the call will return.
    // If destroyJavaVM is one of the calls it will return
    // immediately since isVmCreated is false.
    // Similarly if createJavaVM is being called in parallel
    // with itself or one of the other calls, will block
    // or get the isVmDestroyed lock. Since the VM has not
    // been destroyed and not created, the isVmDestroyed lock
    // is not given up until the VM is initialized and
    // running.
    //

    /**
     * Create a Java VM based on the java arguments defined in
     * {@code jvmArgs}.
     *
     * @param javaHome the absolute path to the jre directory.
     * @param jvmArgs the properties and options used to create the VM.
     * @param callback applciation callback.
     * @return an errorcode on failure and {@code JL_OK} on success.
     */
    public static int createJavaVM(String javaHome,
            String[] jvmArgs, JavaLauncherCallback callback) {

        int result = JL_OK;
        synchronized(isVmDestroyed) {
            if (isVmDestroyed) {
                if (VERBOSE) {
                    log(WARNING,
                        "createJavaVM: destroyJavaVM has been called, " +
                        "cannot re-create the Java VM.");
                }
                return JL_JVMISDESTROYED;
            }
            if (isVmCreated) {
                if (VERBOSE) {
                    log(INFO,
                        "createJavaVM: The Java VM has already been created.");
                }
                return JL_OK;
            }
            if (!initialized) {
                initialize(javaHome);
            }

            result = _createJavaVM(jvmArgs,
                callback == null ? jlc : callback);
            if (result == JL_OK) {
                isVmCreated = true;
                // Create exit thread to properly handle System.exit()
                Thread exitthread = new Thread(new Runnable() {
                    public void run() {
                      int result = exitJava();
                      System.exit(result);
                    }
                  }, "Java-Exit");
                exitthread.start();
            }
        }
        return result;
    }

    static int exitJava() {
        return _exitJava();
    }

    /**
     * Invoke the Java method {@code mainMethod} with {@code signature} and
     * implemented by {@code mainClass}. The {@code appArgs} arguments
     * are passed to the method.
     * <p>
     * Only {@code ([Ljava/lang/String;)V} and {@code ()V}
     * signatures are supported.
     * </p>
     * <p>
     * {@code callback} is invoked on error or if a Java exception occurs.
     * @param mainClass the Java class implementing {@code mainMethod}.
     * @param mainMethod the method to invoke on {@code mainClass}.
     * @param signature the signature of {@code mainMethod}
     * @param appArgs the arguments to pass to {@code mainMethod}
     * @param callback application callback
     * @return an errorcode on failure and {@code JL_OK} on success.
     */
    public static int callJava(String mainClass, String mainMethod,
            String signature, String[] appArgs, JavaLauncherCallback callback) {

        synchronized(isVmDestroyed) {
            if (isVmDestroyed) {
                if (VERBOSE) {
                    log(WARNING, "callJava: destroyJavaVM has been called, " +
                        "cannot execute java code.");
                }
                return JL_JVMISDESTROYED;
            }

            if (!isVmCreated) {
                if (VERBOSE) {
                    log(WARNING, "callJava: createJavaVM has not been called.");
                }
                return JL_CREATEJAVAVMNOTCALLED;
            }
        }
        // Before calling _callJava replace "." with "/" in mainClass
        mainClass = mainClass.replace('.', '/');
        return _callJava(mainClass, mainMethod, signature, appArgs,
                callback == null ? jlc : callback);
    }

    /**
     * Destroy the Java VM instance. Neither
     * {@code createJavaVM} or {@code callJava} can be called
     * successfully after this method completes. If {@code destroyJavaVM}
     * is called again it returns {@code JL_VMISDESTROYED}
     * <p>
     * {@code callback} is invoked on error or if a Java exception occurs.
     * </p>
     * @param callback application callback
     * @return errorcode on failure and {@code JL_OK} on success.
     */
    public static int destroyJavaVM(JavaLauncherCallback callback) {
        synchronized(isVmDestroyed) {
            if (isVmDestroyed) {
                if (VERBOSE) {
                    log(INFO, "destroyJavaVM: destroyJavaVM has " +
                        "already been called.");
                }
                return JL_JVMISDESTROYED;
            }

            if (!isVmCreated) {
                if (VERBOSE) {
                    log(WARNING, "callJava: createJavaVM has not been called.");
                }
                return JL_CREATEJAVAVMNOTCALLED;
            }
            isVmDestroyed = Boolean.TRUE;
        }
        return _destroyJavaVM(callback == null ? jlc : callback);
    }

    // TODO: make these java.util.logging.Level ?
    /**
     * Log a message. Possible {@code level} values are
     * <ul>
     * <li>{@code FATAL}</li>
     * <li>{@code WARNING}</li>
     * <li>{@code INFO}</li>
     * <li>{@code DEBUG}</li>
     * </ul>
     * @param level log level
     * @param msg log message
     */
    private static void log(int level, String msg) {
        log(level, msg, null);
    }

    // TODO: make these java.util.logging.Level ?
    // java.util.logging.Logger
    /**
     * Log a message. Possible {@code level} values are
     * <ul>
     * <li>{@code FATAL}</li>
     * <li>{@code WARNING}</li>
     * <li>{@code INFO}</li>
     * <li>{@code DEBUG}</li>
     * </ul>
     * <p>
     * If {@code e} is not null and {@code VERBOSE} is
     * true, call {@code e.printStackTrace}.
     * @param level log level
     * @param msg log message
     * @param ex the exception that occurred.
     */
    private static void log(int level, String msg, Exception ex) {
        StringBuilder sb = new StringBuilder();
        sb.append("JavaLauncher:: ").append(msg);
        if (ex != null) {
            sb.append("\n").append(ex.getMessage());
        }
        if (VERBOSE && ex != null) {
            ex.printStackTrace();
        }
        // TODO: What is the mechanism we are using for loging
        // on Android java.util.Logging ?
        System.err.println(sb.toString());
    }

    /**
     * The native method that creates the Java VM.
     * @param jvmArgs the properties and options used to create the VM.
     * @param callback application callback.
     * @return errorcode on failure and {@code JL_OK} on success.
     */
    native private static int _createJavaVM(String[] jvmArgs,
        JavaLauncherCallback callback);
    /**
     * The native method that invokes the Java method.
     * @param mainClass the Java class to invoke on {@code mainMethod}.
     * @param mainMethod the method to invoke on {@code mainClass}.
     * @param signature the signature of {@code mainMethod}
     * @param appArgs the arguments to pass to {@code mainMethod}
     * @param callback application callback.
     * @return errorcode on failure and {@code JL_OK} on success.
     */
    native private static int _callJava(String mainClass, String mainMethod,
        String signature, String[] appArgs, JavaLauncherCallback callback);
    /**
     * The native method that destroys the Java VM.
     * @param callback application callback.
     * @return errorcode on failure and {@code JL_OK} on success.
     */
    native private static int _destroyJavaVM(JavaLauncherCallback callback);

    /**
     * Call the javalauncher_api to load libjvm.
     * @param javaHome the absolute path to the root of the jre.
     * @return errorcode on failure and {@code JL_OK} on success.
     */
    native private static int _initialize(String javaHome);
    native private static int _exitJava();
}
