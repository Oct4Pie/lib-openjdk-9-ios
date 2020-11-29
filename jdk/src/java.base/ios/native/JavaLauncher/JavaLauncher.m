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

#import "JavaLauncher.h"

@implementation JavaLauncher

// These are convenience methods.
// A real application will want to call the javalauncher_api.c
// functions directly.

// It's not clear if this needs to be in a new thread or not.
+ (int) createJavaVM:(id)java_args
{
    int result;
    CreateJavaArgs *create_java_args = [java_args getCreateJavaArgs];
    if (create_java_args == NULL) {
        NSLog(@"JavaLauncher::createJavaVM: "
                @"Failed to get CreateJavaArgs from JavaArgs.");
        return JL_BADARGUMENTS;
    }
    // What about the final return value ?
    // Should callback be called with success ?
    result = jl_createJavaVM(create_java_args->jvm_args,
                    create_java_args->jvm_args_len,
                    ((JavaArgs*)java_args).callback,
                    ((JavaArgs*)java_args).app_data);

    jl_freeCreateJavaArgs(create_java_args);
    return result;
}

// Should not be called in the main thread.
+ (int) callJava:(id)java_args
{
    int result;
    CallJavaArgs *call_java_args = [java_args getCallJavaArgs];
    if (call_java_args == NULL) {
        NSLog(@"JavaLauncher::callJava: "
                @"Failed to get CallJavaArgs from JavaArgs.");
        return JL_BADARGUMENTS;
    }
    result = jl_callJava(call_java_args->mainclass,
                call_java_args->mainmethod,
                call_java_args->signature,
                call_java_args->app_args,
                call_java_args->app_args_len,
                ((JavaArgs*)java_args).callback, 
                ((JavaArgs*)java_args).app_data);
    
    jl_freeCallJavaArgs(call_java_args);
    return result;
}

// Should not be called in the main thread.
+ (int) destroyJavaVM:(id)java_args
{
    // Does not return
    return jl_destroyJavaVM(((JavaArgs*)java_args).callback,
                     ((JavaArgs*)java_args).app_data);
    
}

@end
