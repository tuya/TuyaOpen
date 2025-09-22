#!/usr/bin/python3
# -*- coding:utf-8 -*-

##
# @file gen_import_file.py
# @brief Generate import file related information, including file header, file functions, file type definitions, etc.
# @version 1.0.0
# @date 2022-04-10
import re

class parse_new_file(object):
    def __init__(self, path) -> None:
        self.path = path
    
    def __paser_func_return(self, raw_return):
        tmp = re.sub("\s*", "", raw_return)
        return tmp

    def load_file(self):
        file = {}
        file['funcs'] = []
        c = open(self.path, encoding="utf-8")
        s = c.read()
        body = re.findall("#ifdef.+cplusplus.+extern.+#endif(.+)#ifdef.+__cplusplus", s, flags=re.DOTALL)
        if body:
            funcs = body[0]
            #print(funcs)

            # Exclude some interference information to extract API information
            funcs = re.sub("\/\*\*.+?\*\/.+?typedef.+?\(.*?\);", "", funcs, flags=re.DOTALL) # Function pointer
            funcs = re.sub("typedef.+?;", "", funcs) # Type redefinition
            funcs = re.sub("typedef.+{.+}.+?;", "", funcs, flags=re.DOTALL) # Enum, structure definition
            funcs = re.sub("#include.+\".+\"", "", funcs) # Internal header file
            funcs = re.sub("#include.+<.+>", "", funcs) # Library header file
            funcs = re.sub("\/\*\*.+?\*\/.+?#define.+?TKL_ENTER_CRITICAL.+?tkl_system_enter_critical\(.*?\)", "", funcs, flags=re.DOTALL) # Note, this line is only for handling TKL_ENTER_CRITICAL, TKL_EXIT_CRITICAL macros in system
            funcs = re.sub("\/\*\*.+?\*\/.+?#define.+?TKL_EXIT_CRITICAL.+?tkl_system_exit_critical\(.*?\)", "", funcs, flags=re.DOTALL) # Note, this line is only for handling TKL_ENTER_CRITICAL, TKL_EXIT_CRITICAL macros in system
            funcs = re.sub("#define.+\n", "", funcs) # Single-line macro without comments

            # Extract API
            funcs_list = re.findall("\/\*\*.+?\*\/.+?\(.*?\);", funcs, flags=re.DOTALL)

            # Construct API
            if funcs_list:
                for f in funcs_list:
                    #print("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")
                    #print(f)
                    fname = re.findall("\*\/.+?(tkl.+)\s*\(.+?\);", f, flags=re.DOTALL)
                    fname = re.sub(" ", "", fname[0])
                    fname = re.sub("\t", "", fname)
                    rt = re.findall("\*\/\s*\n(.+?\**?)tkl", f, flags=re.DOTALL)
                    func = {}
                    func['return'] = self.__paser_func_return(rt[0])
                    func['name'] = fname
                    func['head'] = re.sub("\);", ")", f)
                    func['isnew'] = True
                    file['funcs'].append(func)
                    #print(func['name'])
                    #print(func['head'])
                    #print("yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy")
            # Debug, view file function information
            #print(file)
            file['udfs'] = []
            return file

if __name__ == "__main__":
    gat = autotest_generate_file()
