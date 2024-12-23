#include <cstdio>
#include <fcntl.h>
#include <node.h>
#include <sys/file.h>
#include <uv.h>

namespace file_lock {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Persistent;
using v8::Promise;
using v8::String;

struct FlockRequest {
  uv_work_t request;
  Persistent<Promise::Resolver> resolver;
  std::string filename;
  int fd;
  std::string error;
};

void ExecuteFlock(uv_work_t *req) {
  FlockRequest *work = static_cast<FlockRequest *>(req->data);

  FILE *file = fopen(work->filename.c_str(), "w+");
  if (file == nullptr) {
    work->error = "Failed to open file";
    return;
  }

  int fileDescriptor = fileno(file);
  if (flock(fileDescriptor, LOCK_EX) != 0) {
    fclose(file);
    work->error = "Failed to lock file";
    return;
  }
  work->fd = fileDescriptor;
}

void HandleFlockCallback(uv_work_t *req, int /*status*/) {
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  FlockRequest *work = static_cast<FlockRequest *>(req->data);
  Local<Promise::Resolver> resolver =
      Local<Promise::Resolver>::New(isolate, work->resolver);

  if (!work->error.empty()) {
    resolver
        ->Reject(
            isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, work->error.c_str()).ToLocalChecked())
        .ToChecked();
  } else {
    resolver
        ->Resolve(isolate->GetCurrentContext(), Integer::New(isolate, work->fd))
        .FromMaybe(false);
  }

  work->resolver.Reset();
  delete work;
}

void Lock(const FunctionCallbackInfo<v8::Value> &args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 1) {
    isolate->ThrowException(
        String::NewFromUtf8(isolate, "Expected at least one argument")
            .ToLocalChecked());
    return;
  }

  String::Utf8Value str(isolate, args[0]);

  if (*str == nullptr) {
    isolate->ThrowException(
        String::NewFromUtf8(isolate, "Invalid string").ToLocalChecked());
    return;
  }

  std::string filename = std::string(*str);

  Local<Promise::Resolver> resolver =
      Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  FlockRequest *work = new FlockRequest();
  work->request.data = work;
  work->filename = filename;
  work->resolver.Reset(isolate, resolver);

  uv_queue_work(uv_default_loop(), &work->request, ExecuteFlock,
                HandleFlockCallback);

  args.GetReturnValue().Set(resolver->GetPromise());
}

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> module,
                void *priv) {
  Isolate *isolate = Isolate::GetCurrent();
  exports
      ->Set(isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, "lock").ToLocalChecked(),
            FunctionTemplate::New(isolate, Lock)
                ->GetFunction(isolate->GetCurrentContext())
                .ToLocalChecked())
      .FromJust();
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

} // namespace file_lock