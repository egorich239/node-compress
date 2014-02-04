/*
 * Copyright 2009, Acknack Ltd. All rights reserved.
 * Copyright 2010, Ivan Egorov (egorich.3.04@gmail.com).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <node.h>

#ifdef WITH_GZIP
#include "gzip.cc"
#endif

#ifdef WITH_BZIP
#include "bzip.cc"
#endif

extern "C" void
init (Handle<Object> target) 
{
  HandleScope scope;

#ifdef WITH_GZIP
  Gzip::Initialize(target);
  Gunzip::Initialize(target);
	target->Set(v8::String::NewSymbol("MAX_WBITS"), v8::Integer::New(MAX_WBITS));
	target->Set(v8::String::NewSymbol("Z_DEFAULT_COMPRESSION"), v8::Integer::New(Z_DEFAULT_COMPRESSION));
#endif

#ifdef WITH_BZIP
  Bzip::Initialize(target);
  Bunzip::Initialize(target);
#endif
}

