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

#include <mono/jit/jit.h>

#pragma once

// translate a char array to a mono string using the active codepage.
MonoString *translate_to_mono_string(char *str, int len);

// translate a mono string to a char array using the active codepage.
char *translate_from_mono_string(MonoString *str);

// load a translation from the /codepages directory.
bool translate_load_file(char *name);
