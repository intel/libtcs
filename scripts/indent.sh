# -----------------------------------------------------------------------------
# File: indent.sh
# -----------------------------------------------------------------------------
# Description:
# The aim of this script is to automatically indent the libtcs
# This script uses the gnu uncrustify tool.
# -----------------------------------------------------------------------------
#
# Copyright (C) Intel 2013
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/bin/bash

function indent_c_code()
{
    local src_list=$(find $PWD -type f -name "*.[ch]")
    [ "$src_list" != "" ] && $PWD/scripts/uncrustify-0.60 --replace --no-backup -c $PWD/scripts/coding_rules.cfg $src_list
}

function android_mk_remove_tabs()
{
    local mk_files=$(find $PWD -name Android.mk)
    [ "$mk_files" != "" ] && sed -i 's:\t:    :g' $mk_files
}

function xml_remove_spaces()
{
    local xml_files=$(find $PWD -name "*.xml")
    [ "$xml_files" != "" ] && sed -i 's: \{4,4\}:\t:g' $xml_files
}

if [ ! -d .git ]; then
    echo "To allow code parsing, you should run this script from your project"\
        "root folder"
    exit 1
fi

indent_c_code
android_mk_remove_tabs
xml_remove_spaces
