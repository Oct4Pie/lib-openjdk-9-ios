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

/**
 * Implement this callback to get notification from the
 * {@code JavaLauncher} methods when Java exceptions or other
 * runtime errors occur.
 */
public interface JavaLauncherCallback {
    /**
     * This method is called when an error or a Java exception occurs
     * during one of the {@code JavaLauncher} methods.
     * <p>
     * If an exception occurs, the {@code ExceptionDescribe} and
     * {@code ExceptionClear} JNI methods are called before
     * invoking {@code callback}. {@code errormsg} is the
     * result of calling {@code toString()} on the Java exception's
     * {@code Throwable}
     * instance. {@code errorcode} is set to one of the
     * {@code JavaLauncher.JL_*} error constants.
     * </p>
     * <p>
     * If an error occurs that is not a Java exception, {@code errormsg}
     * is a general error related to the {@code errorcode}.
     * </p>
     * @param errormsg message describing the error.
     * @param errorcode the error code identifying the error.
     */
    public void callback(String errormsg, int errorcode);
}
