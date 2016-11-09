// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBox_h
#define NGBox_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGConstraintSpace;
class NGFragment;
class NGLayoutAlgorithm;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents a node to be laid out.
class CORE_EXPORT NGBox final : public GarbageCollectedFinalized<NGBox> {
 public:
  explicit NGBox(LayoutObject*);

  // TODO(layout-ng): make it private and declare a friend class to use in tests
  explicit NGBox(ComputedStyle*);

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragment**);

  // Computes the value of min-content and max-content for this box.
  // The return value has the same meaning as for Layout.
  // If the underlying layout algorithm returns NotImplemented from
  // ComputeMinAndMaxContentSizes, this function will synthesize these sizes
  // using Layout with special constraint spaces.
  // It is not legal to interleave a pending Layout() with a pending
  // ComputeOrSynthesizeMinAndMaxContentSizes (i.e. you have to call Layout
  // often enough that it returns true before calling
  // ComputeOrSynthesizeMinAndMaxContentSizes)
  bool ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*);

  const ComputedStyle* Style() const;

  NGBox* NextSibling();

  NGBox* FirstChild();

  void SetNextSibling(NGBox*);
  void SetFirstChild(NGBox*);

  DECLARE_VIRTUAL_TRACE();

 private:
  // This is necessary for interop between old and new trees -- after our parent
  // positions us, it calls this function so we can store the position on the
  // underlying LayoutBox.
  void PositionUpdated();

  bool CanUseNewLayout();
  bool HasInlineChildren();

  // After we run the layout algorithm, this function copies back the geometry
  // data to the layout box.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&);

  // Runs layout on layout_box_ and creates a fragment for the resulting
  // geometry.
  NGPhysicalFragment* RunOldLayout(const NGConstraintSpace&);

  // We can either wrap a layout_box_ or a style_/next_sibling_/first_child_
  // combination.
  LayoutBox* layout_box_;
  RefPtr<ComputedStyle> style_;
  Member<NGBox> next_sibling_;
  Member<NGBox> first_child_;
  Member<NGLayoutAlgorithm> layout_algorithm_;
  Member<NGLayoutAlgorithm> minmax_algorithm_;
  Member<NGPhysicalFragment> fragment_;
};

}  // namespace blink

#endif  // NGBox_h
