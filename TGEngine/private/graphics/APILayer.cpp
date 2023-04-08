#include "../../public/graphics/APILayer.hpp"

namespace tge::graphics {

EntryHolder::EntryHolder(APILayer* api, const size_t internalHandle)
    : internalHandle(internalHandle), referenceID(api->nextCounter()) {
  return;
}
}  // namespace tge::graphics
