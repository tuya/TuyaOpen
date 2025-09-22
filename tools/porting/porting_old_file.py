#!/usr/bin/python3
# -*- coding:utf-8 -*-

##
# @file gen_import_parse_file.py
# @brief Parse existing files to extract function names, function comments, function implementations, function return values, etc.
# @version 1.0.0
# @date 2022-04-10

import os
import sys
import re


# User-defined area start and end markers for additional headers, types, enums, macros, constants, global variables, internal functions, etc., as needed by developers
udf_flags_begin = "// --- BEGIN: user defines and implements ---\n"
udf_flags_end   = "// --- END: user defines and implements ---" 

# Function body start and end markers for additional developer-specific function implementations
func_body_flags_begin = "// --- BEGIN: user implements ---"
func_body_flags_end   = "// --- END: user implements ---"

class parse_old_file(object):
    def __init__(self, path) -> None:
        self.path = path

    def __load_udfs(self, body):
        includes = re.findall("%s(.+)%s"%(udf_flags_begin,udf_flags_end), body, flags=re.DOTALL)
        #print(includes)
        return includes

    def __paser_func_return(self, raw_return):
        tmp = re.sub("%s"%udf_flags_begin, "", raw_return)
        tmp = re.sub("%s"%udf_flags_end, "", tmp)
        tmp = re.sub("\n*", "", tmp)
        tmp = re.sub("\s*", "", tmp)
        return tmp

    def __load_funcs(self, body):
        funcs = []
        #body = re.sub("\/\*\*.+?file.+?\*\/", "", body, flags=re.DOTALL) # Remove function comments
        #body = re.sub("\/\*\*.*?\*\/", "", body, flags=re.DOTALL) # Remove function comments
        body = re.sub("%s.+%s"%(udf_flags_begin,udf_flags_end), "", body, flags=re.DOTALL) # Remove udf area
        
        #print(body)

        # Extract functions
        funcs_list = re.split("\/\*\*.+?\*\/", body, flags=re.DOTALL)
        #funcs_list = re.findall("\/\*\*.+?\*\/.*?\(.*?\).*?\{.*?\}", body, flags=re.DOTALL)
        #print(funcs_list)
        for func in funcs_list:
            #print("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
            #print(func)

            # Some files may not have function definitions, so they need to be ignored
            hastkl = re.findall("tkl", func)
            if not hastkl:
                continue
            

            fhead = re.findall("(.+?tkl.+?\(.+?\))", func, flags=re.DOTALL)
            freturn = re.findall("(.+?\**?)tkl.+?\(.+?\)", func, flags=re.DOTALL) # When processing the return value, consider inconsistencies in writing styles, such as VOID_T* func and VOID_T *func
            fname = re.findall(".+?(tkl.+?)\s*\(.+\)", func, flags=re.DOTALL)
            fname = re.sub(" ", "", fname[0])
            fname = re.sub("\t", "", fname)
            fbody = re.findall("%s.+?%s"%(func_body_flags_begin,func_body_flags_end), func, flags=re.DOTALL)
            if not fbody:
                #print("xxxxxxxxxxxxxxxxxxxxxxxxxxxx")
                fbody = re.findall("{(.*?)}", func, flags=re.DOTALL)

            #print(fname)
            #print(fhead)
            #print(fbody)
            #print(freturn[0])
            #print("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")
            f = {}
            f['return'] = self.__paser_func_return(freturn[0])
            f['name'] = fname
            f['head'] = fhead[0]
            f['body'] = fbody[0]
            funcs.append(f)

        return funcs                    
        
    def __load_x_udfs(self, body):
        udfs = self.__load_udfs(body) # Try to parse, handle if not parsed
        if not udfs:
            pass
        
        return udfs

    def __load_x_funcs(self, body):
        funcs = self.__load_funcs(body) # Try to parse, handle if not parsed
        if not funcs:
            pass

        return funcs
    
    def load_file(self):
        if not os.path.exists(self.path):
            return 

        file = {}
        file['funcs'] = {}
        print(self.path)
        c = open(self.path, encoding='utf-8')
        s = c.read()

        #print(s)
        # For non-automatically generated files developed independently, try to load the content to reconstruct an automatically generated file
        udfs = self.__load_x_udfs(s)
        funcs = self.__load_x_funcs(s)

        file['udfs'] = udfs
        file['funcs'] = funcs

        # debug, view file function information
        #print(file)        
        return file
    
