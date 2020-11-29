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
#include "javalauncher_api.h"

/*!
 * \struct CreateJavaArgs
 * \brief Encapsulate arguments for the jl_createJavaVM() function.
 * \details  CreateJavaArgs is used to encapsulate the arguments needed
 * to call jl_createJavaVM().
 */
typedef struct {
    /*! The Java Virtual Machine configuration properties
     * and options. The options and properties are the same as defined by the
     * java command. Each element of the jvm_args array is one of the
     * white space separated properties or options that would be specified
     * on the java command line such as "-Djava.class.path=.:foo.jar" or
     * "-Xmx256m". Options such as "-cp .:foo.jar" that include white
     * space cannot be used as a value in jvmArgs. */
    char **jvm_args;
    /*! jvm_args_len The number of elements in the jvm_args array. */
    int jvm_args_len;
} CreateJavaArgs;
extern void jl_freeCreateJavaArgs(CreateJavaArgs *create_java_args);

/*!
 * \struct CallJavaArgs
 * \brief Encapsulate arguments for the jl_callJava() function.
 * \details CallJavaArgs is used to encapsulate the arguments needed to call
 * jl_callJava().
 */
typedef struct {
    /*! The Java class that implements mainmethod */
    char *mainclass;
    /*! The static void method to invoke on mainclass */
    char *mainmethod;
    /*! The "mainmethod" signature. Only the ([Ljava/lang/String;)V,
     * and ()V signatures are supported. */
    char *signature;
    /*! The mainmethod arguments. */
    char **app_args;
    /*! The number of elements in the app_args array. */
    int app_args_len;
} CallJavaArgs;
extern void jl_freeCallJavaArgs(CallJavaArgs *call_java_args);

// Internal constants

/*!
 * \private The name of the Properties file containing java arguments.
 */
#define JAVAARGS_PROPERTIES @"JavaLauncherArgs"
/*!
 * \private The name of the Properties file containing debug mode
 * java arguments
 */
#define JAVADBGARGS_PROPERTIES @"JavaLauncherArgs-debug"
/*!
 * \private The name of the dictionary that contains the java arguments
 * and values
 */
#define VMARGS_PROP @"jvmArgs"
/*!
 * \private The name of the property that defines the main class
 * containing the main method that the vm will invoke.
 */
#define JAVAMAINCLASS_PROP @"mainClass"
/*!
 * \private The name of the property that defines the method to call on
 * the mainClass
 */
#define JAVAMAINMETHOD_PROP @"mainMethod"
/*!
 * \private The name of the property that defines the signature of
 * the mainMethod. Only two signatures are currently supported
 * ([Ljava/lang/String;)V and ()V
 */
#define JAVASIGNATURE_PROP @"signature"
/*!
 * \private The name of the array containing the application
 * arguments passed to the main class's main method.
 */
#define APPARGS_PROP @"appArgs"
// Note that currently the ios implementation does not use this value.
/*!
 * \private The name of the property that defines the location
 * of java home, the root of the jre hierarchy
 */
#define JAVAHOME_PROP @"javaHome"
/*!
 * \private The name of the property that controls merging command line
 * arguments from argv.
 */
#define MERGEARGV_PROP @"mergeArgv"
/*!
 * \private The substitution parameter that will be replaced with
 * the bundle path when found in a VMARGS_PROP, VMDBGARGS_PROP,
 * or APPARGS_PROP property value. */
#define BUNDLEPATHFORMAT @"%@DD"
/*!
 * \private The default java method to invoke.
 */
#define DEFAULT_METHOD @"main"
/*!
 * \private The default signature for the default method
 */
#define DEFAULT_SIGNATURE @"([Ljava/lang/String;)V"

/*!
 * \class JavaArgs
 * \brief Encapsulate the Java Virtual Machine
 * configuration properties and options.
 * \details
 * The JavaArgs class encapsulates Java runtime
 * properties and options such as -Djava.class.path and
 * -Xms32m, an initial class, a java method to invoke and the method arguments.
 * <p>
 * Constructors without a properties argument obtain initial
 * Java runtime properties and options from a file
 * called JavaLauncherArgs.properties, if it exists in the application
 * directory. Constructors that
 * accept a properties argument initialize the instance using the
 * properties file defined by the properties file argument.
 * The format of the properties file is defined by the
 * java.util.Properties class.
 * </p>
 * <p>
 * Any property value may include the substitution parameter sequence
 * %\@DD. It is replaced by the bundle path value.
 * </p>
 * <p>
 * The recognized properties in the properties file are
 * <table border="1" summary="Recognized properties">
 * <tr>
 * <td>jvmArgs</td>
 * <td>A space separated list of options and properties
 * that become the JavaVMOptions used in the call to
 * JNI_CreateJavaVM. The options and properties
 * are the same as defined by the java command such as
 * jvmArgs=-Djava.class.path=.:foo.jar -Xmx256m. Options
 * such as "-cp" that include white space cannot be used
 * as a value in jvmArgs.
 * </td>
 * </tr>
 * <tr>
 * <td>mainClass</td>
 * <td>The Java class name that implements mainMethod.</td>
 * </tr>
 * <tr>
 * <td>mainMethod</td>
 * <td>The name of the static void method to invoke on mainClass.</td>
 * </tr>
 * <tr>
 * <td>signature</td>
 * <td>The method signature defining the return value
 * and arguments of mainMethod. Only two signatures are supported.
 * <ul>
 * <li>([Ljava/lang/String;)V</li>
 * <li>()V</li>
 * </ul>
 * </tr>
 * <tr>
 * <td>appArgs</td>
 * <td>The application arguments. The value is a white space separated list of
 * strings. Each string will represent one element in the String[]
 * passed to the mainMethod. White space that is part of an
 * argument must be escaped with '\'. The '\' character can be escaped with '\'.
 * </td>
 * </tr>
 * </table>
 * An example of a properties file.
 *
 \code
 #
 jvmArgs=-Djava.class.path=%@DD/jars/MyApp.jar:%@DD/jars/HerApp.jar -Xmx256m
 appArgs=-verbose -debug
 mainClass=com.applications.MyApp
 #
 \endcode
 *
 * The "main" method on "com.applications.MyApp" will be called with
 * signature "([Ljava/lang/String;)V".
 */
@interface JavaArgs : NSObject {

NSString *_mainclass;
NSString *_mainmethod;
NSString *_signature;
NSString *_javahome;
NSMutableArray *_app_args;
NSMutableDictionary *_java_args;
JavaLauncherCallback _callback;
void *_app_data;
bool _merge_argv;

}

/*!
 * The Java class that implements mainMethod
 */
@property (copy) NSString *mainclass;
/*!
 * The static void method to invoke on mainClass. The default value is "main".
 */
@property (copy) NSString *mainmethod;
/*!
 * The "mainmethod" signature. Only the ([Ljava/lang/String;)V,
 * and ()V signatures are supported. "([Ljava/lang/String;)V" is the default.
 */
@property (copy) NSString *signature;
/*!
 * The absolute path to the jre root directory
 */
@property (copy) NSString *javahome;
/*!
 * The mainMethod arguments, passed as a Java "String[]" array to
 * the mainMethod.
 */
@property (retain) NSMutableArray *app_args;
/*!
 * The Java properties and options passed to JNI_CreateJavaVM as
 * JavaVMOption elements.
 */
@property (retain) NSMutableDictionary *java_args;
/*!
 * The exception and error application callback. This method is called
 * on exceptions or errors during calls to jl_createJavaVM(), jl_callJavaVM()
 * or jl_destroyJavaVM()
 */
@property (assign) JavaLauncherCallback callback;
/*! Application defined context pointer */
@property (assign) void *app_data;
/*! Flag to turn on merging command line arguments with jvmArgs */
@property (assign) bool merge_argv;

/*!
 * \brief Create an instance of JavaArgs.
 * \details If getJavaArgsFromProperties is called successfully,
 * an instance of JavaArgs is returned. If getJavaArgsFromProperties
 * returns false or memory cannot be allocated return nil.
 * The arguments are obtained from the JAVAARGS_PROPERTIES file.
 * \result An instance of JavaArgs or nil
 */
- (id) init;
/*!
 * \brief Create an instance of JavaArgs providing Bundle object.
 * \details If getJavaArgsFromProperties is called successfully,
 * an instance of JavaArgs is returned. If getJavaArgsFromProperties
 * returns false or memory cannot be allocated return nil.
 * The arguments are obtained from the JAVAARGS_PROPERTIES file.
 * \param nsbundle will enable JavaArgs to search for dependent
 * files in the provided NSBundle directory tree rather than
 * the default mainBundle.
 * \result An instance of JavaArgs or nil
 */
- (id) init: (NSBundle *)nsbundle;
/*!
 * \brief Create an instance of JavaArgs with debug properties.
 * \details If getJavaArgsFromProperties is called successfully,
 * an instance of JavaArgs is returned. If getJavaArgsFromProperties
 * returns false or memory cannot be allocated return nil.
 * If debug is true, then the arguments
 * are obtained from the JAVADBGARGS_PROPERTIES file.
 * \param debug If true arguments are obtained from JAVADBGARGS_PROPERTIES.
 * \result An instance of JavaArgs or nil.
 */
- (id) initWithDebug:(bool)debug;

/*!
 * \brief Create an instance of JavaArgs using arguments defined in the file
 * named by the properties parameter.
 * \details If getJavaArgsFromProperties is called successfully,
 * an instance of JavaArgs is returned. If getJavaArgsFromProperties
 * returns false or memory cannot be allocated return nil.
 * \param properties The absolute path to a java.util.Properties formatted
 * file.
 * \result An instance of JavaArgs or nil.
 */
- (id) initWithProperties:(NSString *)properties;

/*!
 * \brief Return an instance of CreateJavaArgs.
 * \details Return an instance of CreateJavaArgs with values from the
 * java_args dictionary. The members can be passed to the jl_createJavaVM()
 * function. The caller is responsible for freeing the returned CreateJavaArgs
 * instance with jl_freeCreateJavaArgs(CreateJavaArgs *createJavaArgs).
 * \result An instance of CreateJavaArgs.
 */
-(CreateJavaArgs*) getCreateJavaArgs;

/*!
 * \brief Return an instance of CallJavaArgs.
 * \details Return an instance of CallJavaArgs. A copy of the java_args
 * array and other properties are used to initialize the CallJavaArgs
 * instance. The members can be passed to the jl_callJava() function.
 * The caller is responsible for freeing the returned CallJavaArgs
 * instance with jl_freeCallJavaArgs(CallJavaArgs *createJavaArgs).
 * \result An instance of CallJavaArgs.
 */
-(CallJavaArgs*) getCallJavaArgs;


/*!
 * \brief Populate java_args with the Java arguments obtained from
 * a file identified by properties.
 * \details Populate java_args with the Java arguments obtained from
 * a file identified by properties. properties is
 * the absolute path to a file
 * formatted as defined by java.util.Properties. Any property value may contain
 * the substitution parameter %\@DD. This string is replaced with the
 * bundle path. The recognized properties are
 *
 * <table border="1" summary="Recognized properties">
 * <tr>
 * <td>jvmArgs</td>
 * <td>A space separated list of options and properties
 * that become the JavaVMOptions used in the call to
 * JNI_CreateJavaVM. The options and properties
 * are the same as defined by the java command such as
 * jvmArgs=-Djava.class.path=.:foo.jar -Xmx256m. Options
 * such as "-cp" that include white space cannot be used
 * as a value in jvmArgs.
 * </td>
 * </tr>
 * <tr>
 * <td>mainClass</td>
 * <td>The Java class name that implements mainMethod.</td>
 * </tr>
 * <tr>
 * <td>mainMethod</td>
 * <td>The name of the static void method to invoke on mainClass.</td>
 * </tr>
 * <tr>
 * <td>signature</td>
 * <td>The method signature defining the return value
 * and arguments of mainMethod. Only two signatures are supported.
 * <ul>
 * <li>([Ljava/lang/String;)V</li>
 * <li>()V</li>
 * </ul>
 * </tr>
 * <tr>
 * <td>appArgs</td>
 * <td>The application arguments. The value is a white space separated list of
 * strings. Each string will represent one element in the String[]
 * passed to the mainMethod. White space that is part of an
 * argument must be escaped with '\'. The '\' character can be escaped with '\'.
 * </td>
 * </tr>
 * <tr>
 * <td>mergeArgv</td>
 * <td>If this propety is set to "true", the getJavaArgs method will call
 * getJavaArgsFromProperties and getJavaArgsFromCmdLine methods. It will merge
 * properties file arguments with the command line arguments, with the command
 * line arguments taking precedence. The default value for this property is false.
 * If the property is set to any value but "true", it will be set to "false".
 * </td>
 * </tr>
 * </table>
 *
 * An example of a properties file.
 *
 \code
 #
 jvmArgs=-Djava.class.path=%@DD/jars/MyApp.jar:%@DD/jars/HerApp.jar -Xmx256m
 appArgs=-verbose -debug
 mainClass=com.applications.MyApp
 #
 \endcode
 *
 * The "main" method on "com.applications.MyApp" will be called with
 * signature "([Ljava/lang/String;)V".
 *
 * \param properties The name of the properties file containing the Java
 * argument properties
 * \param java_args An instance of JavaArgs to contain the parsed properties.
 * \result Return true if the properties can be successfully read and assigned
 * to members of java_Args, else false if not or properties is NULL.
 */
+ (bool)getJavaArgs :(JavaArgs *)java_args :(NSString *)properties;

/*!
 * \privatesection
 */
/*!
 * \internal
 * \brief Merge Java arguments from Java arguments defined in
 * runtime_java_args and the properties file identified by properties.
 * \details The Java arguments defined in runtime_java_args are
 * merged with the Java arguments defined in the properties file
 * identified by the absolute file path "properties" and returned
 * in "java_args". Java arguments are read from properties by calling
 * "getJavaArgsFromProperties".
 * \param java_args A JavaArgs instance used to contain the merged
 * Java arguments from runtime_java_args and the properties file
 * identified by "properties"
 * \param properties The absolute path to a properties file that defines
 * Java arguments.
 * \result Return true if the Java arguments are successfully merged and
 * stored in java_args, else false.
 */
+ (bool)getJavaArgs :(JavaArgs *)java_args :(JavaArgs *)runtime_java_args
                    :(NSString *)properties;

/*!
 * \internal
 * \brief Populate java_args with Java arguments from the properties file
 * identified by "java_args_properties".
 * \details "getJavaArgsFromProperties" is called to read the properties
 * from the properties file "java_args_properties" and stored in java_args.
 * \param java_args The JavaArgs instance used to store the Java arguments
 * read from the properties file "java_args_properties".
 * \param java_args_properties The absolute path to a properties file
 * that defines Java arguments.
 * \result Return true if the Java arguments are successfully read and
 * and stored in java_args, else false.
 */
+ (bool)getJavaArgsFromProperties:(NSString *)java_args_properties :(JavaArgs *)java_args;

/*!
 * \internal
 * \brief Obtain Java VM properties and options from the command line.
 * \details Command line arguments are obtained from the argv array
 * passed to main. These arguments are typically defined in the Xcode
 * Scheme's Arguments property sheet.  The argv[0] element is ignored.
 *
 * The order of the arguments in the scheme arguments property sheet is
 * significant and should be ordered as they would be on the java command
 * line, VM arguments first, followed by the main class and application
 * arguments.
 *
 * The expected VM argument format is "-<javaarg>=<value>" as they would appear
 * on the java command line. For example when adding an argument in the scheme
 * arguments property sheet the property and value including the "=" sign would
 * appear in a single text field, such as
 * "-Djava.class.path=%\@DD/HelloWorld.jar".
 * When parsed and returned in the java_args dictionary, the key is
 * "-Djava.class.path" and the value is "%\@DD/HelloWorld.jar". In other words
 * it is "split" at the first "=" sign and the text following the "=" sign
 * becomes the value.
 *
 * The application arguments are limited to the following convention. Each text
 * field in the scheme arguments property sheet is equivalent to one of the
 * elements of a white space separated list of arguments as they would appear on
 * the java command line after the main class. For example, given the
 * following on the java command line:
 *
 * .... helloworld.HelloWorld -welcome "Hello World" -indent=5
 *
 * The first argv element that does not begin with "-" is taken to be the
 * main class and all arguments that follow are treated as application
 * arguments.
 *
 * Arguments obtained from the command line override the same arguments
 * that are defined in "java_args".
 *
 * \param java_args A JavaArgs instance to receive the command line arguments.
 * \result Return true if the command line arguments can be successfully
 * parsed and stored in java_args.
 */
+ (bool)getJavaArgsFromCmdLine:(JavaArgs *)java_args;

/*!
 * \internal
 * \brief Combine more_args with java_args.
 * \details
 * Merge the "more_args" and "java_args" java arguments. The "merge_args"
 * values override similar arguments in "java_args". A typical use case
 * of this method would be to merge arguments from the command line
 * with arguments for a properties file. For example, if arguments are
 * specified in the Xcode Scheme's Arguments property sheet, the values
 * of those arguments override the same arguments if specified in a
 * properties file.
 * \param java_args An instance of JavaArgs
 * \param more_args An instance of JavaArgs to merge with java_args whose
 * argument values override the same arguments in java_args.
 */
+(void)mergeArgs:(JavaArgs *)java_args : (JavaArgs *)more_args;

/*!
 * \internal
 * \brief Add a Java argument to java_args.
 * \details Add a Java argument "arg" with value "value" to
 * the java_args dictionary.
 * \param arg The Java argument
 * \param value The value of arg.
 */
-(void) addJavaArg :(NSString *)arg :(id)value;

/*!
 * \internal
 * \brief Add an application argument to the app_args array.
 * \details Appends app_arg to the app_args array of application
 * arguments.
 * \param app_arg An application argument.
 */
-(void) addAppArg :(NSString *)app_arg;
@end
