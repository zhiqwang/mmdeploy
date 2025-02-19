// Copyright (c) OpenMMLab. All rights reserved.

#include "compose.h"

#include "archive/json_archive.h"
#include "archive/value_archive.h"
#include "core/utils/formatter.h"

namespace mmdeploy {

Compose::Compose(const Value& args, int version) : Transform(args) {
  assert(args.contains("context"));

  Value context;
  context = args["context"];
  context["stream"].get_to(stream_);
  for (auto cfg : args["transforms"]) {
    cfg["context"] = context;
    auto type = cfg.value("type", std::string{});
    DEBUG("creating transform: {} with cfg: {}", type, mmdeploy::to_json(cfg).dump(2));
    auto creator = Registry<Transform>::Get().GetCreator(type, version);
    if (!creator) {
      ERROR("unable to find creator: {}", type);
      throw std::invalid_argument("unable to find creator");
    }
    auto transform = creator->Create(cfg);
    if (!transform) {
      ERROR("failed to create transform: {}", type);
      throw std::invalid_argument("failed to create transform");
    }
    transforms_.push_back(std::move(transform));
  }
}

Result<Value> Compose::Process(const Value& input) {
  Value output = input;
  for (auto& transform : transforms_) {
    auto t = transform->Process(output);
    OUTCOME_TRY(stream_.Wait());
    if (!t) {
      return t;
    }
    output = std::move(t).value();
  }
  return std::move(output);
}

class ComposeCreator : public Creator<Transform> {
 public:
  const char* GetName() const override { return "Compose"; }
  int GetVersion() const override { return version_; }
  ReturnType Create(const Value& args) override {
    return std::make_unique<Compose>(args, version_);
  }

 private:
  int version_{1};
};

REGISTER_MODULE(Transform, ComposeCreator);
}  // namespace mmdeploy
