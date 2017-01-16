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

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace SampSharp.Bridge
{
    public class Bridge
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int SetTimer(int interval, bool repeat, object args);//todo

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool KillTimer(int timerid);//todo

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Print(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetCodepage(string codepage);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int GetNative(string name);
        
        // nativeFormat:
        // i int            integer value
        // d int            integer value(same as 'i')
        // b bool           boolean value
        // f double         floating-point value
        // r const cell*    const reference (input only)
        // R cell *	        non-const reference (both input and output)
        // s const char*    const string (input only)
        // S char*          non-const string (both input and output)
        // a const cell*    const array (input only)
        // A cell *	        non-const array (both input and output)
        // 
        // For the 'S', 'a' and 'A' specifiers you have to specify the size of the string/array in square brackets, e.g. "a[100]" (fixed size) or s[*2] (size passed via 2nd argument).
        // 
        // In Pawn, variadic functions always take their variable arguments (those represented by "...") by reference. This means that for such functions you have to use the 'r' specifier where you would normally use 'b', 'i' 'd' or 'f'.
        //
        //
        // argsFormat:
        // d int
        // s string
        // a int[]
        // D ref int
        // S ref string
        // A  ref int[]
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int InvokeNative(int id, string nativeFormat, string argsFormat, object[] args, int[] sizes);
        
        public void Tick()
        {
        }

        public void Test()
        {
            int isPlayerConnected = GetNative("IsPlayerConnected");
     
            Console.WriteLine("IsPlayerConnected handle: " + isPlayerConnected);

            {
                var ns = GetNative("GetNetworkStats");
                var args = new object[] { null, 200 };
                InvokeNative(ns, "S[200]d", "Sd", args, new[] { 200 });
                Console.WriteLine(args[0]);
            }
            {
                var ns = GetNative("SendRconCommand");
                var args = new object[] { "loadfs empty" };
                InvokeNative(ns, "s", "s", args, null);
            }
        }

        private readonly Dictionary<string, string> _publicCalls = new Dictionary<string, string>() {
            ["OnGameModeInit"] = null,
            ["OnGameModeExit"] = null,
            ["OnRconCommand"] = "s",
        };

        private readonly Dictionary<string, int[]> _publicCalls2 = new Dictionary<string, int[]>() {
            ["OnGameModeInit"] = null,
            ["OnGameModeExit"] = null,
            ["OnRconCommand"] = null,
        };

        public bool GetPublicCallData(string name, int paramCount, out string format, out int[] counts)
        {
            counts = null;
            return _publicCalls.TryGetValue(name, out format) &&
                   (format?.Length ?? 0) == paramCount &&
                   _publicCalls2.TryGetValue(name, out counts);
        }

        public int ProcessPublicCall(string name, object[] args)
        {
            switch (name)
            {
                case "OnGameModeInit":
                    Console.WriteLine("GMI");
                    Test();
                    break;
                case "OnGameModeExit":
                    Console.WriteLine("GME");
                    break;
                case "OnRconCommand":
                    Console.WriteLine("RC: " + args[0]);
                    break;
            }

            return 1;
        }
    }
}