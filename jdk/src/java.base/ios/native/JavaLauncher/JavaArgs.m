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

#import "JavaArgs.h"

static char *filterBundlePath(NSString *bundlePath, NSString *documentsPath, NSString *value);
static NSMutableDictionary *propvalueToDict(NSArray *values);
static NSArray *propertiesFileToArray(NSString *propertiesfile);
static NSString *readLine(NSArray *lines, int *linesindex);
static bool getJavaProperty(NSString *line, NSString **key, NSMutableArray **value);

static NSBundle *bundle = NULL;

@implementation JavaArgs

@synthesize mainclass = _mainclass;
@synthesize mainmethod = _mainmethod;
@synthesize signature = _signature;
@synthesize javahome = _javahome;
@synthesize app_args = _app_args;
@synthesize java_args = _java_args;
@synthesize callback = _callback;
@synthesize app_data = _app_data;
@synthesize merge_argv = _merge_argv;

- (id)initWithProperties :(NSString *)properties
{
    self = [super init];
    if (_app_args == NULL) {
        self.app_args = [NSMutableArray arrayWithCapacity:0];
    }
    if (_java_args == NULL) {
        self.java_args = [NSMutableDictionary dictionaryWithCapacity:0];
    }
    bool result = [JavaArgs getJavaArgs :(JavaArgs*)self :properties];
    if (result) {
        return self;
    } else {
        [self release];
        return nil;
    }
}


- (id)initWithDebug :(bool)debug
{
    return [self initWithProperties :
        debug ? JAVADBGARGS_PROPERTIES : JAVAARGS_PROPERTIES];
}

- (id)init
{
    bundle = (NSBundle *)NULL;
    return [self initWithDebug :false];
}

- (id)init: (NSBundle *)nsbundle
{
    bundle = nsbundle;
    return [self initWithDebug :false];
}

- (void)dealloc
{
    self.mainclass = NULL;
    self.mainmethod = NULL;
    self.signature = NULL;
    self.javahome = NULL;
    self.app_args = NULL;
    self.java_args = NULL;
    self.merge_argv = false;
    [super dealloc];
}

- (void)addJavaArg :(NSString *)key :(id)value
{
    if (_java_args == NULL) {
        self.java_args = [NSMutableDictionary dictionaryWithCapacity:0];
    }
    [self.java_args setValue:value forKey:key];
}

- (void)addAppArg :(NSString *)app_arg
{
    if (_app_args == NULL) {
        self.app_args = [NSMutableArray arrayWithCapacity:0];
    }
    [self.app_args addObject:app_arg];
}

#define SINGLEVALUEPROP(v, def) [(v) count] > 0 ? \
    (NSString*)[(v) objectAtIndex:0] : \
    (def)

+(bool) getJavaArgsFromProperties :(NSString *)propertiesfile
                                  :(JavaArgs*)java_args
{
    if (propertiesfile == NULL) {
        NSLog(@"JavaArgs::getJavaArgsFromProperties: "
              "propertiesfile is NULL");
        return false;
    }
    if (java_args == NULL) {
        NSLog(@"JavaArgs::getJavaArgsFromProperties: "
              "java_args is NULL");
        return false;
    }
    NSArray *lines = NULL;
    @try {
        // It's ok to have no properties unlesss the properties file
        // cannot be found or read, which is an exception
        lines = propertiesFileToArray(propertiesfile);
        if (lines == NULL || [lines count] == 0) {
            [lines release];
            return true;
        }
    }
    @catch (NSString *excmsg) {
        // msgs are logged already
        return false;
    }

    int linesindex = 0;
    while (linesindex < [lines count]) {
        NSString *line = readLine(lines, &linesindex);

        if (line == NULL) {
            break;
        }
        NSString *key = NULL;
        NSMutableArray *value = NULL;
        if (!getJavaProperty(line, &key, &value)) {
            NSLog(@"JavaArgs::getJavaArgsFromProperties: "
                @"Failed to parse property key and value from '%@'", line);
            [line release];
            return false;
        }
#ifdef DUMPPROPERTIES
        printf("linesindex: %d\n", linesindex);
        printf("key: '%s'\n", [key UTF8String]);
        if (value == NULL) {
            printf("value: NULL\n");
        } else if ([value count] == 0) {
            printf("value: no value\n");
        } else {
            for (int j = 0; j < [value count]; ++j) {
                printf("value: '%s'\n", [[value objectAtIndex:j] UTF8String]);
            }
        }
#endif
        
        // process the value of the key as needed
        if ([key hasPrefix: VMARGS_PROP]) {
            NSMutableDictionary *vmargs_props = propvalueToDict(value);
            if (java_args.java_args == NULL) {
                java_args.java_args = vmargs_props;
            } else {
                if (vmargs_props != NULL) {
                    [java_args.java_args addEntriesFromDictionary
                        :vmargs_props];
                }
            }
            [vmargs_props release];
        } else if ([key hasPrefix: APPARGS_PROP]) {
            if (java_args.app_args == NULL) {
                java_args.app_args = value;
            } else {
                if (value != NULL) {
                    [java_args.app_args addObjectsFromArray :value];
                }
            }
        } else if ([key hasPrefix: JAVAMAINCLASS_PROP]) {
            java_args.mainclass = SINGLEVALUEPROP(value, NULL);
        } else if ([key hasPrefix: JAVASIGNATURE_PROP]) {
            java_args.signature = SINGLEVALUEPROP(value, NULL);
        } else if ([key hasPrefix: JAVAMAINMETHOD_PROP ]) {
            java_args.mainmethod = SINGLEVALUEPROP(value, NULL);
        } else if ([key hasPrefix: JAVAHOME_PROP]) {
            java_args.javahome = SINGLEVALUEPROP(value, NULL);
            // Add java home to jvm args
            [java_args addJavaArg: (NSString *)@"-Djava.home" : (id)java_args.javahome];
        } else if ([key hasPrefix: MERGEARGV_PROP]) {
            // If the value is not literally "true" set it to "false"
            NSString* boolstr = SINGLEVALUEPROP(value, NULL);
            java_args.merge_argv = false;
            if (boolstr != NULL) {
                NSComparisonResult isequal =
                    [boolstr caseInsensitiveCompare: @"true"];
                if (isequal == NSOrderedSame) {
                    java_args.merge_argv = true;
                }
            }
        }
#ifdef DEBUG
        else {
            NSLog(@"JavaArgs::getJavaArgsFromProperties: " 
                @"Unrecognized property %@.", key);
        }
#endif
        [value release];
        [key release];
        [line release];
    }

    if (java_args.mainmethod == NULL || [java_args.mainmethod length] == 0) {
        java_args.mainmethod = DEFAULT_METHOD;
    }
    if (java_args.signature == NULL || [java_args.signature length] == 0) {
        java_args.signature = DEFAULT_SIGNATURE;
    }
    [lines release];
    return true;
}

// Need the list of arguments possible for the simulator that we
// must ignore, since they are passed onto the application.
// -SimulateApplication is specified in the build.xml file for
// the templates, -RegisterForSystemEvents just shows up
// when run from the command line. When the simulator is launched
// from Xcode it does not appear.
#define IGNOREARGANDVALUE 1
#define IGNOREARG 2

static int ignore_arg(NSString *arg)
{
    if ([arg isEqualToString:@"-SimulateApplication"]) {
        return IGNOREARGANDVALUE;
    } else if ([arg isEqualToString:@"-RegisterForSystemEvents"]) {
        return IGNOREARG;
    }

    return 0;
}

+(bool)getJavaArgsFromCmdLine :(JavaArgs *)java_args
{

    NSArray *userVmArgs = [[NSProcessInfo processInfo] arguments];
    int count = [userVmArgs count];
    // We don't care about the executable argv[0]
    if (count < 2) {
        //NSLog(@"JavaArgs::get_java_args_from_cmdline: No command line args.");
        return false;
    }

    // track howmany args we add
    int howmany = 0;

    // Look for the first non "-" option, which will be the main class
    // and app args. skip argv[0]
    int argi = 1;
    for (; argi < count; ++argi) {
        NSString *arg = [userVmArgs objectAtIndex:argi];
        int ignorevalue = ignore_arg(arg);
        switch (ignorevalue) {
        case IGNOREARGANDVALUE:
            ++argi;
            continue;
            break;
        case IGNOREARG:
            continue;
            break;
        }
        //NSLog(@"%@", arg);
        if (![arg hasPrefix:@"-"]) {
            break;
        }
        // Have a least one
        ++howmany;
        // Split the value at the first "="
        // The first element will be the key and the rest of the elements will
        // be the value
        // Add back the "=" into the value, for options like
        // "agentlib" which can have more than one "="
        NSArray *nvpair = [arg componentsSeparatedByString:@"="];
        if ([nvpair count] == 1) {
            [java_args.java_args setObject:[NSNull null]
                forKey:(id)[nvpair objectAtIndex:0]];
        } else if ([nvpair count] == 2) {
            [java_args.java_args setValue:[nvpair objectAtIndex:1]
                forKey:[nvpair objectAtIndex:0]];
        } else {
            NSString *key = [nvpair objectAtIndex:0];
            NSString *value = [nvpair objectAtIndex:1];
            unsigned int i;
            for (i = 2; i < [nvpair count]; ++i) {
                value = [value stringByAppendingFormat:@"=%@",
                    [nvpair objectAtIndex:i]];
            }
            [java_args.java_args setObject:value forKey:(id)key];
        }
    }

    // see if we have a mainClass arg
    if (argi < count) {
        NSString *mc = [userVmArgs objectAtIndex:argi++];
        if (mc != NULL && [mc length] != 0) {
            java_args.mainclass = mc;
            java_args.mainmethod = DEFAULT_METHOD;
            java_args.signature = DEFAULT_SIGNATURE;
        }
        // See if we have app_args
        if (argi < count) {
            NSRange range;
            range.location = argi;
            range.length = count - argi;
            java_args.app_args = [NSMutableArray arrayWithArray:
                [userVmArgs subarrayWithRange:range]];
        }
    }
    return howmany > 0;
}

+(void)mergeArgs:(JavaArgs *)java_args : (JavaArgs *)more_args
{
    // more_args overrides all args.
    if (more_args != NULL) {
        if (more_args.java_args != NULL) {
            for (NSString *key in more_args.java_args) {
                [java_args.java_args setObject:(id)
                 [more_args.java_args objectForKey:key] forKey:(id)key];
            }
        }
        if (more_args.mainclass != NULL) {
            java_args.mainclass = more_args.mainclass;
            java_args.mainmethod = more_args.mainmethod;
            java_args.app_args = more_args.app_args;
        }
    }
}

+(bool)getJavaArgs :(JavaArgs *)java_args :(NSString *)properties
{
    return [JavaArgs getJavaArgs :java_args :NULL :properties];
}

+(bool)getJavaArgs :(JavaArgs *)java_args :(JavaArgs *)more_args
                   :(NSString *)properties
{
    bool havejavaargs = [JavaArgs getJavaArgsFromProperties:properties
        :java_args];

    if (!havejavaargs) {
        NSLog(@"JavaArgs::getJavaArgs: Failed to parse properties file %@.",
              properties == NULL ? @"NULL" : properties);
        return false;
    }
    if (java_args.merge_argv) {
        [JavaArgs getJavaArgsFromCmdLine :java_args];
    }

    // Now override with "runtime" application arguments
    [JavaArgs mergeArgs :java_args :more_args];

    return true;
}
+(NSString *) documentsDirectory
{
    NSString *dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if(![fileManager fileExistsAtPath:dir]) {
        [fileManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    }
    return dir;
}

- (CreateJavaArgs *)getCreateJavaArgs
{

    JavaArgs *java_args = self;

    NSBundle *mainBundle = (bundle != NULL)? bundle : [NSBundle mainBundle];
    NSString *bundlePath = [mainBundle bundlePath];
    NSString *documentsPath = [JavaArgs documentsDirectory];

    CreateJavaArgs *create_java_args = (CreateJavaArgs*)
        calloc(1, sizeof(CreateJavaArgs));
    if (create_java_args == NULL) {
        NSLog(@"JavaArgs::getCreateJavaArgs: Cannot allocate memory for CreateJavaArgs.");
        return NULL;
    }

    // process args into arrays
    int args_len = [java_args.java_args count];
    char **args = (char**)calloc(args_len, sizeof(char*));
    if (args == NULL) {
        NSLog(@"JavaArgs::getCreateJavaArgs: "
            @"Cannot allocate memory for char* array for %d CreateJavaArgs java_args.\n",
              args_len);
        jl_freeCreateJavaArgs(create_java_args);
        return NULL;
    }
    create_java_args->jvm_args = args;
    create_java_args->jvm_args_len = args_len;

    // Reformat the key/value pair as "key=value"
    int index = 0;
    for (NSString *key in java_args.java_args) {
        NSString *value = [java_args.java_args objectForKey:key];
        if (value != (NSString*)[NSNull null] && [value length] > 0) {
	  args[index] = filterBundlePath(bundlePath, documentsPath,
                [key stringByAppendingFormat:@"=%@", value]);
        } else {
	  args[index] = filterBundlePath(bundlePath, documentsPath, key);
        }
        if (args[index++] == NULL) {
            jl_freeCreateJavaArgs(create_java_args);
            return NULL;
        }
    }
    return create_java_args;
}

- (CallJavaArgs *)getCallJavaArgs
{
    JavaArgs *java_args = self;

    NSBundle *mainBundle = (bundle != NULL)? bundle : [NSBundle mainBundle];

    NSString *bundlePath = [mainBundle bundlePath];
    NSString *documentsPath = [JavaArgs documentsDirectory];

    CallJavaArgs *call_java_args = 
        (CallJavaArgs*)calloc(1, sizeof(CallJavaArgs));
    if (call_java_args == NULL) {
        NSLog(@"JavaArgs::getCallJavaArgs: Cannot allocate memory for CallJavaArgs.");
        return NULL;
    }

    int args_len = 0;
    char *tmpstr = NULL;

    if (java_args.mainclass != NULL  && [java_args.mainclass length] != 0) {
        tmpstr = (char *)[[java_args.mainclass
            stringByReplacingOccurrencesOfString:@"." withString:@"/"] UTF8String];
        if (tmpstr != NULL && *tmpstr != '\0') {
            tmpstr = strndup(tmpstr, strlen(tmpstr));
            if (tmpstr == NULL) {
                NSLog(@"JavaArgs::getCallJavaArgs: Cannot allocate memory for mainClass.");
                goto _gcja_fail;
            }
            call_java_args->mainclass = tmpstr;
        }
    }
    
    tmpstr = (char*)[java_args.mainmethod UTF8String];
    if (tmpstr != NULL && *tmpstr != '\0') {
        tmpstr = strndup(tmpstr, strlen(tmpstr));
        if (tmpstr == NULL) {
            NSLog(@"JavaArgs::getCallJavaArgs: Cannot allocate memory for mainMethod.");
            goto _gcja_fail;
        }
        call_java_args->mainmethod = tmpstr;
    }

    tmpstr = (char*)[java_args.signature UTF8String];
    if (tmpstr != NULL && *tmpstr != '\0') {
        tmpstr = strndup(tmpstr, strlen(tmpstr));
        if (tmpstr == NULL) {
            NSLog(@"JavaArgs::getCallJavaArgs: Cannot allocate memory for signature.");
            goto _gcja_fail;
        }
        call_java_args->signature = tmpstr;
    }

    char **args = NULL;
    int index = 0;
    if (java_args.app_args != NULL) {
        args_len = [java_args.app_args count];
        if (args_len != 0) {
            args = (char**)calloc(args_len, sizeof(char*));
            if (args == NULL) {
                NSLog(@"JavaArgs::getCallJavaArgs: "
                  @"Cannot allocate memory for char** array for %d CallJavaArgs app_args.",
                      args_len);
                goto _gcja_fail;
            }
            call_java_args->app_args = args;
            call_java_args->app_args_len = args_len;
            for (NSString *arg in java_args.app_args) {
	        tmpstr = filterBundlePath(bundlePath, documentsPath,arg);
                if (tmpstr == NULL) {
                    goto _gcja_fail;
                }
                args[index++] = tmpstr;
            }
        }
    }
    return call_java_args;

_gcja_fail:
    jl_freeCallJavaArgs(call_java_args);

    return NULL;
}

@end

void
jl_freeCallJavaArgs(CallJavaArgs *call_java_args)
{

    free(call_java_args->mainclass);
    free(call_java_args->mainmethod);
    free(call_java_args->signature);
    int i = 0;
    for (i = 0; i < call_java_args->app_args_len; ++i) {
        free(call_java_args->app_args[i]);
    }
    free(call_java_args->app_args);
    free(call_java_args);
}

void
jl_freeCreateJavaArgs(CreateJavaArgs *java_args)
{
    int i;
    for (i = 0; i < java_args->jvm_args_len; ++i) {
        free(java_args->jvm_args[i]);
    }
    free(java_args->jvm_args);
    free(java_args);
}

/** Call this method to filter BUNDLEPATHFORMAT into the absolute path
 * to the root of the application bundle. Specify BUNDLEPATHFORMAT "%@DD
 * in strings in the JAVAARGS_PROPERTIES file. Note: a special
 * check is made for "%@DD/../Documents" due to directory layout changes 
 * in iOS 8.
 */
static char *filterBundlePath(NSString *bundlePath, NSString *documentsPath, NSString *value)
{
    char *utfstring = (char*)
        [[[value stringByReplacingOccurrencesOfString:@"%@DD/../Documents"
            withString:documentsPath] 
           stringByReplacingOccurrencesOfString:BUNDLEPATHFORMAT
            withString:bundlePath] UTF8String];
    return utfstring != NULL ?
        strndup(utfstring, strlen(utfstring)) : NULL;
}


static NSMutableDictionary *propvalueToDict(NSArray *values)
{
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    for (NSString *jprop in values) {
        if (jprop == NULL || [jprop length] == 0) {
            continue;
        }
        NSString *key = jprop;
        NSString *value = NULL;
        NSRange range = [jprop rangeOfString:@"="];
        if (range.location != NSNotFound) {
            key = [jprop substringToIndex: range.location];
            value = [jprop substringFromIndex: range.location + 1];
        }
        [dict setObject:(
            value == NULL ? (NSString*)[NSNull null] : value) forKey:key];
    }
    return dict;
}

static NSString *propertiesFileToString(NSString *propertiesfile)
{
    NSBundle *mainBundle = (bundle != NULL)? bundle : [NSBundle mainBundle];
    NSString *propfile =
        [mainBundle pathForResource:propertiesfile ofType:@"properties"];

    if (propfile == NULL) {
      propfile = [[JavaArgs documentsDirectory]
		   pathForResource:propertiesfile ofType:@"properties"];
      if (propfile == NULL) {
        NSLog(@"JavaArgs::propertiesFileToString: "
              "%@.properties not found", propertiesfile);
        return NULL;
      }
    }
    
    NSString *properties = [NSString stringWithContentsOfFile:propfile
        encoding:NSUTF8StringEncoding error:NULL];
    if (properties == NULL) {
        NSLog(@"JavaArgs::propertiesFileToString: "
              @"Cannot read properties file %@", propertiesfile);
        return NULL;
    }
    return [properties copy];
}

static NSArray *propertiesFileToArray(NSString *propertiesfile)
{
    NSString *properties = propertiesFileToString(propertiesfile);
    if (properties == NULL) {
        @throw @"Properties file cannot be read";
    }
    
    // componentsSeparateByCharacterSet results in containing a last
    // element that is empty because of the terminating '\n'.
    // Strip off terminating newlines
    // location is from the beginning of the string.
    NSRange nlrange = [properties rangeOfCharacterFromSet:
        [[NSCharacterSet newlineCharacterSet] invertedSet]
            options: NSBackwardsSearch];
    if (nlrange.location == NSNotFound) {
#ifdef DEBUG
        NSLog(@"JavaArgs::propertiesFileToArray: Only empty lines in "
              @"properties file %@", propertiesfile);
#endif
        // Nothing but newline characters
        [properties release];
        return NULL;
    }

    NSString *properties_nonl = [properties substringToIndex:nlrange.location + 1];
    
    NSArray *lines = [properties_nonl componentsSeparatedByCharactersInSet:
                      [NSCharacterSet newlineCharacterSet]];
    if (lines == NULL || [lines count] == 0) {
#ifdef DEBUG
        NSLog(@"JavaArgs::propertiesFileToArray: "
              @"Cannot create array from properties file string "
              @"from file %@", propertiesfile);
#endif
        [properties release];
        return NULL;
    }
    [properties release];
    return [lines retain];
}

#define ATEOL() (lineindex >= linelen)

// Reset the state variables to process a new line
#define RESETPARSESTATE() \
    isContinuation = false;\
    skipWhiteSpace = true;\
    precedingBackslash = false

#define ISWHITE(c) (c == ' ' || c == '\t' || c == '\f')
#define ISCOMMENT(c) (c == '#' || c == '!')

// return the first non comment or non blank line
// from linesindex
static NSString *readLine(NSArray *lines, int *linesindex)
{
    
    // Get a mutable string start off with 80 char capacity.
    NSMutableString *lineBuf = [NSMutableString stringWithCapacity:80];
    
    // The length of the line being parsed.
    int linelen = 0;
    // The index of the current character being parsed.
    int lineindex = 0;
    // The current character.
    unichar c = 0;
    // Set to true to trim leading white space
    // Set to false if parsing a continuation line.
    bool skipWhiteSpace = true;
    // Tracks occurrences of "\". Toggles if
    // escaping a "\"
    bool precedingBackslash = false;
    // Set to true if the last character is in a line is "\"
    // and is not being escaped.
    bool isContinuation = false;

    int currentline = *linesindex; 
    int linecount = [lines count];
    NSString *line = NULL;
    
    while (currentline < linecount) {
        
        // first time in the loop
        // continuing to skip comments or blank lines
        // appending a continuation line.

        // Get the continuation line, or the next line after a blank line
        // or comment
        line = [lines objectAtIndex:currentline];
        lineindex = 0;
        
        // Not sure what a NULL line would mean or if it is possible
        // But an empty line ends parsing.
        if (line != NULL && [line length] != 0) {
            linelen = [line length];
        } else {
            ++currentline;
            if (isContinuation) {
                // if it is a continuation line, a blank line ends
                // the continuation unless the current line is also empty.
                if ([lineBuf length] == 0) {
                    RESETPARSESTATE();
                    continue;
                }
                break;
            } else {
                // continue to eat comments and blank lines.
                continue;
            }
        }
        if (skipWhiteSpace) {
            while (!ATEOL()) {
                c = [line characterAtIndex:lineindex++];
                if (!ISWHITE(c)) {
                    skipWhiteSpace = false;
                    break;
                }
            }
            // See if it the line was all white space
            if (skipWhiteSpace && ISWHITE(c)) {
                RESETPARSESTATE();
                ++currentline;
                continue;
            }
        }
        
        // If not a continuation line, leading white space has been stripped
        // and we have a comment, eat the line and continue.
        if (!isContinuation && ISCOMMENT(c)) {
            RESETPARSESTATE();
            ++currentline;
            continue;
        }
        
        while (!ATEOL()) {
            if (c == '\\') {
                precedingBackslash = !precedingBackslash;
            } else {
                precedingBackslash = false;
            }
            [lineBuf appendFormat:@"%c",c];
            // Next character
            c = [line characterAtIndex:lineindex++];
        }
        // Need to check for terminating backslash
        if (c == '\\') {
            precedingBackslash = !precedingBackslash;
        }
        
        // Point to the next unparsed line.
        ++currentline;

        // If the last character is "\" and is not being
        // escaped precedingBackslash == false
        // its a continuation line, skip leading white space
        // and continue to parsee and append the next line
        if (precedingBackslash) {
            skipWhiteSpace = true;
            precedingBackslash = false;
            isContinuation = true;
            continue;
        }
        // Store the last character unless it is a line of all whitespace
        if (!skipWhiteSpace) {
            [lineBuf appendFormat:@"%c",c];
        }
        RESETPARSESTATE();
        
        // empty line ?
        if ([lineBuf length] == 0) {
            continue;
        }
        // Have a non empty and non comment line
        // stop parsing lines
        break;
    }
    // currentline points to the next unparsed line
    *linesindex = currentline;
    return [lineBuf length] == 0 ? NULL : [lineBuf copy];

}

// If line was obtained from readLine
// We don't have comments or leading white space and continuation lines
// have been concatenated.
// value is always an array even for singe value properties.
// If a property is a single value property, just use element 0.
// Never called with line NULL or empty.

static bool getJavaProperty(NSString *line, NSString **key, NSMutableArray **value)
{
    
    unsigned lineindex = 0;
    unsigned linelen = [line length];
    bool haveEscape = false;
    NSMutableString *propkey = [NSMutableString stringWithCapacity: 80];
    unichar c = '\0';

    while (lineindex < linelen) {
        c = [line characterAtIndex:lineindex++];
        // If haveEscape just save c
        if (haveEscape) {
            haveEscape = false;
        } else if (c == '\\') {
            haveEscape = true;
            // eat the first esacpe char.
            continue;
        } else if (c == '=' || c == ':') {
            // key has been parsed.
            break;
        } else if (ISWHITE(c)) {
            continue;
        }        
        [propkey appendFormat: @"%c", c];
    }
    
    // If there wasn't a key separator or there was no key
    // bail out, badly formatted file.
    if ((c != '=' && c != ':') || [propkey length] == 0) {
        NSLog(@"JavaArgs::getJavaProperty: missing key in line '%@",
              line);
        return false;
    }
    
    // skip white space after the '=' or ':'
    while (lineindex < linelen) {
        c = [line characterAtIndex:lineindex++];
        if (!ISWHITE(c)) {
            // get the character again, simplifies logic
            --lineindex;
            break;
        }
    }
    
    NSMutableString *propvalue = [NSMutableString stringWithCapacity:80];
    NSMutableArray *proparrayvalue = [[NSMutableArray alloc] initWithCapacity: 0];
    haveEscape = false;
    
    while (lineindex < linelen) {
        c = [line characterAtIndex:lineindex++];  
        // Save escaped characters
        if (haveEscape) {
            haveEscape = false;
        } else if (c == '\\') {
            // eat the first escape character
            haveEscape = true;
            continue;
        } else if (c == ' ' || c == '\t') {
            // have white space separator
            NSString * tmp = [propvalue copy];
            [proparrayvalue addObject: tmp];
            [tmp release];
            [propvalue setString:@""];
            // skip separator characters
            while (lineindex < linelen) {
                c = [line characterAtIndex:lineindex++];
                if (!ISWHITE(c)) {
                    // get the character again, simplifies logic
                    --lineindex;
                    break;
                }
            }
            continue;
        }
        [propvalue appendFormat:@"%c", c];              
    }
    // Need to add the last arg to arrayvalue
    if ([propvalue length] > 0) {
        NSString *tmp = [propvalue copy];
        [proparrayvalue addObject: tmp];
        [tmp release];
    }
    
    // If we are here we have a key but value
    // could be NULL.
    *key = [propkey copy];
    if ([proparrayvalue count] == 0) {
        [proparrayvalue release];
        proparrayvalue = NULL;
    }
    *value = proparrayvalue;
    return true;
}




















