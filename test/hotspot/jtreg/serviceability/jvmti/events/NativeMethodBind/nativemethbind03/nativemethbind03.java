/*
 * Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

import jdk.test.lib.jvmti.DebugeeClass;

import java.io.*;


/*
 * @test
 *
 * @summary converted from VM Testbase nsk/jvmti/NativeMethodBind/nativemethbind003.
 * VM Testbase keywords: [quick, jpda, jvmti, noras]
 * VM Testbase readme:
 * DESCRIPTION
 *     This test exercises the JVMTI event NativeMethodBind.
 *     It verifies that the event will not be sent when the native
 *     method is unbound.
 *     The test works as follows. The java part invokes the native method
 *     'registerNative()' which registers native method 'nativeMethod()'
 *     for the dummy class 'TestedClass' and then unregisters it.
 *     Registration/unregistration is made through the JNI
 *     RegisterNatives()/UnregisterNatives() calls.
 *     In accordance with the spec, it is expected that the NativeMethodBind
 *     will be generated only one time for the nativeMethod().
 * COMMENTS
 *     The test has been fixed due to the bug 4967116.
 *     Fixed the 4995867 bug.
 *
 * @library /test/lib
 * @run main/othervm/native
 *      -agentlib:nativemethbind03 nativemethbind03
 */

/**
 * This test exercises the JVMTI event <code>NativeMethodBind</code>.
 * <br>It verifies that the event will not be sent when the native
 * method is unbound.<p>
 * The test works as follows. The java part invokes the native method
 * <code>registerNative()</code> which registers native method
 * <code>nativeMethod()</code> for the dummy class <code>TestedClass</code>
 * and then unregisters it. Registration/unregistration is made through
 * the JNI RegisterNatives()/UnregisterNatives() calls.<br>
 * In accordance with the spec, it is expected that the NativeMethodBind
 * will be generated only one time for the nativeMethod().
 */
public class nativemethbind03 {
    static {
        try {
            System.loadLibrary("nativemethbind03");
        } catch (UnsatisfiedLinkError ule) {
            System.err.println("Could not load \"nativemethbind03\" library");
            System.err.println("java.library.path:"
                + System.getProperty("java.library.path"));
            throw ule;
        }
    }

    native void registerNative();

    public static void main(String[] argv) {
        new nativemethbind03().runThis();
    }

    private void runThis() {
        // register native method 'nativeMethod' with 'TestedClass'
        registerNative();
    }

   /**
    * Dummy class used only to register/unregister native method
    * <code>nativeMethod</code> with it
    */
    class TestedClass {
        native void nativeMethod();
    }
}
