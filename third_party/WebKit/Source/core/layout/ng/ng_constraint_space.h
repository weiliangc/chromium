// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_constraint_space.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutBox;
class NGFragment;
class NGLayoutOpportunityIterator;

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within. It is a view on
// top of a NGPhysicalConstraintSpace and provides accessor methods in the
// logical coordinate system defined by the writing mode given.
class CORE_EXPORT NGConstraintSpace final
    : public GarbageCollected<NGConstraintSpace> {
 public:
  // Constructs a constraint space based on an existing backing
  // NGPhysicalConstraintSpace. Sets this constraint space's size to the
  // physical constraint space's container size, converted to logical
  // coordinates.
  NGConstraintSpace(NGWritingMode, NGDirection, NGPhysicalConstraintSpace*);

  // Constructs a constraint space with a new backing NGPhysicalConstraintSpace.
  // The size will be used for both for the physical constraint space's
  // container size and this constraint space's Size().
  // TODO(layout-dev): Remove once NGConstraintSpaceBuilder exists.
  NGConstraintSpace(NGWritingMode, NGDirection, NGLogicalSize);

  // Constructs a derived constraint space that shares the exclusions of the
  // input constraint space, but has a different container size, writing mode
  // and direction. Sets the offset to zero. For use by layout algorithms
  // to use as the basis to find layout opportunities for children.
  // TODO(layout-dev): Remove once NGConstraintSpaceBuilder exists.
  NGConstraintSpace(NGWritingMode,
                    NGDirection,
                    const NGConstraintSpace& other,
                    NGLogicalSize);

  // This should live on NGBox or another layout bridge and probably take a root
  // NGConstraintSpace or a NGPhysicalConstraintSpace.
  static NGConstraintSpace* CreateFromLayoutObject(const LayoutBox&);

  // Mutable Getters.
  // TODO(layout-dev): remove const constraint from MutablePhysicalSpace method
  NGPhysicalConstraintSpace* MutablePhysicalSpace() const {
    return physical_space_;
  }

  // Read-only Getters.
  const NGPhysicalConstraintSpace* PhysicalSpace() const {
    return physical_space_;
  }

  NGDirection Direction() const { return static_cast<NGDirection>(direction_); }

  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }

  // Adds the exclusion in the physical constraint space.
  // Passing the exclusion ignoring the writing mode is fine here since the
  // exclusion is set in physical coordinates.
  void AddExclusion(const NGExclusion* exclusion) const;

  // Size of the container. Used for the following three cases:
  // 1) Percentage resolution.
  // 2) Resolving absolute positions of children.
  // 3) Defining the threshold that triggers the presence of a scrollbar. Only
  //    applies if the corresponding scrollbarTrigger flag has been set for the
  //    direction.
  NGLogicalSize ContainerSize() const;

  // Offset relative to the root constraint space.
  NGLogicalOffset Offset() const { return offset_; }

  // Returns the effective size of the constraint space. Equal to the
  // ContainerSize() for the root constraint space but derived constraint spaces
  // return the size of the layout opportunity.
  virtual NGLogicalSize Size() const { return size_; }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const;

  // Whether exceeding the containerSize triggers the presence of a scrollbar
  // for the indicated direction.
  // If exceeded the current layout should be aborted and invoked again with a
  // constraint space modified to reserve space for a scrollbar.
  bool InlineTriggersScrollbar() const;
  bool BlockTriggersScrollbar() const;

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  bool FixedInlineSize() const;
  bool FixedBlockSize() const;

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const;

  // Modifies constraint space to account for a placed fragment. Depending on
  // the shape of the fragment this will either modify the inline or block
  // size, or add an exclusion.
  void Subtract(const NGFragment*);

  NGLayoutOpportunityIterator* LayoutOpportunities(
      unsigned clear = NGClearNone,
      bool for_inline_or_bfc = false);

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(physical_space_); }

  // The setters for the NGConstraintSpace should only be used when constructing
  // a derived NGConstraintSpace.
  void SetOverflowTriggersScrollbar(bool inlineTriggers, bool blockTriggers);
  void SetFixedSize(bool inlineFixed, bool blockFixed);
  void SetFragmentationType(NGFragmentationType);
  void SetIsNewFormattingContext(bool is_new_fc);

  String ToString() const;

 private:
  Member<NGPhysicalConstraintSpace> physical_space_;
  NGLogicalOffset offset_;
  NGLogicalSize size_;
  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGConstraintSpace& value) {
  return stream << value.ToString();
}

}  // namespace blink

#endif  // NGConstraintSpace_h
