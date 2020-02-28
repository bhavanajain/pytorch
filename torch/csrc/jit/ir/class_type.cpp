#include <ATen/core/jit_type.h>
#include <c10/macros/Macros.h>
#include <torch/csrc/jit/api/module.h>

namespace c10 {

void ClassType::addMethod(Function* method) {
  TORCH_CHECK(
      getMethod(method->name()) == nullptr,
      "Can't redefine method: ",
      method->name(),
      " on class: ",
      python_str());
  methods_.push_back(method);
}

Function* ClassType::getMethod(const std::string& name) const {
  for (auto method : methods_) {
    if (name == method->name()) {
      return method;
    }
  }
  return nullptr;
}

#ifndef USE_MOBILE_CLASSTYPE

// This file exists because we need to reference module.h, which we can't from
// c10. Sigh...
FunctionType::FunctionType(Function* function)
    : NamedType(TypeKind::FunctionType, function->qualname()),
      function_(function) {}

ClassTypePtr ClassType::refine(at::ArrayRef<TypePtr> refined_slots) const {
  auto ptr = ClassType::create(name(), compilation_unit_);
  AT_ASSERT(numAttributes() == refined_slots.size());
  for (size_t i = 0; i < attributeNames_.size(); ++i) {
    AT_ASSERT(refined_slots[i]->isSubtypeOf(attributeTypes_[i]));
    ptr->addAttribute(attributeNames_[i], refined_slots[i]);
  }
  // Copy methods over
  for (const auto& method : methods()) {
    ptr->addMethod(method);
  }
  return ptr;
}

bool ClassType::isSubtypeOfExt(const TypePtr rhs, std::ostream* why_not) const {
  // to improve performance, this check can be cached
  if (auto iface = rhs->cast<InterfaceType>()) {
    // ClassType is not a subtype of InterfaceType if the InterfaceType is a
    // Module Interface Type but the Class Type is not a Module Class Type
    if (!is_module() && iface->is_module()) {
      if (why_not) {
        *why_not << "Class '" << python_str() << "' is not a subtype of "
                 << "the module interface '" << rhs->python_str()
                 << "' , only ScriptModule class can be subtype of module"
                 << " interface.\n";
      }
      return false;
    }
    for (const FunctionSchema& schema : iface->methods()) {
      auto self_method = getMethod(schema.name());
      if (!self_method) {
        if (why_not) {
          *why_not << "Class '" << python_str() << "' does not have method '"
                   << schema.name() << "' but '" << rhs->python_str()
                   << "' does.\n";
        }
        return false;
      }
      if (!self_method->getSchema().isSubtypeOf(
              schema, /*is_method=*/true, why_not)) {
        if (why_not) {
          *why_not << "Method on class '" << python_str()
                   << "' (1) is not compatible with interface '"
                   << rhs->python_str() << "' (2)\n"
                   << "  (1) " << self_method->getSchema() << "\n"
                   << "  (2) " << schema << "\n";
        }
        return false;
      }
    }
    return true;
  }
  return Type::isSubtypeOfExt(rhs, why_not);
}
#else
bool ClassType::isSubtypeOfExt(const TypePtr rhs, std::ostream* why_not) const {
  return Type::isSubtypeOfExt(rhs, why_not);
}
#endif
} // namespace c10
