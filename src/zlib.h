/*
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

#ifndef NODE_COMPRESS_ZLIB_H__
#define NODE_COMPRESS_ZLIB_H__

// To have (std::nothrow).
#include <new>

#include <pthread.h>

#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>

#include "utils.h"


#ifdef DEBUG

#include <stdio.h>

#define DEBUG_P(fmt, args...) \
  do { \
    fprintf(stderr, "%s: %d %s " fmt "\n", \
        __FILE__, __LINE__, __PRETTY_FUNCTION__, ##args); \
  } while (0);


#else

#define DEBUG_P(fmt, args...)

#endif

using namespace v8;
using namespace node;

template <class Processor>
class ZipLib : ObjectWrap {
 private:
  enum State {
    Idle,
    Destroyed,
    Data,
    Eos,
    Error
  };

 private:
  typedef typename Processor::Utils Utils;
  typedef typename Processor::Blob Blob;

  typedef ZipLib<Processor> Self;
  typedef StateTransition<State> Transition;

  struct Request {
   public:
    enum Kind {
      RWrite,
      RClose,
      RDestroy
    };
   private:
    Request(ZipLib *self, Local<Value> inputBuffer, Local<Function> callback)
      : kind_(RWrite), self_(self),
      buffer_(Persistent<Value>::New(inputBuffer)),
      data_(Buffer::Data(inputBuffer->ToObject())),
      length_(Buffer::Length(inputBuffer->ToObject())),
      callback_(Persistent<Function>::New(callback))
    {}
    
    Request(ZipLib *self, Local<Function> callback)
      : kind_(RClose), self_(self),
      callback_(Persistent<Function>::New(callback))
    {}

    Request(ZipLib *self)
      : kind_(RDestroy), self_(self)
    {}

   public:
    ~Request() {
      if (!buffer_.IsEmpty()) {
        buffer_.Dispose();
      }
      if (!callback_.IsEmpty()) {
        callback_.Dispose();
      }
    }

   public:
    static Request* Write(Self *self, Local<Value> inputBuffer,
        Local<Function> callback) {
      DEBUG_P("WRITE");
      return new(std::nothrow) Request(self, inputBuffer, callback);
    }

    static Request* Close(Self *self, Local<Function> callback) {
      DEBUG_P("CLOSE");
      return new(std::nothrow) Request(self, callback);
    }

    static Request* Destroy(Self *self) {
      DEBUG_P("DESTROY");
      return new(std::nothrow) Request(self);
    }

   public:
    void setStatus(int status) {
      status_ = status;
    }
    
    char* buffer() const {
      return data_;
    }

    int length() const {
      return length_;
    }

    Self *self() const {
      assert(this != 0);
      return self_;
    }

    Blob &output() {
      return out_;
    }

    Kind kind() const {
      return kind_;
    }

    int status() const {
      return status_;
    }

    Persistent<Function> callback() const {
      return callback_;
    }

   private:
    Kind kind_;

    ZipLib *self_;

    // We store persistent Buffer object reference to avoid garbage collection,
    // but it's not thread-safe to reference it from non-JS script, so we also
    // store raw buffer data and length.
    Persistent<Value> buffer_;
    char *data_;
    int length_;

    Persistent<Function> callback_;

    // Output structures.
    Blob out_;
    int status_;
  };

 public:
  static void Initialize(v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Self::constructor_ = Persistent<FunctionTemplate>::New(
        FunctionTemplate::New(New));
    Self::constructor_->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(Self::constructor_, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(Self::constructor_, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(Self::constructor_, "destroy", Destroy);

    NODE_SET_METHOD(Self::constructor_, "createInstance_", Create);

    target->Set(String::NewSymbol(Processor::Name),
        Self::constructor_->GetFunction());
  }

 public:
  static Handle<Value> New(const Arguments &args) {
    Self *result = new(std::nothrow) Self();
    if (result == 0) {
      return ThrowGentleOom();
    }
    result->Wrap(args.This());

    Transition t(result->state_, Self::Error);
    Handle<Value> exception = result->processor_.Init(args);
    if (!exception->IsUndefined()) {
      return exception;
    }

    t.alter(Self::Data);
    return args.This();
  }


  static Handle<Value> Create(const Arguments &args) {
    HandleScope scope;

    Handle<Value> *params = new(std::nothrow) Handle<Value>[args.Length()];
    if (params == 0) {
      return ThrowGentleOom();
    }
    for (int i = 0; i < args.Length(); ++i) {
      params[i] = args[i];
    }

    Handle<Value> result = Self::constructor_->GetFunction()->
        NewInstance(args.Length(), params);
    delete[] params;
    return result;
  }


  static Handle<Value> Write(const Arguments& args) {
    HandleScope scope;

    if (!Buffer::HasInstance(args[0])) {
      Local<Value> exception = Exception::TypeError(
          String::New("Input must be of type Buffer"));
      return ThrowException(exception);
    }

    Local<Function> cb;
    if (args.Length() > 1 && !args[1]->IsUndefined()) {
      if (!args[1]->IsFunction()) {
        return ThrowCallbackExpected();
      }
      cb = Local<Function>::Cast(args[1]);
    }

    Self *self = ObjectWrap::Unwrap<Self>(args.This());
    Request *request = Request::Write(self, args[0], cb);
    return self->PushRequest(request);
  }


  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    Local<Function> cb;
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsFunction()) {
        return ThrowCallbackExpected();
      }
      cb = Local<Function>::Cast(args[0]);
    }

    Self *self = ObjectWrap::Unwrap<Self>(args.This());
    Request *request = Request::Close(self, cb);
    return self->PushRequest(request);
  }


  static Handle<Value> Destroy(const Arguments& args) {
    HandleScope scope;

    Self *self = ObjectWrap::Unwrap<Self>(args.This());
    Request *request = Request::Destroy(self);
    return self->PushRequest(request);
  }


 private:
  // Attempt to push request.
  // Executed in V8 thread.
  Handle<Value> PushRequest(Request *request) {
    if (request == 0) {
      return ThrowGentleOom();
    }

    pthread_mutex_lock(&requestsMutex_);
    bool pushed = requestsQueue_.Push(request);
    bool startProcessing = !processorActive_;
    if (pushed && startProcessing) {
      processorActive_ = true;
    }
    pthread_mutex_unlock(&requestsMutex_);
    DEBUG_P("PUSH: startProcessing = %d", startProcessing);
    
    if (!pushed) {
      return ThrowGentleOom();
    }

    if (startProcessing) {
      eio_custom(Self::DoProcess, EIO_PRI_DEFAULT,
          Self::DoHandleCallbacks, request);
    }

    DEBUG_P("ev_ref()");
    ev_ref(EV_DEFAULT_UC);
    DEBUG_P(" ev_ref() done");

    DEBUG_P("Ref()");
    Ref();
    DEBUG_P(" Ref() done");
    return Undefined();
  }

  // Process requests queue.
  // Executed in worker thread.
  static int DoProcess(eio_req *req) {
    Self *self = reinterpret_cast<Request*>(req->data)->self();
    self->DoProcess();
    return 0;
  }

  void DoProcess() {
    Request *request;

    volatile bool flag;
    do {
      while (ReentrantPop(requestsQueue_, requestsMutex_, request)) {
        DEBUG_P("POP: kind = %d", request->kind());
        switch (request->kind()) {
          case Request::RWrite:
            request->setStatus(
                this->Write(request->buffer(), request->length(),
                  request->output()));
            break;

          case Request::RClose:
            request->setStatus(this->Close(request->output()));
            break;

          case Request::RDestroy:
            this->Destroy();
            request->setStatus(Utils::StatusOk());
            break;
        }

        pthread_mutex_lock(&callbackMutex_);
        bool success = callbackQueue_.Push(request);
        pthread_mutex_unlock(&callbackMutex_);
        ev_async_send(EV_DEFAULT_UC_ &callbackNotify_);

        // Unref counter triggered by request.
        DEBUG_P("ev_unref() ...");
        ev_unref(EV_DEFAULT_UC);
        DEBUG_P("  ev_unref() done");
        if (!success) {
          // Normally we should unref self() after callback called in
          // DoHandleCallbacks(), but as we failed to push request for 
          // callback for this request, we should unref here.
          DEBUG_P("request->self()->Unref()");
          request->self()->Unref();
          DEBUG_P("  request->self()->Unref() done");
        }
      }

      pthread_mutex_lock(&requestsMutex_);
      flag = requestsQueue_.length() != 0;
      processorActive_ = flag;
      pthread_mutex_unlock(&requestsMutex_);
    } while (flag);

  }

  // Handle callbacks.
  // Executed in V8 threads.
  static int DoHandleCallbacks(eio_req *req) {
    Request *request;
    while (ReentrantPop(callbackQueue_, callbackMutex_, request)) {
      DEBUG_P("CALLBACK");

      Self *self = request->self();
      self->DoCallback(request->callback(),
          request->status(), request->output());

      DEBUG_P("self->Unref()");
      self->Unref();
      DEBUG_P(" self->Unref() done");
      delete request;
    }
    return 0;
  }

  static void DoHandleCallbacks2(EV_P_ ev_async *evt, int revents) {
    DoHandleCallbacks(0);
  }

  static void DoCallback(Persistent<Function> cb, int r, Blob &out) {
    if (!cb.IsEmpty()) {
      HandleScope scope;

      Local<Value> argv[2];
      argv[0] = Utils::GetException(r);
      argv[1] = Encode(out.data(), out.length(), BINARY);

      TryCatch try_catch;

      cb->Call(Context::GetCurrent()->Global(), 2, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
  }

 private:
  static bool ReentrantPop(Queue<Request*> &queue, pthread_mutex_t &mutex,
      Request*& request) {
    request = 0;

    pthread_mutex_lock(&mutex);
    bool result = queue.length() != 0;
    if (result) {
      request = queue.Pop();
    }
    pthread_mutex_unlock(&mutex);

    return result;
  }

 private:

  ZipLib()
    : ObjectWrap(), state_(Self::Idle), processorActive_(false)
  {
    pthread_mutex_init(&requestsMutex_, 0);

    // Lazy init. Safe to do it here as this always happen in JS-thread.
    if (!callbackInitialized_) {
      pthread_mutex_init(&callbackMutex_, 0);
      ev_async_init(EV_DEFAULT_UC_ &callbackNotify_, Self::DoHandleCallbacks2);
      ev_async_start(EV_DEFAULT_UC_ &callbackNotify_);
      ev_unref(EV_DEFAULT_UC);
    }
  }


  ~ZipLib() {
    this->Destroy();
  }


  int Write(char *data, int dataLength, Blob &out) {
    COND_RETURN(state_ != Self::Data, Utils::StatusSequenceError());

    Transition t(state_, Self::Error);

    data += dataLength;
    int ret = Utils::StatusOk();
    while (dataLength > 0) { 
      COND_RETURN(!out.GrowBy(dataLength + 1), Utils::StatusMemoryError());
      
      ret = this->processor_.Write(data - dataLength, dataLength, out);
      COND_RETURN(Utils::IsError(ret), ret);
      if (ret == Utils::StatusEndOfStream()) {
        t.alter(Self::Eos);
        return ret;
      }
    }
    t.abort();
    return Utils::StatusOk();
  }


  int Close(Blob &out) {
    COND_RETURN(state_ == Self::Idle || state_ == Self::Destroyed,
        Utils::StatusOk());

    Transition t(state_, Self::Error);

    int ret = Utils::StatusOk();
    if (state_ == Self::Data) {
      ret = Finish(out);
    }

    t.abort();
    this->Destroy();
    return ret;
  }


  void Destroy() {
    if (state_ != Self::Idle && state_ != Self::Destroyed) {
      this->processor_.Destroy();
    }
    state_ = Self::Destroyed;
  }


  int Finish(Blob &out) {
    const int Chunk = 128;

    int ret;
    do {
      COND_RETURN(!out.GrowBy(Chunk), Utils::StatusMemoryError());
      
      ret = this->processor_.Finish(out);
      COND_RETURN(Utils::IsError(ret), ret);
    } while (ret != Utils::StatusEndOfStream());
    return Utils::StatusOk();
  }


 private:
  static Handle<Value> ThrowGentleOom() {
    V8::LowMemoryNotification();
    Local<Value> exception = Exception::Error(
        String::New("Insufficient space"));
    return ThrowException(exception);
  }

  static Handle<Value> ThrowCallbackExpected() {
    Local<Value> exception = Exception::TypeError(
        String::New("Callback must be a function"));
    return ThrowException(exception);
  }


 private:
  Processor processor_;
  State state_;

  pthread_mutex_t requestsMutex_;
  Queue<Request*> requestsQueue_;

  static Persistent<FunctionTemplate> constructor_;
  static bool callbackInitialized_;
  static pthread_mutex_t callbackMutex_;
  static Queue<Request*> callbackQueue_;
  static ev_async callbackNotify_;

  volatile bool processorActive_;
};

template <class T> Persistent<FunctionTemplate> ZipLib<T>::constructor_;
template <class T> bool ZipLib<T>::callbackInitialized_ = false;
template <class T> pthread_mutex_t ZipLib<T>::callbackMutex_;
template <class T> ev_async ZipLib<T>::callbackNotify_;

template <class T>
Queue<typename ZipLib<T>::Request*> ZipLib<T>::callbackQueue_;

#endif

