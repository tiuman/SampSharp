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

#include <amx/amx.h>

#pragma once

// loads the bridge. returns false if the bridge could not be loaded. the plugin
// should be terminated at this point.
bool bridge_load(const char *assembly_dir,
    const char *config_dir,
    const char *bridge_dir,
    const char *trace_level,
    const char *debugger_address);

// unloads the server. returns false if the server could not be unloaded.
bool bridge_unload();

// returns true if the server has been loaded.
bool bridge_loaded();

// handles a tick.
void bridge_tick();

// handles a public call.
void bridge_call(AMX *amx, const char *name, cell *params, cell *retval);
