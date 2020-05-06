/*
 * Copyright 2020-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stratum/hal/lib/phal/tai/taish_wrapper.h"

#include <algorithm>
#include <string>

#include "gflags/gflags.h"
#include "grpcpp/grpcpp.h"
#include "stratum/lib/macros.h"

DEFINE_string(taish_addr, "", "The gRPC address of TAI shell.");

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaishWrapper* TaishWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaishWrapper::init_lock_(absl::kConstInit);

util::Status TaishWrapper::Initialize() {
  CHECK_RETURN_IF_FALSE(!initialized_);
  CHECK_RETURN_IF_FALSE(!FLAGS_taish_addr.empty());
  auto channel = ::grpc::CreateChannel(FLAGS_taish_addr,
                                       grpc::InsecureChannelCredentials());
  taish_stub_ = ::taish::TAI::NewStub(channel);

  // Gets object id of all module, network interfaces, and host interfaces.
  grpc::ClientContext context;
  taish::ListModuleRequest list_module_req;
  taish::ListModuleResponse list_module_resp;

  auto modules_reader = taish_stub_->ListModule(&context, list_module_req);
  while (modules_reader->Read(&list_module_resp)) {
    const auto& module = list_module_resp.module();
    modules_.push_back(module);
  }
  modules_reader->Finish();

  // Get attributes from module, network interace, and host interface.
  taish::ListAttributeMetadataRequest attr_meta_req;
  taish::ListAttributeMetadataResponse attr_meta_resp;

  // Module attributes
  {
    attr_meta_req.set_object_type(taish::MODULE);
    auto attr_meta_reader =
        taish_stub_->ListAttributeMetadata(&context, attr_meta_req);
    while (attr_meta_reader->Read(&attr_meta_resp)) {
      const auto& attr_meta = attr_meta_resp.metadata();
      module_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  // Network interface attributes
  {
    attr_meta_req.set_object_type(taish::NETIF);
    auto attr_meta_reader =
        taish_stub_->ListAttributeMetadata(&context, attr_meta_req);
    while (attr_meta_reader->Read(&attr_meta_resp)) {
      const auto& attr_meta = attr_meta_resp.metadata();
      netif_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  // Host interface attributes
  {
    attr_meta_req.set_object_type(taish::HOSTIF);
    auto attr_meta_reader =
        taish_stub_->ListAttributeMetadata(&context, attr_meta_req);
    while (attr_meta_reader->Read(&attr_meta_resp)) {
      const auto& attr_meta = attr_meta_resp.metadata();
      hostif_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  initialized_ = true;
  return util::OkStatus();
}

util::StatusOr<std::vector<uint64>> TaishWrapper::GetModuleIds() {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  std::vector<uint64> oids;
  for (const auto& module : modules_) {
    oids.push_back(module.oid());
  }
  return oids;
}

util::StatusOr<std::vector<uint64>>
    TaishWrapper::GetNetworkInterfaceIds(const uint64 module_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  auto it = std::find_if(
      modules_.begin(), modules_.end(),
      [module_id](taish::Module m) { return m.oid() == module_id; });
  CHECK_RETURN_IF_FALSE(it != modules_.end())
      << "Cannot find module with id " << module_id;
  taish::Module module = *it;
  std::vector<uint64> oids;
  for (const auto& netif : module.netifs()) {
    oids.push_back(netif.oid());
  }
  return oids;
}

util::StatusOr<std::vector<uint64>>
    TaishWrapper::GetHostInterfaceIds(const uint64 module_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  auto it = std::find_if(
      modules_.begin(), modules_.end(),
      [module_id](taish::Module m) { return m.oid() == module_id; });
  CHECK_RETURN_IF_FALSE(it != modules_.end())
      << "Can not find module with id " << module_id;
  taish::Module module = *it;
  std::vector<uint64> oids;
  for (const auto& hostif : module.hostifs()) {
    oids.push_back(hostif.oid());
  }
  return oids;
}

util::StatusOr<uint64>
    TaishWrapper::GetTxLaserFrequency(const uint64 netif_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrTxLaserFreq]));
  // TODO(Yi): Handle exceptions.
  return std::stoull(attr_str_val);
}

util::StatusOr<double>
    TaishWrapper::GetCurrentInputPower(const uint64 netif_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrCurrentInputPower]));
  // TODO(Yi): Handle exceptions.
  return std::stod(attr_str_val);
}

util::StatusOr<double>
    TaishWrapper::GetCurrentOutputPower(const uint64 netif_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrCurrentOutputPower]));
  // TODO(Yi): Handle exceptions.
  return std::stod(attr_str_val);
}

util::StatusOr<double>
    TaishWrapper::GetTargetOutputPower(const uint64 netif_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrOutputPower]));
  // TODO(Yi): Handle exceptions.
  return std::stod(attr_str_val);
}

util::StatusOr<uint64>
    TaishWrapper::GetModulationFormats(const uint64 netif_id) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrModulationFormat]));
  // TODO(Yi): Handle exceptions.
  return std::stoull(attr_str_val);
}

util::Status TaishWrapper::SetTargetOutputPower(const uint64 netif_id,
                                                const double power) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrOutputPower],
                      std::to_string(power));
}

util::Status TaishWrapper::SetModulationsFormats(const uint64 netif_id,
                                                 const uint64 mod_format) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrModulationFormat],
                      std::to_string(mod_format));
}

util::Status TaishWrapper::SetTxLaserFrequency(const uint64 netif_id,
                                               const uint64 frequency) {
  absl::WriterMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrTxLaserFreq],
                      std::to_string(frequency));
}

util::Status TaishWrapper::Shutdown() {
  absl::WriterMutexLock l(&init_lock_);
  initialized_ = false;
  return util::OkStatus();
}

util::StatusOr<std::string> TaishWrapper::GetAttribute(uint64 obj_id,
                                                       uint64 attr_id) {
  grpc::ClientContext context;
  taish::GetAttributeRequest request;
  taish::GetAttributeResponse response;

  request.set_oid(obj_id);
  request.mutable_serialize_option()->set_value_only(true);
  request.mutable_serialize_option()->set_human(false);
  request.mutable_serialize_option()->set_json(false);
  request.mutable_attribute()->set_attr_id(attr_id);

  auto status = taish_stub_->GetAttribute(&context, request, &response);
  CHECK_RETURN_IF_FALSE(status.ok()) << status.error_message();
  return response.attribute().value();
}

util::Status TaishWrapper::SetAttribute(uint64 obj_id, uint64 attr_id,
                                        std::string value) {
  grpc::ClientContext context;
  taish::SetAttributeRequest request;
  taish::SetAttributeResponse response;

  request.set_oid(obj_id);
  request.mutable_serialize_option()->set_value_only(true);
  request.mutable_serialize_option()->set_human(false);
  request.mutable_serialize_option()->set_json(false);
  request.mutable_attribute()->set_attr_id(attr_id);
  request.mutable_attribute()->set_value(value);

  auto status = taish_stub_->SetAttribute(&context, request, &response);
  CHECK_RETURN_IF_FALSE(status.ok()) << status.error_message();
  return util::OkStatus();
}

TaishWrapper* TaishWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new TaishWrapper();
    if (!singleton_->Initialize().ok()) {
      LOG(ERROR) << "Failed to initialize TaishWrapper";
      delete singleton_;
      return nullptr;
    }
  }
  return singleton_;
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum