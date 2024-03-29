#ifndef __BIN_LIBRARY_H__
#define __BIN_LIBRARY_H__

#include <memory>

#include "module.h"

namespace bin {

class Library {
public:
  static Module::ptr GetModule(const std::string &path);
};

} // namespace bin

#endif
