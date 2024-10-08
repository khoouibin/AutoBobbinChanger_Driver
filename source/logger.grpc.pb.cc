// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: logger.proto

#include "logger.pb.h"
#include "logger.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace logger {

static const char* Logger_method_names[] = {
  "/logger.Logger/AddLog",
};

std::unique_ptr< Logger::Stub> Logger::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< Logger::Stub> stub(new Logger::Stub(channel));
  return stub;
}

Logger::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_AddLog_(Logger_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status Logger::Stub::AddLog(::grpc::ClientContext* context, const ::logger::LogRequest& request, ::logger::LogReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_AddLog_, context, request, response);
}

void Logger::Stub::experimental_async::AddLog(::grpc::ClientContext* context, const ::logger::LogRequest* request, ::logger::LogReply* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_AddLog_, context, request, response, std::move(f));
}

void Logger::Stub::experimental_async::AddLog(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::logger::LogReply* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_AddLog_, context, request, response, std::move(f));
}

void Logger::Stub::experimental_async::AddLog(::grpc::ClientContext* context, const ::logger::LogRequest* request, ::logger::LogReply* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_AddLog_, context, request, response, reactor);
}

void Logger::Stub::experimental_async::AddLog(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::logger::LogReply* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_AddLog_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::logger::LogReply>* Logger::Stub::AsyncAddLogRaw(::grpc::ClientContext* context, const ::logger::LogRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::logger::LogReply>::Create(channel_.get(), cq, rpcmethod_AddLog_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::logger::LogReply>* Logger::Stub::PrepareAsyncAddLogRaw(::grpc::ClientContext* context, const ::logger::LogRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::logger::LogReply>::Create(channel_.get(), cq, rpcmethod_AddLog_, context, request, false);
}

Logger::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      Logger_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< Logger::Service, ::logger::LogRequest, ::logger::LogReply>(
          std::mem_fn(&Logger::Service::AddLog), this)));
}

Logger::Service::~Service() {
}

::grpc::Status Logger::Service::AddLog(::grpc::ServerContext* context, const ::logger::LogRequest* request, ::logger::LogReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace logger

