#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;

class SignalWrap : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
    Environment* env = Environment::GetCurrent(context);
    Local<FunctionTemplate> constructor = env->NewFunctionTemplate(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "\x53\x69\x67\x6e\x61\x6c"));

    env->SetProtoMethod(constructor, "\x63\x6c\x6f\x73\x65", HandleWrap::Close);
    env->SetProtoMethod(constructor, "\x72\x65\x66", HandleWrap::Ref);
    env->SetProtoMethod(constructor, "\x75\x6e\x72\x65\x66", HandleWrap::Unref);
    env->SetProtoMethod(constructor, "\x68\x61\x73\x52\x65\x66", HandleWrap::HasRef);
    env->SetProtoMethod(constructor, "\x73\x74\x61\x72\x74", Start);
    env->SetProtoMethod(constructor, "\x73\x74\x6f\x70", Stop);

    target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x53\x69\x67\x6e\x61\x6c"),
                constructor->GetFunction());
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    CHECK(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args);
    new SignalWrap(env, args.This());
  }

  SignalWrap(Environment* env, Local<Object> object)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&handle_),
                   AsyncWrap::PROVIDER_SIGNALWRAP) {
    int r = uv_signal_init(env->event_loop(), &handle_);
    CHECK_EQ(r, 0);
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    SignalWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
    int signum = args[0]->Int32Value();
#if defined(__POSIX__) && HAVE_INSPECTOR
    if (signum == SIGPROF) {
      Environment* env = Environment::GetCurrent(args);
      if (env->inspector_agent()->IsStarted()) {
        fprintf(stderr, "process.on(SIGPROF) is reserved while debugging\n");
        return;
      }
    }
#endif
    int err = uv_signal_start(&wrap->handle_, OnSignal, signum);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    SignalWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
    int err = uv_signal_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void OnSignal(uv_signal_t* handle, int signum) {
    SignalWrap* wrap = ContainerOf(&SignalWrap::handle_, handle);
    Environment* env = wrap->env();
    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());

    Local<Value> arg = Integer::New(env->isolate(), signum);
    wrap->MakeCallback(env->onsignal_string(), 1, &arg);
  }

  uv_signal_t handle_;
};


}  // namespace node


NODE_MODULE_CONTEXT_AWARE_BUILTIN(signal_wrap, node::SignalWrap::Initialize)
