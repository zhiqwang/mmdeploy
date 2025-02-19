// Copyright (c) OpenMMLab. All rights reserved.

#include "transform_module.h"

#include "archive/value_archive.h"
#include "core/utils/formatter.h"
#include "experimental/module_adapter.h"
#include "preprocess/transform/transform.h"

namespace mmdeploy {

TransformModule::~TransformModule() = default;

TransformModule::TransformModule(const Value& args) {
  const auto type = "Compose";
  auto creator = Registry<Transform>::Get().GetCreator(type, 1);
  if (!creator) {
    ERROR("unable to find creator: {}", type);
    throw_exception(eEntryNotFound);
  }
  auto cfg = args;
  if (cfg.contains("device")) {
    WARN("force using device: {}", cfg["device"].get<const char*>());
    auto device = Device(cfg["device"].get<const char*>());
    cfg["context"]["device"] = device;
    cfg["context"]["stream"] = Stream::GetDefault(device);
  }
  transform_ = creator->Create(cfg);
}

Result<Value> TransformModule::operator()(const Value& input) {
  auto output = transform_->Process(input);
  if (!output) {
    ERROR("error: {}", output.error().message().c_str());
  }
  auto& ret = output.value();
  if (ret.is_object()) {
    // pass
  } else if (ret.is_array() && ret.size() == 1 && ret[0].is_object()) {
    ret = ret[0];
  } else {
    ERROR("unsupported return value: {}", ret);
    return Status(eNotSupported);
  }
  return ret;
}

class TransformModuleCreator : public Creator<Module> {
 public:
  const char* GetName() const override { return "Transform"; }
  int GetVersion() const override { return 0; }
  std::unique_ptr<Module> Create(const Value& value) override {
    return CreateTask(TransformModule{value});
  }
};

REGISTER_MODULE(Module, TransformModuleCreator);

}  // namespace mmdeploy
