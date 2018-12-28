#!/usr/bin/python

# Copyright (c) 2018 Blockchain B.V.
#
# The MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

#################################################################################
# This script is called everytime cmake is called. It generates code to certain #
# files which are related to the creation of plugins.                           #
# The working directory of the script is the project root directory.            #
# Script is called from libraries/utilities/CMakeLists.txt                      #
#################################################################################


import json 
import os


# list of files where generated code is used (relative to project root)
# libraries/app/include/graphene/api.hpp
# libraries/app/api.cpp
# libraries/app/application.cpp

generated_files_root_dir = "libraries/utilities/include/graphene/utilities/"

plugin_root_dir = "libraries/plugins/"

generated_code_warning = "\
/*****************************************/\n\
/* This code is generated automatically. */\n\
/* To generate new plugin code create a  */\n\
/* plugin.json file in a subdirectory    */\n\
/* of the plugin root directory.         */\n\
/*****************************************/\n\
\n\
\n"

# this is a dictionary of the generated filename and the generated code
# variables in the code are replaced by the variables from the plugin.json file
templates = {
"generated_api_cpp_case_code.hpp":
"else if( api_name == \"{plugin_name}_api\" ) {{ \n\
    _{plugin_name}_api = std::make_shared< {plugin_name}_api >( std::ref(_app) );\n\
}} \n",

"generated_api_cpp_login_api_code.hpp":
"fc::api<graphene::{plugin_namespace}::{plugin_name}_api> login_api::{plugin_name}() const \n\
{{ \n\
    FC_ASSERT(_{plugin_name}_api); \n\
    return *_{plugin_name}_api; \n\
}}\n\n",

"generated_api_hpp_include_code.hpp":
"#include <graphene/{plugin_name}/{plugin_name}_api.hpp> \n",

"generated_api_hpp_login_api_method_def_code.hpp":
"/// @brief Retrieve the {plugin_name} API \n\
fc::api<graphene::{plugin_namespace}::{plugin_name}_api> {plugin_name}()const;\n",

"generated_api_hpp_login_api_apiobj_code.hpp":
"optional< fc::api<graphene::{plugin_namespace}::{plugin_name}_api> > _{plugin_name}_api;\n",

"generated_api_hpp_reflect_code.hpp":
"({plugin_name})\n",

"generated_application_cpp_allowed_apis_code.hpp":
"wild_access.allowed_apis.push_back( \"{plugin_name}_api\" );\n",

}


# searches for all plugin.json files in the plugin directory
# parses them and returns a list of dictionarys with the parsed data
def get_all_plugin_data():
    
    # holds content of all plugin.json files as dictionary
    plugin_data_list = []
    # traverse plugin root dir search for plugin.json files
    for root, dirs, files in os.walk( "./" + plugin_root_dir ):
        for file in files:
            if file == "plugin.json":
                with open( os.path.join( root, file ) ) as f:
                    parsed_data = json.load( f )
                    data_dic = dict()
                    data_dic['plugin_name']      = parsed_data['plugin_name']
                    data_dic['plugin_namespace'] = parsed_data['plugin_namespace']
                    data_dic['plugin_project']   = parsed_data['plugin_project']
                    plugin_data_list.append( data_dic )                 
    
    return plugin_data_list


def generate_all_files( plugin_data_list ):
    print "--"
    # t_fn = template_filename, t_con = template_content
    for t_fn, t_con in templates.items():
        f_dir = os.path.join( generated_files_root_dir, t_fn )
        with open( f_dir, 'w') as f:
            f.write( generated_code_warning )
            # p_data = plugin_data
            for p_data in plugin_data_list:
                # **p_data = values of the dic
                content = t_con.format( **p_data ) 
                f.write( content )
            print( "-- plugin_code_generator: " + t_fn + " successfully generated" )


def main():

    plugin_data_list = get_all_plugin_data()

    generate_all_files( plugin_data_list )
 
    print "-- plugin_code_generator: plugin code generation complete\n--"


if __name__ == "__main__":
    main()


