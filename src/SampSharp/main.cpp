// SampSharp
// Copyright 2017 Tim Potze
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "main.h"
#include "PathUtil.h"
#include "StringUtil.h"
#include "environment.h"
#include "ConfigReader.h"
#include <string>
#include <sstream>
#include <string.h>
#include <iostream>
#include <sampgdk/sampgdk.h>

// must load bridge.h late, because it will redefine int32_t.
#include "bridge.h"

extern void *pAMXFunctions;

using std::string;
using std::ostringstream;
using sampgdk::logprintf;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return sampgdk::Supports() | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    // load sampgdk. if loading fails, prevent the whole plugin from loading.
    if (!sampgdk::Load(ppData)) {
        return false;
    }

    // store amx functions pointer in order to be able to interface with amx.
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];

    logprintf("");
    logprintf("SampSharp Plugin");
    logprintf("----------------");
    logprintf("v%s, (C)2014-2017 Tim Potze", PLUGIN_VERSION);
    logprintf("");

    ConfigReader server_cfg("server.cfg");

    // validate loaded script gamemodes.
    for (int i = 0; i <= 15; i++) {
        ostringstream gamemode_key;
        gamemode_key << "gamemode";
        gamemode_key << i;

        string gamemode_value;
        server_cfg.GetOptionAsString(gamemode_key.str(), gamemode_value);
        gamemode_value = StringUtil::TrimString(gamemode_value);

        if ((i == 0 && gamemode_value.compare("empty 1") != 0) ||
            (i > 0 && gamemode_value.length() > 0)) {
            logprintf("ERROR: Can not load sampsharp if a non-SampSharp"
                "gamemode is set to load.");
            logprintf("ERROR: Please ensure you only specify one script"
                "gamemode, namely 'gamemode0 empty 1' in your server.cfg file.");
            // during development, the server crashed while returning false
            // here. seeing the user will need to change their configuration
            // file, it doesn't really matter.
            return false;
        }
    }

    // load configuration from server.cfg.
    string
        assembly_dir,
        config_dir,
        trace_level = "error",
        bridge_dir,
        debugger,
        debugger_address;

    bridge_dir = PathUtil::GetPathInBin("gamemode/");
    server_cfg.GetOptionAsString("trace_level", trace_level);
    server_cfg.GetOptionAsString("mono_assembly_dir", assembly_dir);
    server_cfg.GetOptionAsString("mono_config_dir", config_dir);
    server_cfg.GetOptionAsString("debugger", debugger);
    server_cfg.GetOptionAsString("debugger_address", debugger_address);

    if (debugger.compare("1") != 0) {
        debugger_address = "";
    }

    char *env_debugger = environment_get("debugger_address");
    if (env_debugger) {
        debugger_address = string(env_debugger);
    }

    // when on windows and no assembly_dir or config_dir has been loaded, use the default.
#if SAMPSHARP_WINDOWS
    if (assembly_dir.empty()) {
        assembly_dir = PathUtil::GetLibDirectory();
    }
    if (config_dir.empty()) {
        config_dir = PathUtil::GetConfigDirectory();
    }
#endif

    bridge_load(
        assembly_dir.empty() ? NULL : assembly_dir.c_str(),
        config_dir.empty() ? NULL : config_dir.c_str(),
        bridge_dir.empty() ? NULL : bridge_dir.c_str(),
        trace_level.empty() ? NULL : trace_level.c_str(),
        debugger_address.empty() ? NULL : debugger_address.c_str());

    // resume server log format.
    logprintf("Server Plugins");
    logprintf("--------------");

    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    if (bridge_loaded()) {
        bridge_unload();
    }
    sampgdk::Unload();
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
    if (bridge_loaded()) {
        bridge_tick();
    }
    sampgdk::ProcessTick();
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPublicCall(AMX *amx, const char *name,
    cell *params, cell *retval) {
    if (bridge_loaded()) {
        bridge_call(amx, name, params, retval);
    }
    return true;
}
