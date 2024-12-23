#include <cstdio>
#include <fcntl.h>
#include <node.h>
#include <sys/file.h>
#include <uv.h>

namespace file_lock {

using v8::Boolean;
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
using v8::Undefined;

struct LockRequest {
  uv_work_t request;
  Persistent<Promise::Resolver> resolver;
  std::string filename;
  int fd;
  std::string error;
};

struct UnlockRequest {
  uv_work_t request;
  Persistent<Promise::Resolver> resolver;
  int fd;
  std::string error;
};

struct IsLockedRequest {
  uv_work_t request;
  Persistent<Promise::Resolver> resolver;
  std::string filename;
  std::string error;
  bool locked;
};

void ExecuteLock(uv_work_t *req) {
  LockRequest *work = static_cast<LockRequest *>(req->data);

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

void ExecuteUnlock(uv_work_t *req) {
  UnlockRequest *work = static_cast<UnlockRequest *>(req->data);

  int fileDescriptor = work->fd;
  if (flock(fileDescriptor, LOCK_UN) != 0) {
    work->error = "Failed to unlock file";
    return;
  }
}

void ExecuteIslocked(uv_work_t *req) {
  IsLockedRequest *work = static_cast<IsLockedRequest *>(req->data);

  FILE *file = fopen(work->filename.c_str(), "w+");
  if (file == nullptr) {
    work->error = "Failed to open file";
    return;
  }

  int fileDescriptor = fileno(file);

  if (flock(fileDescriptor, LOCK_EX | LOCK_NB) == 0) {
    flock(fileDescriptor, LOCK_UN);
    work->locked = false;
  } else {
    work->locked = true;
  }
  fclose(file);
}

void HandleLockCallback(uv_work_t *req, int /*status*/) {
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  LockRequest *work = static_cast<LockRequest *>(req->data);
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

void HandleUnlockCallback(uv_work_t *req, int /*status*/) {
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  UnlockRequest *work = static_cast<UnlockRequest *>(req->data);
  Local<Promise::Resolver> resolver =
      Local<Promise::Resolver>::New(isolate, work->resolver);

  if (!work->error.empty()) {
    resolver
        ->Reject(
            isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, work->error.c_str()).ToLocalChecked())
        .ToChecked();
  } else {
    resolver->Resolve(isolate->GetCurrentContext(), Undefined(isolate))
        .FromMaybe(false);
  }

  work->resolver.Reset();
  delete work;
}

void HandleIsLockedCallback(uv_work_t *req, int /*status*/) {
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  IsLockedRequest *work = static_cast<IsLockedRequest *>(req->data);
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
        ->Resolve(isolate->GetCurrentContext(),
                  Boolean::New(isolate, work->locked))
        .FromMaybe(false);
  }

  work->resolver.Reset();
  delete work;
}

void Lock(const FunctionCallbackInfo<v8::Value> &args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 1) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "First argument must be string")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  String::Utf8Value str(isolate, args[0]);

  if (*str == nullptr) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "Invalid string argument")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  std::string filename = std::string(*str);

  Local<Promise::Resolver> resolver =
      Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  LockRequest *work = new LockRequest();
  work->request.data = work;
  work->filename = filename;
  work->resolver.Reset(isolate, resolver);

  uv_queue_work(uv_default_loop(), &work->request, ExecuteLock,
                HandleLockCallback);

  args.GetReturnValue().Set(resolver->GetPromise());
}

void Unlock(const FunctionCallbackInfo<v8::Value> &args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 1) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "First argument must be integer")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  if (!args[0]->IsNumber()) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "First argument must be a number")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  int32_t fd = static_cast<int32_t>(
      args[0]->NumberValue(isolate->GetCurrentContext()).ToChecked());

  Local<Promise::Resolver> resolver =
      Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  UnlockRequest *work = new UnlockRequest();
  work->request.data = work;
  work->fd = fd;
  work->resolver.Reset(isolate, resolver);

  uv_queue_work(uv_default_loop(), &work->request, ExecuteUnlock,
                HandleUnlockCallback);

  args.GetReturnValue().Set(resolver->GetPromise());
}

void IsLocked(const FunctionCallbackInfo<v8::Value> &args) {
  Isolate *isolate = args.GetIsolate();

  if (args.Length() < 1) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "First argument must be string")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  String::Utf8Value str(isolate, args[0]);

  if (*str == nullptr) {
    Local<String> error_message =
        String::NewFromUtf8(isolate, "Invalid string argument")
            .ToLocalChecked();
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
    resolver->Reject(isolate->GetCurrentContext(), error_message).FromJust();
    args.GetReturnValue().Set(resolver->GetPromise());
    return;
  }

  std::string filename = std::string(*str);

  Local<Promise::Resolver> resolver =
      Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();

  IsLockedRequest *work = new IsLockedRequest();
  work->request.data = work;
  work->filename = filename;
  work->resolver.Reset(isolate, resolver);

  uv_queue_work(uv_default_loop(), &work->request, ExecuteIslocked,
                HandleIsLockedCallback);

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
  exports
      ->Set(isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, "unlock").ToLocalChecked(),
            FunctionTemplate::New(isolate, Unlock)
                ->GetFunction(isolate->GetCurrentContext())
                .ToLocalChecked())
      .FromJust();
  exports
      ->Set(isolate->GetCurrentContext(),
            String::NewFromUtf8(isolate, "isLocked").ToLocalChecked(),
            FunctionTemplate::New(isolate, IsLocked)
                ->GetFunction(isolate->GetCurrentContext())
                .ToLocalChecked())
      .FromJust();
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

} // namespace file_lock