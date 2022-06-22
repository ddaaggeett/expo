// Copyright © 2021-present 650 Industries, Inc. (aka Expo)

#include "JavaScriptModuleObject.h"
#include "JSIInteropModuleRegistry.h"

#include <folly/dynamic.h>
#include <jsi/JSIDynamic.h>
#include <react/jni/ReadableNativeArray.h>
#include <fbjni/detail/Hybrid.h>
#include <ReactCommon/TurboModuleUtils.h>
#include <jni/JCallback.h>
#include <jsi/JSIDynamic.h>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>

#include <utility>
#include <tuple>
#include <algorithm>

namespace jni = facebook::jni;
namespace jsi = facebook::jsi;
namespace react = facebook::react;

namespace expo {

jni::local_ref<jni::HybridClass<JavaScriptModuleObject>::jhybriddata>
JavaScriptModuleObject::initHybrid(jni::alias_ref<jhybridobject> jThis) {
  return makeCxxInstance(jThis);
}

void JavaScriptModuleObject::registerNatives() {
  registerHybrid({
                   makeNativeMethod("initHybrid", JavaScriptModuleObject::initHybrid),
                   makeNativeMethod("exportConstants", JavaScriptModuleObject::exportConstants),
                   makeNativeMethod("registerSyncFunction",
                                    JavaScriptModuleObject::registerSyncFunction),
                   makeNativeMethod("registerAsyncFunction",
                                    JavaScriptModuleObject::registerAsyncFunction),
                 });
}

std::shared_ptr<jsi::Object> JavaScriptModuleObject::getJSIObject(jsi::Runtime &runtime) {
  if (jsiObject == nullptr) {
    auto hostObject = std::make_shared<JavaScriptModuleObject::HostObject>(this);
    jsiObject = std::make_shared<jsi::Object>(
      jsi::Object::createFromHostObject(runtime, hostObject));
  }

  return jsiObject;
}

void JavaScriptModuleObject::exportConstants(
  jni::alias_ref<react::NativeMap::javaobject> constants
) {
  auto dynamic = constants->cthis()->consume();
  assert(dynamic.isObject());

  for (const auto &[key, value]: dynamic.items()) {
    this->constants[key.asString()] = value;
  }
}

void JavaScriptModuleObject::registerSyncFunction(
  jni::alias_ref<jstring> name,
  jint args,
  jni::alias_ref<jni::JArrayInt> desiredTypes,
  jni::alias_ref<JNIFunctionBody::javaobject> body
) {
  std::string cName = name->toStdString();
  std::unique_ptr<int[]> types = desiredTypes->getRegion(0, args);

  methodsMetadata.try_emplace(
    cName,
    cName,
    args,
    false,
    std::move(types),
    jni::make_global(body)
  );
}

void JavaScriptModuleObject::registerAsyncFunction(
  jni::alias_ref<jstring> name,
  jint args,
  jni::alias_ref<jni::JArrayInt> desiredTypes,
  jni::alias_ref<JNIAsyncFunctionBody::javaobject> body
) {
  auto cName = name->toStdString();
  std::unique_ptr<int[]> types = desiredTypes->getRegion(0, args);

  methodsMetadata.try_emplace(
    cName,
    cName,
    args,
    true,
    std::move(types),
    jni::make_global(body)
  );
}

JavaScriptModuleObject::HostObject::HostObject(
  JavaScriptModuleObject *jsModule) : jsModule(jsModule) {}

jsi::Value JavaScriptModuleObject::HostObject::get(jsi::Runtime &runtime,
                                                   const jsi::PropNameID &name) {
  auto cName = name.utf8(runtime);

  auto constantsRecord = jsModule->constants.find(cName);
  if (constantsRecord != jsModule->constants.end()) {
    auto dynamic = constantsRecord->second;
    return jsi::valueFromDynamic(runtime, dynamic);
  }

  auto metadataRecord = jsModule->methodsMetadata.find(cName);
  if (metadataRecord == jsModule->methodsMetadata.end()) {
    return jsi::Value::undefined();
  }
  auto &metadata = metadataRecord->second;
  return jsi::Value(runtime, *metadata.toJSFunction(runtime, jsModule->jsiInteropModuleRegistry));
}

void JavaScriptModuleObject::HostObject::set(
  jsi::Runtime &runtime,
  const jsi::PropNameID &name,
  const jsi::Value &value
) {
  throw jsi::JSError(
    runtime,
    "RuntimeError: Cannot override the host object for expo module '" + name.utf8(runtime) + "'"
  );
}

std::vector<jsi::PropNameID> JavaScriptModuleObject::HostObject::getPropertyNames(
  jsi::Runtime &rt
) {
  auto &metadata = jsModule->methodsMetadata;
  std::vector<jsi::PropNameID> result;
  std::transform(
    metadata.begin(),
    metadata.end(),
    std::back_inserter(result),
    [&rt](const auto &kv) {
      return jsi::PropNameID::forUtf8(rt, kv.first);
    }
  );

  auto &constants = jsModule->constants;
  std::transform(
    constants.begin(),
    constants.end(),
    std::back_inserter(result),
    [&rt](const auto &kv) {
      return jsi::PropNameID::forUtf8(rt, kv.first);
    }
  );

  return result;
}
} // namespace expo
