// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstddef>
#include <new>

namespace sense {

template <typename Base, typename... Types>
class CommonBaseUnion {
 public:
  static_assert((std::is_base_of_v<Base, Types> && ...));

  template <typename... Args>
  CommonBaseUnion(Args&&... args) {
    new (data) typename First<Types...>::type(std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  void emplace(Args&&... args) {
    // All types are trivially destructible.
    static_assert((std::is_same_v<T, Types> || ...));
    get().~Base();
    new (data) T(std::forward<Args>(args)...);
  }

  Base& get() { return *std::launder(reinterpret_cast<Base*>(data)); }
  const Base& get() const {
    return *std::launder(reinterpret_cast<const Base*>(data));
  }

 private:
  template <typename FirstType, typename...>
  struct First {
    using type = FirstType;
  };

  static constexpr size_t kSizeBytes = std::max({sizeof(Types)...});
  static constexpr size_t kAlignmentBytes = std::max({alignof(Types)...});

  alignas(kAlignmentBytes) std::byte data[kSizeBytes];
};

}  // namespace sense
