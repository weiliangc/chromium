// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_CACHE_H_
#define CHROMEOS_PRINTING_PPD_CACHE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {
namespace printing {

// PpdCache manages a cache of locally-stored PPD files.  At its core, it
// operates like a persistent hash from PpdReference to files.  If you give the
// same PpdReference to Find() that was previously given to store, you should
// get the same FilePath back out (unless the previous entry has timed out of
// the cache).  However, changing *any* field in PpdReference will make the
// previous cache entry invalid.  This is the intentional behavior -- we want to
// re-run the resolution logic if we have new meta-information about a printer.
class CHROMEOS_EXPORT PpdCache {
 public:
  // Create and return a Ppdcache that uses cache_dir to store state.
  static std::unique_ptr<PpdCache> Create(const base::FilePath& cache_base_dir);
  virtual ~PpdCache() {}

  // Find a PPD that was previously cached with the given reference.  Note that
  // all fields of the reference must be the same, otherwise we'll miss in the
  // cache and re-run resolution for the PPD.
  //
  // If a FilePath is returned, it is guaranteed to be non-empty and
  // remain valid until the next Store() call.
  virtual base::Optional<base::FilePath> Find(
      const Printer::PpdReference& reference) const = 0;

  // Take the contents of a PPD file, store it to the cache, and return the
  // path to the stored file keyed on reference.
  //
  // If a different PPD was previously Stored for the given reference, it
  // will be replaced.
  //
  // If a FilePath is returned, it is guaranteed to be non-empty and
  // remain valid until the next Store() call.
  virtual base::Optional<base::FilePath> Store(
      const Printer::PpdReference& reference,
      const std::string& ppd_contents) = 0;
};

}  // namespace printing
}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_CACHE_H_
