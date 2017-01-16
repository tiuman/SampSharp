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

#include "platforms.h"
#include "translator.h"
#include <assert.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <sampgdk/sampgdk.h>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/mono-debug.h>
#include <mono/utils/mono-logger.h>

// must load bridge.h late, because it will redefine int32_t.
#include "bridge.h"


#define MAX_NATIVE_ARGS                     (32)
#define MAX_CALLBACK_PARAM_COUNT            (16)
#define BRIDGE_NAMESPACE                    "SampSharp.Bridge"
#define BRIDGE_CLASS                        "Bridge"
#define BRIDGE_CLASS_FULL                   BRIDGE_NAMESPACE "." BRIDGE_CLASS
// todo: mono_free strings


bool loaded = false;
MonoDomain* domain_root;
MonoClass* bridge_class;
MonoObject* bridge_instance;
int32_t bridge_handle;

std::vector<AMX_NATIVE> bridge_natives;
std::map<std::string, int> bridge_natives_map;

MonoMethod* bridge_method_tick;
MonoMethod* bridge_method_call_data;
MonoMethod* bridge_method_call;
MonoMethod* bridge_method_exception;
MonoString* string_empty;

void bridge_load_icall(const char *name, const void *method);
MonoObject *bridge_invoke_method(MonoMethod* method, void **params);
void bridge_log_exception(MonoObject* exception);

void bridge_icall_print(MonoString* str);
void bridge_icall_set_codepage(MonoString* name);
int bridge_icall_get_native(MonoString* name);
int bridge_icall_invoke_native(int id, MonoString *native_format,
    MonoString *args_format, MonoArray *args_array, MonoArray* sizes_array);
int bridge_raise_invalid_operation(const char *msg);

bool bridge_load(const char *assembly_dir, const char *config_dir,
    const char *bridge_dir,
    const char *trace_level,
    const char *debugger_address) {
    bool waiting_for_debugger = false;
    size_t len;
    char
        *bridge_path,
        *bridge_config_path;

    // do not load if the bridge has already been loaded.
    if (bridge_loaded()) {
        return false;
    }

    // set mono directories if an assembly and config directory have been
    // specified.
    if (assembly_dir && config_dir) {
        mono_set_dirs(assembly_dir, config_dir);
    }

    // parse jit options if a soft debugger is being attached.
    if (debugger_address) {
        sampgdk_logprintf("Soft Debugger");
        sampgdk_logprintf("-------------");

        char* agent = new char[128];
        snprintf(agent, 128,
            "--debugger-agent=transport=dt_socket,address=%s,server=y",
            debugger_address);

        sampgdk_logprintf("Launching debugger at %s...", debugger_address);

        const char* jit_options[] = {
            "--soft-breakpoints",
            agent
        };

        mono_jit_parse_options(2, (char**)jit_options);

        delete agent;

        sampgdk::logprintf("Waiting for debugger to attach...");
        waiting_for_debugger = true;

        sampgdk_logprintf("");
    }


    sampgdk_logprintf("SampSharp Bridge");
    sampgdk_logprintf("----------------");

    len = strlen(bridge_dir);
    bridge_path = new char[len + 21];
    bridge_config_path = new char[len + 28];

    snprintf(bridge_path, len + 21, "%s" BRIDGE_NAMESPACE ".dll", bridge_dir);
    snprintf(bridge_config_path, len + 28, "%s.config", bridge_path);

    // initialize mono.
    mono_debug_init(MONO_DEBUG_FORMAT_MONO);
    mono_trace_set_level_string(trace_level);

    domain_root = mono_jit_init(bridge_path);
    mono_domain_set_config(domain_root, bridge_dir, bridge_config_path);

    if (waiting_for_debugger) {
        sampgdk_logprintf("Debugger attached!");
        sampgdk_logprintf("");
    }

    sampgdk_logprintf("Loading bridge assembly...");
    MonoAssembly* bridge_assembly = mono_domain_assembly_open(domain_root,
        bridge_path);
    if (!bridge_assembly) {
        sampgdk_logprintf("Failed to load " BRIDGE_NAMESPACE " assembly!");
        return false;
    }

    sampgdk_logprintf("Loading bridge image...");
    MonoImage* bridge_image = mono_assembly_get_image(bridge_assembly);
    if (!bridge_image) {
        sampgdk_logprintf("Failed to load " BRIDGE_NAMESPACE " image!");
        return false;
    }

    sampgdk_logprintf("Loading bridge class...");
    bridge_class = mono_class_from_name(bridge_image,
        BRIDGE_NAMESPACE, BRIDGE_CLASS);
    if (!bridge_class) {
        sampgdk_logprintf("Failed to load " BRIDGE_NAMESPACE "::" BRIDGE_CLASS
            " class!");
        return false;
    }

    bridge_method_tick = mono_class_get_method_from_name(bridge_class,
        "Tick", 0);
    bridge_method_call = mono_class_get_method_from_name(bridge_class,
        "ProcessPublicCall", 2);
    bridge_method_call_data = mono_class_get_method_from_name(bridge_class,
        "GetPublicCallData", 4);

    if (!bridge_method_tick) {
        sampgdk_logprintf("Bridge is missing Tick method!");
        return false;
    }

    if (!bridge_method_call_data) {
        sampgdk_logprintf("Bridge is missing ProcessPublicCall method!");
        return false;
    }

    if (!bridge_method_call) {
        sampgdk_logprintf("Bridge is missing GetPublicCallData method!");
        return false;
    }

    bridge_load_icall("Print", (void *)bridge_icall_print);
    bridge_load_icall("SetCodepage", (void *)bridge_icall_set_codepage);
    bridge_load_icall("GetNative", (void *)bridge_icall_get_native);
    bridge_load_icall("InvokeNative", (void *)bridge_icall_invoke_native);

    sampgdk_logprintf("Loading bridge instance...");
    bridge_instance = mono_object_new(domain_root, bridge_class);
    if (!bridge_instance) {
        sampgdk_logprintf("Failed to create an instance of "
            BRIDGE_NAMESPACE "::" BRIDGE_CLASS " class!");
        return false;
    }

    bridge_handle = mono_gchandle_new(bridge_instance, false);
    assert(bridge_handle);

    mono_runtime_object_init(bridge_instance);

    loaded = true;

    if (!string_empty) {
        string_empty = mono_string_new(domain_root, "");
    }

    sampgdk_logprintf("");
    return true;
}

bool bridge_unload() {
    return true;
}

bool bridge_loaded() {
    return loaded;
}

void bridge_tick() {
    if (!bridge_loaded()) {
        return;
    }

    bridge_invoke_method(bridge_method_tick, NULL);
}

void bridge_call(AMX *amx, const char *name, cell *params, cell *retval) {
    int len;
    uint32_t len_idx = 0;
    cell *addr = NULL;
    MonoArray *arr;
    MonoArray *args;
    MonoArray *sizes;
    MonoString *format;
    void *data_args[4];

    if (!bridge_loaded()) {
        return;
    }

    assert(name);

    // OnRconCommand is processed on different thread.
    mono_thread_attach(domain_root);

    MonoString* name_m = translate_to_mono_string((char *)name, strlen(name));
    // todo: free name_m?

    MonoString **formatp = NULL;
    MonoArray **sizesp = NULL;
    data_args[0] = name_m;
    data_args[1] = params;
    data_args[2] = &formatp;
    data_args[3] = &sizesp;

    MonoObject* res = bridge_invoke_method(bridge_method_call_data, data_args);
    if (!res || !*(bool *)mono_object_unbox(res)) {
        return;
    }

    format = formatp ? *formatp : NULL;
    sizes = sizesp ? *sizesp : NULL;

    args = mono_array_new(domain_root, mono_get_object_class(), *params);
    char *format_c = mono_string_to_utf8(format);
    for (int i = 0; i < params[0]; i++) {
        switch (format_c[i]) {
        case 'd':
            mono_array_set(args, MonoObject*, i, mono_value_box(domain_root,
                mono_get_int32_class(), &params[i + 1]));
            break;
        case 'f':
            mono_array_set(args, MonoObject*, i, mono_value_box(domain_root,
                mono_get_single_class(), &params[i + 1]));
            break;
        case 'b':
            mono_array_set(args, MonoObject*, i, mono_value_box(domain_root,
                mono_get_boolean_class(), &params[i + 1]));
            break;
        case 's':
            amx_GetAddr(amx, params[i + 1], &addr);
            amx_StrLen(addr, &len);

            if (len) {
                len++;

                char* text = new char[len];

                amx_GetString(text, addr, 0, len);
                mono_array_set(args, MonoObject*, i,
                    (MonoObject *)translate_to_mono_string(text, len));
            }
            else {
                mono_array_set(args, MonoObject*, i,
                    (MonoObject *)string_empty);
            }
            break;

        case 'D':
            if (!sizes || mono_array_length(sizes) <= len_idx) {//sizes
                sampgdk_logprintf("[SampSharp] ERROR: No sizes supplied for "
                    "%s.", name);
                return;
            }

            len = mono_array_get(sizes, int, len_idx++);
            arr = mono_array_new(mono_domain_get(), mono_get_int32_class(),
                len);

            if (len > 0) {
                cell* addr = NULL;
                amx_GetAddr(amx, params[i + 1], &addr);

                for (int i = 0; i < len; i++) {
                    mono_array_set(arr, int, i, *(addr + i));
                }
            }
            mono_array_set(args, MonoObject*, i, (MonoObject *)arr);
            break;
        case 'F':
            if (!sizes || mono_array_length(sizes) <= len_idx) {//sizes
                sampgdk_logprintf("[SampSharp] ERROR: No sizes supplied for "
                    "%s.", name);
                return;
            }

            len = mono_array_get(sizes, int, len_idx++);
            arr = mono_array_new(mono_domain_get(), mono_get_int32_class(),
                len);

            if (len > 0) {
                cell* addr = NULL;
                amx_GetAddr(amx, params[i + 1], &addr);

                for (int i = 0; i < len; i++) {
                    mono_array_set(arr, float, i, amx_ctof(*(addr + i)));
                }
            }
            mono_array_set(args, MonoObject*, i, (MonoObject *)arr);
            break;

        case 'B':
            if (!sizes || mono_array_length(sizes) <= len_idx) {//sizes
                sampgdk_logprintf("[SampSharp] ERROR: No sizes supplied for "
                    "%s.", name);
                return;
            }

            len = mono_array_get(sizes, int, len_idx++);
            arr = mono_array_new(mono_domain_get(), mono_get_int32_class(),
                len);

            if (len > 0) {
                cell* addr = NULL;
                amx_GetAddr(amx, params[i + 1], &addr);

                for (int i = 0; i < len; i++) {
                    mono_array_set(arr, bool, i, !!*(addr + i));
                }
            }
            mono_array_set(args, MonoObject*, i, (MonoObject *)arr);
            break;

        default:
            sampgdk_logprintf("[SampSharp] ERROR: Signature of %s contains "
                "unsupported parameters.", name);
            return;
        }
    }

    mono_free(format_c);

    data_args[1] = &args;
    res = bridge_invoke_method(bridge_method_call, data_args);

    if (retval && res) {
        *retval = (cell)*(int *)mono_object_unbox(res);
    }
}

int bridge_raise_invalid_operation(const char *msg) {
    mono_raise_exception(mono_get_exception_invalid_operation(msg));

    return -1;
}

void bridge_load_icall(const char *name, const void *method) {
    // Construct combination of 'namespace::method'.
    size_t len = strlen(BRIDGE_CLASS_FULL) + 2 /* :: */ + strlen(name) + 1;
    char * call = new char[len];
    snprintf(call, len, BRIDGE_CLASS_FULL "::%s", name);

    mono_add_internal_call(call, method);

    delete[] call;
}

MonoObject* bridge_invoke_method(MonoMethod* method, void **params) {
    assert(method);

    MonoObject *exception;
    MonoObject *response = mono_runtime_invoke(method,
        mono_gchandle_get_target(bridge_handle), params, &exception);

    if (exception) {
        bridge_log_exception(exception);
    }

    return response;
}

void bridge_log_exception(MonoObject* exception) {
    char *stacktrace = mono_string_to_utf8(
        mono_object_to_string(exception, NULL));

    /* Print error to console. Cannot print the exception to logprintf; the
    * buffer is too small.
    */
    std::cout << "[SampSharp] Unhandled exception:" << stacktrace << std::endl;

    // Append error to log file.
    time_t now = time(0);
    tm _Tm;

#if SAMPSHARP_WINDOWS
    localtime_s(&_Tm, &now);
#elif SAMPSHARP_LINUX
    _Tm = *localtime_r(&now, &_Tm);
#endif

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%d/%m/%Y %H:%M:%S]", &_Tm);

    std::ofstream logfile;
    logfile.open("SampSharp_errors.log", std::ios::app | std::ios::binary);
    logfile << timestamp << " Unhandled exception:\n" << stacktrace << "\n";
    logfile.close();

    mono_free(stacktrace);
}

void bridge_icall_print(MonoString* str) {
    char *buffer = translate_from_mono_string(str);
    sampgdk_logprintf("%s", buffer);
    delete[] buffer;
}

void bridge_icall_set_codepage(MonoString* name) {
    char *cname = mono_string_to_utf8(name);
    if (!translate_load_file(cname)) {
        mono_raise_exception(mono_get_exception_file_not_found(name));
    }

    mono_free(cname);
}

int bridge_icall_get_native(MonoString* name) {
    char *cname = mono_string_to_utf8(name);
    std::string stdname = std::string(cname);
    mono_free(cname);

    std::map<std::string,int>::iterator it = bridge_natives_map.find(stdname);
    if (it != bridge_natives_map.end()) {
        return it->second;
    }

    AMX_NATIVE native = sampgdk_FindNative(cname);

    if (!native) {
        return bridge_raise_invalid_operation("native not found");
    }

    int id = bridge_natives.size();
    bridge_natives.push_back(native);
    bridge_natives_map[stdname] = id;

    return id;
}

int bridge_icall_invoke_native(int id, MonoString *native_format,
    MonoString *args_format, MonoArray *args_array, MonoArray* sizes_array) {
    uint32_t size_idx = 0;

    if (args_format && (!args_array ||
        mono_string_length(args_format) != mono_array_length(args_array))) {
        return bridge_raise_invalid_operation("invalid arguments");
    }

    if (id < 0 || (size_t)id >= bridge_natives.size()) {
        return bridge_raise_invalid_operation("invalid id");
    }

    AMX_NATIVE native = bridge_natives[id];

    char *args_format_c = args_format
        ? mono_string_to_utf8(args_format)
        : NULL;
    char *native_format_c = native_format
        ? mono_string_to_utf8(native_format)
        : NULL;

    char eos = '\0';
    if (!native_format_c) {
        native_format_c = &eos;
    }

    int len = !args_format_c ? 0 : strlen(args_format_c);
    void *params[MAX_NATIVE_ARGS];
    cell param_value[MAX_NATIVE_ARGS];
    int param_size[MAX_NATIVE_ARGS];

    for (int i = 0; i < len; i++) {
        MonoObject* obj = mono_array_get(args_array, MonoObject *, i);

        switch (args_format_c[i]) {
        case 'd': // integer
            params[i] = mono_object_unbox(obj);
            break;
        case 's': { // const string
            params[i] = translate_from_mono_string((MonoString*)obj);
            break;
        }
        case 'a': { // array of integers
            if (!sizes_array || mono_array_length(sizes_array) <= size_idx) {
                if (args_format) {
                    mono_free(args_format_c);
                }
                if (native_format) {
                    mono_free(native_format_c);
                }

                return bridge_raise_invalid_operation(
                    "value size not supplied");
            }

            MonoArray *values_array = (MonoArray*)obj;
            int sz = mono_array_get(sizes_array, int, size_idx++);
            param_size[i] = sz;

            cell *value = new cell[param_size[i]];
            for (int j = 0; j < param_size[i]; j++) {
                value[j] = mono_array_get(values_array, int, j);
            }
            params[i] = value;
            break;
        }
        case 'D': { // integer reference
            params[i] = obj
                ? *(int **)mono_object_unbox(obj)
                : &param_value[i];
            break;
        }
        case 'S': { // non-const string (writeable)
            int sz = mono_array_get(sizes_array, int, size_idx++);
            param_size[i] = sz;

            char *str = new char[param_size[i] + 1];
            str[0] = '\0';
            params[i] = str;
            break;
        }
        case 'A': { // array of integers reference
            int sz = mono_array_get(sizes_array, int, size_idx++);
            param_size[i] = sz;

            cell *value = new cell[param_size[i]];
            for (int j = 0; j < param_size[i]; j++) {
                // Set default value to int.MinValue
                value[j] = std::numeric_limits<int>::min();
            }
            params[i] = value;
            break;
        }
        default:
            return bridge_raise_invalid_operation("invalid format type");
        }
    }

    int return_value = sampgdk_InvokeNativeArray(native, native_format_c,
        params);

    for (int i = 0; i < len; i++) {
        switch (args_format_c[i]) {
        case 's': // const string
        case 'a': // array of integers
            delete[] params[i];
            break;
        case 'D': { // integer reference
            int result = *(int *)params[i];
            MonoObject *obj = mono_value_box(mono_domain_get(),
                mono_get_int32_class(), &result);
            mono_array_set(args_array, MonoObject*, i, obj);
            break;
        }
        case 'S': { // non-const string (writeable)
            MonoString *str = translate_to_mono_string((char *)params[i],
                param_size[i]);
            mono_array_set(args_array, MonoString *, i, str);
            delete[] params[i];
            break;
        }
        case 'A': { // array of integers reference
            cell *param_array = (cell *)params[i];
            MonoArray *arr = mono_array_new(mono_domain_get(),
                mono_get_int32_class(), param_size[i]);
            for (int j = 0; j < param_size[i]; j++) {
                mono_array_set(arr, int, j, param_array[j]);
            }
            mono_array_set(args_array, MonoArray *, i, arr);

            delete[] params[i];
            break;
        }
        }
    }

    if (args_format) {
        mono_free(args_format_c);
    }
    if (native_format) {
        mono_free(native_format_c);
    }

    return return_value;
}
