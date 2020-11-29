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

import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.util.MissingResourceException;
import java.util.Properties;
import java.util.ResourceBundle;
import java.util.Locale;

/**
 * The {@code JavaArgs} class encapsulates Java runtime
 * properties and options such as {@code -Djava.class.path} and
 * {@code -Xms32m}, an inital class, method, and method
 * arguments to invoke.
 * <p>
 * Constructors without a {@code Properties} argument obtain initial
 * Java runtime properties and options from a {@code ResourceBundle}
 * called {@code JavaLauncherArgs}, if it exists. Constructors that
 * accept a {@code Properties} argument initalize the instance using the
 * properties defined in the {@code Properties} argument.
 * The format of the {@code ResourceBundle} is defined by the
 * {@code java.util.Properties} class.
 * </p>
 * <p>
 * Any property value may include the substition parameter sequence
 * {@code %@DD}. It is replaced by the {@code ApplicationInfo.dataDir} value.
 * </p>
 * <p>
 * The expected properties in the {@code ResourceBundle} or {@code Properties}
 * instance are
 * <table border=1 summary="Recognized properties">
 * <tr>
 * <td>javaHome</td>
 * <td>The absolute path of the jre directory.
 * If not set {@code %@DD/storage/jvm} is used.
 * </td>
 * </tr>
 * <tr>
 * <td>jvmArgs</td>
 * <td>A space separated list of options and properties
 * that become the {@code JavaVMOptions} used in the call to
 * {@code JNI_CreateJavaVM}. The options and properties
 * are the same as defined by the java command. For example
 * {@code jvmArgs=-Djava.class.path=.:foo.jar -Xmx256m}, with the
 * exception that options such as "-cp" that include white space cannot be used
 * as a value in {@code jvmArgs}.
 * </td>
 * </tr>
 * <tr>
 * <td>mainClass</td>
 * <td>The Java class name that implements {@code mainMethod}.</td>
 * </tr>
 * <tr>
 * <td>{@code mainMethod}</td>
 * <td>The name of the static void method to invoke on {@code mainClass}.</td>
 * </tr>
 * <tr>
 * <td>signature</td>
 * <td>The method signature defining the return value
 * and arguments of {@code mainMethod}. Only two signatures are supported.
 * <ul>
 * <li>([Ljava/lang/String;)V</li>
 * <li>()V</li>
 * </ul>
 * </tr>
 * <tr>
 * <td>appArgs</td>
 * <td>The application arguments. The value is a white space separated list of
 * strings. Each string will represent one element in the {@code String[]}
 * passed to the {@code mainMethod}. White space that is part of an
 * argument must be escaped with '\'. The '\' character can be escaped with '\'.
 * </td>
 * </tr>
 * </table>
 */
public class JavaArgs {

    /** The default {@code ResourceBundle} name. */
    private static final String JAVALAUNCHERARGSPROPERTIES =
        "JavaLauncherArgs";
    /** The substitution parameter for {@code ApplicationInfo.dataDir}. */
    private static final String DATADIR_PARAM = "%@DD";
    /** The default Java home path. */
    private static final String JAVAHOME_VALUE = "%@DD/storage/jvm";
    /** The Java {@code main} method signature. */
    private static final String MAINMETHODSIGNATURE = "([Ljava/lang/String;)V";
    /** The no argument void method signature. */
    private static final String NOARGMETHODSIGNATURE = "()V";
    /** The {@code main} method name. */
    private static final String MAINMETHOD_VALUE = "main";

    /** The value of ApplicationInfo.dataDir. */
    private String dataDir;
    /** The Java home path. */
    private String javaHome;

    /** The {@code javaHome} property. */
    public static final String JAVAHOME_PROP = "javaHome";
    /** The {@code jvmArgs} property. */
    public static final String JVMARGS_PROP = "jvmArgs";
    /** The {@code mainClass} property. */
    public static final String MAINCLASS_PROP = "mainClass";
    /** The {@code mainMethod} property. */
    public static final String MAINMETHOD_PROP = "mainMethod";
    /** The {@code signature} property. */
    public static final String SIGNATURE_PROP = "signature";
    /** The {@code appArgs} property. */
    public static final String APPARGS_PROP = "appArgs";


    /**
     * The Java runtime properties and options.
     * Values may contain the substitution parameter {@code %@DD}
     * which is replaced with
     * {@code Context.getApplicationInfo().dataDir}
     * Values are formatted as they would be on the Java command line
     * with the exception of options such as {@code -cp} that are
     * separated from its value by white space.
     * <ul>
     * <li>-Djava.class.path=.:/classes</li>
     * <li>-Xms32m</li>
     * </ul>
     */
    private String[] jvmArgs;
    /**
     * The Java class implementing the method to be invoked.
     */
    private String mainClass;
    /**
     * The Java method to invoke on the Java class {@code mainClass}.
     * If this value is not set, {@code main} is invoked.
     */
    private String mainMethod;
    /**
     * The signature of the method {@code mainMethod}.
     * Only two signatures are supported.
     * <ul>
     * <li>([Ljava/lang/String;)V</li>
     * <li>()V</li>
     * </ul>
     */
    private String signature;
    /**
     * The arguments passed to the method {@code mainMethod}.
     * Values may contain the substitution parameter {@code %@DD}
     * and replaced with
     * {@code Context.getApplicationInfo().dataDir}
     * The values are formatted as
     * <ul>
     * <li>-verbose</li>
     * <li>-count=1</li>
     * <li>-file</li>
     * <li>/tmp/foo</li>
     * </ul>
     */
    private String[] appArgs;

    /** The {@code java.util.Logger} instance */
    private static final Logger logger =
        Logger.getLogger(JavaArgs.class.getName());

    /**
     * Create an instance of {@code JavaArgs} initialized
     * with properties defined in a {@code ResourceBundle}
     * called {@code JavaLauncherArgs}, if it exists.
     * If {@code JavaArgs.JAVAHOME_PROP} is not defined in
     * {@code JavaLauncherArgs} then Java home will be expected
     * at {@code <dataDir>/storage/jvm}.
     *
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @throws IllegalArgumentException if {@code JavaLauncherArgs}
     * cannot be successfully read, or {@code dataDir} is null or an
     * empty string.
     */
    public JavaArgs(String dataDir) {
        this(dataDir, (String)null);
    }
    /**
     * Create an instance of {@code JavaArgs} initialized
     * with properties defined in a {@code ResourceBundle}
     * called {@code JavaLauncherArgs}, if it exists.
     * The {@code javaHome} argument will override the {@code JAVAHOME_PROP}
     * property if defined in {@code JavaLauncherArgs}.
     *
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @param javaHome The absolute path of the jre directory.
     *
     * @throws IllegalArgumentException if {@code JavaLauncherArgs}
     * cannot be successfully read, or {@code dataDir} is null or an
     * empty string.
     */
    public JavaArgs(String dataDir, String javaHome) {
        super();
        if (dataDir == null || dataDir.isEmpty()) {
            throw new IllegalArgumentException("dataDir must be specified");
        }
        this.dataDir = dataDir;
        this.javaHome = javaHome;
        load();
    }
    /**
     * Create an instance of {@code JavaArgs} with initial
     * properties defined in {@code javaProperties}.
     * If {@code JavaArgs.JAVAHOME_PROP} is not defined in
     * {@code javaProperties} then Java home will be expected
     * at {@code <dataDir>/storage/jvm}.
     *
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @param javaProperties defines Java arguments.
     * @throws IllegalArgumentException if {@code dataDir} is null or an
     * empty string.
     */
    public JavaArgs(String dataDir, Properties javaProperties) {
        super();
        if (dataDir == null || dataDir.isEmpty()) {
            throw new IllegalArgumentException("dataDir must be specified");
        }
        this.dataDir = dataDir;
        load(javaProperties);
    }
    /**
     * Create an instance of {@code JavaArgs} with initial
     * properties defined in the {@code Properties} file with absolute path
     * {@code javaPropertiesPath}.
     * The {@code javaHome} argument will override the {@code JAVAHOME_PROP}
     * property if defined in the {@code javaPropertiesPath}
     * {@code Properties} file.
     *
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @param javaHome the absolute path of the jre directory.
     * @param javaPropertiesPath the absolute path of a
     * {@code Properties} file containing Java arguments.
     * @throws IllegalArgumentException if the {@code Properties} file
     * {@code javaPropertiesPath} file cannot be successfully read or
     * {@code dataDir} is null or an empty string.
     */
    public JavaArgs(String dataDir, String javaHome,
            String javaPropertiesPath) {
        super();
        if (dataDir == null || dataDir.isEmpty()) {
            throw new IllegalArgumentException("dataDir must be specified");
        }
        this.dataDir = dataDir;
        this.javaHome = javaHome;
        load(javaPropertiesPath);
    }
    /**
     * Create an instance of {@code JavaArgs} initialized
     * with the given arguments.
     * If any of {@code javaHome}, {@code mainClass},
     * {@code mainMethod}, or {@code signature} are {@code null}
     * or an empty string, {@code IllegalArgumentException} is thrown.
     * The values cannont contain the {@code DATADIR_PARAM} substitution
     * parameter.
     *
     * @param javaHome the absolute path to the jre directory.
     * @param jvmArgs the properties and options used to create the VM.
     * @param mainClass the Java class that implements {@code mainMethod}.
     * @param mainMethod the method to invoke on {@code mainClass}.
     * @param signature the signature of {@code mainMethod}
     * @param appArgs the arguments passed to {@code mainMethod}
     * @throws IllegalArgumentException if any argument values are null
     * or an empty string.
     */
    public JavaArgs(String javaHome, String[] jvmArgs, String mainClass,
            String mainMethod, String signature, String[] appArgs) {

        String emsg = null;
        if (javaHome == null || javaHome.isEmpty()) {
            emsg = "javaHome is null or empty.";
        } else
        if (mainClass == null || mainClass.isEmpty()) {
            emsg = "mainClass is null or empty.";
        } else
        if (mainMethod == null || mainMethod.isEmpty()) {
            emsg = "mainMethod is null or empty.";
        } else
        if (signature == null || signature.isEmpty()) {
            emsg = "signature is null or empty.";
        }
        if (emsg != null) {
            throw new IllegalArgumentException("JavaArgs: " + emsg);
        }
        this.javaHome = javaHome;
        this.mainClass = mainClass;
        this.mainMethod = mainMethod;
        this.signature = signature;
        this.jvmArgs = jvmArgs;
        this.appArgs = appArgs;
        setLocale(this.jvmArgs);
    }

    /**
     * Set the value of the jre directory to {@code javaHome}.
     *
     * @param javaHome the absolute path to the jre directory.
     */
    public void setJavaHome(String javaHome) {
        this.javaHome = javaHome;
    }
    /**
     * Accessor for {@code javaHome}.
     * @return the {@code javaHome} value.
     */
    public String getJavaHome() {
       return javaHome;
    }
    /**
     * The {@code mainMethod}'s class.
     *
     * @param mainClass the class to invoke mainMethod on.
     */
    public void setMainClass(String mainClass) {
        this.mainClass = mainClass;
    }
    /**
     * Accessor for {@code mainClass}.
     * @return the {@code mainClass} value.
     */
     public String getMainClass() {
        return mainClass;
    }
    /**
     * Set the value of the {@code mainClass} method.
     * The method must be a static method that returns void. The supported
     * signatures are {@code ([Ljava/lang/String;)V} and {@code ()V}.
     *
     * @param mainMethod the method to invoke on {@code mainClass}.
     */
    public void setMainMethod(String mainMethod) {
        this.mainMethod = mainMethod;
    }
    /**
     * Accessor for {@code mainMethod}
     * @return the {@code mainMethod} value.
     */
    public String getMainMethod() {
        return mainMethod;
    }
    /**
     * Set the value of the signature for the {@code mainMethod}.
     * The supported signatures are {@code ([Ljava/lang/String;)V} and
     * {@code ()V}.
     *
     * @param signature the mainMethod signature.
     */
    public void setSignature(String signature) {
        this.signature = signature;
    }
    /**
     * Accessor for {@code signature}.
     * @return the {@code signature} value.
     */
    public String getSignature() {
        return signature;
    }
    /**
     * Set the value of the application arguments passed to
     * {@code mainMethod}.
     *
     * @param appArgs the application arguments passed to mainMethod.
     */
    public void setAppArgs(String[] appArgs) {
        this.appArgs = appArgs;
    }
    /**
     * Accessor for {@code appArgs}.
     * @return the {@code appArgs} value.
     */
    public String[] getAppArgs() {
        return appArgs;
    }
    /**
     * Set the value of the Java runtime properties and options.
     *
     * @param jvmArgs the properties and options used to create the Java VM.
     */
    public void setJvmArgs(String[] jvmArgs) {
        this.jvmArgs = jvmArgs;
        setLocale(this.jvmArgs);
    }
    /**
     * Accessor for {@code jvmArgs}.
     * @return the {@code jvmArgs} value.
     */
    public String[] getJvmArgs() {
        return jvmArgs;
    }

    /**
     * Obtain the property {@code prop} from {@code ResourceBundle}
     * {@code props}. If {@code prop} is not found, then
     * {@code propdefault} is returned.
     */
    private String getResourceBundleProp(ResourceBundle props,
            String prop, String propdefault) {
        String value = propdefault;
        try {
            value = props.getString(prop);
        } catch (MissingResourceException e) {
            debugout("JavaArgs::getResourceBundleProp: " +
                "Property " + prop + "is not set, using default '" +
                (String)(propdefault == null ? "null" : propdefault) + "'");
        } catch (Exception e) {
            debugout("JavaArgs::getResourceBundleProp: " + e.getMessage());
        }
        return value;
    }
    /**
     * Load the initial Java runtime properties and options
     * from the {@code JAVALAUNCHERARGSPROPERTIES}
     * {@code ResourceBundle}.
     */
    protected void load() {
        ResourceBundle props = null;
        try {
            props = ResourceBundle.getBundle(JAVALAUNCHERARGSPROPERTIES);
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        }

        String value = null;
        if (getJavaHome() == null) {
            value = getResourceBundleProp(props, JAVAHOME_PROP,
                    JAVAHOME_VALUE);
            value = filterAppDirPath(value, dataDir);
            setJavaHome(value);
        }

        String[] arrayvalue = null;
        value = getResourceBundleProp(props, JVMARGS_PROP, null);
        if (value != null) {
            arrayvalue = parseArrayValue(value);
            for (int i = 0; i < arrayvalue.length; ++i) {
                arrayvalue[i] = filterAppDirPath(arrayvalue[i], dataDir);
            }
        }
        setJvmArgs(arrayvalue);

        arrayvalue = null;
        value = getResourceBundleProp(props, APPARGS_PROP, null);
        if (value != null) {
            arrayvalue = parseArrayValue(value);
            for (int i = 0; i < arrayvalue.length; ++i) {
                arrayvalue[i] = filterAppDirPath(arrayvalue[i], dataDir);
            }
        }
        setAppArgs(arrayvalue);

        value = getResourceBundleProp(props, MAINCLASS_PROP, null);
        if (value == null) {
            debugout("JavaArgs::load(): " + MAINCLASS_PROP + " is not set.");
        }
        setMainClass(value);

        value = getResourceBundleProp(props, MAINMETHOD_PROP, MAINMETHOD_VALUE);
        setMainMethod(value);

        value = getResourceBundleProp(props, SIGNATURE_PROP, null);
        if (value == null && mainMethod.equals(MAINMETHOD_VALUE)) {
            setSignature(MAINMETHODSIGNATURE);
            debugout("JavaArgs::load(): " + SIGNATURE_PROP + " is not set " +
                "using " + MAINMETHODSIGNATURE);
        } else {
            setSignature(value);
        }
        if (value == null && mainMethod.equals(MAINMETHOD_VALUE)) {
            setSignature(MAINMETHODSIGNATURE);
            debugout("JavaArgs::load(): " + SIGNATURE_PROP + " is not set " +
                "using " + MAINMETHODSIGNATURE);
        } else {
            setSignature(value);
        }
    }

    /**
     * Return the value of {@code prop}
     * from the {@code Properties} props.
     * If {@code prop} is not found, {@code propdefault}
     * is returned.
     *
     * @param props the {@code Properties} defining {@code prop}
     * @param prop the property to retreive.
     * @param propdefault the default value if {@code prop} prop does
     * not exist.
     * @return the value of {@code prop} or {@code propdefault}
     */
    private String getPropertiesProp(Properties props,
            String prop, String propdefault) {
        String value =  props.getProperty(prop, propdefault);
        if (value == null) {
            debugout("JavaArgs::getPropertiesProp: " +
                "Property " + prop + "is not set.");
        }
        return value;
    }

    /**
     * Load the initial Java runtime properties and options
     * from the {@code Properites} instance {@code props}
     *
     * @param props contains the Java arguments.
     */
    protected void load(Properties props) {

        String[] arrayvalue = null;
        String value = getPropertiesProp(props, JAVAHOME_PROP,
                JAVAHOME_VALUE);
        value = filterAppDirPath(value, dataDir);
        setJavaHome(value);

        value = getPropertiesProp(props, JVMARGS_PROP, null);
        if (value != null) {
            arrayvalue = parseArrayValue(value);
        }
        setJvmArgs(arrayvalue);

        arrayvalue = null;
        value = getPropertiesProp(props, APPARGS_PROP, null);
        if (value != null) {
            arrayvalue = parseArrayValue(value);
        }
        setAppArgs(arrayvalue);

        value = getPropertiesProp(props, MAINCLASS_PROP, null);
        if (value == null) {
            debugout("JavaArgs::load(): " + MAINCLASS_PROP + " is not set.");
        }
        setMainClass(value);

        value = getPropertiesProp(props, MAINMETHOD_PROP, MAINMETHOD_VALUE);
        setMainMethod(value);

        value = getPropertiesProp(props, SIGNATURE_PROP, null);
        if (value == null && mainMethod.equals(MAINMETHOD_VALUE)) {
            setSignature(MAINMETHODSIGNATURE);
            debugout("JavaArgs::load(): " + SIGNATURE_PROP + " is not set " +
                "using " + MAINMETHODSIGNATURE);
        } else {
            setSignature(value);
        }
    }

    /**
     * Load the initial Java runtime properties and options
     * from the {@code Properites} file with absolute path
     * {@code javaPropertiesPath}.
     * If the {@code javaPropertiesPath} file cannot be successfully read
     * {@code IllegalArgumentException} is thrown.
     *
     * @param javaPropertiesPath the absolute path to a {@code Properties}
     * file containing the Java arguments.
     * @throws IllegalArgumentException if the {@code javaPropertiesPath}
     * file cannot be successfully read
     */
    protected void load(String javaPropertiesPath) {
        Properties properties = null;
        FileInputStream fis = null;
        try {
            fis = new FileInputStream(javaPropertiesPath);
            properties = new Properties();
            properties.load(fis);
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        } finally {
            if (fis != null) {
                try {
                    fis.close();
                } catch (Exception e) {
                }
            }
        }
        load(properties);
    }

    /**
     * Replace {@code DATADIR_PARAM} in {@code args} elements with
     * {@code dataDir}. The {@code args} array is modified.
     *
     * @param args trings containing {@code DATADIR_PARAM}.
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @return the {@code args} with {@code DATADIR_PARAM}
     * replaced with {@code dataDir}.
     */
    public static String[] filterAppDirPath(String[] args, String dataDir) {
        if (args == null || dataDir == null || dataDir.isEmpty()) {
            return args;
        }
        for (int i = 0; i < args.length; ++i) {
            args[i] =  args[i].replaceAll(DATADIR_PARAM, dataDir);
        }
        return args;
    }
    /**
     * Replace {@code DATADIR_PARAM} in {@code arg} with {@code dataDir}.
     *
     * @param arg a string containing {@code DATADIR_PARAM}.
     * @param dataDir the replacement value for {@code DATADIR_PARAM}.
     * @return {@code arg} with {@code DATADIR_PARAM}
     * replaced with {@code dataDir}.
     */
    public static String filterAppDirPath(String arg, String dataDir) {
        return arg == null || arg.isEmpty() || dataDir == null ||
            dataDir.isEmpty() ? arg : arg.replaceAll(DATADIR_PARAM, dataDir);
    }

    /**
     * {@code argslist} is a white space separated list of strings.
     * Return a {@code String} array containing the strings in the
     * order that they are found.
     * @param argslist A white space separated list of strings
     * @return A {@code String} array containing the strings in the
     * order that they are found or {@code null}.
     */
    private String[] parseArrayValue(String argslist) {

        // If it's an empty string return null.
        if (argslist == null || argslist.length() == 0) {
            return null;
        }
        // The index of the next character
        int charindex = 0;
        // true if last seen c is '\\'
        boolean haveescape = false;
        int linelen = argslist.length();
        ArrayList<String> args = new ArrayList<String>();
        StringBuilder arg = new StringBuilder();

        while (charindex < linelen) {
            char c = argslist.charAt(charindex++);
            if ((!Character.isWhitespace(c) && c != '\\') || haveescape) {
                arg.append(c);
                haveescape = false;
                continue;
            }
            if (c == '\\') {
                haveescape = true;
                continue;
            }

            // c is an unescaped  white space separator.
            // save the last parsed arg.
            if (arg.length() > 0) {
                args.add(arg.toString());
                arg.setLength(0);
            }
        }
        // One arg in argslist
        if (arg.length() > 0) {
            args.add(arg.toString());
        }
        if (args.size() == 0) {
            return null;
        } else {
            return args.toArray(new String[args.size()]);
        }
    }

    private void setLocale(String[] jvmArgs) {
        Locale defaultLocale = Locale.getDefault();
        String androidLanguage = defaultLocale.getLanguage();
        String androidCountry = defaultLocale.getCountry();
        String javaLanguageProp = "-Duser.language=" + androidLanguage;
        String javaCountryProp = "-Duser.country=" + androidCountry;
        int len = (jvmArgs == null ? 0 : jvmArgs.length) + 2;
        String[] newJvmArgs = new String[len];
        newJvmArgs[0] = javaLanguageProp;
        newJvmArgs[1] = javaCountryProp;
        if (jvmArgs != null) {
          System.arraycopy(jvmArgs, 0, newJvmArgs, 2, jvmArgs.length);
        }
        this.jvmArgs = newJvmArgs;
    }

    /**
     * Debug output.
     *
     * @param msg the debug message.
     */
    private void debugout(String msg) {
        if (logger.isLoggable(Level.INFO)) {
            logger.log(Level.INFO, msg);
        }
    }
}
