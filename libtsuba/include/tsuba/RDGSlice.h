#ifndef KATANA_LIBTSUBA_TSUBA_RDGSLICE_H_
#define KATANA_LIBTSUBA_TSUBA_RDGSLICE_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "katana/Result.h"
#include "katana/Uri.h"
#include "katana/config.h"
#include "tsuba/FileView.h"
#include "tsuba/tsuba.h"

namespace tsuba {

class RDGMeta;
class RDGCore;

/// A contiguous piece of an RDG
class KATANA_EXPORT RDGSlice {
public:
  RDGSlice(const RDGSlice& no_copy) = delete;
  RDGSlice& operator=(const RDGSlice& no_copy) = delete;

  ~RDGSlice();
  RDGSlice(RDGSlice&& other) noexcept;
  RDGSlice& operator=(RDGSlice&& other) noexcept;

  struct SliceArg {
    std::pair<uint64_t, uint64_t> node_range;
    std::pair<uint64_t, uint64_t> edge_range;
    uint64_t topo_off;
    uint64_t topo_size;
  };

  static katana::Result<RDGSlice> Make(
      RDGHandle handle, const SliceArg& slice,
      const std::vector<std::string>* node_props = nullptr,
      const std::vector<std::string>* edge_props = nullptr);

  static katana::Result<RDGSlice> Make(
      const std::string& rdg_meta_path, const SliceArg& slice,
      const std::vector<std::string>* node_props = nullptr,
      const std::vector<std::string>* edge_props = nullptr);

  const std::shared_ptr<arrow::Table>& node_table() const;
  const std::shared_ptr<arrow::Table>& edge_table() const;
  const FileView& topology_file_storage() const;

private:
  static katana::Result<RDGSlice> Make(
      const RDGMeta& meta, const std::vector<std::string>* node_props,
      const std::vector<std::string>* edge_props, const SliceArg& slice);

  RDGSlice(std::unique_ptr<RDGCore>&& core);

  katana::Result<void> DoMake(
      const katana::Uri& metadata_dir, const SliceArg& slice);

  //
  // Data
  //

  std::unique_ptr<RDGCore> core_;
};

}  // namespace tsuba

#endif
