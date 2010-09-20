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

