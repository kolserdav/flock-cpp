#include <node.h>

namespace hello {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;

void SayHello(const FunctionCallbackInfo<v8::Value> &args) {
  Isolate *isolate = args.GetIsolate();
  args.GetReturnValue().Set(
      String::NewFromUtf8(isolate, "Hello, World!").ToLocalChecked());
}

void Initialize(Local<Object> exports) {
  Isolate *isolate = Isolate::GetCurrent();
  exports
      ->Set(isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, "sayHello").ToLocalChecked(),
            FunctionTemplate::New(isolate, SayHello)
                ->GetFunction(isolate->GetCurrentContext())
                .ToLocalChecked())
      .FromJust();
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

} // namespace hello